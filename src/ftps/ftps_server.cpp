#include "../custom_utils.h"
#include "ftps_server.h"
#include "../database/fs_handler.h"
#include "ftp_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast.hpp>

#include <memory>
#include <string>
#include <stdexcept>

namespace ftps_server
{
    using custom_utils::println;

    server::server()
        : m_acceptor(m_io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT)), m_socket(m_io_ctx)
    {
        // get_public_ip();

        check_tls_keys();

        start_accepting();

        println("FTPS server listening on port -> " + std::to_string(PORT), custom_utils::COLOR::GREEN);

        m_strand = std::make_unique<boost::asio::strand<boost::asio::io_context::executor_type>>(
            boost::asio::make_strand(m_io_ctx.get_executor()));

        // run async operations in worker threads
        std::thread io_ctx_thread1([&] { m_io_ctx.run(); });
        std::thread io_ctx_thread2([&] { m_io_ctx.run(); });
        std::thread io_ctx_thread3([&] { m_io_ctx.run(); });
        m_io_ctx.run();

        io_ctx_thread1.join();
        io_ctx_thread2.join();
        io_ctx_thread3.join();
    }

    void server::check_tls_keys()
    {
        if (!fs_handler::directory_exists("tls"))
        {
            println("TLS folder not found, creating one", custom_utils::COLOR::YELLOW);
            fs_handler::create_directory("tls");
            throw std::runtime_error("TLS certificate required. (tls/cert.pem, tls/key.pem)");
        }

        if (!fs_handler::file_exists("tls/cert.pem"))
        {
            println("TLS certificate not found", custom_utils::COLOR::RED);
            throw std::runtime_error("Requires tls/cert.pem");
        }

        if (!fs_handler::file_exists("tls/key.pem"))
        {
            println("TLS private key not found", custom_utils::COLOR::RED);
            throw std::runtime_error("Requires tls/key.pem");
        }

        if (!fs_handler::file_exists("tls/dh.pem"))
        {
            println("TLS dh private key not found", custom_utils::COLOR::RED);
            throw std::runtime_error("Requires tls/dh.pem");
        }

        println("TLS keys checked", custom_utils::COLOR::GREEN);
    }

    void server::get_public_ip()
    {
        // todo: do a simple http get request to public ip api
        // pass it into ftp_session, then into ftps_session
        // to allow PASV to send correct ip to client

        // boost::asio::io_context io_ctx;

        // boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::method::sslv23_client);
        // boost::asio::ssl::stream<boost::asio::ip::tcp::socket> ssl_socket(ssl_ctx, io_ctx);

        // boost::asio::ip::tcp::resolver resolver(ioc);

        // boost::beast::tcp_stream stream(ioc);

        // auto const results = resolver.resolve("icanhazip.com", "80");
        // stream.connect(results);

        // boost::beast::http::request<boost::beast::http::string_body> req{boost::beast::http::verb::get, "/", 11};
        // req.set(boost::beast::http::field::host, "icanhazip.com");
        // req.set(boost::beast::http::field::user_agent, BOOST_BEAST_VERSION_STRING);

        // boost::beast::http::write(stream, req);

        // boost::beast::flat_buffer buffer;

        // boost::beast::http::response<boost::beast::http::dynamic_body> res;

        // boost::beast::http::read(stream, buffer, res);

        // println("\"" + boost::beast::buffers_to_string(buffer.data()) + "\"");

        // boost::beast::error_code ec;
        // stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

        // // ignore not_connected
        // //
        // if (ec && ec != boost::beast::errc::not_connected)
        //     throw boost::beast::system_error{ec};
    }

    void server::start_accepting()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                println("Failed to accept connection -> " + ec.message(), custom_utils::COLOR::RED);
                return;
            }

            std::make_shared<ftp_session::session>(std::move(m_socket))->start();

            this->start_accepting();
        });
    }

} // namespace ftps_server