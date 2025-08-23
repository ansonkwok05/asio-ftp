#include "ftp_server.h"
#include "session.h"
#include "custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>

namespace ftp_server
{
    using custom_utils::print;

    server::server()
    {
        boost::asio::io_context io_ctx;
        boost::asio::ip::tcp::acceptor acceptor(io_ctx,
                                                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT));

        // boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::sslv23);
        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12);

        ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                            boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                            boost::asio::ssl::context::single_dh_use);
        ssl_ctx.use_certificate_chain_file("tls/cert.pem");
        ssl_ctx.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        ssl_ctx.use_tmp_dh_file("tls/dh.pem");
        // ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds);

        accept_connection(acceptor, io_ctx, ssl_ctx);

        // blocks until all async operation is finished
        io_ctx.run();

        print("FTP server listening on port -> " + std::to_string(PORT) + "\n");
    }

    void server::accept_connection(boost::asio::ip::tcp::acceptor &acceptor, boost::asio::io_context &io_ctx,
                                   boost::asio::ssl::context &ssl_ctx)
    {
        acceptor.async_accept(
            [this, &acceptor, &io_ctx, &ssl_ctx](boost::system::error_code err, boost::asio::ip::tcp::socket socket) {
                if (err)
                {
                    print("Failed to accept connection\n");
                }

                // create FTP session for new connection
                manager.start(std::make_shared<session::session>(std::move(socket), ssl_ctx));

                accept_connection(acceptor, io_ctx, ssl_ctx);
            });
    }
} // namespace ftp_server