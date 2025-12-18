#include "ftps_session.h"
#include "../custom_utils.h"
#include "../database/fs_handler.h"
#include "../database/user_db.h"
#include "../database/virtual_fs_db.h"

#include <boost/asio.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

namespace ftps_session
{
    session::session(boost::asio::ip::tcp::socket socket, bool isImplicit)
        : m_ssl_context(boost::asio::ssl::context::tlsv13_server), m_buffer(BUFFER_SIZE)
    {
        m_session_id = custom_utils::generate_uuid_string(8);

        // set implicit mode (for deciding to send welcome message)
        m_isImplicit = isImplicit;

        m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                  boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                                  boost::asio::ssl::context::single_dh_use);
        m_ssl_context.use_certificate_chain_file("tls/cert.pem");
        m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        m_ssl_context.use_tmp_dh_file("tls/dh.pem");

        m_control_socket =
            std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(std::move(socket), m_ssl_context);

        // check conenction is lan or wan
        {
            std::vector<std::string> local_ip_vector =
                custom_utils::splitString(m_control_socket->next_layer().local_endpoint().address().to_string(), '.');
            std::vector<std::string> remote_ip_vector =
                custom_utils::splitString(m_control_socket->next_layer().remote_endpoint().address().to_string(), '.');

            m_isLan = true;

            // if first three numbers matches, connection is in lan
            size_t i = 0;
            while (i < 3)
            {
                if (local_ip_vector.at(i) != remote_ip_vector.at(i))
                {
                    m_isLan = false;
                    break;
                }
                i++;
            }
        }

