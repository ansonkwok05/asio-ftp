#include "session.h"
#include "../sqlite/fs_handler.h"
#include "../sqlite/sqlite_wrapper.h"
#include "../custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <stdexcept>
#include <functional>
#include <fstream>

namespace session
{
    using custom_utils::print;

    session::session(boost::asio::ip::tcp::socket s, boost::asio::io_context &i_ctx, boost::asio::ssl::context &s_ctx)
        : control_socket(std::move(s), s_ctx), io_context(i_ctx), ssl_context(s_ctx)
    {
    }

    void session::start(std::function<void(std::string)> onDestroy, std::string session_id,
                        bool already_sent_welcome_message)
    {
        destroy_callback = onDestroy;
        s_id = session_id;

        boost::system::error_code err;
        control_socket.handshake(boost::asio::ssl::stream_base::handshake_type::server, err);
        if (err)
        {
            print("Control socket TLS handshake failed -> " + std::string(err.what()) + "\n", "red");

            stop();
            throw std::runtime_error("");
        }
        print("Control socket TLS handshake completed successfully\n");

        database.connect();

        if (!already_sent_welcome_message)
        {
            send_message(FTP_WELCOMEMESSAGE);
        }
        read_incoming_message();
    }

    void session::stop()
    {
        print("FTPS session stopping\n", "green");

        if (control_socket.lowest_layer().is_open())
        {
            try
            { // try to ignore "Broken pipe" error, which happens when trying to send data over closed socket
                control_socket.shutdown(); // test
            }
            catch (boost::system::system_error err)
            {
                // ignore shutdown errors
            }
            control_socket.lowest_layer().close();
        }

        if (acceptor)
        {
            if (acceptor->is_open())
                acceptor->close();

            acceptor.reset();
        }

        if (data_socket)
        {
            if (data_socket->lowest_layer().is_open())
                data_socket->lowest_layer().close();

            data_socket.reset();
        }

        // boost::asio::strand
        destroy_callback(s_id);
    }

    void session::send_message(std::string message)
    {
        message += "\r\n"; // all messages need to end in \r\n
        boost::asio::write(control_socket, boost::asio::buffer(message));
        // boost::asio::async_write(control_socket, boost::asio::buffer(message),
        //                          [this](boost::system::error_code err, size_t bytes_transferred) {
        //                              if (err)
        //                              {
        //                                  print("async_write error -> " + err.what() + "\n");
        //                                  stop();
        //                                  return;
        //                              }
        //                              //  print("Sent message to control channel\n");
        //                          });
        print("Control channel send -> " + message, "black");
    }

    void session::read_incoming_message()
    {
        char data[1024] = {0};
        control_socket.async_read_some(
            boost::asio::buffer(data, 1024), [this, &data](boost::system::error_code err, size_t bytes_transferred) {
                if (err)
                {
                    // client disconnected
                    if (err.message() == "stream truncated" || err.message() == "End of file" ||
                        err.message() == "Connection reset by peer" || err.message() == "Operation canceled")
                    {
                        print("FTPS client disconnected\n", "green");
                        stop();
                        return;
                    }

                    print("async_read_some error -> " + err.message() + "\n", "red");
                    stop();
                    return;
                }

                // remove unwanted data at the end of buffer
                // size of valid data is bytes_transferred
                // - 2 just to remove "\n" at the end of the message
                std::string sanitizedData = "";
                for (size_t i = 0; i < bytes_transferred - 2; i++)
                {
                    sanitizedData += data[i];
                }

                handle_FTP_command(sanitizedData);
                read_incoming_message();
            });
    }

