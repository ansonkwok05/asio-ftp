#pragma once

#include "../custom_utils.h"
#include "../database/user_db.h"
#include "../database/virtual_fs_db.h"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/streambuf.hpp>

#include <queue>
#include <string>

namespace ftps
{
    // server welcome message
    static constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome. Encryption required.";

    // buffer size in Bytes for receiving ftp messages
    constexpr size_t MESSAGE_BUFFER_SIZE = 512;

    // buffer size in Bytes for receiving files
    constexpr size_t RECEIVE_BUFFER_SIZE = 1024 * 512;

    // buffer size in Bytes for sending files
    constexpr size_t SEND_BUFFER_SIZE = 1024 * 512;

    class adaptive_session : public std::enable_shared_from_this<adaptive_session>
    {
    public:
        adaptive_session(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context &ssl_context);
        ~adaptive_session();

        void start();

    private:
        // interval time to check for client responses
        static constexpr int IMPLICIT_CHECK_INTERVAL_MS = 20;

        // time to wait for an implicit connection (tls handshake)
        static constexpr int IMPLICIT_TIMEOUT_MS = 500;

        custom_utils::stopwatch m_stopwatch;
        boost::asio::steady_timer m_timer;

        // session identifier, for logging purposes
        std::string m_session_id;

        boost::asio::ssl::context &m_ssl_context;
        boost::asio::ip::tcp::socket m_control_socket;

        void wait_for_implicit();

        std::vector<uint8_t> m_buffer;
        std::string m_received_string;

        void control_send(std::string message);
        void control_receive();
        void handle_received_string();
        void handle_FTP_command(std::string command, std::string argument);

        void control_close();

        void start_ftps_session(bool is_implicit);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };

    class secure_session : public std::enable_shared_from_this<secure_session>
    {
    public:
        secure_session(boost::asio::ip::tcp::socket socket, boost::asio::ssl::context &ssl_context, bool is_implicit,
                       std::string session_id);
        ~secure_session();

        void start();

    private:
        enum class CONNECTION_STAGE
        {
            UNAUTHENTICATED,
            LOGGED_IN
        };

        static constexpr int DATA_CHANNEL_BEGIN_PORT = 7000;

        // list of commands supported
        static constexpr std::array<std::string_view, 20> FTP_COMMANDS = {
            // Authentication username
            "USER",

            // Authentication password
            "PASS",

            // Return system type
            "SYST",

            // Disconnect.
            "QUIT",

            // Print working directory. Returns the current directory of the host.
            "PWD",

            // Sets the transfer mode (ASCII/Binary).
            "TYPE",

            // Enter passive mode.
            "PASV",

            // Returns information of a file or directory if specified, else information of the current working
            // directory is returned.
            "LIST",

            // Get the feature list implemented by the server.
            "FEAT",

            // Change to Parent Directory.
            "CDUP",

            // Change working directory.
            "CWD",

            // Make directory.
            "MKD",

            // Remove a directory.
            "RMD",

            // Delete file.
            "DELE",

            // Accept the data and to store the data as a file at the server site
            "STOR",

            // Retrieve a copy of the file
            "RETR",

            // Return the size of a file.
            "SIZE",

            // Protection Buffer Size
            "PBSZ",

            // Data Channel Protection Level.
            "PROT",

            // Select options for a feature (for example OPTS UTF8 ON).
            "OPTS",
        };

        std::string m_session_id;

        bool m_is_implicit;

        boost::asio::ssl::context &m_ssl_context;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_control_socket;

        boost::asio::ip::tcp::acceptor m_data_socket_acceptor;
        boost::asio::ssl::stream<boost::asio::ip::tcp::socket> m_data_socket;

        std::vector<uint8_t> m_buffer;
        std::string m_received_string;

        CONNECTION_STAGE m_connection_stage;

        void control_send(std::string message);

        void control_receive();
        void handle_received_string();
        void handle_FTP_command(std::string &command, std::string &argument);

        void control_close();

        std::string m_pending_directory_list;
        bool m_pending_directory_list_all;

        std::string m_pending_write_file;
        std::string m_sendable_file_id;

        user_db::user m_user;
        virtual_fs_db::virtual_fs m_virtual_fs;

        std::string m_userid;
        std::string m_username;

        std::string m_working_directory = "/";

        void data_acceptor_start_accept();

        std::queue<std::string> m_data_send_queue;
        void data_send(std::string message);
        void data_async_write();

        void data_directory_listing();

        boost::asio::streambuf m_receive_buffer;
        std::string m_receive_file_name;
        std::string m_receive_file_path;
        long long m_received_file_size;
        bool m_receive_end = false;
        std::unique_ptr<std::ofstream> m_receive_file_stream;
        void data_receive_file();
        void data_async_receive();

        char m_send_buffer[SEND_BUFFER_SIZE];
        bool m_send_end = false;
        std::unique_ptr<std::ifstream> m_send_file_stream;
        void data_send_file();
        void data_async_send();

        void data_close();

        std::string parse_metadata_time(std::string time_str);
        std::string return_parent_directory(std::string directory);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace ftps