#include "../custom_utils.h"
#include "session.h"

#include <boost/system/system_error.hpp>
#include <string>
#include <vector>
#include <fstream>

#include <boost/asio.hpp>
#include <boost/asio/detail/chrono.hpp>

namespace ftp
{
    session::session(boost::asio::ip::tcp::socket socket)
        : m_control_socket(std::move(socket)), m_data_socket_acceptor(m_control_socket.get_executor()),
          m_data_socket(m_control_socket.get_executor()), m_buffer(MESSAGE_BUFFER_SIZE),
          m_receive_buffer(RECEIVE_BUFFER_SIZE)
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

        println("session created for " + m_control_socket.remote_endpoint().address().to_string() + ":" +
                    std::to_string(m_control_socket.remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
    }

    session::~session()
    {
        println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
    }

    void session::start()
    {
        m_connection_stage = CONNECTION_STAGE::UNAUTHENTICATED;

        control_send(FTP_WELCOMEMESSAGE);
        control_receive();
    }

    void session::control_send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_control_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unknown async_write error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }
                                 });
    }

    void session::control_receive()
    {
        m_control_socket.async_read_some(
            boost::asio::buffer(m_buffer, MESSAGE_BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                if (ec)
                {
                    if (ec == boost::asio::error::eof)
                    {
                        // client disconnected
                        self->control_close();
                        return;
                    }
                    self->println("Unknown read_some error -> " + ec.message(), custom_utils::COLOR::YELLOW);
                    return;
                }

                if (bytes_received == 1 || bytes_received == 2)
                {
                    self->println("received incomplete message (length " + std::to_string(bytes_received) +
                                  "), disconnecting");
                    return;
                }

                if (bytes_received == 0)
                {
                    self->println("received nothing, disconnecting");
                    return;
                }

                self->m_received_string = "";

                // remove unwanted data
                for (size_t i = 0; i < bytes_received - 2; i++)
                {
                    self->m_received_string += self->m_buffer.at(i);
                }

                self->handle_received_string();
            });
    }

    void session::handle_received_string()
    {
        std::vector<std::string> split_received_string = custom_utils::splitString(m_received_string, ' ');

        std::string FTP_command = split_received_string[0];
        std::string FTP_argument = "";

        // convert commands to uppercase
        for (auto &c : FTP_command)
        {
            c = toupper(c);
        }

        if (split_received_string.size() > 1)
        {
            // received message has argument(s)
            FTP_argument = custom_utils::vectorStrJoin(
                std::vector<std::string>(split_received_string.begin() + 1, split_received_string.end()), " ");
        }

        println(FTP_command + " -> \"" + FTP_argument + "\"", custom_utils::COLOR::BLUE);

        handle_FTP_command(FTP_command, FTP_argument);
    }

    void session::handle_FTP_command(std::string &command, std::string &argument)
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
                int port = m_data_socket_acceptor.local_endpoint().port();

                // forming response
                std::string response_format = "227 Entering Passive Mode (";

                response_format +=
                    custom_utils::replaceString(m_control_socket.local_endpoint().address().to_string(), ".", ",");

                response_format += "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";

                control_send(response_format);
                control_receive();

                println("FTPS data channel listening on port " + std::to_string(port), custom_utils::COLOR::GREEN);
                return;
            }

            if (command == "LIST")
            {
                if (m_pending_directory_list != "")
                {
                    println("Previous list directory operation not finished -> " + m_pending_directory_list,
                            custom_utils::COLOR::RED);
                    return;
                }

                m_pending_directory_list = m_working_directory;

                if (argument == "-la")
                {
                    m_pending_directory_list_all = true;
                }

                // if data socket is already accepted, that means LIST command is received late
                if (m_data_socket.is_open())
                {
                    println("late LIST command received", custom_utils::COLOR::YELLOW);

                    control_send("150 Opening connection.");
                    data_directory_listing();
                    control_receive();
                    return;
                }

                control_send("150 Opening connection.");
                control_receive();

                data_acceptor_start_accept();
                return;
            }

            if (command == "FEAT")
            {
                control_send("211-Features:\r\n AUTH TLS\r\n PASV\r\n PBSZ\r\n PROT\r\n UTF8\r\n211 End");
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
                        println("Cannot change working directory: " + virtual_path + " -> " + children[0] +
                                    ", not found",
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
                // no argument
                if (argument == "")
                {
                    control_send("501 No arguments presented.");
                    control_receive();
                    return;
                }

                if (m_pending_write_file != "")
                {
                    println("Previous write operation not finished -> " + m_pending_write_file,
                            custom_utils::COLOR::RED);
                    return;
                }

                // set to path
                if (m_working_directory == "/")
                {
                    m_pending_write_file = m_working_directory + argument;
                }
                else
                {
                    m_pending_write_file = m_working_directory + "/" + argument;
                }

                // don't allow uploads to a directory that doesn't exists
                std::string parent_directory = return_parent_directory(m_pending_write_file);
                std::string parent_of_parent_directory = return_parent_directory(parent_directory);

                std::string parent_directory_name = custom_utils::splitString(parent_directory, '/').back();

                // if not root directory and parent directory not found
                if (parent_directory != "/" &&
                    m_virtual_fs.get_object(m_userid, parent_directory_name, parent_of_parent_directory).size() == 0)
                {
                    println("Parent directory doesn't exists -> " + m_pending_write_file, custom_utils::COLOR::RED);
                    return;
                }

                // if data socket is already accepted, that means STOR command is received late
                if (m_data_socket.is_open())
                {
                    println("late STOR command received", custom_utils::COLOR::YELLOW);

                    control_send("150 Opening connection.");
                    data_receive_file();
                    control_receive();
                    return;
                }

                control_send("150 Waiting for connection");
                control_receive();

                data_acceptor_start_accept();
                return;
            }

            if (command == "RETR")
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
                    println("Previous send file operation not finished -> " + m_sendable_file_id,
                            custom_utils::COLOR::RED);
                    return;
                }

                // handle multiple types of arguments
                // type 1 "one"         // object name only
                // type 2 "/test/one"   // absolute path

                std::string file_path;
                std::string file_name;
                if (custom_utils::splitString(argument, '/').size() == 1)
                {
                    // type 1
                    file_path = m_working_directory;
                    file_name = argument;
                }
                else
                {
                    // type 2
                    file_path = return_parent_directory(argument);
                    file_name = custom_utils::splitString(argument, '/').back();
                }

                std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, file_name, file_path);
                if (v_obj.size() == 0)
                {
                    // file does not exists
                    println("Client requested -> " + argument + " which does not exists", custom_utils::COLOR::RED);
                    control_send("550 File unavailable.");
                    return;
                }
                m_sendable_file_id = v_obj[0];

                // if data socket is already accepted, that means RETR command is received late
                if (m_data_socket.is_open())
                {
                    println("late RETR command received", custom_utils::COLOR::YELLOW);

                    control_send("150 Opening connection.");
                    data_send_file();
                    control_receive();
                    return;
                }

                control_send("150 Waiting for connection");
                control_receive();

                data_acceptor_start_accept();
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

    void session::control_close()
    {
        boost::system::error_code ec = m_control_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

        if (ec)
        {
            println("Error during control socket shutdown", custom_utils::COLOR::YELLOW);
            return;
        }

        m_control_socket.close();
    }

    void session::data_acceptor_start_accept()
    {
        m_data_socket_acceptor.async_accept(
            [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (ec)
                {
                    self->println("data socket acceptor error -> " + ec.message(), custom_utils::COLOR::RED);
                    return;
                }

                // create socket
                self->m_data_socket = std::move(socket);

                // check if list directory
                if (self->m_pending_directory_list != "")
                {
                    self->data_directory_listing();
                    return;
                }

                // check if receive file
                if (self->m_pending_write_file != "")
                {
                    self->data_receive_file();
                    return;
                }

                // check if send file
                if (self->m_sendable_file_id != "")
                {
                    self->data_send_file();
                    return;
                }

                self->println("Data socket nothing done, possible late commands", custom_utils::COLOR::CYAN);
            });
    }

    void session::data_send(std::string message)
    {
        message += "\r\n";

        m_data_send_queue.push(message);
    }

    void session::data_async_write()
    {
        if (m_data_send_queue.size() == 0)
        {
            // inform client through control channel that data channel finished
            m_pending_directory_list = "";
            control_send("226 Transferred.");
            data_close();
            return;
        }

        boost::asio::async_write(m_data_socket, boost::asio::buffer(m_data_send_queue.front()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unknown async_write error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }

                                     self->m_data_send_queue.pop();
                                     self->data_async_write();
                                 });
    }

    void session::data_directory_listing()
    {
        std::vector<std::string> v_object_list = m_virtual_fs.get_object_list(m_userid);

        // also send "." and ".." entries
        if (m_pending_directory_list_all)
        {
            m_pending_directory_list_all = false;
            data_send("drwxrwxrwx 1 home home 0 Jan 1 00:00 .");
            data_send("drwxrwxrwx 1 home home 0 Jan 1 00:00 ..");
        }

        size_t i = 2;
        while (i < v_object_list.size())
        {
            if (v_object_list[i] != m_pending_directory_list)
            {
                i += 6;
                continue;
            }

            std::string listing_format = "";

            // is file
            if (v_object_list[i + 3] == "0")
            {
                listing_format += "-rwxrwxrwx 1 ";
            }
            else if (v_object_list[i + 3] == "1")
            {
                // is directory
                listing_format += "drwxrwxrwx 1 ";
            }

            // file owner
            listing_format += m_username + " " + m_username + " ";

            // file_size
            listing_format += v_object_list[i + 1] + " ";

            // modified_time
            listing_format += parse_metadata_time(v_object_list[i + 2]) + " ";

            // file_name
            listing_format += v_object_list[i - 1];

            data_send(listing_format);

            i += 6;
        }

        // start sending directory list
        data_async_write();
    }

    void session::data_receive_file()
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

    void session::data_async_receive()
    {
        if (m_receive_end)
        {
            m_receive_end = false;

            m_receive_file_stream->close();
            m_receive_file_stream.reset();

            println("updating object metadata in db", custom_utils::COLOR::CYAN);
            m_virtual_fs.update_virtual_object(m_userid, m_receive_file_name, m_receive_file_path,
                                               m_received_file_size);

            // allow another file to be received
            m_pending_write_file = "";

            // notify client
            control_send("226 Received.");

            // close data channel
            data_close();
            return;
        }

        boost::asio::async_read(m_data_socket, m_receive_buffer,
                                [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                                    self->m_received_file_size += bytes_read;

                                    if (bytes_read > 0)
                                    {
                                        *self->m_receive_file_stream << &self->m_receive_buffer;
                                    }

                                    if (self->m_receive_file_stream->fail())
                                    {
                                        self->println("Error writing output file stream", custom_utils::COLOR::RED);
                                        self->m_receive_end = true;
                                    }

                                    // no more file data to receive
                                    if (ec == boost::asio::error::eof)
                                    {
                                        self->m_receive_end = true;
                                    }
                                    else if (ec.message() != "Success")
                                    {
                                        self->println("Unexpected error while receiving file data -> " + ec.message(),
                                                      custom_utils::COLOR::YELLOW);
                                        self->m_receive_end = true;
                                    }

                                    self->data_async_receive();
                                });
    }

    void session::data_send_file()
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

    void session::data_async_send()
    {
        if (m_send_end)
        {
            m_send_end = false;

            m_send_file_stream->close();
            m_send_file_stream.reset();

            // allow another file to be sent
            m_sendable_file_id = "";

            // notify client
            control_send("226 Sent.");

            // close data channel
            data_close();
            return;
        }

        m_send_file_stream->read(m_send_buffer, SEND_BUFFER_SIZE);

        boost::asio::async_write(m_data_socket, boost::asio::buffer(m_send_buffer, m_send_file_stream->gcount()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_sent) {
                                     if (self->m_send_file_stream->eof())
                                     {
                                         self->m_send_end = true;
                                     }
                                     else if (ec == boost::asio::error::broken_pipe)
                                     {
                                         self->println("Data channel disconnected mid file send",
                                                       custom_utils::COLOR::YELLOW);
                                         self->m_send_end = true;
                                     }
                                     else if (ec.message() != "Success")
                                     {
                                         self->println("Unexpected error while sending file data -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         self->m_send_end = true;
                                     }

                                     self->data_async_send();
                                 });
    }

    void session::data_close()
    {
        // close data channel
        println("Operation finished, closing data socket", custom_utils::COLOR::GREEN);

        if (!m_data_socket.is_open())
        {
            println("Unable to close data socket. It is not open?", custom_utils::COLOR::YELLOW);
            return;
        }

        boost::system::error_code ec = m_data_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("Error during data socket shutdown, possible client sudden disconnect", custom_utils::COLOR::RED);
        }
        m_data_socket.close();
    }

    std::string session::parse_metadata_time(std::string time_str)
    {
        // format: 2025-08-28 05:12:43 -> Aug 28 05:12

        std::string parsedStr = "";

        switch (time_str.at(5))
        {
        // < 10
        case '0': {
            switch (time_str.at(6))
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
            switch (time_str.at(6))
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

    std::string session::return_parent_directory(std::string directory)
    {
        // return root when already at root directory
        if (directory == "/")
            return "/";

        std::vector<std::string> split_directory = custom_utils::splitString(directory, '/');

        // input only have one child
        // such as "/test" or "/one"
        if (split_directory.size() <= 2)
        {
            return "/";
        }

        // combine split string except last, adding '/' in between
        std::string parent = "";
        for (int i = 0; i < split_directory.size() - 1; i++)
        {
            // ignore empty strings, they were "/" before splitting
            if (split_directory.at(i) == "")
                continue;

            parent += "/" + split_directory.at(i);
        }

        return parent;
    }

    void session::println(std::string message)
    {
        custom_utils::println("[FTP] [" + m_session_id + "] " + message);
    }

    void session::println(std::string message, custom_utils::COLOR color)
    {
        custom_utils::println("[FTP] [" + m_session_id + "] " + message, color);
    }
} // namespace ftp