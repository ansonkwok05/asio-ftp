#include "insecure_session.h"
#include "base_session.h"
#include "../helpers.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

namespace ftp
{
    session::session(boost::asio::ip::tcp::socket socket)
        : base_session(socket.get_executor()),
          m_control_socket(std::move(socket)),
          m_data_socket(m_control_socket.get_executor())
    {
        m_session_type = "FTP";

        println("session created for " + m_control_socket.remote_endpoint().address().to_string() + ":" +
                    std::to_string(m_control_socket.remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
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
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_sent) {
                                     self->handle_control_send_callback(ec, bytes_sent);
                                 });
    }

    void session::control_receive()
    {
        m_control_socket.async_read_some(
            boost::asio::buffer(m_buffer, MESSAGE_BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                auto [FTP_command, FTP_argument] = self->handle_control_receive_callback(ec, bytes_received);

                self->handle_command(FTP_command, FTP_argument);
            });
    }

    void session::run_PASV()
    {
        int port = m_data_socket_acceptor.local_endpoint().port();

        // forming response
        std::string response_format = "227 Entering Passive Mode (";

        response_format += string_replace(m_control_socket.local_endpoint().address().to_string(), ".", ",");

        response_format += "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";

        control_send(response_format);
        control_receive();

        println("FTPS data channel listening on port " + std::to_string(port), custom_utils::COLOR::GREEN);
    }

    void session::run_LIST(const std::string &argument)
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
    }

    void session::run_STOR(const std::string &argument)
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
            println("Previous write operation not finished -> " + m_pending_write_file, custom_utils::COLOR::RED);
            return;
        }

        // set to path
        // handle different argument type
        // type 1: filename
        // type 2: /filename

        if (argument[0] != '/')
        {
            if (m_working_directory == "/")
            {
                m_pending_write_file = m_working_directory + argument;
            }
            else
            {
                m_pending_write_file = m_working_directory + "/" + argument;
            }
        }
        else
        {
            m_pending_write_file = argument;
        }

        // don't allow uploads to a directory that doesn't exists
        std::string parent_directory = get_parent_path(m_pending_write_file);

        // if not root directory and parent directory not found
        if (parent_directory != "/" &&
            m_fs_objects_table.get_object(m_userid, get_basename(parent_directory), get_parent_path(parent_directory))
                    .size() == 0)
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
    }

    void session::run_RETR()
    {
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

                // disables nagle's algorithm to reduce small packet latency
                socket.set_option(boost::asio::ip::tcp::no_delay(true));

                self->m_data_socket = std::move(socket);

                // check if need to list directory
                if (self->m_pending_directory_list != "")
                {
                    self->data_directory_listing();
                    return;
                }

                // check if need to receive file
                if (self->m_pending_write_file != "")
                {
                    self->data_receive_file();
                    return;
                }

                // check if need to send file
                if (self->m_sendable_file_id != "")
                {
                    self->data_send_file();
                    return;
                }

                self->println("Data socket nothing done, possible late commands", custom_utils::COLOR::CYAN);
            });
    }

    void session::data_send(const std::string &message)
    {
        boost::asio::async_write(m_data_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     self->handle_data_send_callback(ec, bytes_written);
                                 });
    }

    void session::data_async_receive()
    {
        boost::asio::async_read(m_data_socket, m_receive_buffer,
                                [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                                    self->handle_data_async_receive_callback(ec, bytes_read);
                                });
    }

    void session::data_async_send()
    {
        m_send_file_stream->read(m_send_buffer, SEND_BUFFER_SIZE);

        boost::asio::async_write(m_data_socket, boost::asio::const_buffer(m_send_buffer, m_send_file_stream->gcount()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_sent) {
                                     self->handle_data_async_send_callback(ec, bytes_sent);
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

        // cancel all async operations
        m_data_socket.cancel();

        boost::system::error_code ec = m_data_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        if (ec)
        {
            println("Error during data socket shutdown, possible client sudden disconnect", custom_utils::COLOR::RED);
        }
        m_data_socket.close();
    }
} // namespace ftp