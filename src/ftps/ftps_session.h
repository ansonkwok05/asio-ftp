#pragma once

#include "../custom_utils.h"
#include "../database/user_db.h"
#include "../database/virtual_fs_db.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <string_view>
#include <vector>
#include <queue>

namespace ftps_session
{
    enum class CONNECTION_STAGE
    {
        UNAUTHENTICATED,
        LOGGED_IN
    };

    // 128B buffer size for receiving messages
    constexpr size_t MESSAGE_BUFFER_SIZE = 128;

    // 16MB buffer size for receiving files
    constexpr size_t RECEIVE_BUFFER_SIZE = 1024 * 1024 * 16;

    constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome.";

    // list of commands supported
    constexpr std::array<std::string_view, 20> FTP_COMMANDS = {
        // No operation (dummy packet; used mostly on keepalives).
        "NOOP",

        // Return system type
        "SYST",

        // Disconnect.
        "QUIT",

        // Authentication username
        "USER",

        // Authentication password
        "PASS",

        // Get the feature list implemented by the server.
        "FEAT",

        // Protection Buffer Size
        "PBSZ",

        // Data Channel Protection Level.
        "PROT",

        // Select options for a feature (for example OPTS UTF8 ON).
        "OPTS",

        // Print working directory. Returns the current directory of the host.
        "PWD",

        // Sets the transfer mode (ASCII/Binary).
        "TYPE",

        // Enter passive mode.
        "PASV",

        // Returns information of a file or directory if specified, else information of the current working
        // directory is returned.
        "LIST",

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
    };

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket, bool isImplicit);
        ~session();

        void start();

    private:
        // session identifier, for logging purposes
        std::string m_session_id;

        bool m_isImplicit;

        bool m_isLan;

        boost::asio::ssl::context m_ssl_context;
        std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_control_socket;

        std::unique_ptr<boost::asio::ip::tcp::acceptor> m_data_socket_acceptor;
        std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_data_socket;

        std::vector<char> m_buffer;
        std::string m_received_string;

        // int m_connection_stage;
        CONNECTION_STAGE m_connection_stage;

        user_db::user m_user;
        virtual_fs_db::virtual_fs m_virtual_fs;

        std::string m_userid;
        std::string m_username;
        std::string m_working_directory = "/";

        bool control_send_lock = false;
        std::queue<std::string> m_control_send_queue;
        void control_send(std::string message);
        void control_async_write();

        void control_receive();
        void handle_received_string();
        void handle_FTP_command(std::string &command, std::string &argument);

        std::string m_pending_directory_list;
        std::string m_pending_write_file;
        std::string m_pending_read_file;

        void data_acceptor_start_accept();

        std::queue<std::string> m_data_send_queue;
        void data_send(std::string message);
        void data_async_write();

        void data_directory_listing();

        boost::asio::streambuf m_receive_buffer;
        std::string m_receive_file_name;
        std::string m_receive_file_path;
        std::string m_receive_file_id;
        long long m_receive_file_size;
        bool m_receive_end = false;
        void data_receive_file();
        void data_async_receive();

        void data_send_file();

        void data_close();

        std::string parse_metadata_time(std::string time_str);
        std::string return_parent_directory(std::string directory);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace ftps_session