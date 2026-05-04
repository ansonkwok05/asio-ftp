#pragma once

#include "../custom_utils.h"
#include "../database/user_db.h"
#include "../database/virtual_fs_db.h"

#include <queue>
#include <string>
#include <fstream>

#include <boost/asio.hpp>

#include "constants.h"

namespace ftp
{
    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        // server welcome message for non-secure session
        static constexpr char FTP_WELCOMEMESSAGE[] = "220 Welcome. This connection is not secure";

        // session identifier, for logging purposes
        std::string m_session_id;

        boost::asio::ip::tcp::socket m_control_socket;

        boost::asio::ip::tcp::acceptor m_data_socket_acceptor;
        boost::asio::ip::tcp::socket m_data_socket;

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
} // namespace ftp