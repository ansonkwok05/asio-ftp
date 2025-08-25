#include "ftps_server.h"
#include "session.h"
#include "session_manager.h"
#include "../custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>

namespace ftps_server
{
    using custom_utils::print;

    server::server()
    {
        boost::asio::io_context io_ctx;
        boost::asio::ip::tcp::acceptor acceptor(io_ctx,
                                                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT));

        boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv13_server);

        ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
                            boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
                            boost::asio::ssl::context::single_dh_use);
        ssl_ctx.use_certificate_chain_file("tls/cert.pem");
        ssl_ctx.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        ssl_ctx.use_tmp_dh_file("tls/dh.pem");

        accept_connection(acceptor, io_ctx, ssl_ctx);

        print("FTPS server listening on port -> " + std::to_string(PORT) + "\n", "green");

        // blocks until all async operation is finished
        io_ctx.run();
    }

    void server::accept_connection(boost::asio::ip::tcp::acceptor &acceptor, boost::asio::io_context &io_ctx,
                                   boost::asio::ssl::context &ssl_ctx)
    {
        acceptor.async_accept(
            [this, &acceptor, &io_ctx, &ssl_ctx](boost::system::error_code err, boost::asio::ip::tcp::socket socket) {
                if (err)
                {
                    print("Failed to accept FTPS connection\n");
                    accept_connection(acceptor, io_ctx, ssl_ctx);
                    return;
                }
                print("FTPS connection accepted\n", "green");

                custom_utils::stopwatch stopwatch;
                stopwatch.start();

                // wait for TLS handshake (explicit mode)
                // give up after WAIT_TIME
                print("Start explicit mode\n", "green");
                while (stopwatch.lapMs() < WAIT_TIME)
                {
                    // message length longer than this is TLS handshake??
                    if (socket.available() > 30)
                        break;
                    custom_utils::sleep(50);
                }

                // if no TLS handshake initiated by client,
                // start FTPS implicit mode
                if (socket.available() == 0)
                {
                    print("Explicit mode failed\n", "green");
                    print("Start implicit mode\n", "green");
                    // start by sending welcome message
                    boost::asio::write(socket, boost::asio::buffer(FTP_WELCOMEMESSAGE));

                    // within WAIT_TIME,
                    // if no "AUTH" command is received,
                    // connection will be terminated
                    stopwatch.start();
                    while (stopwatch.lapMs() < WAIT_TIME)
                    {
                        // no available message
                        if (socket.available() == 0)
                        {
                            custom_utils::sleep(50);
                            continue;
                        }

                        print("len:" + std::to_string(socket.available()) + " -> ");

                        char data[1024] = {0};
                        size_t bytes_transferred = socket.read_some(boost::asio::buffer(data, 1024));
                        if (bytes_transferred > 2)
                        {
                            bytes_transferred -= 2; // remove "\n"
                        }

                        std::string sanitizedStr = "";
                        for (size_t i = 0; i < bytes_transferred; i++)
                        {
                            sanitizedStr += data[i];
                        }
                        print(sanitizedStr + "\n");

                        // start TLS handshake
                        if (sanitizedStr == "AUTH TLS" || sanitizedStr == "AUTH SSL")
                        {
                            // when some FTP clients connects in explicit mode
                            // instead of initiating the TLS handshake, they disconnects immediately after this
                            // FileZilla is able to initiate TLS handshake and establish a conection
                            // low priority todo: figure out how to deal with this
                            // log: alright i don't know how i fixed it, now my phone can connect. really confused
                            boost::asio::write(socket, boost::asio::buffer("234 Ready\r\n"));
                            break;
                        }
                        else
                        {
                            // probably non secure FTP clients trying to login
                            print("Unknown/Not AUTH command -> " + sanitizedStr + "\n");
                            boost::asio::write(socket, boost::asio::buffer("503 Use AUTH first\r\n"));
                        }
                    }

                    // waiting to receive ClientHello handshake
                    stopwatch.start();
                    while (stopwatch.lapMs() < WAIT_TIME)
                    {
                        if (socket.available() == 0)
                        {
                            custom_utils::sleep(50);
                            continue;
                        }
                    }

                    // ClientHello handshake not received,
                    // terminate connection
                    if (socket.available() == 0)
                    {
                        print("Handshake not received! Client is not initiating the handshake\n", "yellow");
                        print("Connection rejected\n", "red");
                        accept_connection(acceptor, io_ctx, ssl_ctx);
                        return;
                    }
                }

                print("Handshake size: " + std::to_string(socket.available()) + "\n");

                // create FTP session for new connection
                manager.start(std::make_shared<session::session>(std::move(socket), ssl_ctx));

                accept_connection(acceptor, io_ctx, ssl_ctx);
            });
    }
} // namespace ftps_server