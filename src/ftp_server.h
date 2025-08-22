#include <boost/asio.hpp>

#include <string>
#include <array>
#include <set>

namespace ftp_server
{
    class session
    {
    public:
        session(boost::asio::ip::tcp::socket);
        void start();
        void stop();
        void send_message(std::string);
        void read_incoming_message();
        void handle_FTP_command(std::string &);

    private:
        const std::string FTP_WELCOMEMESSAGE = "220 Hello from my C++ FTP server!";

        const std::vector<std::string> FTP_COMMANDS = {
            "AUTH",
            "USER",
        };

        boost::asio::ip::tcp::socket socket;

        std::string username;
    };

    typedef std::shared_ptr<session> session_ptr;

    class session_manager
    {
    public:
        void start(session_ptr);
        void stop(session_ptr);
        void stop_all(session_ptr);

    private:
        std::set<session_ptr> sessions;
    };

    class server
    {
    public:
        server();

    private:
        const int PORT = 6921; // FTP control port

        session_manager manager;

        void accept_connection(boost::asio::ip::tcp::acceptor &);
    };

} // namespace ftp_server