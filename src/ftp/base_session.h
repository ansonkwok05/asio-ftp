#pragma once

#include "constants.h"

#include "../database/users.h"
#include "../database/fs_objects.h"
#include "../custom_utils.h"

#include <string>
#include <fstream>

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/streambuf.hpp>

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
    void handle_control_send_callback(boost::system::error_code ec, size_t bytes_sent);

    virtual void control_receive() = 0;
    std::pair<std::string, std::string> handle_control_receive_callback(boost::system::error_code ec,
                                                                        size_t bytes_received);

    void handle_command(const std::string &command, const std::string &argument);
    virtual void run_PASV() = 0;
    virtual void run_LIST(const std::string &argument) = 0;
    virtual void run_STOR(const std::string &argument) = 0;

    void parse_RETR_argument(const std::string &argument);
    virtual void run_RETR() = 0;

    virtual void control_close() = 0;

    std::string m_pending_directory_list;
    bool m_pending_directory_list_all = false;

    std::string m_pending_write_file;
    std::string m_sendable_file_id;

    users::users m_users_table;
    fs_objects::fs_objects m_fs_objects_table;

    std::string m_userid;
    std::string m_username;

    std::string m_working_directory = "/";

    virtual void data_acceptor_start_accept() = 0; // todo extract data socket handling if logics

    virtual void data_send(const std::string &message) = 0;
    void handle_data_send_callback(boost::system::error_code ec, size_t bytes_sent);

    void data_directory_listing();

    boost::asio::streambuf m_receive_buffer;
    std::string m_receive_file_name;
    std::string m_receive_file_path;
    long long m_received_file_size;
    std::unique_ptr<std::ofstream> m_receive_file_stream;
    void data_receive_file();
    virtual void data_async_receive() = 0;
    void handle_data_async_receive_callback(boost::system::error_code ec, size_t bytes_read);
    void data_async_receive_end();

    char m_send_buffer[SEND_BUFFER_SIZE];
    std::unique_ptr<std::ifstream> m_send_file_stream;
    void data_send_file();
    virtual void data_async_send() = 0;
    void handle_data_async_send_callback(boost::system::error_code ec, size_t bytes_sent);
    void data_async_send_end();

    virtual void data_close() = 0;

    void println(const std::string &message);
    void println(const std::string &message, custom_utils::COLOR color);

private:
    std::pair<std::string, std::string> parse_buffer(size_t bytes_received);

    std::string create_directory_list(const std::vector<std::string> &fs_objects, const std::string &target_directory,
                                      std::string owner, bool include_special_entries);
};

namespace
{
    std::string parse_metadata_time(const std::string &time_str);
} // namespace