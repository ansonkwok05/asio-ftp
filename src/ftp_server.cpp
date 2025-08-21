#include <boost/asio.hpp>

#include "ftp_server.h"

namespace ftp_server
{
    server::server() : io_ctx(), acceptor(io_ctx)
    {
        boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), PORT);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();
    }

    server::~server()
    {
    }
}