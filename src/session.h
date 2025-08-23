#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>

namespace session
{
    class session
    {
    public:
        session(boost::asio::ip::tcp::socket, boost::asio::ssl::context &);
        void start();
        void stop();

    private:
        const std::string FTP_WELCOMEMESSAGE = "220 Hello from my C++ FTP server!";

        // list of commands supported
        const std::vector<std::string> FTP_COMMANDS = {"AUTH", "USER", "PASS", "SYST", "FEAT", "PBSZ",
                                                       "PROT", "PWD",  "TYPE", "PASV", "PORT"};

        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket;

        std::string username;
        std::string password;

        void send_message(std::string);
        void read_incoming_message();
        void handle_FTP_command(std::string &);
    };
} // namespace session