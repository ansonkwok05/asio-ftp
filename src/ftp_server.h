#include <boost/asio.hpp>

namespace ftp_server
{
    class server
    {
    public:
        server();
        ~server();

    private:
        const int PORT = 21; // FTP control port

        boost::asio::io_context io_ctx;
        boost::asio::ip::tcp::acceptor acceptor;
    };
}