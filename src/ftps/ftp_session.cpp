#include "../custom_utils.h"
#include "ftp_session.h"
#include "ftps_session.h"

#include <boost/asio.hpp>
#include <boost/asio/detail/chrono.hpp>
#include <boost/asio/ssl.hpp>

#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <string>
#include <vector>

namespace ftp_session
{
    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket)), m_buffer(BUFFER_SIZE)
    {
        println("session created for " + m_socket.remote_endpoint().address().to_string() + ":" +
                    std::to_string(m_socket.remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
    }

    session::~session()
    {
        println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
    }

    void session::start()
    {
        m_stopwatch.start();

        timer = std::make_unique<boost::asio::steady_timer>(m_socket.get_executor(),
                                                            std::chrono::milliseconds(IMPLICIT_CHECK_INTERVAL_MS));

        wait_for_implicit();
    }

    void session::wait_for_implicit()
    {
        if (m_stopwatch.lapMs() > IMPLICIT_TIMEOUT_MS)
        {
            // timeout, enter explicit mode
            send(FTP_WELCOMEMESSAGE);
            receive();
            return;
        }

        if (m_socket.available() > 100)
        {
            // enter implicit mode
            start_ftps_session(true);
            return;
        }

        timer->expires_at(timer->expiry() + std::chrono::milliseconds(IMPLICIT_CHECK_INTERVAL_MS));
        timer->async_wait([self = shared_from_this()](boost::system::error_code ec) { self->wait_for_implicit(); });
    }

    void session::send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unknown async_write error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }
                                 });
    }

    void session::receive()
    {
        m_socket.async_read_some(boost::asio::buffer(m_buffer, BUFFER_SIZE),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                                     if (ec)
                                     {
                                         if (ec == boost::asio::error::eof)
                                         {
                                             // client disconnected

                                             self->m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                                             self->m_socket.close();
                                             return;
                                         }
                                         self->println("Unknown read_some error -> " + ec.message(),
                                                       custom_utils::COLOR::YELLOW);
                                         return;
                                     }

                                     if (bytes_received == 1 || bytes_received == 2)
                                     {
                                         self->println("received incomplete message (length " +
                                                       std::to_string(bytes_received) + "), disconnecting");
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

                                     self->handle_FTP_command();
                                 });
    }

    void session::handle_FTP_command()
    {
        println("received: " + m_received_string);

        std::vector<std::string> split_received_string = custom_utils::splitString(m_received_string, ' ');

        std::string FTP_command = split_received_string[0];
        std::string FTP_argument = "";

        if (split_received_string.size() > 1)
        {
            // received message has argument(s)
            FTP_argument = custom_utils::vectorStrJoin(
                std::vector<std::string>(split_received_string.begin() + 1, split_received_string.end()), " ");
        }

        if (FTP_command != "AUTH")
        {
            // client not starting a secure connection
            send("503 Not recognized.");
            return;
        }

        // AUTH command received, checking argument
        if (FTP_argument != "TLS" && FTP_argument != "SSL")
        {
            // unexpected security extensions
            send("502 Not supported.");
            return;
        }

        send("234 Proceed.");
        start_ftps_session(false);
    }

    void session::start_ftps_session(bool isImplicit)
    {
        std::make_shared<ftps_session::session>(std::move(m_socket), isImplicit)->start();
    }

    void session::println(std::string message)
    {
        custom_utils::println("[FTP ] " + message);
    }

    void session::println(std::string message, custom_utils::COLOR color)
    {
        custom_utils::println("[FTP ] " + message, color);
    }
} // namespace ftp_session