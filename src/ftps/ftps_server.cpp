#include "../custom_utils.h"
#include "ftps_server.h"
#include "../sqlite/fs_handler.h"
#include "ftp_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <stdexcept>

namespace ftps_server
{
    using custom_utils::println;

    server::server()
        : m_acceptor(io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT)), m_socket(io_ctx)
    {
        check_tls_keys();

        start_accepting();

        println("FTPS server listening on port -> " + std::to_string(PORT), "green");

        io_ctx.run(); // start all async operations, blocks until all async operations are done
    }

    void server::check_tls_keys()
    {
        if (!fs_handler::directory_exists("tls"))
        {
            println("TLS folder not found, creating one", "yellow");
            fs_handler::create_directory("tls");
        }

        if (!fs_handler::file_exists("tls/cert.pem"))
        {
            println("TLS certificate not found", "red");
            throw std::runtime_error("Requires tls/cert.pem");
        }

        if (!fs_handler::file_exists("tls/key.pem"))
        {
            println("TLS private key not found", "red");
            throw std::runtime_error("Requires tls/key.pem");
        }

        if (!fs_handler::file_exists("tls/dh.pem"))
        {
            println("TLS dh private key not found", "red");
            throw std::runtime_error("Requires tls/dh.pem");
        }

        println("TLS keys checked", "green");
    }

    void server::start_accepting()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                println("Failed to accept connection -> " + ec.message(), "red");
                return;
            }

            std::make_shared<ftp_session::session>(std::move(m_socket))->start();

            this->start_accepting();
        });
    }

} // namespace ftps_server