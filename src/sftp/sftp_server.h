#pragma once

#include <boost/asio.hpp>

namespace sftp_server
{
    constexpr int PORT = 6922;

    class server
    {
    public:
        server();

    private:
        std::unique_ptr<boost::asio::strand<boost::asio::io_context::executor_type>> m_strand;

        // io context needed to run async operations
        boost::asio::io_context m_io_ctx;

        // connection acceptor
        boost::asio::ip::tcp::acceptor m_acceptor;

        // a temporary socket for acceptor, which gets moved to a ftp session
        boost::asio::ip::tcp::socket m_socket;

        void start_accepting();
    };
} // namespace sftp_server