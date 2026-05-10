#include "server.h"
#include "../config/parser.h"
#include "../custom_utils.h"
#include "../database/fs_handler.h"
#include "insecure_session.h"
#include "secure_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include <string>
#include <stdexcept>

namespace ftp
{
    using custom_utils::println;

    server::server(config::parsed_config cfg)
        : m_ssl_context(boost::asio::ssl::context::tlsv13_server),
          m_acceptor(m_io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), cfg.port)),
          m_socket(m_io_context)
    {
        // get_public_ip();

        if (cfg.secure)
        {
            check_tls_keys();

            m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds |
                                      boost::asio::ssl::context::no_sslv2 | boost::asio::ssl::context::no_tlsv1 |
                                      boost::asio::ssl::context::no_tlsv1_1 | boost::asio::ssl::context::single_dh_use);
            m_ssl_context.use_certificate_chain_file("tls/cert.pem");
            m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
            m_ssl_context.use_tmp_dh_file("tls/dh.pem");

            start_accepting_secure();

            println("FTPS server listening on port -> " + std::to_string(cfg.port), custom_utils::COLOR::GREEN);
        }
        else
        {
            start_accepting_insecure();

            println("FTP server listening on port -> " + std::to_string(cfg.port), custom_utils::COLOR::GREEN);
        }

        m_io_context.run();
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

    void server::start_accepting_insecure()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                println("Failed to accept connection -> " + ec.message(), custom_utils::COLOR::RED);
                return;
            }

            std::make_shared<ftp::session>(std::move(m_socket))->start();

            this->start_accepting_insecure();
        });
    }

    void server::start_accepting_secure()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                println("Failed to accept connection -> " + ec.message(), custom_utils::COLOR::RED);
                return;
            }

            std::make_shared<ftps::adaptive_session>(std::move(m_socket), m_ssl_context)->start();

            this->start_accepting_secure();
        });
    }

} // namespace ftp