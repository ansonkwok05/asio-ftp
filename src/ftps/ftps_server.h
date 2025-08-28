#pragma once

#include "session_manager.h"

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

        // milliseconds to wait for client
        // if client cannot fulfill request within this window, connection will be discarded
        const int WAIT_TIME = 200;

        const std::string FTP_WELCOMEMESSAGE = "220 Hello from my C++ FTPS server!\r\n";

        session_manager::session_manager manager;

        void accept_connection(boost::asio::ip::tcp::acceptor &acceptor, boost::asio::io_context &io_ctx,
                               boost::asio::ssl::context &ssl_ctx);
    };

} // namespace ftps_server