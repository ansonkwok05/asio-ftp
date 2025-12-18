#pragma once

#include "../custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>

namespace ftp_session
{
    // time to wait for an implicit connection (tls handshake)
    constexpr int IMPLICIT_TIMEOUT_MS = 400;

    constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome.";

    // 16B buffer size for messages
    constexpr size_t BUFFER_SIZE = 64;

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        boost::asio::ip::tcp::socket m_socket;

        std::vector<char> m_buffer;

        std::string m_received_string;

        void send(std::string message);
        void receive();
        void handle_FTP_command();

        void start_ftps_session(bool isImplicit);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace ftp_session