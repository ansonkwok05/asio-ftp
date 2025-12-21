#pragma once

#include "../custom_utils.h"

#include <boost/asio.hpp>

#include <string>

namespace sftp_session
{
    // interval time to check for client responses
    constexpr int INIT_CHECK_INTERVAL_MS = 20;

    // time to wait for the client init message
    constexpr int INIT_TIMEOUT_MS = 1000;

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        boost::asio::ip::tcp::socket m_socket;

        custom_utils::stopwatch m_stopwatch;
        std::unique_ptr<boost::asio::steady_timer> timer;

        std::vector<uint8_t> m_receive_buffer;

        void wait_for_client_init();

        void protocol_version_exchange();
        void key_exchange_init();

        void close_connection();

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace sftp_session