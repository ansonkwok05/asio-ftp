#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_server
{
    class server
    {
    public:
        server();

    private:
        const int PORT = 6921; // port for FTPS server

        boost::asio::io_context io_ctx; // io context needed to run async operations

        boost::asio::ip::tcp::acceptor m_acceptor; // connection acceptor

        boost::asio::ip::tcp::socket m_socket; // a temporary socket for acceptor, which gets moved to a ftp session

        std::string m_public_ip; // public ip from api

        void check_tls_keys();
        void get_public_ip();

        void start_accepting();
    };

} // namespace ftps_server