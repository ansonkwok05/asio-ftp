#include "base_session.h"
#include "constants.h"
#include "../helpers.h"
#include "../custom_utils.h"

#include <string>
#include <fstream>

#include <boost/asio/error.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/error.hpp>

base_session::base_session(boost::asio::any_io_executor executor)
    : m_session_id(generate_uuid_string(8)),
      m_data_socket_acceptor(executor),
      m_buffer(MESSAGE_BUFFER_SIZE),
      m_receive_buffer(RECEIVE_BUFFER_SIZE)
{
    // prepare acceptor for data_socket connection acception
    boost::system::error_code ec = m_data_socket_acceptor.open(boost::asio::ip::tcp::v4(), ec);

    // choosing a port starting from this
    int port = DATA_CHANNEL_BEGIN_PORT;
    while (true)
    {
        ec = m_data_socket_acceptor.bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), ec);

        // break when successfully bind to a usable port
        if (ec.message() == "Success")
            break;

        port++;
    }
    m_data_socket_acceptor.listen();
}

base_session::~base_session()
{
    println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
}

void base_session::handle_control_send_callback(boost::system::error_code ec, size_t bytes_written)
{
    if (ec)
    {
        if (ec != boost::asio::error::broken_pipe)
        {
            println("Unknown async_write error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        }

        // disconnect
        return;
    }
}

std::pair<std::string, std::string> base_session::handle_control_receive_callback(boost::system::error_code ec,
                                                                                  size_t bytes_received)
{
    if (ec)
    {
        if (ec != boost::asio::error::eof && ec != boost::asio::ssl::error::stream_truncated)
        {
            println("Unknown async_read_some error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        }

        // disconnected
        return std::pair<std::string, std::string>();
    }

    // received incomplete message (message must contain at least "\r\n")
    if (bytes_received <= 2)
    {
        return std::pair<std::string, std::string>();
    }

    return parse_buffer(bytes_received);
}

void base_session::handle_command(const std::string &command, const std::string &argument)
{
    if (command == "")
        return;

    // check if command is supported
    if (std::find(FTP_COMMANDS.begin(), FTP_COMMANDS.end(), command) == FTP_COMMANDS.end())
    {
        println("Unknown command -> " + command, custom_utils::COLOR::YELLOW);
        control_send("502 Command not implemented.");
        control_receive();
        return;
    }

    println(command + " -> \"" + argument + "\"", custom_utils::COLOR::BLUE);

    // UNAUTHENTICATED
    {
        if (command == "USER")
        {
            if (argument == "")
            {
                // argument is empty
                println("Unknown username -> \"" + argument + "\"", custom_utils::COLOR::YELLOW);
                control_send("530 Invalid username.");
                control_receive();
                return;
            }

            // get user_id by name from database users table
            std::string uid = m_user.get_id_by_name(argument);

            if (uid == "")
            {
                // username does not exist in database
                println("Unknown username -> \"" + argument + "\"", custom_utils::COLOR::YELLOW);
                control_send("530 Invalid username.");
                control_receive();
                return;
            }

            m_userid = uid;
            m_username = argument;

            println("Known username, need password", custom_utils::COLOR::GREEN);
            control_send("331 Username okay, password needed.");
            control_receive();
            return;
        }

        if (command == "PASS")
        {
            if (m_userid == "")
            {
                control_send("530 User not found.");
                control_receive();
                return;
            }

            if (m_user.check_password(m_userid, argument))
            {
                m_connection_stage = CONNECTION_STAGE::LOGGED_IN;
                println("User \"" + m_username + "\" logged in", custom_utils::COLOR::GREEN);
                control_send("230 Logged in.");
                control_receive();
                return;
            }

            println("Wrong password from client trying to login as \"" + m_username + "\"",
                    custom_utils::COLOR::YELLOW);
            control_send("530 Wrong password.");
            control_receive();
            return;
        }

        if (command == "SYST")
        {
            control_send("215 UNIX Type: L8");
            control_receive();
            return;
        }

        if (command == "QUIT")
        {
            control_send("221 Closing control connection.");
            control_close();
            return;
        }
    }

    // deny access to unauthenicated clients
    if (m_connection_stage != CONNECTION_STAGE::LOGGED_IN)
    {
        control_send("530 Not logged in.");
        control_receive();
        return;
    }

    {
        if (command == "PWD")
        {
            control_send("257 \"" + m_working_directory + "\"");
            control_receive();
            return;
        }

        if (command == "TYPE")
        {
            // binary flag, reply 200 to accept
            // A: Turn the binary flag off.
            // A N: Turn the binary flag off.
            // I: Turn the binary flag on.
            // L 8: Turn the binary flag on.

            // only supports binary anyways
            control_send("200 OK.");
            control_receive();
            return;
        }

        if (command == "PASV")
        {
            run_PASV();
            return;
        }

        if (command == "LIST")
        {
            run_LIST(argument);
            return;
        }

        if (command == "FEAT")
        {
            control_send("211-Features:\r\n AUTH TLS\r\n PASV\r\n PBSZ\r\n PROT\r\n UTF8\r\n211 End");
            control_receive();
            return;
        }

        if (command == "PBSZ")
        {
            control_send("200 OK.");
            control_receive();
            return;
        }

        if (command == "PROT")
        {
            control_send("200 OK.");
            control_receive();
            return;
        }

        if (command == "OPTS")
        {
            if (argument == "UTF8 ON")
            {
                control_send("200 Enabled UTF-8 encoding.");
                control_receive();
                return;
            }

            println("Unknown OPTS argument -> \"" + argument + "\"", custom_utils::COLOR::YELLOW);
            control_receive();
            return;
        }

        if (command == "CDUP")
        {
            m_working_directory = get_parent_path(m_working_directory);

            control_send("250 Okay.");
            control_receive();
            return;
        }

        if (command == "CWD")
        {
            // check empty argument
            if (argument == "")
            {
                control_send("501 No arguments presented.");
                control_receive();
                return;
            }

            // return to last slash
            if (argument == "..")
            {
                m_working_directory = get_parent_path(m_working_directory);
                control_send("250 Changed working directory.");
                control_receive();
                return;
            }

            // change directory to root
            if (argument == "/")
            {
                m_working_directory = "/";
                control_send("250 Changed working directory.");
                control_receive();
                return;
            }

            // handle multiple types of arguments
            // type 1 "/test/one"   // absolute path
            // type 2 "test/one"    // absolute path using current working directory as parent

            std::string object_path = "";
            std::vector<std::string> path_segments = string_split(argument, "/");

            if (argument[0] == '/')
            {
                // type 1
                // use root as first path
                object_path = "/";

                // remove first element because of "/"
                path_segments.erase(path_segments.begin());
            }
            else
            {
                // type 2
                // use working_directory as first path
                object_path = m_working_directory;
            }

            // validate if each subdirectory exists
            {
                size_t i = 0; // path iterator

                // check if first subdirectory exists
                if (m_virtual_fs.get_object(m_userid, path_segments[0], object_path).size() == 0)
                {
                    println("Cannot change working directory: " + object_path + " -> " + path_segments[0] +
                                ", not found",
                            custom_utils::COLOR::YELLOW);
                    control_send("550 No such file or directory.");
                    control_receive();
                    return;
                }

                // prepare cumulative_path before processing
                // '/' will be added in loop
                if (object_path == "/")
                {
                    object_path.pop_back();
                }

                // check each subdirectory in order
                while (i < path_segments.size() - 1)
                {
                    object_path += "/";
                    object_path += path_segments[i];

                    if (m_virtual_fs.get_object(m_userid, path_segments[i + 1], object_path).size() == 0)
                    {
                        println("Cannot change working directory: " + object_path + " -> " + path_segments[i + 1] +
                                    ", not found",
                                custom_utils::COLOR::YELLOW);
                        control_send("550 No such file or directory.");
                        control_receive();
                        return;
                    }

                    i++;
                }
            }

            // set new working directory
            m_working_directory = object_path;
            if (object_path != "/")
            {
                m_working_directory += "/";
            }
            m_working_directory += path_segments.back();

            control_send("250 Changed working directory.");
            control_receive();
            return;
        }

        if (command == "MKD")
        {
            if (argument == "")
            {
                println("Cannot make directory, no arguments", custom_utils::COLOR::YELLOW);
                control_send("501 No arguments presented.");
                control_receive();
                return;
            }

            // todo: recursively create directory
            // e.g.
            // working_directory = "/test files (small)/New Folder"
            // argument = "test"
            // check "/" has "test files (small)", if not then create
            // check "/test files (small)" has "/test files (small)/New Folder", if not then create

            // todo: handle absolute path (/New Folder)

            // handle multiple types of arguments
            // type 1 "one"         // object name only
            // type 2 "test/one"    // absolute path but missing "/" at front
            std::string object_path;
            std::string object_name;
            if (string_split(argument, "/").size() == 1)
            {
                // type 1

                object_path = m_working_directory;
                object_name = argument;
            }
            else
            {
                // type 2

                object_path = get_parent_path("/" + argument);
                object_name = get_basename(argument);
            }

            std::string vobj_id = m_virtual_fs.create_virtual_object(m_userid, object_name, object_path, 0, true);

            // check if created successfully
            if (vobj_id == "")
            {
                println("Unable to create virtual object", custom_utils::COLOR::YELLOW);
                control_send("250 Failed.");
                control_receive();
                return;
            }

            println("Created virtual directory \"" + object_path + "\" \"" + object_name + "\"",
                    custom_utils::COLOR::GREEN);

            control_send("250 Directory created.");
            control_receive();
            return;
        }

        if (command == "RMD")
        {
            // no argument
            if (argument == "")
            {
                control_send("501 No arguments presented.");
                control_receive();
                return;
            }

            // handle multiple types of arguments
            // type 1 "one"         // object name only
            // type 2 "test/one"    // absolute path but missing "/" at front
            // type 3 "/test"       // absolute path
            std::string object_path;
            std::string object_name;

            if (string_split(argument, "/").size() == 1)
            {
                // type 1

                object_path = m_working_directory;
                object_name = argument;
            }
            else if (argument[0] != '/')
            {
                // type 2

                object_path = get_parent_path("/" + argument);
                object_name = get_basename(argument);
            }
            else
            {
                // type 3

                object_path = get_parent_path(argument);
                object_name = get_basename(argument);
            }

            m_virtual_fs.remove_virtual_object(m_userid, object_name, object_path);
            control_send("250 Deleted.");

            control_receive();
            return;
        }

        if (command == "DELE")
        {
            if (argument == "")
            {
                control_send("501 No arguments presented.");
                control_receive();
                return;
            }

            // handle multiple types of arguments
            // type 1 "one"         // object name only
            // type 2 "test/one"    // absolute path but missing "/" at front

            std::string object_path;
            std::string object_name;

            if (string_split(argument, "/").size() == 1)
            {
                // type 1

                object_path = m_working_directory;
                object_name = argument;
            }
            else
            {
                // type 2

                object_path = get_parent_path("/" + argument);
                object_name = get_basename(argument);
            }

            m_virtual_fs.remove_virtual_object(m_userid, object_name, object_path);
            control_send("250 Deleted.");

            control_receive();
            return;
        }

        if (command == "STOR")
        {
            run_STOR(argument);
            return;
        }

        if (command == "RETR")
        {
            parse_RETR_argument(argument);
            run_RETR();
            return;
        }

        if (command == "SIZE")
        {
            std::string object_path = get_parent_path(argument);
            std::string object_name = get_basename(argument);

            std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, object_name, object_path);
            if (v_obj.size() == 0)
            {
                // file does not exists
                println("Client requested -> " + argument + " which does not exists", custom_utils::COLOR::RED);
                control_send("550 File unavailable.");
                return;
            }

            control_send("213 " + v_obj[3]);
            return;
        }
    }

    println("Known but unprocessed command -> \"" + command + "\"", custom_utils::COLOR::RED);
}

void base_session::parse_RETR_argument(const std::string &argument)
{
    // no argument
    if (argument == "")
    {
        control_send("501 No arguments presented.");
        control_receive();
        return;
    }

    if (m_sendable_file_id != "")
    {
        println("Previous send file operation not finished -> " + m_sendable_file_id, custom_utils::COLOR::RED);
        return;
    }

    // handle multiple types of arguments
    // type 1 "one"         // object name only
    // type 2 "/test/one"   // absolute path
    // type 3 "test/one"    // relative path

    std::string object_path;
    std::string object_name;

    if (string_split(argument, "/").size() == 1)
    {
        // type 1
        object_path = m_working_directory;
        object_name = argument;
    }
    else if (string_starts_with(argument, "/"))
    {
        // type 2
        object_path = get_parent_path(argument);
        object_name = get_basename(argument);
    }
    else
    {
        // type 3
        object_path = get_parent_path(m_working_directory + argument);
        object_name = get_basename(argument);
    }

    std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, object_name, object_path);
    if (v_obj.size() == 0)
    {
        // file does not exists
        println("Client requested -> " + argument + " which does not exists", custom_utils::COLOR::RED);
        control_send("550 File unavailable.");
        return;
    }

    m_sendable_file_id = v_obj[0];
}

void base_session::handle_data_send_callback(boost::system::error_code ec, size_t bytes_sent)
{
    if (ec)
    {
        println("Unknown async_write error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        return;
    }

    // inform client through control channel that data channel finished
    m_pending_directory_list = "";
    control_send("226 Transferred.");
    data_close();
}

void base_session::data_directory_listing()
{
    std::vector<std::string> v_object_list = m_virtual_fs.get_object_list(m_userid);

    // send directory list over data channel
    data_send(create_directory_list(v_object_list, m_pending_directory_list, m_username, m_pending_directory_list_all));

    m_pending_directory_list_all = false;
}

void base_session::data_receive_file()
{
    m_receive_file_path = get_parent_path(m_pending_write_file);
    m_received_file_size = 0;
    m_receive_file_name = get_basename(m_pending_write_file);

    // check if virtual object exists already
    std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, m_receive_file_name, m_receive_file_path);
    if (v_obj.size() == 0)
    {
        // virtual object not found, create one
        std::string created_file_id =
            m_virtual_fs.create_virtual_object(m_userid, m_receive_file_name, m_receive_file_path, 0, false);
        println("creating new file", custom_utils::COLOR::CYAN);
        m_receive_file_stream =
            std::make_unique<std::ofstream>("data/" + created_file_id, std::ios_base::app | std::ios::binary);
    }
    else
    {
        // virtual object found in db
        std::string existing_file_id = v_obj[0];
        m_received_file_size = std::stoll(v_obj[3]);
        println("appending existing file", custom_utils::COLOR::CYAN);
        m_receive_file_stream = std::make_unique<std::ofstream>("data/" + existing_file_id, std::ios::binary);
    }

    if (!m_receive_file_stream->is_open())
    {
        println("Error opening output file stream", custom_utils::COLOR::RED);
        data_close();
        return;
    }

    // start receiving file
    data_async_receive();
}

void base_session::handle_data_async_receive_callback(boost::system::error_code ec, size_t bytes_read)
{
    m_received_file_size += bytes_read;

    // write to file if received any file data
    if (bytes_read > 0)
    {
        *m_receive_file_stream << &m_receive_buffer;
    }

    // error writing file
    if (m_receive_file_stream->fail())
    {
        println("Error writing output file stream", custom_utils::COLOR::RED);
        data_async_receive_end();
        return;
    }

    // entire file is received
    if (ec == boost::asio::error::eof)
    {
        data_async_receive_end();
        return;
    }

    // maybe client disconnect or something
    if (ec.message() != "Success")
    {
        println("Unexpected error while receiving file data -> " + ec.message(), custom_utils::COLOR::YELLOW);
        data_async_receive_end();
        return;
    }

    data_async_receive();
}

void base_session::data_async_receive_end()
{
    m_receive_file_stream->close();
    m_receive_file_stream.reset();

    println("updating object metadata in db", custom_utils::COLOR::CYAN);
    m_virtual_fs.update_virtual_object(m_userid, m_receive_file_name, m_receive_file_path, m_received_file_size);

    // allow another file to be received
    m_pending_write_file = "";

    // notify client
    control_send("226 Received.");

    // close data channel
    data_close();
}

void base_session::data_send_file()
{
    m_send_file_stream = std::make_unique<std::ifstream>("data/" + m_sendable_file_id, std::ios::binary);
    if (!m_send_file_stream->is_open())
    {
        println("Unable to open read file stream of -> \"./data/" + m_sendable_file_id + "\"",
                custom_utils::COLOR::RED);
        data_close();
        return;
    }

    // start sending file
    data_async_send();
}

void base_session::handle_data_async_send_callback(boost::system::error_code ec, size_t bytes_sent)
{
    // entire file is read
    if (m_send_file_stream->eof())
    {
        data_async_send_end();
        return;
    }

    if (ec == boost::asio::error::broken_pipe)
    {
        println("Data channel disconnected mid file send", custom_utils::COLOR::YELLOW);
        data_async_send_end();
        return;
    }

    if (ec.message() != "Success")
    {
        println("Unexpected error while sending file data -> " + ec.message(), custom_utils::COLOR::YELLOW);
        data_async_send_end();
        return;
    }

    data_async_send();
}

void base_session::data_async_send_end()
{
    m_send_file_stream->close();
    m_send_file_stream.reset();

    // allow another file to be sent
    m_sendable_file_id = "";

    // notify client
    control_send("226 Sent.");

    // close data channel
    data_close();
}

void base_session::println(const std::string &message)
{
    custom_utils::println("[" + m_session_type + "] [" + m_session_id + "] " + message);
}

void base_session::println(const std::string &message, custom_utils::COLOR color)
{
    custom_utils::println("[" + m_session_type + "] [" + m_session_id + "] " + message, color);
}

std::pair<std::string, std::string> base_session::parse_buffer(size_t bytes_received)
{
    std::vector<std::string> splitted =
        string_split(std::string(m_buffer.begin(), m_buffer.begin() + bytes_received - 2), " ");

    std::string FTP_command = splitted[0];
    std::string FTP_argument = "";

    splitted.erase(splitted.begin());

    // received message has an argument
    if (splitted.size() > 0)
        FTP_argument = string_join(splitted, " ");

    return std::pair<std::string, std::string>(string_to_uppercase(FTP_command), FTP_argument);
}

std::string base_session::create_directory_list(const std::vector<std::string> &virtual_object_list,
                                                const std::string &target_directory, std::string owner,
                                                bool include_special_entries)
{
    std::string directory_list = "";

    // also send "." and ".." entries
    if (include_special_entries)
    {
        directory_list += "drwxrwxrwx 1 " + owner + " " + owner + " 0 Jan 1 00:00 .\r\n";
        directory_list += "drwxrwxrwx 1 " + owner + " " + owner + " 0 Jan 1 00:00 ..\r\n";
    }

    size_t i = 2;
    while (i < virtual_object_list.size())
    {
        if (virtual_object_list[i] != target_directory)
        {
            i += 6;
            continue;
        }

        std::string listing_format = "";

        // is file
        if (virtual_object_list[i + 3] == "0")
        {
            listing_format += "-rwxrwxrwx 1 ";
        }
        else if (virtual_object_list[i + 3] == "1")
        {
            // is directory
            listing_format += "drwxrwxrwx 1 ";
        }

        // file owner
        listing_format += owner + " " + owner + " ";

        // file_size
        listing_format += virtual_object_list[i + 1] + " ";

        // modified_time
        listing_format += parse_metadata_time(virtual_object_list[i + 2]) + " ";

        // file_name
        listing_format += virtual_object_list[i - 1];

        directory_list += listing_format + "\r\n";

        i += 6;
    }

    return directory_list;
}

namespace
{
    std::string parse_metadata_time(const std::string &time_str)
    {
        // format: 2025-08-28 05:12:43 -> Aug 28 05:12

        std::string parsedStr = "";

        if (time_str.length() < 19)
        {
            return parsedStr;
        }

        switch (time_str[5])
        {
        // < 10
        case '0': {
            switch (time_str[6])
            {
            // 01
            case '1': {
                parsedStr += "Jan ";
                break;
            }
            // 02
            case '2': {
                parsedStr += "Feb ";
                break;
            }
            // 03
            case '3': {
                parsedStr += "Mar ";
                break;
            }
            // 04
            case '4': {
                parsedStr += "Apr ";
                break;
            }
            // 05
            case '5': {
                parsedStr += "May ";
                break;
            }
            // 06
            case '6': {
                parsedStr += "Jun ";
                break;
            }
            // 07
            case '7': {
                parsedStr += "July ";
                break;
            }
            // 08
            case '8': {
                parsedStr += "Aug ";
                break;
            }
            // 09
            case '9': {
                parsedStr += "Sep ";
                break;
            }
            }
            break;
        }
        // >= 10
        case '1': {
            switch (time_str[6])
            {
            // 10
            case '0': {
                parsedStr += "Oct ";
                break;
            }
            // 11
            case '1': {
                parsedStr += "Nov ";
                break;
            }
            // 12
            case '2': {
                parsedStr += "Dec ";
                break;
            }
            }
            break;
        }
        }

        // add day
        parsedStr += time_str.substr(8, 2) + " ";

        // add hour:minute
        parsedStr += time_str.substr(11, 5);

        return parsedStr;
    }
} // namespace
