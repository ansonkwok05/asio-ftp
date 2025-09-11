#include "../custom_utils.h"
#include "ftps_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    using custom_utils::print;
    using custom_utils::println;

    session::session(boost::asio::ip::tcp::socket socket, bool send_welcome_message)
        : m_socket(std::move(socket)), m_ssl_context(boost::asio::ssl::context::tlsv13_server)
    {
        println("FTPS session created", "black");

        m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                                  boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                                  boost::asio::ssl::context::single_dh_use);
        m_ssl_context.use_certificate_chain_file("tls/cert.pem");
        m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        m_ssl_context.use_tmp_dh_file("tls/dh.pem");

        m_send_welcome_message = send_welcome_message;
    }

    session::~session()
    {
        println("FTPS session destroyed", "black");
    }

    void session::start()
    {

        if (m_send_welcome_message)
        {
            send(FTP_WELCOMEMESSAGE);
        }
        receive();
    }

    void session::send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Uncaught async_write error -> " + ec.message(), "yellow");
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
                                             return;
                                         }
                                         self->println("Uncaught read_some error -> " + ec.message(), "yellow");
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
        std::string FTP_argument = custom_utils::vectorStrJoin(
            std::vector<std::string>(split_received_string.begin() + 1, split_received_string.end()), " ");

        println(FTP_command);
        println(FTP_argument);
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