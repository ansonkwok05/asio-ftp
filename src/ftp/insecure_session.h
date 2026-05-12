#pragma once

#include "base_session.h"

#include <memory>

#include <boost/asio/ip/tcp.hpp>

namespace ftp
{
    class session : public base_session, public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);

        void start() override;

    private:
        // server welcome message for non-secure session
        static constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome. This connection is not secure";

        boost::asio::ip::tcp::socket m_control_socket;

        boost::asio::ip::tcp::socket m_data_socket;

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
} // namespace ftp