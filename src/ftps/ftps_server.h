#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_server
{
    // port for FTPS server
    constexpr int PORT = 6921;

    class server
    {
    public:
        server();

    private:
        // io context needed to run async operations
        boost::asio::io_context io_ctx;

        // connection acceptor
        boost::asio::ip::tcp::acceptor m_acceptor;

        // a temporary socket for acceptor, which gets moved to a ftp session
        boost::asio::ip::tcp::socket m_socket;

        // public ip from api
        std::string m_public_ip;

        void check_tls_keys();
        void get_public_ip();

        void start_accepting();
    };

} // namespace ftps_server