#pragma once

#include <boost/asio.hpp>

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

        const std::string FTP_WELCOMEMESSAGE = "220 Welcome."; // must start with 220 and ends with \r\n

        boost::asio::ip::tcp::socket m_socket;

        const size_t BUFFER_SIZE = 64; // buffer size in bytes
        std::vector<int> m_buffer;

        std::string m_received_string;

        void send(std::string message);
        void receive();
        void handle_FTP_command();
    };
} // namespace ftp_session