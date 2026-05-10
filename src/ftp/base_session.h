#pragma once

#include "constants.h"

#include "../database/user_db.h"
#include "../database/virtual_fs_db.h"
#include "../custom_utils.h"

#include <string>
#include <fstream>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/streambuf.hpp"

class base_session
{
public:
    base_session(boost::asio::any_io_executor executor);
    ~base_session();

    virtual void start() = 0;

protected:
    // session identifier, for logging purposes
    std::string m_session_id;
    std::string m_session_type;

    boost::asio::ip::tcp::acceptor m_data_socket_acceptor;

    std::vector<uint8_t> m_buffer;

    CONNECTION_STAGE m_connection_stage;

    virtual void control_send(std::string message) = 0;
    void handle_control_send_callback(boost::system::error_code ec, size_t bytes_written);

    virtual void control_receive() = 0;
    void handle_control_receive_callback(boost::system::error_code ec, size_t bytes_received);

    void handle_command(const std::string &command, const std::string &argument);
    virtual void run_PASV() = 0;
    virtual void run_LIST(const std::string &argument) = 0;
    virtual void run_STOR(const std::string &argument) = 0;
    virtual void run_RETR(const std::string &argument) = 0;

    virtual void control_close() = 0;

    std::string m_pending_directory_list;
    bool m_pending_directory_list_all = false;

    std::string m_pending_write_file;
    std::string m_sendable_file_id;

    user_db::user m_user;
    virtual_fs_db::virtual_fs m_virtual_fs;

    std::string m_userid;
    std::string m_username;

    std::string m_working_directory = "/";

    virtual void data_acceptor_start_accept() = 0; // todo extract data socket handling if logics

    virtual void data_send(const std::string &message) = 0;
    void handle_data_send_callback(boost::system::error_code ec, size_t bytes_written);

    void data_directory_listing();

    boost::asio::streambuf m_receive_buffer;
    std::string m_receive_file_name;
    std::string m_receive_file_path;
    long long m_received_file_size;
    bool m_receive_end = false;
    std::unique_ptr<std::ofstream> m_receive_file_stream;
    void data_receive_file();
    virtual void data_async_receive() = 0; // todo: maybe the m_receive_end can be checked inside the callback

    char m_send_buffer[SEND_BUFFER_SIZE];
    bool m_send_end = false;
    std::unique_ptr<std::ifstream> m_send_file_stream;
    void data_send_file();
    virtual void data_async_send() = 0; // todo: maybe the m_send_end can be checked inside the callback

    virtual void data_close() = 0;

    void println(const std::string &message);
    void println(const std::string &message, custom_utils::COLOR color);
};