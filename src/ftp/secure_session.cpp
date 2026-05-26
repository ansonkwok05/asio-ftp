#include "secure_session.h"

#include "base_session.h"
#include "../helpers.h"
#include "../custom_utils.h"

#include <string>
#include <fstream>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/read.hpp>

namespace ftps
{
    secure_session::secure_session(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context &ssl_context)
        : base_session(socket.get_executor()),
          m_timer(socket.get_executor(), std::chrono::milliseconds(IMPLICIT_CHECK_INTERVAL_MS)),
          m_probe_socket(std::move(socket)),
          m_ssl_context(ssl_context),
          m_control_socket(socket.get_executor(), m_ssl_context),
          m_data_socket(socket.get_executor(), m_ssl_context)
    {
        m_session_type = "FTP ";

        println("session created for " + m_probe_socket.lowest_layer().remote_endpoint().address().to_string() + ":" +
                    std::to_string(m_probe_socket.lowest_layer().remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
    }

    void secure_session::start()
    {
        m_stopwatch.start();
        wait_for_implicit();
    }

    void secure_session::wait_for_implicit()
    {
        if (m_stopwatch.lapMs() > IMPLICIT_TIMEOUT_MS)
        {
            // timeout, enter explicit mode
            probe_send(FTP_WELCOMEMESSAGE);
            probe_receive();
            return;
        }

        // wait for implicit ClientHello
        if (m_probe_socket.available() > 100)
        {
            // enter implicit mode
            start_secure(true);
            return;
        }

        m_timer.expires_at(m_timer.expiry() + std::chrono::milliseconds(IMPLICIT_CHECK_INTERVAL_MS));
        m_timer.async_wait([self = shared_from_this()](boost::system::error_code ec) {
            self->wait_for_implicit();
        });
    }

    void secure_session::probe_send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_probe_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unknown async_write error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }
                                 });
    }

    void secure_session::probe_receive()
    {
        m_probe_socket.async_read_some(
            boost::asio::buffer(m_buffer, MESSAGE_BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                auto [FTP_command, FTP_argument] = self->handle_control_receive_callback(ec, bytes_received);

                self->handle_auth(FTP_command, FTP_argument);
            });
    }

    void secure_session::handle_auth(const std::string &command, const std::string &argument)
    {
        println(command + " -> \"" + argument + "\"", custom_utils::COLOR::BLUE);

        if (command != "AUTH")
        {
            // client not starting a secure connection
            probe_send("503 Not AUTH.");
            probe_receive();
            return;
        }

        // AUTH command received, check argument
        if (argument != "TLS" && argument != "SSL")
        {
            // unrecognized security extensions
            probe_send("502 Not supported.");
            probe_receive();
            return;
        }

        probe_send("234 Proceed.");
        start_secure(false);
    }

    void secure_session::probe_close()
    {
        boost::system::error_code ec = m_probe_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        m_probe_socket.close();
    }

    void secure_session::start_secure(bool is_implicit)
    {
        m_session_type = "FTPS";

        m_control_socket =
            boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(std::move(m_probe_socket), m_ssl_context);
        m_control_socket.async_handshake(boost::asio::ssl::stream_base::handshake_type::server,
                                         [self = shared_from_this(), is_implicit](boost::system::error_code ec) {
                                             if (ec)
                                             {
                                                 self->println("handshake error", custom_utils::COLOR::RED);
                                                 return;
                                             }

                                             if (is_implicit)
                                             {
                                                 // clients in implicit mode haven't seen the welcome message yet
                                                 self->control_send(FTP_WELCOMEMESSAGE);
                                             }

                                             self->m_connection_stage = CONNECTION_STAGE::UNAUTHENTICATED;

                                             // clients in explicit mode should send login details immediately
                                             self->control_receive();
                                         });
    }

    void secure_session::control_send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_control_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     self->handle_control_send_callback(ec, bytes_written);
                                 });
    }

    void secure_session::control_receive()
    {
        m_control_socket.async_read_some(
            boost::asio::buffer(m_buffer, MESSAGE_BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                auto [FTP_command, FTP_argument] = self->handle_control_receive_callback(ec, bytes_received);

                self->handle_command(FTP_command, FTP_argument);
            });
    }

    void secure_session::run_PASV()
    {
        int port = m_data_socket_acceptor.local_endpoint().port();

        // forming response
        std::string response_format = "227 Entering Passive Mode (";

        response_format +=
            string_replace(m_control_socket.lowest_layer().local_endpoint().address().to_string(), ".", ",");

        response_format += "," + std::to_string(port / 256) + "," + std::to_string(port % 256) + ")";

        control_send(response_format);
        control_receive();

        println("FTPS data channel listening on port " + std::to_string(port), custom_utils::COLOR::GREEN);
    }

    void secure_session::run_LIST(const std::string &argument)
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
        if (m_data_socket.lowest_layer().is_open())
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

    void secure_session::run_STOR(const std::string &argument)
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
                .id.empty())
        {
            println("Parent directory doesn't exists -> " + m_pending_write_file, custom_utils::COLOR::RED);
            return;
        }

        // if data socket is already accepted, that means STOR command is received late
        if (m_data_socket.lowest_layer().is_open())
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

    void secure_session::run_RETR()
    {
        // if data socket is already accepted, that means RETR command is received late
        if (m_data_socket.lowest_layer().is_open())
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

    void secure_session::control_close()
    {
        boost::system::error_code ec =
            m_control_socket.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

        if (ec)
        {
            println("Error during control socket shutdown", custom_utils::COLOR::YELLOW);
            return;
        }

        m_control_socket.next_layer().close();
    }

    void secure_session::data_acceptor_start_accept()
    {
        m_data_socket_acceptor.async_accept([self = shared_from_this()](boost::system::error_code ec,
                                                                        boost::asio::ip::tcp::socket socket) {
            if (ec)
            {
                self->println("data socket acceptor error -> " + ec.message(), custom_utils::COLOR::RED);
                return;
            }

            // disables nagle's algorithm to reduce small packet latency
            socket.set_option(boost::asio::ip::tcp::no_delay(true));

            self->m_data_socket =
                boost::asio::ssl::stream<boost::asio::ip::tcp::socket>(std::move(socket), self->m_ssl_context);

            self->m_data_socket.async_handshake(
                boost::asio::ssl::stream_base::handshake_type::server, [self](boost::system::error_code handshake_ec) {
                    if (handshake_ec)
                    {
                        self->println("handshake error", custom_utils::COLOR::RED);
                        return;
                    }

                    self->println("Data socket accepted and handshaked", custom_utils::COLOR::GREEN);

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
        });
    }

    void secure_session::data_send(const std::string &message)
    {
        boost::asio::async_write(m_data_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_sent) {
                                     self->handle_data_send_callback(ec, bytes_sent);
                                 });
    }

    void secure_session::data_async_receive()
    {
        boost::asio::async_read(m_data_socket, m_receive_buffer,
                                [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                                    self->handle_data_async_receive_callback(ec, bytes_read);
                                });
    }

    void secure_session::data_async_send()
    {
        m_send_file_stream->read(m_send_buffer, SEND_BUFFER_SIZE);

        boost::asio::async_write(m_data_socket, boost::asio::const_buffer(m_send_buffer, m_send_file_stream->gcount()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_sent) {
                                     self->handle_data_async_send_callback(ec, bytes_sent);
                                 });
    }

    void secure_session::data_close()
    {
        // close data channel
        println("Operation finished, closing data socket", custom_utils::COLOR::GREEN);

        if (!m_data_socket.next_layer().is_open())
        {
            println("Unable to close data socket. It is not open?", custom_utils::COLOR::YELLOW);
            return;
        }

        // cancel all async operations
        m_data_socket.next_layer().cancel();

        m_data_socket.async_shutdown([self = shared_from_this()](const boost::system::error_code &ec) {
            if (ec)
            {
                self->println("Error during data socket ssl shutdown, possible client sudden disconnect",
                              custom_utils::COLOR::RED);
                self->m_data_socket.next_layer().close();
                return;
            }

            boost::system::error_code temp_ec =
                self->m_data_socket.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both, temp_ec);
            if (temp_ec)
            {
                self->println("Error during data socket shutdown, possible client sudden disconnect",
                              custom_utils::COLOR::RED);
            }
            self->m_data_socket.next_layer().close();
        });
    }
} // namespace ftps