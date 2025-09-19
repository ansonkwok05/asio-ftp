#pragma once

#include "../sqlite/sqlite_wrapper.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ftps_session
{
    constexpr int UNAUTHENTICATED = 0;
    constexpr int LOGGED_IN = 1;

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket, bool isImplicit);
        ~session();

        void start();

    private:
        const size_t BUFFER_SIZE = 128; // buffer size in bytes, for receiving messages

        const std::string FTP_WELCOMEMESSAGE = "220 Welcome.";

        // list of commands supported
        const std::vector<std::string> FTP_COMMANDS = {
            "SYST", // Return system type
            "QUIT", // Disconnect.
            "USER", // Authentication username
            "PASS", // Authentication password

            "FEAT", // Get the feature list implemented by the server.
            "PBSZ", // Protection Buffer Size
            "PROT", // Data Channel Protection Level.
            "OPTS", // Select options for a feature (for example OPTS UTF8 ON).

            "PWD",  // Print working directory. Returns the current directory of the host.
            "TYPE", // Sets the transfer mode (ASCII/Binary).
            "PASV", // Enter passive mode.
            "LIST", // Returns information of a file or directory if specified, else information of the current working
                    // directory is returned.
            "CDUP", // Change to Parent Directory.
            "CWD",  // Change working directory.
            "MKD",  // Make directory.
            "RMD",  // Remove a directory.
            "DELE", // Delete file.
            "STOR", // Accept the data and to store the data as a file at the server site
            "RETR", // Retrieve a copy of the file
        };

        std::string m_session_id;

        bool m_isImplicit;

        boost::asio::ssl::context m_ssl_context;
        std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_control_socket;

        std::unique_ptr<boost::asio::ip::tcp::acceptor> m_data_socket_acceptor;
        std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> m_data_socket;

        std::vector<char> m_buffer;
        std::string m_received_string;

        int m_connection_stage;

        sqlite_wrapper::SQLiteDb m_database;
        std::string m_userid;
        std::string m_username;
        std::string m_working_directory = "/";

        std::string m_pending_write_file;
        std::string m_pending_read_file;

        // bug: data socket error when transferring a lot of files
        // only happens when doing lots of files at the same time

        // possible fix:
        // use a vector of strings to store m_pending_write_file
        // use a vector of strings to store m_pending_read_file

        // bug: sometimes cannot find file because
        // use absolute paths instead of name only
        // working_directory is changed before file is sent,
        // possible due to read_some is asynchronous

        std::vector<std::string> m_data_send_buffer;

        void control_send(std::string message);
        void control_receive();
        void handle_received_string();
        void handle_FTP_command(std::string &command, std::string &argument);

        void data_acceptor_start_accept();
        void data_send(std::string message);

        std::string parse_metadata_time(std::string time_str);
        std::string get_last_slash(std::string directory);
        std::vector<std::string> get_files_metadatas();

        void println(std::string message);
        void println(std::string message, std::string color);
    };
} // namespace ftps_session