#pragma once

#include "../custom_utils.h"

#include <boost/asio.hpp>

#include <string>

namespace ftp_session
{
    // interval time to check for client responses
    constexpr int IMPLICIT_CHECK_INTERVAL_MS = 20;

    // time to wait for an implicit connection (tls handshake)
    constexpr int IMPLICIT_TIMEOUT_MS = 500;

    constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome.";

    // buffer size in Bytes for receiving ftp messages
    constexpr size_t BUFFER_SIZE = 512;

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        custom_utils::stopwatch m_stopwatch;
        std::unique_ptr<boost::asio::steady_timer> timer;

        boost::asio::ip::tcp::socket m_socket;

        void wait_for_implicit();

        std::vector<uint8_t> m_buffer;

        std::string m_received_string;

        void send(std::string message);
        void receive();
        void handle_FTP_command();

        void start_ftps_session(bool isImplicit);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace ftp_session