        println("session created for " + m_control_socket->lowest_layer().remote_endpoint().address().to_string() +
                    ":" + std::to_string(m_control_socket->lowest_layer().remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
    }

    session::~session()
    {
        println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
    }

    void session::start()
    {
        boost::system::error_code ec =
            m_control_socket->handshake(boost::asio::ssl::stream_base::handshake_type::server, ec);
        if (ec)
        {
            println("handshake error", custom_utils::COLOR::RED);
            return;
        }

        m_connection_stage = UNAUTHENTICATED;

        if (m_isImplicit)
        {
            // clients in implicit mode haven't seen the welcome message yet
            control_send(FTP_WELCOMEMESSAGE);
        }
        // clients in explicit mode will send login details immediately
        control_receive();
    }

    void session::control_send_old(std::string message)
    {
        message += "\r\n";

        boost::system::error_code ec;
        boost::asio::write(*m_control_socket, boost::asio::buffer(message), ec);
        if (ec)
        {
            println("Unknown async_write error -> " + ec.message(), custom_utils::COLOR::YELLOW);
            return;
        }
    }

    void session::control_send(std::string message)
    {
        message += "\r\n";

        m_control_send_queue.push(message);

        if (control_send_lock)
            return;

        // initiate a write chain, next async_write is called when the last one is finished
        control_send_lock = true;
        control_async_write();
    }

    void session::control_async_write()
    {
        boost::asio::async_write(*m_control_socket, boost::asio::buffer(m_control_send_queue.front()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     //  self->println("debug async_write");
                                     if (ec)
                                     {
                                         self->println("Unknown async_write error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }
                                     self->m_control_send_queue.pop();

                                     if (self->m_control_send_queue.size() > 0)
                                     {
                                         self->println("continue queue");
                                         self->control_async_write();
                                     }
                                     else
                                     {
                                         self->control_send_lock = false;
                                     }
                                 });
    }

    void session::control_receive()
    {
        m_control_socket->async_read_some(
            boost::asio::buffer(m_buffer, BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                // self->println("debug async_read_some");
                if (ec)
                {
                    if (ec == boost::asio::ssl::error::stream_truncated || ec == boost::asio::error::connection_reset ||
                        ec == boost::asio::error::eof)
                    {
                        // client disconnected
                        self->println("Client disconnected -> " + ec.message(), custom_utils::COLOR::GREEN);

                        boost::system::error_code shutdownEC = self->m_control_socket->next_layer().shutdown(
                            boost::asio::ip::tcp::socket::shutdown_both, shutdownEC);

                        if (shutdownEC)
                        {
                            self->println("Error during control socket shutdown", custom_utils::COLOR::YELLOW);
                            return;
                        }

                        self->m_control_socket->next_layer().close();
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

        // boost::system::error_code ec;
        // size_t bytes_received = m_control_socket->read_some(boost::asio::buffer(m_buffer, BUFFER_SIZE), ec);
        // if (ec)
        // {
        //     if (ec == boost::asio::ssl::error::stream_truncated)
        //     {
        //         // client disconnected
        //         println("Client disconnected -> " + ec.message(), custom_utils::COLOR::GREEN);

        //         m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        //         m_control_socket->next_layer().close();
        //         return;
        //     }
        //     println("Unknown read_some error -> " + ec.message(), custom_utils::COLOR::YELLOW);
        //     return;
        // }

        // if (bytes_received == 1 || bytes_received == 2)
        // {
        //     println("received incomplete message (length " + std::to_string(bytes_received) + "), disconnecting");
        //     return;
        // }

        // if (bytes_received == 0)
        // {
        //     println("received nothing, disconnecting");
        //     return;
        // }

        // println("bytes received: " + std::to_string(bytes_received));

        // m_received_string = "";

        // // remove unwanted data
        // for (size_t i = 0; i < bytes_received - 2; i++)
        // {
        //     m_received_string += m_buffer.at(i);
        // }

        // handle_received_string();
    }

    void session::handle_received_string()
    {
        std::vector<std::string> split_received_string = custom_utils::splitString(m_received_string, ' ');

        std::string FTP_command = split_received_string[0];
        std::string FTP_argument = "";

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

        // handle restricted commands with connection stages vv

        // UNAUTHENTICATED
        {
            if (command == "NOOP")
            {
                control_send("200 OK.");
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

                boost::system::error_code ec = m_control_socket->shutdown(ec);
                ec = m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

                if (ec)
                {
                    println("Error during control socket shutdown", custom_utils::COLOR::YELLOW);
                    return;
                }

                m_control_socket->next_layer().close();
                return;
            }

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
                std::vector<std::string> id_results = m_user.get_id_by_name(argument);

                // username does not exist in database
                if (id_results.size() == 0)
                {
                    println("Unknown username -> \"" + argument + "\"", custom_utils::COLOR::YELLOW);
                    control_send("530 Invalid username.");
                    control_receive();
                    return;
                }

                m_userid = id_results[0];
                m_username = m_user.get_name_by_id(m_userid)[0];

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

                if (m_user.get_password_by_id(m_userid)[0] == argument)
                {
                    m_connection_stage = LOGGED_IN;
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
        }

        // deny access to unauthenticated clients
        if (m_connection_stage < LOGGED_IN)
        {
            control_send("530 Not logged in.");
            control_receive();
            return;
        }

        // LOGGED_IN
        {
            // server/connection details
            {
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
            }

            // file access
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

                    // currently everything is sent in binary
                    control_send("200 OK.");
                    control_receive();
                    return;
                }

                if (command == "PASV")
                {
                    // prepare acceptor for data_socket connection acception
                    m_data_socket_acceptor =
                        std::make_unique<boost::asio::ip::tcp::acceptor>(m_control_socket->get_executor());

                    boost::system::error_code ec = m_data_socket_acceptor->open(boost::asio::ip::tcp::v4(), ec);

                    // choosing a port that isn't in use
                    int port = 6921;
                    do
                    {
                        port++;
                        ec = m_data_socket_acceptor->bind(
                            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port), ec);
                    } while (ec);
                    m_data_socket_acceptor->listen();

                    // forming response
                    std::string response_format = "227 Entering Passive Mode (";

                    if (m_isLan)
                    {
                        response_format += custom_utils::replaceString(
                            m_control_socket->next_layer().local_endpoint().address().to_string(), ".", ",");
                    }
                    else
                    {
                        // todo: use public ip
                        // get the public ip to pass from ftps_server.cpp
                        // to be implemented
                    }

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

                    // if data socket is already accepted, that means LIST command is received late
                    if (m_data_socket && m_data_socket->lowest_layer().is_open())
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

                if (command == "CDUP")
                {
                    println(m_working_directory + " -> " + return_parent_directory(m_working_directory));
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

                    // check if object exists
                    if (m_virtual_fs.get_object(m_userid, argument, m_working_directory).size() != 0)
                    {
                        println("Cannot make directory, already exists", custom_utils::COLOR::YELLOW);
                        control_send("250 Directory already exist.");
                        control_receive();
                        return;
                    }

                    // folder does not exists, create one
                    m_virtual_fs.create_virtual_object(m_userid, argument, m_working_directory, 0, true);

                    println("Created virtual directory \"" + m_working_directory + "\" \"" + argument + "\"",
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

                    std::string v_obj_id = m_virtual_fs.remove_virtual_object(m_userid, object_name, virtual_path);
                    if (v_obj_id != "")
                    {
                        fs_handler::remove_file("data/" + v_obj_id);
                    }

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

                    // if data socket is already accepted, that means STOR command is received late
                    if (m_data_socket && m_data_socket->lowest_layer().is_open())
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

                    if (m_pending_read_file != "")
                    {
                        println("Previous read operation not finished -> " + m_pending_read_file,
                                custom_utils::COLOR::RED);
                        return;
                    }

                    // set to path
                    if (m_working_directory == "/")
                    {
                        m_pending_read_file = m_working_directory + argument;
                    }
                    else
                    {
                        m_pending_read_file = m_working_directory + "/" + argument;
                    }

                    // if data socket is already accepted, that means RETR command is received late
                    if (m_data_socket && m_data_socket->lowest_layer().is_open())
                    {
                        println("late RETR command received", custom_utils::COLOR::YELLOW);

                        control_send("150 Opening connection.");
                        data_send_file();
                        // data_async_send_file(); // experimental
                        control_receive();
                        return;
                    }

                    control_send("150 Waiting for connection");
                    control_receive();

                    data_acceptor_start_accept();
                    return;
                }
            }
        }

        println("Known but unprocessed command -> \"" + command + "\"", custom_utils::COLOR::RED);
    }

    void session::data_acceptor_start_accept()
    {
        m_data_socket_acceptor->async_accept(
            [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (ec)
                {
                    self->println("data socket acceptor error -> " + ec.message(), custom_utils::COLOR::RED);
                    return;
                }

                // create ssl stream socket
                self->m_data_socket = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
                    std::move(socket), self->m_ssl_context);

                // TLS handshake
                {
                    boost::system::error_code ec =
                        self->m_data_socket->handshake(boost::asio::ssl::stream_base::handshake_type::server, ec);
                    if (ec)
                    {
                        if (ec == boost::asio::ssl::error::stream_truncated)
                        {
                            // client disconnected
                            self->println("Data socket disconnected -> " + ec.message(), custom_utils::COLOR::YELLOW);

                            boost::system::error_code shutdownEC = self->m_control_socket->next_layer().shutdown(
                                boost::asio::ip::tcp::socket::shutdown_both, shutdownEC);

                            if (ec)
                            {
                                self->println("Error during data socket shutdown", custom_utils::COLOR::YELLOW);
                                return;
                            }

                            self->m_control_socket->next_layer().close();
                            return;
                        }
                        self->println("Unknown TLS handshake error -> " + ec.message(), custom_utils::COLOR::RED);
                        return;
                    }
                    self->println("Data socket accepted and handshaked", custom_utils::COLOR::GREEN);
                }

                // check if list directory
                if (self->m_pending_directory_list != "")
                {
                    self->data_directory_listing();
                    return;
                }

                // check if write file
                if (self->m_pending_write_file != "")
                {
                    self->data_receive_file();
                    return;
                }

                // check if read file
                if (self->m_pending_read_file != "")
                {
                    self->data_send_file();
                    // self->data_async_send_file(); // experimental
                    return;
                }

                self->println("Data socket nothing done, possible late commands", custom_utils::COLOR::CYAN);
            });
    }

    void session::data_send(std::string message)
    {
        if (message != "")
        {
            // add message to buffer
            m_data_send_buffer.push_back(message); // all messages need to end in \r\n
        }

        if (!m_data_socket || !m_data_socket->lowest_layer().is_open())
        {
            // print("data channel not open yet\n", custom_utils::COLOR::YELLOW);
            return;
        }

        // buffer is empty, no message to send
        if (m_data_send_buffer.size() == 0)
        {
            return;
        }

        // flush buffer and send
        std::string tempStr = "";
        for (std::string str : m_data_send_buffer)
        {
            tempStr += str + "\r\n";
        }
        m_data_send_buffer.clear();

        boost::asio::write(*m_data_socket, boost::asio::buffer(tempStr));
        // boost::asio::async_write(*m_data_socket, boost::asio::buffer(tempStr),
        //                          [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written)
        //                          {
        //                              //  self->println("debug async_write");
        //                              if (ec)
        //                              {
        //                                  self->println("Unknown async_write error -> " + ec.message(),
        //                                  custom_utils::COLOR::YELLOW); return;
        //                              }
        //                          });
    }

    void session::data_directory_listing()
    {
        std::vector<std::string> v_object_list = m_virtual_fs.get_object_list(m_userid);

        size_t i = 1;
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
                listing_format += "-rw-r--r-- 1 ";
            }
            else if (v_object_list[i + 3] == "1")
            {
                // is directory
                listing_format += "drwxr-xr-x 1 ";
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

        control_send("226 Transferred.");

        // close data channel, so client knows operation is done
        println("Directory listing of " + m_pending_directory_list + " done, closing data socket",
                custom_utils::COLOR::GREEN);

        m_pending_directory_list = "";

        boost::system::error_code ec = m_data_socket->shutdown(ec);
        ec = m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", custom_utils::COLOR::RED);
        }
        m_data_socket->next_layer().close();
    }

    void session::data_receive_file()
    {
        // create the virtual object if not exists before receiving, file size = 0
        std::string file_path = return_parent_directory(m_pending_write_file);
        std::string file_name;
        std::string file_id;

        {
            std::vector<std::string> temp = custom_utils::splitString(m_pending_write_file, '/');
            file_name = temp.at(temp.size() - 1);

            // create virtual object if not exists
            std::vector<std::string> v_obj = m_virtual_fs.get_object(m_userid, file_name, file_path);
            if (v_obj.size() == 0)
            {
                println("creating new object in db as -> " + file_path + " + " + file_name, custom_utils::COLOR::CYAN);
                file_id = m_virtual_fs.create_virtual_object(m_userid, file_name, file_path, 0, false);
            }
            else
            {
                file_id = v_obj[5];
            }
        }

        long long file_size = 0;

        std::ofstream output_file_stream("data/" + file_id);
        if (!output_file_stream.is_open())
        {
            println("Error opening output file stream", custom_utils::COLOR::RED);
            return;
        }

        const size_t RECEIVE_BUFFER_SIZE = 1024 * 1024;
        char data[RECEIVE_BUFFER_SIZE];
        while (true)
        {
            boost::system::error_code ec;
            // todo is it possible to turn this into async_read, or something that async read in order, then write
            // the buffer to output file stream
            size_t bytes_read = boost::asio::read(*m_data_socket, boost::asio::buffer(data, RECEIVE_BUFFER_SIZE), ec);

            file_size += bytes_read;
            output_file_stream.write(data, bytes_read);

            if (output_file_stream.fail())
            {
                println("Error writing output file stream", custom_utils::COLOR::RED);
                return;
            }

            // good chunks of data received
            if (ec.message() == "Success")
                continue;

            // no more file data to read
            if (ec == boost::asio::error::eof)
                break;

            // connection canceled/abandoned
            if (ec == boost::asio::ssl::error::stream_truncated)
            {
                println("data connection disconnected mid file receive", custom_utils::COLOR::YELLOW);
                break;
            }

            // unexpected error
            println("error while receiving file data -> " + ec.message(), custom_utils::COLOR::YELLOW);
        }

        output_file_stream.close();

        // file is written to os, update the file size of the object in database
        println("updating object in db -> " + file_path + " + " + file_name, custom_utils::COLOR::CYAN);
        m_virtual_fs.update_virtual_object(m_userid, file_name, file_path, file_size);

        // allow another file to be received
        m_pending_write_file = "";

        control_send("226 Received.");

        // close data channel, so client knows operation is done
        println("File received, closing data socket", custom_utils::COLOR::GREEN);

        boost::system::error_code ec = m_data_socket->shutdown(ec);
        ec = m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", custom_utils::COLOR::RED);
        }
        m_data_socket->next_layer().close();
    }

    void session::data_send_file()
    {
        std::vector<std::string> temp = custom_utils::splitString(m_pending_read_file, '/');

        std::string file_path = return_parent_directory(m_pending_read_file);
        std::string file_name = temp.at(temp.size() - 1);

        // read db records and search for the according file

        std::string target_file_id;

        std::vector<std::string> v_object = m_virtual_fs.get_object(m_userid, file_name, file_path);

        if (v_object.size() != 0)
        {
            target_file_id = v_object[5];
        }

        if (target_file_id == "")
        {
            // file not found
            println("Cannot find file -> " + m_pending_read_file + " to send to client", custom_utils::COLOR::RED);

            println(m_working_directory + " " + m_pending_read_file, custom_utils::COLOR::RED);
            m_data_socket->shutdown();

            boost::system::error_code shutdownEC =
                m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdownEC);
            if (shutdownEC)
            {
                println("Error during data socket shutdown", custom_utils::COLOR::YELLOW);
                return;
            }

            m_data_socket->next_layer().close();
            return;
        }

        println("FOUND TARGET FILE TO SEND -> " + target_file_id);

        if (!fs_handler::file_exists("data/" + target_file_id))
        {
            println("Actual file not found on OS", custom_utils::COLOR::RED);
            return;
        }

        std::ifstream readFileStream("data/" + target_file_id);
        if (!readFileStream.is_open())
        {
            println("Unable to open read file stream", custom_utils::COLOR::RED);
            return;
        }

        const int SEND_BUFFER_SIZE = 1024 * 1024;
        char fileBuffer[SEND_BUFFER_SIZE] = {0};

        int bytes_sent = 0;
        // bool error_during_async_write = false;

        while (!readFileStream.eof())
        {
            readFileStream.read(fileBuffer, SEND_BUFFER_SIZE);

            boost::system::error_code ec;

            boost::asio::write(*m_data_socket, boost::asio::buffer(fileBuffer, readFileStream.gcount()), ec);

            bytes_sent += readFileStream.gcount();

            // good chunks of data received
            if (ec.message() == "Success")
                continue;

            // connection canceled/abandoned
            if (ec == boost::asio::error::broken_pipe)
            {
                println("data connection disconnected mid file send", custom_utils::COLOR::YELLOW);
                break;
            }

            // unexpected error
            println("error while receiving file data -> " + ec.message(), custom_utils::COLOR::YELLOW);
        }

        m_pending_read_file = "";

        control_send("226 Sent.");
        println("bytes sent: " + std::to_string(bytes_sent));

        // close data channel, so client knows operation is done
        println("File sent, closing data socket", custom_utils::COLOR::GREEN);
        boost::system::error_code ec = m_data_socket->shutdown(ec);
        ec = m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", custom_utils::COLOR::RED);
        }
        m_data_socket->next_layer().close();
    }

    void session::data_async_send_file()
    {
        std::vector<std::string> temp = custom_utils::splitString(m_pending_read_file, '/');

        std::string file_path = return_parent_directory(m_pending_read_file);
        std::string file_name = temp.at(temp.size() - 1);

        // read db records and search for the according file

        std::string target_file_id;

        std::vector<std::string> v_object = m_virtual_fs.get_object(m_userid, file_name, file_path);

        if (v_object.size() != 0)
        {
            target_file_id = v_object[5];
        }

        if (target_file_id == "")
        {
            // file not found
            println("Cannot find file -> " + m_pending_read_file + " to send to client", custom_utils::COLOR::RED);

            println(m_working_directory + " " + m_pending_read_file, custom_utils::COLOR::RED);
            m_data_socket->shutdown();

            boost::system::error_code shutdownEC =
                m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, shutdownEC);
            if (shutdownEC)
            {
                println("Error during data socket shutdown", custom_utils::COLOR::YELLOW);
                return;
            }

            m_data_socket->next_layer().close();
            return;
        }

        println("FOUND TARGET FILE TO SEND -> " + target_file_id);

        if (!fs_handler::file_exists("data/" + target_file_id))
        {
            println("Actual file not found on OS", custom_utils::COLOR::RED);
            return;
        }

        std::ifstream readFileStream("data/" + target_file_id);
        if (!readFileStream.is_open())
        {
            println("Unable to open read file stream", custom_utils::COLOR::RED);
            return;
        }

        const int SEND_BUFFER_SIZE = 1024 * 1024;
        int bytes_read = 0;

        bool stop_reading = false;

        while (!readFileStream.eof() && !stop_reading)
        {
            char fileBuffer[SEND_BUFFER_SIZE] = {0};
            readFileStream.read(fileBuffer, SEND_BUFFER_SIZE);

            bytes_read += readFileStream.gcount();

            // async part
            // cannot ensure the order of data delivery, async_Write_some does not guarantee order
            // call readFileStream.read after each async_write to send the next chunk?
            m_data_socket->async_write_some(
                boost::asio::buffer(fileBuffer, readFileStream.gcount()),
                [self = shared_from_this(), &stop_reading](boost::system::error_code temp_ec, size_t bytes_written) {
                    if (temp_ec)
                    {
                        self->println("EC ASYNC WRITE SOME", custom_utils::COLOR::YELLOW);
                        // connection canceled/abandoned
                        if (temp_ec == boost::asio::error::broken_pipe)
                        {
                            stop_reading = true;
                            return;
                        }
                    }
                });
        }

        boost::asio::post(m_data_socket->get_executor(), [self = shared_from_this(), &bytes_read, &stop_reading]() {
            self->m_pending_read_file = "";

            self->control_send("226 Sent.");
            self->println("bytes sent: " + std::to_string(bytes_read));

            if (stop_reading)
            {
                self->println("stopped suddenly due to error", custom_utils::COLOR::YELLOW);
            }

            // close data channel, so client knows operation is done
            self->println("File sent, closing data socket", custom_utils::COLOR::GREEN);

            boost::system::error_code ec = self->m_data_socket->shutdown(ec);
            ec = self->m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
            if (ec)
            {
                self->println("error during data socket shutdown, possible client sudden disconnect",
                              custom_utils::COLOR::RED);
            }
            self->m_data_socket->next_layer().close();
        });
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
        custom_utils::println("[FTPS] [" + m_session_id + "] " + message);
    }

    void session::println(std::string message, custom_utils::COLOR color)
    {
        custom_utils::println("[FTPS] [" + m_session_id + "] " + message, color);
    }
} // namespace ftps_session