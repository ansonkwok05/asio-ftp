#pragma once

#include "base_session.h"

#include "../custom_utils.h"

#include <string>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>

namespace ftps
{
    // server welcome message for secure only session
    static constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome. Encryption required.";

    class secure_session : public base_session, public std::enable_shared_from_this<secure_session>
    {
    public:
        secure_session(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context &ssl_context);

        void start() override;

    private:
        // interval time to check for client responses
        static constexpr int IMPLICIT_CHECK_INTERVAL_MS = 20;

        // time to wait for an implicit connection (tls handshake)
        static constexpr int IMPLICIT_TIMEOUT_MS = 500;

        // timer for waiting imlicit connection
        custom_utils::stopwatch m_stopwatch;
        boost::asio::steady_timer m_timer;

        // temporal socket to handle explicit/implicit switching
        boost::asio::ip::tcp::socket m_probe_socket;

        void wait_for_implicit();

        void probe_send(std::string message);
        void probe_receive();

        void handle_auth(const std::string &command, const std::string &argument);

        void probe_close();

        void start_secure(bool is_implicit);

        boost::asio::ssl::context &m_ssl_context;

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_control_socket;

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_data_socket;

        void control_send(std::string message) override;

        void control_receive() override;

        void run_PASV() override;
        void run_LIST(const std::string &argument) override;
        void run_STOR(const std::string &argument) override;

        void run_RETR() override;

        void control_close() override;

        void data_acceptor_start_accept() override;

        void data_send(const std::string &message) override;

        void data_async_receive() override;

        void data_async_send() override;

        void data_close() override;
    };
} // namespace ftps