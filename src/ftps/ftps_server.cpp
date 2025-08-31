#include "../custom_utils.h"
#include "ftps_server.h"
#include "ftp_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>

namespace ftps_server
{
    using custom_utils::print;

    server::server()
        : m_acceptor(io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT)), m_socket(io_ctx)
    {
        start_accepting();

        print("FTPS server listening on port -> " + std::to_string(PORT) + "\n", "green");

        io_ctx.run(); // start all async operations, blocks until all async operations are done
    }

    void server::start_accepting()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                print("Failed to accept connection -> " + ec.message(), "red");
                return;
            }

            std::make_shared<ftp_session::session>(std::move(m_socket))->start();

            this->start_accepting();
        });
    }

} // namespace ftps_server