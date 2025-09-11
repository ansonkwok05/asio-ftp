#include "../custom_utils.h"
#include "ftps_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    session::session(boost::asio::ip::tcp::socket socket, bool isImplicit)
        : m_ssl_context(boost::asio::ssl::context::tlsv13_server), m_buffer(BUFFER_SIZE)
    // : m_control_socket(std::move(socket), m_ssl_context), m_ssl_context(std::move(ssl_context))
    {
        m_isImplicit = isImplicit;

        m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                  boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                                  boost::asio::ssl::context::single_dh_use);
        m_ssl_context.use_certificate_chain_file("tls/cert.pem");
        m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        m_ssl_context.use_tmp_dh_file("tls/dh.pem");

        m_control_socket =
            std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>(std::move(socket), m_ssl_context);

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

        if (m_isImplicit)
        {
            // clients in implicit mode haven't seen the welcome message yet
            send(FTP_WELCOMEMESSAGE);
        }
        else
        {
            // clients in explicit mode will send login details immediately
            receive();
        }
    }

    void session::send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(*m_control_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     self->println("debug async_write");
                                     if (ec)
                                     {
                                         self->println("Uncaught async_write error -> " + ec.message(), "yellow");
                                         return;
                                     }
                                     self->receive();
                                 });
    }

    void session::receive()
    {
        m_control_socket->async_read_some(
            boost::asio::buffer(m_buffer, BUFFER_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                self->println("debug async_read_some");
                if (ec)
                {
                    if (ec == boost::asio::error::eof)
                    {
                        // client disconnected
                        return;
                    }
                    self->println("Uncaught read_some error -> " + ec.message(), "yellow");
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

                self->println("bytes received: " + std::to_string(bytes_received));

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

        println(FTP_command);
        println(FTP_argument);

        // todo: handle ftp commands

        send("XD"); // todo: change this to the unknown command response code
        receive();
    }

    void session::println(std::string message)
    {
        custom_utils::println("[FTPS] " + message);
    }

    void session::println(std::string message, std::string color)
    {
        custom_utils::println("[FTPS] " + message, color);
    }
} // namespace ftps_session