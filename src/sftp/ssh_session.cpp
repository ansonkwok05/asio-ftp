#include "ssh_session.h"
#include "../custom_utils.h"
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <unistd.h>

namespace sftp_session
{
    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket))
    {
        println("session created for " + m_socket.remote_endpoint().address().to_string() + ":" +
                    std::to_string(m_socket.remote_endpoint().port()),
                custom_utils::COLOR::BRIGHTBLACK);
    }

    session::~session()
    {
        println("session destroyed", custom_utils::COLOR::BRIGHTBLACK);
    }

    void session::start()
    {
        m_stopwatch.start();

        timer = std::make_unique<boost::asio::steady_timer>(m_socket.get_executor(),
                                                            std::chrono::milliseconds(INIT_CHECK_INTERVAL_MS));
        wait_for_client_init();
    }

    void session::wait_for_client_init()
    {
        if (m_stopwatch.lapMs() > INIT_TIMEOUT_MS)
        {
            // timeout, connection too weak
            close_connection();
            return;
        }

        if (m_socket.available() > 0)
        {
            protocol_version_exchange();
            return;
        }

        timer->expires_at(timer->expiry() + std::chrono::milliseconds(INIT_CHECK_INTERVAL_MS));
        timer->async_wait([self = shared_from_this()](boost::system::error_code ec) { self->wait_for_client_init(); });
    }

    void session::protocol_version_exchange()
    {
        // send server protocol version
        boost::asio::async_write(m_socket, boost::asio::buffer("SSH-2.0-fileserver\r\n"),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unable to send SSH protocol", custom_utils::COLOR::RED);
                                         self->close_connection();
                                         return;
                                     }
                                 });

        // receive client protocol version
        constexpr int MAX_SSH_PROTOCOL_VERSION_EXCHANGE_SIZE = 255;
        m_receive_buffer.resize(MAX_SSH_PROTOCOL_VERSION_EXCHANGE_SIZE);
        m_socket.async_read_some(
            boost::asio::buffer(m_receive_buffer, MAX_SSH_PROTOCOL_VERSION_EXCHANGE_SIZE),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                if (ec)
                {
                    self->println("Unable to receive SSH protocol version", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                if (bytes_read <= 2)
                {
                    self->println("Client protocol version too short", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                std::string client_protocol(self->m_receive_buffer.begin(),
                                            self->m_receive_buffer.begin() + bytes_read);

                // expect last two characters to be CRLF (\r\n)
                if (client_protocol.substr(bytes_read - 2, 2) != "\r\n")
                {
                    self->println("Client protocol version missing CRLF", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                // expect the protocol to be SSH-2.0
                if (client_protocol.substr(0, 7) != "SSH-2.0")
                {
                    self->println("Unsupported client protocol version", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                self->key_exchange_init();
            });
    }

    void session::key_exchange_init()
    {
        // send server key exchange init
        {
            //   name-list    kex_algorithms
            //   name-list    server_host_key_algorithms
            //   name-list    encryption_algorithms_client_to_server
            //   name-list    encryption_algorithms_server_to_client
            //   name-list    mac_algorithms_client_to_server
            //   name-list    mac_algorithms_server_to_client
            //   name-list    compression_algorithms_client_to_server
            //   name-list    compression_algorithms_server_to_client
            //   name-list    languages_client_to_server
            //   name-list    languages_server_to_client
            //   boolean      first_kex_packet_follows
            //   uint32       0 (reserved for future extension)

            std::vector<uint8_t> negotiation_packet;

            // SSH_MSG_KEXINIT
            negotiation_packet.push_back(20);

            // cookie (random bytes)
            for (size_t i = 0; i < 16; i++)
            {
                negotiation_packet.push_back(rand() % 256);
            }

            // std::string temp = "waiting to send -> ";
            // for (auto b : negotiation_packet)
            // {
            //     temp += std::to_string(b) + " ";
            // }
            // println(temp);
        }

        // receive client key exchange init
        // read first 4 bytes for the packet_length info
        m_socket.async_read_some(
            boost::asio::buffer(m_receive_buffer, 4),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                if (ec)
                {
                    self->println("Unable to receive client key exchange packet length", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                if (bytes_read != 4)
                {
                    self->println("Incomplete client key exchange packet length received", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                uint32_t packet_length = (self->m_receive_buffer[0] << 24) + (self->m_receive_buffer[1] << 16) +
                                         (self->m_receive_buffer[2] << 8) + self->m_receive_buffer[3];

                self->m_receive_buffer.resize(packet_length);
                self->m_socket.async_read_some(
                    boost::asio::buffer(self->m_receive_buffer, packet_length),
                    [self, packet_length](boost::system::error_code ec, size_t bytes_read) {
                        if (ec)
                        {
                            self->println("Unable to receive client key exchange payload", custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }

                        if (packet_length != bytes_read)
                        {
                            self->println("Mismatch between packet length and actual received length",
                                          custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }

                        uint8_t padding_length = self->m_receive_buffer[0];

                        int payload_length = packet_length - padding_length - 1;
                        constexpr int MAXIMUM_PAYLOAD_SIZE = 32768;
                        if (payload_length > MAXIMUM_PAYLOAD_SIZE)
                        {
                            self->println("Surpassed maximum payload size", custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }

                        std::string temp = "waiting to parse -> ";
                        for (size_t i = 1; i < payload_length; i++)
                        {
                            temp += std::to_string(self->m_receive_buffer[i]) + " ";
                        }
                        self->println(temp);
                    });
            });
    }

    void session::close_connection()
    {
        boost::system::error_code ec = m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        m_socket.close();
    }

    void session::println(std::string message)
    {
        custom_utils::println("[SFTP] " + message);
    }

    void session::println(std::string message, custom_utils::COLOR color)
    {
        custom_utils::println("[SFTP] " + message, color);
    }
} // namespace sftp_session