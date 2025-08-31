#include "../custom_utils.h"
#include "ftps_session.h"

namespace ftps_session
{
    using custom_utils::print;

    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket))
    {
        print("ftps_session class construct.\n", "white");
        // boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv13_server);

        // ssl_ctx.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
        //                     boost::asio::ssl::context::no_tlsv1 | boost::asio::ssl::context::no_tlsv1_1 |
        //                     boost::asio::ssl::context::single_dh_use);
        // ssl_ctx.use_certificate_chain_file("tls/cert.pem");
        // ssl_ctx.use_private_key_file("tls/key.pem", boost::asio::ssl::context::pem);
        // ssl_ctx.use_tmp_dh_file("tls/dh.pem");
    }

    session::~session()
    {
        print("ftps_session class destruct\n", "black");
    }

    void session::start()
    {
    }
} // namespace ftps_session