    void session::data_channel_send(std::string message)
    {
        if (message != "") // ignore empty messages
        {
            pending_data_channel_buffer.push_back(message); // all messages need to end in \r\n
        }

        if (!data_socket || !data_socket->lowest_layer().is_open())
        { // accumulating buffer
            // print("data channel not open yet\n", "yellow");
            return;
        }

        // flush buffer and send
        std::string tempStr = "";
        for (std::string str : pending_data_channel_buffer)
        {
            tempStr += str + "\r\n";
        }
        pending_data_channel_buffer.clear();

        if (tempStr == "")
        { // nothing to send? (empty directory)
            // print("data_channel_send empty buffer\n", "yellow");
            return;
        }

        // blocking write
        boost::asio::write(*data_socket, boost::asio::buffer(tempStr));
        print("Data channel send -> " + tempStr, "black");
    }

    void session::data_channel_start_accept()
    {
        acceptor->async_accept([this](boost::system::error_code err, boost::asio::ip::tcp::socket socket) {
            if (err)
            {
                print("Failed to accept connection in data socket\n", "red");
                stop();
                return;
            }
            print("Data channel connection accepted\n", "green");

            { // wait for TLS ClientHello
                custom_utils::stopwatch stopwatch;
                stopwatch.start();
                // give up after WAIT_TIME
                print("Waiting for data channel handshake\n", "green");
                while (stopwatch.lapMs() < WAIT_TIME)
                {
                    // message length longer than this is TLS handshake??
                    // low priority todo: experiment with different length
                    if (socket.available() > 30)
                        break;
                    custom_utils::sleep(20);
                }
                print("Handshake size: " + std::to_string(socket.available()) + "\n", "black");
            }

            { // create ssl data_socket
                data_socket = std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
                    std::move(socket), ssl_context);
                print("ssl data_socket created\n", "green");
            }

            { // TLS handshake
                boost::system::error_code err;
                data_socket->handshake(boost::asio::ssl::stream_base::handshake_type::server, err);
                if (err)
                {
                    print("Data socket TLS handshake failed -> " + std::string(err.what()) + "\n", "red");
                    stop();
                    return;
                }
                print("Data socket TLS handshake completed successfully\n", "green");
            }

            if (pending_write_filename != "")
            { // write file operation
                std::string file_id = custom_utils::generate_uuid_string(16);
                int file_size = 0;

                std::ofstream output_file_stream("data/" + file_id);
                if (!output_file_stream.is_open())
                {
                    print("Error opening output file stream", "red");
                    return;
                }

                size_t buffer_size = 1024 * 1024;
                char data[buffer_size];
                while (true)
                {
                    boost::system::error_code ec;
                    size_t bytes_read = boost::asio::read(*data_socket, boost::asio::buffer(data, buffer_size), ec);

                    file_size += bytes_read;
                    output_file_stream.write(data, bytes_read);

                    if (output_file_stream.fail())
                    {
                        print("Error writing output file stream\n", "red");
                        return;
                    }

                    if (ec.message() == "End of file") // no more file data to read
                        break;
                }

                output_file_stream.close();

                database.insert_data("files", {"user_id", "file_id"}, {userid, file_id});

                database.insert_data("files_metadata",
                                     {
                                         "file_name",
                                         "file_path",
                                         "file_size",
                                         "is_directory",
                                         "file_id",
                                     },
                                     {
                                         pending_write_filename,
                                         working_directory,
                                         std::to_string(file_size),
                                         "0",
                                         file_id,
                                     });

                pending_write_filename = "";

                send_message("226 Received.");

                // close data channel, so client knows operation is done
                print("Data channel operation done, closing\n", "green");
                try
                {
                    data_socket->shutdown();
                }
                catch (boost::system::system_error err)
                {
                    // ignore shut down errors
                }
                data_socket->lowest_layer().close();
                return;
            }

            if (pending_read_filename != "")
            {
                // todo: implement this download feature
                // int file_size = 0;

                // std::ofstream output_file_stream("data/" + file_id);
                // if (!output_file_stream.is_open())
                // {
                //     print("Error opening output file stream", "red");
                //     return;
                // }

                // size_t buffer_size = 1024 * 1024;
                // char data[buffer_size];
                // while (true)
                // {
                //     boost::system::error_code ec;
                //     size_t bytes_written =
                //         boost::asio::write(*data_socket, boost::asio::buffer(data, buffer_size), ec);

                //     file_size += bytes_read;
                //     output_file_stream.write(data, bytes_read);

                //     if (output_file_stream.fail())
                //     {
                //         print("Error writing output file stream\n", "red");
                //         return;
                //     }

                //     if (ec.message() == "End of file") // no more file data to read
                //         break;
                // }

                // output_file_stream.close();
                // pending_read_filename = "";

                // send_message("226 Done.");
            }

            { // send directory listing operation

                data_channel_send(""); // use empty string to flush buffer

                send_message("226 Transferred.");

                // close data channel, so client knows operation is done
                print("Data channel operation done, closing\n", "green");
                try
                {
                    data_socket->shutdown();
                }
                catch (boost::system::system_error err)
                {
                    // ignore shut down errors
                }
                data_socket->lowest_layer().close();

                return;
            }
        });
    }

    void session::handle_FTP_command(std::string &data)
    {
        std::vector<std::string> splitStr = custom_utils::splitString(std::string(data), ' ');
        const std::string command = splitStr.at(0);

        if (!command_exists(command))
        {
            log_behavior(data);

            send_message("502 Command not recognized.");
            return;
        }

        print("Known FTP command -> " + command + "\n", "blue");

        { // commands usable anytime
            if (command == "SYST")
            {
                send_message("215 UNIX Type: L8"); // this is fine for now
                return;
            }

            if (command == "QUIT")
            {
                send_message("221 Closing.");
                stop();
                return;
            }
        }

        // commands depends on connection stage
        // do not allow file/folder access before authentication
        switch (connection_stage)
        {
        case UNAUTHENTICATED: { // only allow login commands
            if (command == "USER")
            {
                username = splitStr.at(1);

                std::vector<std::string> return_data;
                if (!database.read_data("users", {"user_id", "username"}, return_data))
                {
                    print("SQLite database error", "red");
                    return;
                }

                // search if username exists in database
                size_t i = 1;
                while (i < return_data.size())
                {
                    if (return_data.at(i) == username)
                    {
                        userid = return_data.at(i - 1);
                        print("Known username, asking client for password\n", "green");
                        send_message("331 Password required.");
                        return;
                    }
                    i += 2;
                }

                // username not found
                print("Unknown username -> " + username + "\n", "yellow");
                send_message("530 User not found.");
                return;
            }

            if (command == "PASS")
            {
                password = splitStr.at(1);

                std::vector<std::string> return_data;
                database.read_data("users", {"username", "password"}, return_data);

                size_t i = 0;
                while (i < return_data.size())
                {
                    // find database record of username
                    if (return_data.at(i) == username)
                    {
                        // username found, check password
                        if (return_data.at(i + 1) == password)
                        {
                            connection_stage = LOGGED_IN;
                            print("User \"" + username + "\" logged in.\n", "green");
                            send_message("230 User " + username + " logged in.");
                            return;
                        }
                        print("Wrong password from client trying to log into \"" + username + "\"\n", "yellow");
                        send_message("530 Wrong password.");
                        return;
                    }
                    i += 2;
                }

                send_message("530 Wrong password.");
                return;
            }
        }

        case LOGGED_IN: { // allow file access on that user, and more
            if (command == "FEAT")
            {
                // client requests info about all supported features of this FTP server
                // low priority todo: check what is this about

                send_message("211-Features:\nUTF8\n211 End");
                return;
            }

            if (command == "OPTS")
            {
                // low priority todo: add splitStr size check, avoid address boundary access error which will crash
                // low priority todo: make a flag that enables UTF8, find out how to implement utf8

                if (splitStr.at(1) == "UTF8" && splitStr.at(2) == "ON")
                { //"OPTS UTF8 ON"
                    send_message("200 Enabled UTF-8 encoding.");
                    return;
                }

                print("Unknown OPTS command -> " + data + "\n", "yellow");
                return;
            }

            if (command == "PBSZ")
            {
                // low priority todo: check what is this about

                PBSZ = splitStr.at(1);
                send_message("200 OK.");
                return;
            }

            if (command == "PROT")
            {
                // Protection level of the data connection
                //  C - Clear
                //  S - Safe
                //  E - Confidential
                //  P - Private

                // low priority todo: check what is this about

                if (PBSZ == "")
                {
                    send_message("503 No previous PBSZ issued.");
                    return;
                }

                // force "P" protection
                PROT = "P";
                send_message("200 OK.");
                return;
            }

            if (command == "PWD")
            {
                send_message("257 \"" + working_directory + "\"");
                return;
            }

            if (command == "TYPE")
            {
                // binary flag, reply 200 to accept
                // A: Turn the binary flag off.
                // A N: Turn the binary flag off.
                // I: Turn the binary flag on.
                // L 8: Turn the binary flag on.

                // low priority todo: check what is this about
                // not implemented yet, but might need a lookup table that converts characters binary?

                TYPE = splitStr.at(1); // perhaps also need the second argument
                send_message("200 OK.");
                return;
            }

            if (command == "PASV")
            {
                // prepare acceptor for data_socket connection acception
                acceptor = std::make_shared<boost::asio::ip::tcp::acceptor>(io_context);

                boost::system::error_code err;
                acceptor->open(boost::asio::ip::tcp::v4(), err);

                // choosing a port that isn't in use
                int port = 6921;
                do
                {
                    port++;
                    acceptor->bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), err);
                } while (err);
                acceptor->listen();

                // forming response
                std::string response_format = "227 Entering Passive Mode (";
                response_format += custom_utils::replaceString(
                    control_socket.next_layer().remote_endpoint().address().to_string(), ".", ",");
                response_format += "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";

                data_channel_start_accept();
                print("Data socket opened on port " + std::to_string(port) + "\n", "green");
                send_message(response_format);
                return;
            }

            if (command == "LIST")
            {
                std::vector<std::string> file_metadata_list = get_files_metadatas();

                { // send files metadata that is in current working directory
                    size_t i = 1;
                    while (i < file_metadata_list.size())
                    {
                        if (file_metadata_list.at(i) == working_directory)
                        { // send this
                            std::string listing_format = "";

                            if (file_metadata_list.at(i + 3) == "0")
                            { // is file
                                listing_format += "-rw-r--r-- 1 ";
                            }
                            else if (file_metadata_list.at(i + 3) == "1")
                            { // is directory
                                listing_format += "drwxr-xr-x 1 ";
                            }
                            listing_format += username + " " + username + " ";
                            listing_format += file_metadata_list.at(i + 1) + " ";                      // file_size
                            listing_format += parse_metadata_time(file_metadata_list.at(i + 2)) + " "; // modified_time
                            listing_format += file_metadata_list.at(i - 1);                            // file_name

                            data_channel_send(listing_format);
                        }

                        i += 6;
                    }

                    print("Directory list of \"" + working_directory + "\" sent\n", "green");

                    send_message("150 Opening connection.");
                    return;
                }
            }

            if (command == "CWD")
            {
                if (splitStr.size() == 1)
                { // no argument
                    send_message("501 No arguments presented.");
                    return;
                }

                if (splitStr.at(1) == "")
                { // empty string argument
                    send_message("501 No arguments presented.");
                    return;
                }

                if (splitStr.at(1) == "..")
                {
                    print("working_directory 1: \"" + working_directory + "\" -> \"" +
                          get_last_slash(working_directory) + "\"\n");
                    working_directory = get_last_slash(working_directory);
                    send_message("250 Changed working directory.");
                    return;
                }

                // grouping command argument
                std::vector<std::string> args_vector(splitStr.begin() + 1, splitStr.end());
                std::string command_arg = custom_utils::vectorStrJoin(args_vector, " ");

                bool is_absolute_path = false;
                if (command_arg.at(0) == '/')
                {
                    is_absolute_path = true;
                }

                if (is_absolute_path && command_arg == "/")
                { // root directory
                    working_directory = "/";
                    send_message("250 Changed working directory.");
                    return;
                }

                std::string path = "";
                std::string name = "";

                if (is_absolute_path)
                {
                    path = get_last_slash(command_arg);
                    // name = command_arg.substr(path.size(), command_arg.size() - path.size()); // doesnt parse well
                    std::vector<std::string> tempVec = custom_utils::splitString(command_arg, '/');
                    name = tempVec.at(tempVec.size() - 1);
                }
                else
                {
                    path = working_directory;
                    name = command_arg;
                }

                print(command_arg + " " + path + " " + name + "\n");

                std::vector<std::string> metadatas = get_files_metadatas();

                // give different response according to the directory's existance
                size_t i = 0;
                while (i < metadatas.size())
                {
                    if (metadatas.at(i) == name && metadatas.at(i + 1) == path)
                    { // folder exists
                        std::string full_path = "";

                        // something about path doesn't end with '/' but root is '/' so it can end with '/'
                        if (path == "/")
                        {
                            full_path = path + name;
                        }
                        else
                        {
                            full_path = path + "/" + name;
                        }
                        print("working_directory 2: " + working_directory + " -> " + full_path + "\n");
                        working_directory = full_path;
                        send_message("250 Changed working directory.");
                        return;
                    }
                    i += 6;
                }

                print("Cannot change working directory: " + path + name + ", not found\n");
                send_message("550 No such file or directory.");
                return;
            }

            if (command == "CDUP")
            {
                working_directory = get_last_slash(working_directory);
                send_message("250 Okay.");
                return;
            }

            if (command == "MKD")
            { // creates a folder in current working directory
                if (splitStr.size() == 1)
                {
                    print("Cannot make directory, no arguments", "yellow");
                    send_message("501 No arguments presented.");
                    return;
                }

                std::vector<std::string> args_vector(splitStr.begin() + 1, splitStr.end());
                std::string folder_name = custom_utils::vectorStrJoin(args_vector, " ");

                if (folder_name == "")
                { // client want to create empty folder name?
                    // low priority todo: return response
                    print("Client is trying to create folder with no name\n", "yellow");
                    return;
                }

                std::vector<std::string> metadatas = get_files_metadatas();

                size_t i = 0; // file_name
                while (i < metadatas.size())
                {
                    if (metadatas.at(i) == folder_name && metadatas.at(i + 1) == working_directory)
                    { // folder already exists
                        send_message("550 Directory already exist.");
                        return;
                    }
                    i += 6;
                }

                print("Creating virtual directory \"" + working_directory + "\" \"" + folder_name + "\"\n", "green");

                std::string folder_id = custom_utils::generate_uuid_string(16);

                database.insert_data("files",
                                     {
                                         "file_id",
                                         "user_id",
                                     },
                                     {
                                         folder_id,
                                         userid,
                                     });

                database.insert_data("files_metadata",
                                     {
                                         "file_name",
                                         "file_path",
                                         "file_size",
                                         "is_directory",
                                         "file_id",
                                     },
                                     {
                                         folder_name,
                                         working_directory,
                                         "0",
                                         "1",
                                         folder_id,
                                     });

                send_message("250 Directory created.");
                return;
            }

            if (command == "RMD")
            { // similar to DELE
                if (splitStr.size() == 1)
                { // no argument
                    send_message("501 No arguments presented.");
                    return;
                }

                // grouping command argument
                std::vector<std::string> args_vector(splitStr.begin() + 1, splitStr.end());
                std::string target_name = custom_utils::vectorStrJoin(args_vector, " ");

                if (target_name == "")
                {
                    send_message("501 No arguments presented.");
                    return;
                }

                std::vector<std::string> metadatas = get_files_metadatas();

                size_t i = 0;
                while (i < metadatas.size())
                {
                    if (metadatas.at(i) == target_name && metadatas.at(i + 1) == working_directory)
                    { // found

                        // todo: recursively delete everything in this folder
                        // make sure everything is destroyed, no left over in database

                        if (database.delete_data("files", "file_id", metadatas.at(i + 5)) &&
                            database.delete_data("files_metadata", "file_id", metadatas.at(i + 5)))
                        { // if successfully deleted data from database
                            send_message("250 Deleted.");
                            return;
                        }

                        // something went wrong when trying to delete data from database
                        send_message("550 Backend error.");
                        return;

                        // cannot delete file from os
                        send_message("550 Unsuccessful delete.");
                        return;
                    }
                    i += 6;
                }

                send_message("550 Folder not found.");
                return;
            }

            if (command == "STOR")
            {
                if (splitStr.size() == 1)
                { // no argument
                    send_message("501 No arguments presented.");
                    return;
                }

                if (pending_write_filename != "")
                {
                    print("Previous write operation not finished -> " + pending_write_filename, "red");
                    return;
                }

                // grouping command argument
                std::vector<std::string> args_vector(splitStr.begin() + 1, splitStr.end());
                std::string file_name = custom_utils::vectorStrJoin(args_vector, " ");

                if (file_name == "")
                { // client trying to send a file with empty filename
                    send_message("501 No arguments presented.");
                    return;
                }

                pending_write_filename = file_name;
                send_message("150 Waiting for connection");
                return;
            }

            if (command == "RETR")
            {
                if (pending_read_filename != "")
                {
                    print("Previous read operation not finished -> " + pending_read_filename, "red");
                    return;
                }

                pending_read_filename = splitStr.at(1);
                print(data + "\n");
                send_message("150 Waiting for connection");
                return;
            }

            if (command == "DELE")
            { // similar to RMD
                if (splitStr.size() == 1)
                { // no argument
                    send_message("501 No arguments presented.");
                    return;
                }

                // grouping command argument
                std::vector<std::string> args_vector(splitStr.begin() + 1, splitStr.end());
                std::string target_name = custom_utils::vectorStrJoin(args_vector, " ");

                if (target_name == "")
                {
                    send_message("501 No arguments presented.");
                    return;
                }

                std::vector<std::string> metadatas = get_files_metadatas();

                size_t i = 0;
                while (i < metadatas.size())
                {
                    if (metadatas.at(i) == target_name && metadatas.at(i + 1) == working_directory)
                    { // found
                        if (fs_handler::remove_file("data/" + metadatas.at(i + 5)))
                        { // success
                            if (database.delete_data("files", "file_id", metadatas.at(i + 5)) &&
                                database.delete_data("files_metadata", "file_id", metadatas.at(i + 5)))
                            { // if successfully deleted data from database
                                send_message("250 Deleted.");
                                return;
                            }

                            // something went wrong when trying to delete data from database
                            send_message("550 Backend error.");
                            return;
                        }

                        // cannot delete file from os
                        send_message("550 Unsuccessful delete.");
                        return;
                    }
                    i += 6;
                }

                send_message("550 File not found.");
                return;
            }
        }

        default:
            log_behavior(data); // unknown command
        }
        return;
    }

    bool session::command_exists(std::string command)
    {
        if (std::find(FTP_COMMANDS.begin(), FTP_COMMANDS.end(), command) == FTP_COMMANDS.end())
            return false;
        return true;
    }

    std::string session::parse_metadata_time(std::string time_str)
    {
        // format: 2025-08-28 05:12:43 -> Jan 22 10:00

        std::string parsedStr = "";

        switch (time_str.at(5))
        {
        case '0': { // < 10
            switch (time_str.at(6))
            {
            case '1': { // 01
                parsedStr += "Jan ";
                break;
            }
            case '2': { // 02
                parsedStr += "Feb ";
                break;
            }
            case '3': { // 03
                parsedStr += "Mar ";
                break;
            }
            case '4': { // 04
                parsedStr += "Apr ";
                break;
            }
            case '5': { // 05
                parsedStr += "May ";
                break;
            }
            case '6': { // 06
                parsedStr += "Jun ";
                break;
            }
            case '7': { // 07
                parsedStr += "July ";
                break;
            }
            case '8': { // 08
                parsedStr += "Aug ";
                break;
            }
            case '9': { // 09
                parsedStr += "Sep ";
                break;
            }
            }
            break;
        }
        case '1': { // >= 10
            switch (time_str.at(6))
            {
            case '1': { // 11
                parsedStr += "Nov ";
                break;
            }
            case '2': { // 12
                parsedStr += "Dec ";
                break;
            }
            }
            break;
        }
        }

        // adding the day and time
        parsedStr += time_str.substr(8, 2) + " ";
        parsedStr += time_str.substr(11, 5) + " ";

        return parsedStr;
    }

    std::string session::generate_metadata_currentTime()
    {
        std::time_t result = std::time(nullptr);
        std::string time_string = std::string(std::asctime(std::gmtime(&result)));
        return time_string.substr(4, 12);
    }

    std::string session::get_last_slash(std::string directory)
    {
        if (directory == "/")
        { // cannot get last slash when is root directory
            return "/";
        }

        std::vector<std::string> split_directory = custom_utils::splitString(directory, '/');

        if (split_directory.size() < 2)
        { // input doesn't have any '/'
            return "/";
        }

        std::string last_slash = "";
        for (int i = 0; i < split_directory.size() - 1; i++)
        {
            if (split_directory.at(i) == "")
            { // ignore empty strings, they were "/" before splitting
                continue;
            }
            last_slash += "/" + split_directory.at(i);
        }

        if (last_slash == "")
        { // root dir
            return "/";
        }

        return last_slash;
    }

    std::vector<std::string> session::get_files_metadatas()
    {
        std::vector<std::string> file_id_list;
        { // get file ids of this user
            std::vector<std::string> files;
            database.read_data("files", {}, files);

            size_t i = 1;
            while (i < files.size())
            {
                if (files.at(i) == userid)
                { // found user's file
                    file_id_list.push_back(files.at(i - 1));
                }
                i += 2;
            }
        }

        std::vector<std::string> file_metadata_list;
        { // get file metadatas that matches this users' file ids
            std::vector<std::string> files_metadata;
            database.read_data("files_metadata", {}, files_metadata);

            size_t i = 5;
            while (i < files_metadata.size())
            {
                bool found_file = false;
                for (size_t n = 0; n < file_id_list.size(); n++)
                {
                    if (files_metadata.at(i) == file_id_list.at(n))
                    {
                        // low priority todo: remove found file id from file_id_list?
                        found_file = true;
                        break;
                    }
                }

                if (found_file)
                { // matches
                    file_metadata_list.push_back(files_metadata.at(i - 5));
                    file_metadata_list.push_back(files_metadata.at(i - 4));
                    file_metadata_list.push_back(files_metadata.at(i - 3));
                    file_metadata_list.push_back(files_metadata.at(i - 2));
                    file_metadata_list.push_back(files_metadata.at(i - 1));
                    file_metadata_list.push_back(files_metadata.at(i));
                }
                i += 6;
            }
        }

        return file_metadata_list;
    }

    void session::log_behavior(std::string log)
    {
        print("Unknown behavior -> \"" + log + "\", connection stage " + std::to_string(connection_stage) + "\n",
              "red");
    }
} // namespace session