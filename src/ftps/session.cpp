#include "session.h"
#include "../sqlite/sqlite_wrapper.h"
#include "../custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <stdexcept>
#include <functional>

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
        print("FTPS session cleanup\n", "green");

        if (control_socket.lowest_layer().is_open())
        {
            try
            { // try to ignore "Broken pipe" error, which happens when trying to send data over closed socket
                control_socket.shutdown();
            }
            catch (boost::system::system_error err)
            {
                //
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
        destroy_callback(s_id);
    }

    void session::send_message(std::string message)
    {
        message += "\r\n"; // all messages need to end in \r\n
        boost::asio::async_write(control_socket, boost::asio::buffer(message),
                                 [this](boost::system::error_code err, size_t bytes_transferred) {
                                     if (err)
                                     {
                                         print("async_write error -> " + err.what() + "\n");
                                         stop();
                                         return;
                                     }
                                     //  print("Sent message to control channel\n");
                                 });
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

                //    print("\"");
                //    print(sanitizedData);
                //    print("\"\n");

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
        {
            print("data channel not open yet\n", "yellow");
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
        // print("Sent data to data channel\n");
    }

    void session::handle_FTP_command(std::string &data)
    {
        std::vector<std::string> splitStr = custom_utils::splitString(std::string(data), ' ');
        const std::string command = splitStr.at(0);

        if (command_exists(command))
        {
            print("Known FTP command -> ", "blue");
            print(command);
            print("\n");
        }
        else
        {
            // print("FTP command not found -> " + command + "\n", "yellow");
            log_behavior(data);

            send_message("502 Command not recognized.");
            return;
        }

        // connection stage dependent commands
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
                // filezilla uses this to check if support non-ascii codes

                send_message("211 No features.");
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
                        print("Handshake size: " + std::to_string(socket.available()) + "\n");
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
                        }
                        print("Data socket TLS handshake completed successfully\n", "green");
                    }

                    { // sending the directory listing

                        data_channel_send(""); // use empty string to flush buffer

                        send_message("226 Transferred.");

                        // close data channel, so client knows directory listing is done
                        data_socket->shutdown();
                        data_socket->lowest_layer().close();

                        print("Directory list of \"" + working_directory + "\" sent\n", "green");
                    }
                });

                print("Data socket opened on port " + std::to_string(port) + "\n", "green");

                send_message(response_format);
                return;
            }

            if (command == "LIST")
            {
                send_message("150 Opening connection.");

                // todo: get file list from sqlite database
                std::vector<std::string> user_files;
                database.read_data("files", {}, user_files);

                std::vector<std::string> file_id_list;
                { // get file ids of this user
                    size_t i = 0;
                    while (i < user_files.size())
                    {
                        if (user_files.at(i + 1) != userid)
                        { // skip files that are not this user
                            i += 2;
                            continue;
                        }

                        // found user's file
                        file_id_list.push_back(user_files.at(i));
                        i += 2;
                    }
                }

                std::vector<std::string> user_files_metadata;
                database.read_data("files_metadata", {}, user_files_metadata);

                { // get file metadatas of this users' files
                    size_t i = 4;
                    while (i < user_files_metadata.size())
                    {
                        if (std::find(file_id_list.begin(), file_id_list.end(), user_files_metadata.at(i)) ==
                            file_id_list.end())
                        { // skip file metadatas that aren't this users'
                            i += 5;
                            continue;
                        }

                        if (user_files_metadata.at(i - 3) != working_directory)
                        { // skip if file_path is not current directory
                            i += 5;
                            continue;
                        }

                        // found user's file metadata
                        std::string listing_format = "";

                        // todo: check if is directory here, directory use "drwxr-xr-x"
                        listing_format = "-rw-r--r-- 1 " + username + " " + username + " ";
                        listing_format += user_files_metadata.at(i - 2) + " ";                      // file_size
                        listing_format += parse_metadata_time(user_files_metadata.at(i - 1)) + " "; // modified time
                        listing_format += user_files_metadata.at(i - 4);                            // file name

                        data_channel_send(listing_format);

                        // low priority todo: could erase the found file's file_id from the list each time one is
                        i += 5;
                    }
                    print("\n");
                }

                data_channel_send("-rw-r--r-- 1 user group 1024 Jan 22 10:00 example.txt");
                data_channel_send("-rw-r--r-- 1 user group 1024 Jan 22 10:00 test.txt");
                return;
            }
        }

        default: // client can use these commands anytime
            if (command == "SYST")
            {
                send_message("215 UNIX Type: L8"); // this is fine for now
                return;
            }

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

        print("1\n");
        parsedStr += time_str.substr(8, 2) + " ";

        print("2\n");
        parsedStr += time_str.substr(11, 5) + " ";

        return parsedStr;
    }

    void session::log_behavior(std::string log)
    {
        print("Unknown behavior -> \"" + log + "\", connection stage " + std::to_string(connection_stage) + "\n",
              "red");
    }
} // namespace session