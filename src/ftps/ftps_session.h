#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket, bool send_welcome_message);
        ~session();

        void start();

    private:
        const std::string FTP_WELCOMEMESSAGE = "220 Welcome.";

        bool m_send_welcome_message;

        boost::asio::ip::tcp::socket m_control_socket;
        boost::asio::ssl::context m_ssl_context;

        const size_t BUFFER_SIZE = 64; // buffer size in bytes
        std::vector<char> m_buffer;

        std::string m_received_string;

        void send(std::string message);
        void receive();
        void handle_FTP_command();

        void println(std::string message);
        void println(std::string message, std::string color);
    };
} // namespace ftps_session