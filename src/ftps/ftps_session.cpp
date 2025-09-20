#include "../custom_utils.h"
#include "ftps_session.h"
#include "../sqlite/sqlite_wrapper.h"
#include "../sqlite/fs_handler.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <fstream>

namespace ftps_session
{
    session::session(boost::asio::ip::tcp::socket socket, bool isImplicit)
        : m_ssl_context(boost::asio::ssl::context::tlsv13_server), m_buffer(BUFFER_SIZE), m_database(false)
    {
        m_session_id = custom_utils::generate_uuid_string(8);

        m_isImplicit = isImplicit;

        m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                  boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                                  boost::asio::ssl::context::single_dh_use);
        m_ssl_context.use_certificate_chain_file("tls/cert.pem");
        m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        m_ssl_context.use_tmp_dh_file("tls/dh.pem");

        m_control_socket =
            std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(std::move(socket), m_ssl_context);

        m_database.connect();

        println("session created for " + m_control_socket->lowest_layer().remote_endpoint().address().to_string() +
                    ":" + std::to_string(m_control_socket->lowest_layer().remote_endpoint().port()),
                "black");
    }

    session::~session()
    {
        println("session destroyed", "black");
    }

    void session::start()
    {
        m_control_socket->handshake(boost::asio::ssl::stream_base::handshake_type::server);

        m_connection_stage = UNAUTHENTICATED;

        if (m_isImplicit)
        {
            // clients in implicit mode haven't seen the welcome message yet
            control_send(FTP_WELCOMEMESSAGE);
        }
        // clients in explicit mode will send login details immediately
        control_receive();
    }

    void session::control_send(std::string message)
    {
        message += "\r\n";

        // boost::asio::async_write(*m_control_socket, boost::asio::buffer(message),
        //                          [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
        //                              //  self->println("debug async_write");
        //                              if (ec)
        //                              {
        //                                  self->println("Unknown async_write error -> " + ec.message(), "yellow");
        //                                  return;
        //                              }
        //                          });

        boost::system::error_code ec;
        boost::asio::write(*m_control_socket, boost::asio::buffer(message), ec);
        if (ec)
        {
            println("Unknown async_write error -> " + ec.message(), "yellow");
            return;
        }
    }

    void session::control_receive()
    {
        m_control_socket->async_read_some(
            boost::asio::buffer(m_buffer, BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                // self->println("debug async_read_some");
                if (ec)
                {
                    if (ec == boost::asio::ssl::error::stream_truncated)
                    {
                        // client disconnected
                        self->println("Client disconnected -> " + ec.message(), "green");

                        self->m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                        self->m_control_socket->next_layer().close();
                        return;
                    }
                    self->println("Unknown read_some error -> " + ec.message(), "yellow");
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
        //         println("Client disconnected -> " + ec.message(), "green");

        //         m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
        //         m_control_socket->next_layer().close();
        //         return;
        //     }
        //     println("Unknown read_some error -> " + ec.message(), "yellow");
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

        println(FTP_command + " -> \"" + FTP_argument + "\"", "blue");

        handle_FTP_command(FTP_command, FTP_argument);
    }

    void session::handle_FTP_command(std::string &command, std::string &argument)
    {
        // check if command is supported
        if (std::find(FTP_COMMANDS.begin(), FTP_COMMANDS.end(), command) == FTP_COMMANDS.end())
        {
            println("Unknown command -> " + command, "yellow");
            control_send("502 Command not implemented.");
            control_receive();
            return;
        }

        // handle restricted commands with connection stages vv

        // UNAUTHENTICATED
        {
            if (command == "SYST")
            {
                control_send("215 UNIX Type: L8");
                control_receive();
                return;
            }

            if (command == "QUIT")
            {
                control_send("221 Closing control connection.");
                m_control_socket->shutdown();
                m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                m_control_socket->next_layer().close();
                return;
            }

            if (command == "USER")
            {
                if (argument == "")
                {
                    // argument is empty
                    println("Unknown username -> \"" + argument + "\"", "yellow");
                    control_send("530 Invalid username.");
                    control_receive();
                    return;
                }

                // get user_id, username from database users table
                std::vector<std::string> users_table;
                if (!m_database.read_data("users", {"user_id", "username"}, users_table))
                {
                    println("SQLite database read_data error", "red");
                    return;
                }

                // search if username exists in users table
                size_t i = 1;
                while (i < users_table.size())
                {
                    if (users_table.at(i) == argument)
                    {
                        m_userid = users_table.at(i - 1);
                        m_username = users_table.at(i);
                        println("Known username, need password", "green");
                        control_send("331 Username okay, password needed.");
                        control_receive();
                        return;
                    }
                    i += 2;
                }

                // username not found
                println("Unknown username -> \"" + argument + "\"", "yellow");
                control_send("530 Invalid username.");
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

                // get user_id, password from database users table
                std::vector<std::string> users_table;
                if (!m_database.read_data("users", {"user_id", "password"}, users_table))
                {
                    println("SQLite database read_data error", "red");
                    return;
                }

                // search for user_id and check for password
                size_t i = 0;
                while (i < users_table.size())
                {
                    // find user_id
                    if (users_table.at(i) == m_userid)
                    {
                        // user_id found, check password
                        if (users_table.at(i + 1) == argument)
                        {
                            m_connection_stage = LOGGED_IN;
                            println("User id \"" + m_userid + "\" logged in", "green");
                            control_send("230 Logged in.");
                            control_receive();
                            return;
                        }
                        println("Wrong password from client trying to log into \"" + m_userid + "\"", "yellow");
                        control_send("530 Wrong password.");
                        control_receive();
                        return;
                    }
                    i += 2;
                }

                control_send("530 User not found.");
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
                    control_send("211-Features:\nUTF8\n211 End");
                    control_receive();
                    return;
                }

                if (command == "PBSZ")
                {
                    // low priority todo: check what is this about

                    control_send("200 OK.");
                    control_receive();
                    return;
                }

                if (command == "PROT")
                {
                    // low priority todo: check what is this about

                    control_send("200 OK.");
                    control_receive();
                    return;
                }

                if (command == "OPTS")
                {
                    // low priority todo: make a flag that enables UTF8, find out how to implement utf8

                    if (argument == "UTF8 ON")
                    {
                        control_send("200 Enabled UTF-8 encoding.");
                        control_receive();
                        return;
                    }

                    println("Unknown OPTS argument -> \"" + argument + "\"", "yellow");
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

                    // low priority todo: check what is this about
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

                    boost::system::error_code ec;
                    m_data_socket_acceptor->open(boost::asio::ip::tcp::v4(), ec);

                    // choosing a port that isn't in use
                    int port = 6921;
                    do
                    {
                        port++;
                        m_data_socket_acceptor->bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port),
                                                     ec);
                    } while (ec);
                    m_data_socket_acceptor->listen();

                    // forming response
                    std::string response_format = "227 Entering Passive Mode (";
                    response_format += custom_utils::replaceString(
                        m_control_socket->next_layer().remote_endpoint().address().to_string(), ".", ",");
                    response_format += "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";

                    data_acceptor_start_accept();
                    println("FTPS data channel listening on port " + std::to_string(port), "green");

                    control_send(response_format);
                    control_receive();
                    return;
                }

                if (command == "LIST")
                {
                    if (m_pending_directory_list != "")
                    {
                        println("Previous list directory operation not finished -> " + m_pending_directory_list, "red");
                        custom_utils::sleep(5000); // todo: remove sleep after debugging
                        return;
                    }

                    m_pending_directory_list = m_working_directory;

                    // if data socket is already accepted, that means LIST command is received late
                    if (m_data_socket && m_data_socket->lowest_layer().is_open())
                    {
                        println("late LIST command received", "yellow");

                        control_send("150 Opening connection.");
                        data_directory_listing();
                        control_receive();
                        return;
                    }

                    control_send("150 Opening connection.");
                    control_receive();
                    return;
                }

                if (command == "CDUP")
                {
                    println(m_working_directory + " -> " + get_last_slash(m_working_directory));
                    m_working_directory = get_last_slash(m_working_directory);

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
                        println(m_working_directory + " -> " + get_last_slash(m_working_directory));
                        m_working_directory = get_last_slash(m_working_directory);
                        control_send("250 Changed working directory.");
                        control_receive();
                        return;
                    }

                    // root always exists, no need to check
                    // change directory immediately
                    if (argument == "/")
                    {
                        println(m_working_directory + " -> /");
                        m_working_directory = get_last_slash(m_working_directory);
                        m_working_directory = "/";
                        control_send("250 Changed working directory.");
                        control_receive();
                        return;
                    }

                    // check if directory exists
                    {
                        bool is_absolute_path = false;
                        if (argument.at(0) == '/')
                        {
                            is_absolute_path = true;
                        }

                        std::string path = "";
                        std::string name = "";

                        if (is_absolute_path)
                        {
                            // use the last_slash of argument as path
                            // and the difference as name
                            path = get_last_slash(argument);

                            std::vector<std::string> tempVec = custom_utils::splitString(argument, '/');
                            name = tempVec.at(tempVec.size() - 1);
                        }
                        else
                        {
                            path = m_working_directory;
                            name = argument;
                        }

                        // got the actual path and name now
                        // next search database for it

                        std::vector<std::string> files_metadatas = get_files_metadatas();

                        size_t i = 0;
                        while (i < files_metadatas.size())
                        {
                            // find for matches for path
                            if (files_metadatas.at(i + 1) == path)
                            {
                                // find for matches for name
                                if (files_metadatas.at(i) == name)
                                {
                                    // found, the requested directory exists

                                    if (path != "/")
                                    {
                                        // path is not root, so it doesn't end in '/'
                                        // must add '/' between path to prevent path and name combining
                                        path += '/';
                                    }

                                    println(m_working_directory + " -> " + path + name);
                                    m_working_directory = path + name;
                                    control_send("250 Changed working directory.");
                                    control_receive();
                                    return;
                                }
                            }
                            i += 6;
                        }

                        // directory doesn't exists
                        println("Cannot change working directory: " + path + " -> " + name + ", not found");
                        control_send("550 No such file or directory.");
                        control_receive();
                        return;
                    }
                }

                if (command == "MKD")
                {
                    if (argument == "")
                    {
                        println("Cannot make directory, no arguments", "yellow");
                        control_send("501 No arguments presented.");
                        control_receive();
                        return;
                    }

                    std::vector<std::string> files_metadatas = get_files_metadatas();

                    // check if folder already exists
                    size_t i = 0;
                    while (i < files_metadatas.size())
                    {
                        if (files_metadatas.at(i) == argument && files_metadatas.at(i + 1) == m_working_directory)
                        {
                            // folder already exists
                            control_send("550 Directory already exist.");
                            return;
                        }
                        i += 6;
                    }

                    // create folder
                    println("Creating virtual directory \"" + m_working_directory + "\" \"" + argument + "\"", "green");

                    std::string folder_id = custom_utils::generate_uuid_string(16);

                    m_database.insert_data("files",
                                           {
                                               "file_id",
                                               "user_id",
                                           },
                                           {
                                               folder_id,
                                               m_userid,
                                           });

                    m_database.insert_data("files_metadata",
                                           {
                                               "file_name",
                                               "file_path",
                                               "file_size",
                                               "is_directory",
                                               "file_id",
                                           },
                                           {
                                               argument,
                                               m_working_directory,
                                               "0",
                                               "1",
                                               folder_id,
                                           });

                    control_send("250 Directory created.");
                    control_receive();
                    return;
                }

                if (command == "RMD")
                {
                    if (argument == "")
                    { // no argument
                        control_send("501 No arguments presented.");
                        control_receive();
                        return;
                    }

                    std::vector<std::string> files_metadatas = get_files_metadatas();

                    // search and delete folder
                    size_t i = 0;
                    while (i < files_metadatas.size())
                    {
                        // delete target folder
                        if (files_metadatas.at(i) == argument && files_metadatas.at(i + 1) == m_working_directory)
                        {

                            if (!m_database.delete_data("files", "file_id", files_metadatas.at(i + 5)) ||
                                !m_database.delete_data("files_metadata", "file_id", files_metadatas.at(i + 5)))
                            {
                                // something went wrong when trying to delete data from database
                                control_send("550 Backend error.");
                                control_receive();
                                return;
                            }
                        }

                        // delete files/folders in that target folder
                        if (custom_utils::strStartsWith(files_metadatas.at(i + 1), m_working_directory + argument))
                        {
                            if (!m_database.delete_data("files", "file_id", files_metadatas.at(i + 5)) ||
                                !m_database.delete_data("files_metadata", "file_id", files_metadatas.at(i + 5)))
                            {
                                // something went wrong when trying to delete data from database
                                control_send("550 Backend error.");
                                control_receive();
                                return;
                            }
                        }
                        i += 6;
                    }

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

                    std::vector<std::string> files_metadatas = get_files_metadatas();

                    size_t i = 0;
                    while (i < files_metadatas.size())
                    {
                        if (files_metadatas.at(i) == argument && files_metadatas.at(i + 1) == m_working_directory)
                        { // found
                            if (fs_handler::remove_file("data/" + files_metadatas.at(i + 5)))
                            { // success
                                if (m_database.delete_data("files", "file_id", files_metadatas.at(i + 5)) &&
                                    m_database.delete_data("files_metadata", "file_id", files_metadatas.at(i + 5)))
                                { // if successfully deleted data from database
                                    control_send("250 Deleted.");
                                    control_receive();
                                    return;
                                }

                                // something went wrong when trying to delete data from database
                                control_send("550 Backend error.");
                                control_receive();
                                return;
                            }

                            // cannot delete file from os
                            control_send("550 Unsuccessful delete.");
                            control_receive();
                            return;
                        }
                        i += 6;
                    }

                    control_send("550 File not found.");
                    control_receive();
                    return;
                }

                if (command == "STOR")
                {
                    if (argument == "")
                    {
                        // no argument
                        control_send("501 No arguments presented.");
                        control_receive();
                        return;
                    }

                    if (m_pending_write_file != "")
                    {
                        println("Previous write operation not finished -> " + m_pending_write_file, "red");
                        custom_utils::sleep(5000); // todo: remove sleep after debugging
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
                        println("late STOR command received", "yellow");

                        control_send("150 Opening connection.");
                        data_receive_file();
                        control_receive();
                        return;
                    }

                    control_send("150 Waiting for connection");
                    control_receive();
                    return;
                }

                if (command == "RETR")
                {
                    if (argument == "")
                    {
                        // no argument
                        control_send("501 No arguments presented.");
                        control_receive();
                        return;
                    }

                    if (m_pending_read_file != "")
                    {
                        println("Previous read operation not finished -> " + m_pending_read_file, "red");
                        custom_utils::sleep(5000); // todo: remove sleep after debugging
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
                        println("late RETR command received", "yellow");

                        control_send("150 Opening connection.");
                        data_send_file();
                        control_receive();
                        return;
                    }

                    control_send("150 Waiting for connection");
                    control_receive();
                    return;
                }
            }
        }

        println("Known but unprocessed command -> \"" + command + "\"", "red");
    }

    void session::data_acceptor_start_accept()
    {
        m_data_socket_acceptor->async_accept(
            [self = shared_from_this()](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
                if (ec)
                {
                    self->println("data socket acceptor error -> " + ec.message(), "red");
                    return;
                }

                // create ssl stream socket
                self->m_data_socket = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(
                    std::move(socket), self->m_ssl_context);

                // TLS handshake
                {
                    boost::system::error_code ec;
                    self->m_data_socket->handshake(boost::asio::ssl::stream_base::handshake_type::server, ec);
                    if (ec)
                    {
                        if (ec == boost::asio::ssl::error::stream_truncated)
                        {
                            // client disconnected
                            self->println("Data socket disconnected -> " + ec.message(), "yellow");

                            self->m_control_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                            self->m_control_socket->next_layer().close();
                            return;
                        }
                        self->println("Unknown TLS handshake error -> " + ec.message(), "red");
                        return;
                    }
                    self->println("Data socket accepted and handshaked", "green");
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
                    return;
                }

                self->println("Data socket nothing done, possible late commands", "cyan");
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
            // print("data channel not open yet\n", "yellow");
            return;
        }

        if (m_data_send_buffer.size() == 0)
        {
            // buffer is empty, no message to send
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
        //                          [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
        //                              //  self->println("debug async_write");
        //                              if (ec)
        //                              {
        //                                  self->println("Unknown async_write error -> " + ec.message(), "yellow");
        //                                  return;
        //                              }
        //                          });
    }

    void session::data_directory_listing()
    {
        std::vector<std::string> file_metadata_list = get_files_metadatas();

        // send files metadata that is in current working directory
        size_t i = 1;
        while (i < file_metadata_list.size())
        {
            if (file_metadata_list.at(i) == m_pending_directory_list)
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
                listing_format += m_username + " " + m_username + " ";
                listing_format += file_metadata_list.at(i + 1) + " ";                      // file_size
                listing_format += parse_metadata_time(file_metadata_list.at(i + 2)) + " "; // modified_time
                listing_format += file_metadata_list.at(i - 1);                            // file_name

                data_send(listing_format);
            }

            i += 6;
        }

        control_send("226 Transferred.");

        // close data channel, so client knows operation is done
        println("Directory listing of " + m_pending_directory_list + " done, closing data socket", "green");

        m_pending_directory_list = "";

        boost::system::error_code ec;
        m_data_socket->shutdown(ec);
        m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", "red");
        }
        m_data_socket->next_layer().close();
    }

    void session::data_receive_file()
    {
        std::string file_id = custom_utils::generate_uuid_string(16);
        int file_size = 0;

        std::ofstream output_file_stream("data/" + file_id);
        if (!output_file_stream.is_open())
        {
            println("Error opening output file stream", "red");
            return;
        }

        size_t buffer_size = 1024 * 1024;
        char data[buffer_size];
        while (true)
        {
            boost::system::error_code ec;
            size_t bytes_read = boost::asio::read(*m_data_socket, boost::asio::buffer(data, buffer_size), ec);

            file_size += bytes_read;
            output_file_stream.write(data, bytes_read);

            if (output_file_stream.fail())
            {
                println("Error writing output file stream", "red");
                return;
            }

            if (ec.message() == "End of file") // no more file data to read
                break;
        }

        output_file_stream.close();

        // file is written to os, now store the record in database

        std::vector<std::string> temp = custom_utils::splitString(m_pending_write_file, '/');

        std::string file_path = get_last_slash(m_pending_write_file);
        std::string file_name = temp.at(temp.size() - 1);

        println("storing in db as -> " + file_path + " + " + file_name, "cyan");

        m_database.insert_data("files", {"user_id", "file_id"}, {m_userid, file_id});

        m_database.insert_data("files_metadata",
                               {
                                   "file_name",
                                   "file_path",
                                   "file_size",
                                   "is_directory",
                                   "file_id",
                               },
                               {
                                   file_name,
                                   file_path,
                                   std::to_string(file_size),
                                   "0",
                                   file_id,
                               });

        m_pending_write_file = "";

        control_send("226 Received.");

        // close data channel, so client knows operation is done
        println("File received, closing data socket", "green");

        boost::system::error_code ec;
        m_data_socket->shutdown(ec);
        m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", "red");
        }
        m_data_socket->next_layer().close();
    }

    void session::data_send_file()
    {
        std::vector<std::string> temp = custom_utils::splitString(m_pending_read_file, '/');

        std::string file_path = get_last_slash(m_pending_read_file);
        std::string file_name = temp.at(temp.size() - 1);

        // read db records and search for the according file

        std::vector<std::string> files_metadatas = get_files_metadatas();
        std::string target_file_id;

        size_t i = 0;
        while (i < files_metadatas.size())
        {
            if (files_metadatas.at(i) == file_name && files_metadatas.at(i + 1) == file_path)
            {
                // found the target file to send to client
                target_file_id = files_metadatas.at(i + 5);
                break;
            }
            i += 6;
        }

        if (target_file_id == "")
        {
            // file not found
            println("Cannot find file -> " + m_pending_read_file + " to send to client", "red");

            println(m_working_directory + " " + m_pending_read_file, "red");
            m_data_socket->shutdown();
            m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            m_data_socket->next_layer().close();

            custom_utils::sleep(5000); // todo: check if this is unintended behavior, remove sleep when checked
            return;
        }

        println("FOUND TARGET FILE TO SEND -> " + target_file_id);

        if (!fs_handler::file_exists("data/" + target_file_id))
        {
            println("File not found during data socket sending", "red");
            custom_utils::sleep(5000); // todo: check
            return;
        }

        std::ifstream readFileStream("data/" + target_file_id);

        const int SEND_BUFFER_SIZE = 64 * 1024;
        char fileBuffer[SEND_BUFFER_SIZE] = {0};

        int bytes_sent = 0;

        while (!readFileStream.eof())
        {
            readFileStream.read(fileBuffer, SEND_BUFFER_SIZE);

            boost::asio::write(*m_data_socket, boost::asio::buffer(fileBuffer, readFileStream.gcount()));
            bytes_sent += readFileStream.gcount();
        }

        m_pending_read_file = "";

        control_send("226 Sent.");
        println("bytes sent: " + std::to_string(bytes_sent));

        // close data channel, so client knows operation is done
        println("File sent, closing data socket", "green");
        boost::system::error_code ec;
        m_data_socket->shutdown(ec);
        m_data_socket->next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("error during data socket shutdown, possible client sudden disconnect", "red");
        }
        m_data_socket->next_layer().close();
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

        // add day
        parsedStr += time_str.substr(8, 2) + " ";

        // add hour:minute
        parsedStr += time_str.substr(11, 5);

        return parsedStr;
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
            m_database.read_data("files", {}, files);

            size_t i = 1;
            while (i < files.size())
            {
                if (files.at(i) == m_userid)
                { // found user's file
                    file_id_list.push_back(files.at(i - 1));
                }
                i += 2;
            }
        }

        std::vector<std::string> file_metadata_list;
        { // get file metadatas that matches this users' file ids
            std::vector<std::string> files_metadata;
            m_database.read_data("files_metadata", {}, files_metadata);

            size_t i = 5;
            while (i < files_metadata.size())
            {
                bool found_file = false;
                for (size_t n = 0; n < file_id_list.size(); n++)
                {
                    if (files_metadata.at(i) == file_id_list.at(n))
                    {
                        // low priority todo: remove found file id from file_id_list?
                        // can this reduce search time?
                        // cuz every time a file is found, the next search needs to search 1 time less
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

    void session::println(std::string message)
    {
        custom_utils::println("[FTPS] [" + m_session_id + "] " + message);
    }

    void session::println(std::string message, std::string color)
    {
        custom_utils::println("[FTPS] [" + m_session_id + "] " + message, color);
    }
} // namespace ftps_session