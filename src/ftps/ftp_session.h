#pragma once

#include <boost/asio.hpp>

#include <string>

namespace ftp_session
{
    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        const int IMPLICIT_TIMEOUT_MS = 500; // time to wait for an implicit connection (tls handshake)

        const std::string FTP_WELCOMEMESSAGE = "220 Welcome.";

        boost::asio::ip::tcp::socket m_socket;

        const size_t BUFFER_SIZE = 64; // buffer size in bytes
        std::vector<char> m_buffer;

        std::string m_received_string;

        void send(std::string message);
        void receive();
        void handle_FTP_command();

        void println(std::string message);
        void println(std::string message, std::string color);
    };
} // namespace ftp_session