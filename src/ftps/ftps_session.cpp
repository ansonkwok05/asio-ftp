#include "../custom_utils.h"
#include "ftps_session.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    using custom_utils::print;
    using custom_utils::println;

    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket))
    // session::session(boost::asio::ip::tcp::socket socket)
    // : m_socket(std::move(socket)), m_ssl_context(boost::asio::ssl::context::tlsv13_server)
    {
        println("FTPS session created", "black");

        // m_ssl_context.set_options(boost::asio::ssl::context::default_workarounds |
        // boost::asio::ssl::context::no_sslv2 |
        //                           boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
        //                           boost::asio::ssl::context::single_dh_use);
        // m_ssl_context.use_certificate_chain_file("tls/cert.pem");
        // m_ssl_context.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        // m_ssl_context.use_tmp_dh_file("tls/dh.pem");
    }

    session::~session()
    {
        println("FTPS session destroyed", "black");
    }

    void session::start()
    {
    }
} // namespace ftps_session