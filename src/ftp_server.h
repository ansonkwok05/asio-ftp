#pragma once

#include <boost/asio.hpp>

#include "session_manager.h"

namespace ftp_server
{
    class server
    {
    public:
        server();

    private:
        const int PORT = 6921; // FTP control port

        session_manager::session_manager manager;

        void accept_connection(boost::asio::ip::tcp::acceptor &, boost::asio::io_context &,
                               boost::asio::ssl::context &);
    };

} // namespace ftp_server