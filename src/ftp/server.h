#pragma once

#include "../config/parser.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftp
{
    class server
    {
    public:
        server(config::parsed_config cfg);

    private:
        // io context needed to run async operations
        boost::asio::io_context m_io_context;

        // ssl context for FTPS implementation
        boost::asio::ssl::context m_ssl_context;

        // connection acceptor
        boost::asio::ip::tcp::acceptor m_acceptor;

        // a temporary socket for acceptor, which gets moved to a ftp session
        boost::asio::ip::tcp::socket m_socket;

        // public ip from api
        std::string m_public_ip;

        void check_tls_keys();
        void get_public_ip();

        void start_accepting_insecure();
        void start_accepting_secure();
    };

} // namespace ftp