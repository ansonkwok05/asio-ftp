#pragma once

#include "../custom_utils.h"

#include <boost/asio.hpp>

#include <string>

namespace sftp_session
{
    // interval time to check for client responses
    constexpr int INIT_CHECK_INTERVAL_MS = 20;

    // time to wait for the client init message
    constexpr int INIT_TIMEOUT_MS = 1000;

    class session : public std::enable_shared_from_this<session>
    {
    public:
        session(boost::asio::ip::tcp::socket socket);
        ~session();

        void start();

    private:
        boost::asio::ip::tcp::socket m_socket;

        custom_utils::stopwatch m_stopwatch;
        std::unique_ptr<boost::asio::steady_timer> timer;

        std::vector<uint8_t> m_receive_buffer;

        void wait_for_client_init();

        static constexpr int MAX_SSH_PROTOCOL_VERSION_EXCHANGE_SIZE = 255;
        void protocol_version_exchange();

        struct binary_packet_protocol
        {
            static constexpr int MAXIMUM_PAYLOAD_SIZE = 32768;
            uint32_t packet_length = 0;
            uint8_t padding_length = 0;
            int payload_length = 0;
        };
        binary_packet_protocol parse_bpp(std::vector<uint8_t> raw_data);
        bool verify_bpp(binary_packet_protocol bpp);

        struct key_exchange_init_packet
        {
            static constexpr uint8_t SSH_MSG_KEXINIT = 20;
            std::array<uint8_t, 16> cookie;
            std::string kex_algorithms;
            std::string server_host_key_algorithms;
            std::string encryption_algorithms_client_to_server;
            std::string encryption_algorithms_server_to_client;
            std::string mac_algorithms_client_to_server;
            std::string mac_algorithms_server_to_client;
            std::string compression_algorithms_client_to_server;
            std::string compression_algorithms_server_to_client;
            std::string languages_client_to_server;
            std::string languages_server_to_client;
            bool first_kex_packet_follows;
        };
        struct SUPPORTED_ALGORITHMS
        {
            static constexpr char kex_algorithms[] = "ecdh-sha2-nistp256";
            static constexpr char server_host_key_algorithms[] = "ecdsa-sha2-nistp256";
            static constexpr char encryption_algorithms_client_to_server[] = "aes128-ctr";
            static constexpr char encryption_algorithms_server_to_client[] = "aes128-ctr";
            static constexpr char mac_algorithms_client_to_server[] = "hmac-sha2-256";
            static constexpr char mac_algorithms_server_to_client[] = "hmac-sha2-256";
            static constexpr char compression_algorithms_client_to_server[] = "none";
            static constexpr char compression_algorithms_server_to_client[] = "none";
            static constexpr char languages_client_to_server[] = "";
            static constexpr char languages_server_to_client[] = "";
            static constexpr bool first_kex_packet_follows = false;
        };
        void key_exchange_init();

        struct key_exchange_ecdh_init_packet
        {
            static constexpr uint8_t SSH_MSG_KEX_ECDH_INIT = 30;
            std::vector<uint8_t> Q_C;
        };
        struct key_exchange_ecdh_reply_packet
        {
            static constexpr uint8_t SSH_MSG_KEX_ECDH_REPLY = 31;
            static constexpr char HOST_KEY_TYPE[] = "ecdsa-sha2-nistp256";
        };
        std::string m_ecdsa_p256_public_key;
        std::string m_ecdsa_p256_private_key;
        void key_exchange_ecdh_init();

        void close_connection();

        uint32_t uint8_to_uint32(uint8_t uint8_1, uint8_t uint8_2, uint8_t uint8_3, uint8_t uint8_4);

        void println(std::string message);
        void println(std::string message, custom_utils::COLOR color);
    };
} // namespace sftp_session