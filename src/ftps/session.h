#pragma once

#include "../sqlite/sqlite_wrapper.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace session
{
    constexpr int UNAUTHENTICATED = 0;
    constexpr int LOGGED_IN = 1;

    class session
    {
    public:
        session(boost::asio::ip::tcp::socket s, boost::asio::io_context &i_ctx, boost::asio::ssl::context &s_ctx);
        void start(std::function<void(std::string)> onDestroy, std::string session_id,
                   bool already_sent_welcome_message);

        std::string s_id; // need this for session_manager to cleanup

    private:
        const std::string FTP_WELCOMEMESSAGE = "220 Hello from my C++ FTP server!";

        // list of commands supported
        // the comments of the commands is from Wikipedia
        const std::vector<std::string> FTP_COMMANDS = {
            "USER", // Authentication username
            "PASS", // Authentication password

            "PBSZ", // Protection Buffer Size
            "PROT", // Data Channel Protection Level.
            "FEAT", // Get the feature list implemented by the server.
            "PWD",  // Print working directory. Returns the current directory of the host.
            "TYPE", // Sets the transfer mode (ASCII/Binary).
            "PASV", // Enter passive mode.
            "LIST", // Returns information of a file or directory if specified, else information of the current working
                    // directory is returned.

            "SYST", // Return system type

            // todo:
            // "OPTS UTF8 ON"
        };

        const int WAIT_TIME = 200; // milliseconds to wait for TLS handshake

        int connection_stage = 0;

        std::function<void(std::string)> destroy_callback; // call this on session destroy

        boost::asio::io_context &io_context;
        boost::asio::ssl::context &ssl_context;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> control_socket;

        // need to be shared_ptr, or else it gets destroyed during runtime
        std::shared_ptr<boost::asio::ip::tcp::acceptor> acceptor;
        std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> data_socket;

        std::vector<std::string> pending_data_channel_buffer;

        sqlite_wrapper::SQLiteDb database = sqlite_wrapper::SQLiteDb();

        std::string username;
        std::string userid; // got from "users" table database lookup

        std::string password;

        std::string PBSZ = "";
        std::string PROT = "";
        std::string TYPE = ""; // binary flag for whether to send in binary or ascii

        std::string working_directory = "/"; // "/" is the root directory in FTP

        void stop(); // cleanup

        void send_message(std::string message);
        void read_incoming_message();
        void data_channel_send(std::string message);
        void handle_FTP_command(std::string &data);
        bool command_exists(std::string command);

        std::string parse_metadata_time(std::string time_str);

        void log_behavior(std::string log); // log client behavior to implement new features
    };
} // namespace session