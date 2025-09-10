#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    class session
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        boost::asio::ip::tcp::socket m_socket;
        // boost::asio::ssl::context m_ssl_context;
    };
} // namespace ftps_session