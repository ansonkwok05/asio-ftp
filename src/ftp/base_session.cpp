#include "base_session.h"
#include "constants.h"
#include "helpers.h"

#include "../custom_utils.h"

#include <fstream>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/error.hpp"

base_session::base_session(boost::asio::any_io_executor executor)
    : m_data_socket_acceptor(executor), m_buffer(MESSAGE_BUFFER_SIZE), m_receive_buffer(RECEIVE_BUFFER_SIZE)
{
    m_session_id = custom_utils::generate_uuid_string(8);

    // prepare acceptor for data_socket connection acception
    {
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
}

base_session::~base_session()
{
    println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
}

void base_session::handle_control_send_callback(boost::system::error_code ec, size_t bytes_written)
{
    if (ec)
    {
        println("Unknown async_write error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        return;
    }
}

void base_session::handle_control_receive_callback(boost::system::error_code ec, size_t bytes_received)
{
    if (ec)
    {
        if (ec == boost::asio::error::eof || ec == boost::asio::ssl::error::stream_truncated)
        {
            // client disconnected
            control_close();
            return;
        }
        println("Unknown read_some error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        return;
    }

    if (bytes_received == 1 || bytes_received == 2)
    {
        println("received incomplete message (length " + std::to_string(bytes_received) + "), disconnecting");
        return;
    }

    if (bytes_received == 0)
    {
        println("received nothing, disconnecting");
        return;
    }

    auto [FTP_command, FTP_argument] = parse_buffer(m_buffer, bytes_received);

    println(FTP_command + " -> \"" + FTP_argument + "\"", custom_utils::COLOR::BLUE);

    handle_command(FTP_command, FTP_argument);
}

void base_session::handle_command(const std::string &command, const std::string &argument)
{
    // check if command is supported
    if (std::find(FTP_COMMANDS.begin(), FTP_COMMANDS.end(), command) == FTP_COMMANDS.end())
    {
        println("Unknown command -> " + command, custom_utils::COLOR::YELLOW);
        control_send("502 Command not implemented.");
        control_receive();
        return;
    }

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
            m_working_directory = return_parent_directory(m_working_directory);

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
                m_working_directory = return_parent_directory(m_working_directory);
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

            std::string virtual_path = "";
            std::vector<std::string> children;

            if (argument[0] == '/')
            {
                // type 1
                // use root as first path
                virtual_path = "/";

                // use argument without first char as children
                children = custom_utils::splitString(argument.substr(1, argument.size() - 1), '/');
            }
            else
            {
                // type 2
                // use working_directory as first path
                virtual_path = m_working_directory;

                // argument as children
                children = custom_utils::splitString(argument, '/');
            }

            // validate if each subdirectory exists
            {
                size_t i = 0; // path iterator

                // check if first subdirectory exists
                if (m_virtual_fs.get_object(m_userid, children[0], virtual_path).size() == 0)
                {
                    println("Cannot change working directory: " + virtual_path + " -> " + children[0] + ", not found",
                            custom_utils::COLOR::YELLOW);
                    control_send("550 No such file or directory.");
                    control_receive();
                    return;
                }

                // prepare cumulative_path before processing
                // '/' will be added in loop
                if (virtual_path == "/")
                {
                    virtual_path.pop_back();
                }

                // check each subdirectory in order
                while (i < children.size() - 1)
                {
                    virtual_path += "/" + children[i];

                    if (m_virtual_fs.get_object(m_userid, children[i + 1], virtual_path).size() == 0)
                    {
                        println("Cannot change working directory: " + virtual_path + " -> " + children[i + 1] +
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
            if (virtual_path == "/")
            {
                m_working_directory = virtual_path + children.back();
            }
            else
            {
                m_working_directory = virtual_path + "/" + children.back();
            }

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
            std::string virtual_path;
            std::string object_name;
            if (custom_utils::splitString(argument, '/').size() == 1)
            {
                // type 1

                virtual_path = m_working_directory;
                object_name = argument;
            }
            else
            {
                // type 2

                virtual_path = return_parent_directory("/" + argument);
                object_name = custom_utils::splitString(argument, '/').back();
            }

            std::string vobj_id = m_virtual_fs.create_virtual_object(m_userid, object_name, virtual_path, 0, true);

            // check if created successfully
            if (vobj_id == "")
            {
                println("Unable to create virtual object", custom_utils::COLOR::YELLOW);
                control_send("250 Failed.");
                control_receive();
                return;
            }

            println("Created virtual directory \"" + virtual_path + "\" \"" + object_name + "\"",
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
            std::string virtual_path;
            std::string object_name;

            if (custom_utils::splitString(argument, '/').size() == 1)
            {
                // type 1

                virtual_path = m_working_directory;
                object_name = argument;
            }
            else
            {
                // type 2, 3

                if (argument[0] != '/')
                {
                    // type 2

                    virtual_path = return_parent_directory("/" + argument);
                    object_name = custom_utils::splitString(argument, '/').back();
                }
                else
                {
                    // type 3

                    virtual_path = virtual_path = return_parent_directory(argument);
                    object_name = custom_utils::splitString(argument, '/').back();
                }
            }

            m_virtual_fs.remove_virtual_object(m_userid, object_name, virtual_path);
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

            std::string virtual_path;
            std::string object_name;

            if (custom_utils::splitString(argument, '/').size() == 1)
            {
                // type 1

                virtual_path = m_working_directory;
                object_name = argument;
            }
            else
            {
                // type 2

                virtual_path = return_parent_directory("/" + argument);
                object_name = custom_utils::splitString(argument, '/').back();
            }

            m_virtual_fs.remove_virtual_object(m_userid, object_name, virtual_path);
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
            run_RETR(argument);
            return;
        }

        if (command == "SIZE")
        {
            std::string file_path = return_parent_directory(argument);
            std::string file_name = custom_utils::splitString(argument, '/').back();

            std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, file_name, file_path);
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

void base_session::handle_data_send_callback(boost::system::error_code ec, size_t bytes_written)
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
    m_receive_file_path = return_parent_directory(m_pending_write_file);
    m_received_file_size = 0;
    m_receive_file_name = custom_utils::splitString(m_pending_write_file, '/').back();

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

void base_session::println(const std::string &message)
{
    custom_utils::println("[" + m_session_type + "] [" + m_session_id + "] " + message);
}

void base_session::println(const std::string &message, custom_utils::COLOR color)
{
    custom_utils::println("[" + m_session_type + "] [" + m_session_id + "] " + message, color);
}