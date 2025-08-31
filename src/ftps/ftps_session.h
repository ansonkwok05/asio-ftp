#pragma once

#include <boost/asio.hpp>

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
    };
} // namespace ftps_session