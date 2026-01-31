#include "ssh_session.h"
#include "../custom_utils.h"
#include <boost/asio/buffer.hpp>
#include <boost/asio/write.hpp>
#include <boost/system/detail/error_code.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <openssl/bio.h>
#include <openssl/core.h>
#include <openssl/ec.h>
#include <openssl/evp.h>
#include <openssl/obj_mac.h>
#include <openssl/params.h>
#include <openssl/pem.h>
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
        goto temp_skip_key;

        // prepare ecdsa-sha2-nistp256 privatekey and publickey;
        {
            std::ifstream private_pem("./ssh_keys/ecdsa_p256_private.pem");

            {
                std::ifstream public_pem("./ssh_keys/ecdsa_p256_public.pem");

                std::string temp_line;
                while (std::getline(public_pem, temp_line))
                {
                    m_ecdsa_p256_public_key += temp_line;
                }

                int rc;

                BIO *bio = BIO_new(BIO_s_mem());
                BIO_write(bio, m_ecdsa_p256_public_key.c_str(), m_ecdsa_p256_public_key.length());

                EVP_PKEY *pkey = EVP_PKEY_new();
                PEM_read_bio_PUBKEY(bio, &pkey, nullptr, nullptr);
                BIO_free(bio);

                size_t pkey_size;
                rc = EVP_PKEY_get_octet_string_param(pkey, "pub", NULL, 0, &pkey_size);
                println(std::to_string(rc));
                println(std::to_string(pkey_size));

                // todo:
                // stuck trying to extract ec point from ecdsa public key

                // unsigned char buf[100];

                // std::string temp;
                // for (auto s : buf)
                // {
                //     // temp += std::to_string(s) + " ";
                //     temp += (char)(s);
                //     // temp += "";
                // }
                // println(temp);

                // if (content.size() < 3)
                // {
                //     println("Public key missing content", custom_utils::COLOR::RED);
                //     throw std::runtime_error("Incorrect ecdsa_p256 public key");
                // }

                // if (content.front() != "-----BEGIN PUBLIC KEY-----")
                // {
                //     println("Public key missing header", custom_utils::COLOR::RED);
                //     throw std::runtime_error("Incorrect ecdsa_p256 public key");
                // }

                // if (content.back() != "-----END PUBLIC KEY-----")
                // {
                //     println("Public key missing footer", custom_utils::COLOR::RED);
                //     throw std::runtime_error("Incorrect ecdsa_p256 public key");
                // }

                // std::vector<std::string>::iterator content_it = content.begin() + 1;
                // while (content_it < content.end() - 1)
                // {
                //     b64_public_key += *content_it;
                //     content_it++;
                // }
            }

            // todo: parse pem -> ignore header/footer, decode the base64 payload
        }

    temp_skip_key:

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
        std::string server_protocol_version = "SSH-2.0-fileserver\r\n";
        std::vector<uint8_t> send_buffer;

        boost::asio::async_write(m_socket, boost::asio::buffer(server_protocol_version, server_protocol_version.size()),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         self->println("Unable to send SSH protocol", custom_utils::COLOR::RED);
                                         self->close_connection();
                                         return;
                                     }

                                     self->println("SSH server protocol sent to client");
                                 });

        // receive client protocol version
        // prepare receive buffer size for receiving client protocol version
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

                // do next
                self->key_exchange_init();
            });
    }

    session::binary_packet_protocol session::parse_bpp(std::vector<uint8_t> raw_data)
    {
        binary_packet_protocol bpp;

        if (raw_data.size() < 5)
            return bpp;

        // set packet_length to first 4 bytes
        bpp.packet_length = uint8_to_uint32(*raw_data.begin(), *(raw_data.begin() + 1), *(raw_data.begin() + 2),
                                            *(raw_data.begin() + 3));

        // set padding_length to the fifth byte
        bpp.padding_length = *(raw_data.begin() + 4);

        bpp.payload_length = bpp.packet_length - bpp.padding_length - 1;

        return bpp;
    }

    bool session::verify_bpp(session::binary_packet_protocol bpp)
    {
        // invalid bpp
        if (bpp.packet_length == 0 || bpp.padding_length < 4)
        {
            return false;
        }

        // check if payload size is less than 0 or exceeds MAXIMUM_PAYLOAD_SIZE
        if (bpp.payload_length < 0 || bpp.payload_length > bpp.MAXIMUM_PAYLOAD_SIZE)
        {
            return false;
        }

        // "packet_length" + "padding_length" + payload length + random padding
        // must be divisible by 8
        if ((bpp.payload_length + 5 + bpp.padding_length) % 8 != 0)
        {
            return false;
        }

        return true;
    }

    void session::key_exchange_init()
    {
        // send server key exchange init
        {
            key_exchange_init_packet keip;

            // generate cookie
            for (size_t i = 0; i < 16; i++)
            {
                keip.cookie[i] = rand() % 256;
            }

            keip.kex_algorithms = SUPPORTED_ALGORITHMS::kex_algorithms;
            keip.server_host_key_algorithms = SUPPORTED_ALGORITHMS::server_host_key_algorithms;
            keip.encryption_algorithms_client_to_server = SUPPORTED_ALGORITHMS::encryption_algorithms_client_to_server;
            keip.encryption_algorithms_server_to_client = SUPPORTED_ALGORITHMS::encryption_algorithms_server_to_client;
            keip.mac_algorithms_client_to_server = SUPPORTED_ALGORITHMS::mac_algorithms_client_to_server;
            keip.mac_algorithms_server_to_client = SUPPORTED_ALGORITHMS::mac_algorithms_server_to_client;
            keip.compression_algorithms_client_to_server =
                SUPPORTED_ALGORITHMS::compression_algorithms_client_to_server;
            keip.compression_algorithms_server_to_client =
                SUPPORTED_ALGORITHMS::compression_algorithms_server_to_client;
            keip.languages_client_to_server = SUPPORTED_ALGORITHMS::languages_client_to_server;
            keip.languages_server_to_client = SUPPORTED_ALGORITHMS::languages_server_to_client;
            keip.first_kex_packet_follows = SUPPORTED_ALGORITHMS::first_kex_packet_follows;

            std::vector<uint8_t> send_buffer;

            // binary packet protocol placeholder bytes
            send_buffer.push_back(0); // packet length
            send_buffer.push_back(0); // packet length
            send_buffer.push_back(0); // packet length
            send_buffer.push_back(0); // packet length
            send_buffer.push_back(0); // padding length

            send_buffer.push_back(keip.SSH_MSG_KEXINIT);
            for (auto x : keip.cookie)
            {
                send_buffer.push_back(x);
            }

            uint32_t kex_algorithms_length = keip.kex_algorithms.size();
            send_buffer.push_back(kex_algorithms_length >> 24);
            send_buffer.push_back(kex_algorithms_length >> 16);
            send_buffer.push_back(kex_algorithms_length >> 8);
            send_buffer.push_back(kex_algorithms_length);
            for (auto x : keip.kex_algorithms)
            {
                send_buffer.push_back(x);
            }

            uint32_t server_host_key_algorithms_length = keip.server_host_key_algorithms.size();
            send_buffer.push_back(server_host_key_algorithms_length >> 24);
            send_buffer.push_back(server_host_key_algorithms_length >> 16);
            send_buffer.push_back(server_host_key_algorithms_length >> 8);
            send_buffer.push_back(server_host_key_algorithms_length);
            for (auto x : keip.server_host_key_algorithms)
            {
                send_buffer.push_back(x);
            }

            uint32_t encryption_algorithms_client_to_server_length = keip.encryption_algorithms_client_to_server.size();
            send_buffer.push_back(encryption_algorithms_client_to_server_length >> 24);
            send_buffer.push_back(encryption_algorithms_client_to_server_length >> 16);
            send_buffer.push_back(encryption_algorithms_client_to_server_length >> 8);
            send_buffer.push_back(encryption_algorithms_client_to_server_length);
            for (auto x : keip.encryption_algorithms_client_to_server)
            {
                send_buffer.push_back(x);
            }

            uint32_t encryption_algorithms_server_to_client_length = keip.encryption_algorithms_server_to_client.size();
            send_buffer.push_back(encryption_algorithms_server_to_client_length >> 24);
            send_buffer.push_back(encryption_algorithms_server_to_client_length >> 16);
            send_buffer.push_back(encryption_algorithms_server_to_client_length >> 8);
            send_buffer.push_back(encryption_algorithms_server_to_client_length);
            for (auto x : keip.encryption_algorithms_server_to_client)
            {
                send_buffer.push_back(x);
            }

            uint32_t mac_algorithms_client_to_server_length = keip.mac_algorithms_client_to_server.size();
            send_buffer.push_back(mac_algorithms_client_to_server_length >> 24);
            send_buffer.push_back(mac_algorithms_client_to_server_length >> 16);
            send_buffer.push_back(mac_algorithms_client_to_server_length >> 8);
            send_buffer.push_back(mac_algorithms_client_to_server_length);
            for (auto x : keip.mac_algorithms_client_to_server)
            {
                send_buffer.push_back(x);
            }

            uint32_t mac_algorithms_server_to_client_length = keip.mac_algorithms_server_to_client.size();
            send_buffer.push_back(mac_algorithms_server_to_client_length >> 24);
            send_buffer.push_back(mac_algorithms_server_to_client_length >> 16);
            send_buffer.push_back(mac_algorithms_server_to_client_length >> 8);
            send_buffer.push_back(mac_algorithms_server_to_client_length);
            for (auto x : keip.mac_algorithms_server_to_client)
            {
                send_buffer.push_back(x);
            }

            uint32_t compression_algorithms_client_to_server_length =
                keip.compression_algorithms_client_to_server.size();
            send_buffer.push_back(compression_algorithms_client_to_server_length >> 24);
            send_buffer.push_back(compression_algorithms_client_to_server_length >> 16);
            send_buffer.push_back(compression_algorithms_client_to_server_length >> 8);
            send_buffer.push_back(compression_algorithms_client_to_server_length);
            for (auto x : keip.compression_algorithms_client_to_server)
            {
                send_buffer.push_back(x);
            }

            uint32_t compression_algorithms_server_to_client_length =
                keip.compression_algorithms_server_to_client.size();
            send_buffer.push_back(compression_algorithms_server_to_client_length >> 24);
            send_buffer.push_back(compression_algorithms_server_to_client_length >> 16);
            send_buffer.push_back(compression_algorithms_server_to_client_length >> 8);
            send_buffer.push_back(compression_algorithms_server_to_client_length);
            for (auto x : keip.compression_algorithms_server_to_client)
            {
                send_buffer.push_back(x);
            }

            uint32_t languages_client_to_server_length = keip.languages_client_to_server.size();
            send_buffer.push_back(languages_client_to_server_length >> 24);
            send_buffer.push_back(languages_client_to_server_length >> 16);
            send_buffer.push_back(languages_client_to_server_length >> 8);
            send_buffer.push_back(languages_client_to_server_length);
            for (auto x : keip.languages_client_to_server)
            {
                send_buffer.push_back(x);
            }

            uint32_t languages_server_to_client_length = keip.languages_server_to_client.size();
            send_buffer.push_back(languages_server_to_client_length >> 24);
            send_buffer.push_back(languages_server_to_client_length >> 16);
            send_buffer.push_back(languages_server_to_client_length >> 8);
            send_buffer.push_back(languages_server_to_client_length);
            for (auto x : keip.languages_server_to_client)
            {
                send_buffer.push_back(x);
            }
            if (keip.first_kex_packet_follows == false)
            {
                send_buffer.push_back(0);
            }
            else
            {
                send_buffer.push_back(1);
            }

            // reserved protocol bytes
            send_buffer.push_back(0);
            send_buffer.push_back(0);
            send_buffer.push_back(0);
            send_buffer.push_back(0);

            // calculate and set bpp protocol at front
            binary_packet_protocol bpp;

            bpp.padding_length = 8 - (send_buffer.size()) % 8;
            if (bpp.padding_length < 4)
            {
                bpp.padding_length += 8;
            }

            bpp.packet_length = send_buffer.size() - 4 + bpp.padding_length;

            *(send_buffer.begin()) = bpp.packet_length >> 24;
            *(send_buffer.begin() + 1) = bpp.packet_length >> 16;
            *(send_buffer.begin() + 2) = bpp.packet_length >> 8;
            *(send_buffer.begin() + 3) = bpp.packet_length;
            *(send_buffer.begin() + 4) = bpp.padding_length;

            // bpp random padding
            for (size_t i = 0; i < bpp.padding_length; i++)
            {
                send_buffer.push_back(rand() % 256);
            }

            m_socket.async_send(boost::asio::buffer(send_buffer),
                                [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                    if (ec)
                                    {
                                        self->println("Error trying to send key exchange init",
                                                      custom_utils::COLOR::RED);
                                        self->close_connection();
                                        return;
                                    }

                                    self->println("Server key exchange init sent to client");
                                });
        }

        // receive client key exchange init
        // read first 4 bytes for packet_length, 1 byte for padding_length
        m_socket.async_read_some(boost::asio::buffer(m_receive_buffer, 5), [self = shared_from_this()](
                                                                               boost::system::error_code ec,
                                                                               size_t bytes_read) {
            if (ec)
            {
                self->println("Unable to receive client binary packet protocol packet", custom_utils::COLOR::RED);
                self->close_connection();
                return;
            }

            binary_packet_protocol bpp = self->parse_bpp(self->m_receive_buffer);

            if (!self->verify_bpp(bpp))
            {
                self->println("Invalid binary packet protocol", custom_utils::COLOR::RED);
                self->close_connection();
                return;
            }

            // pre-allocate space for payload and padding
            self->m_receive_buffer.resize(bpp.payload_length + bpp.padding_length);

            // read the payload and random padding
            self->m_socket.async_read_some(
                boost::asio::buffer(self->m_receive_buffer, bpp.payload_length + bpp.padding_length),
                [self, bpp](boost::system::error_code ec, size_t bytes_read) {
                    if (ec)
                    {
                        self->println("Unable to receive client key exchange init", custom_utils::COLOR::RED);
                        self->close_connection();
                        return;
                    }

                    if (bytes_read != bpp.payload_length + bpp.padding_length)
                    {
                        self->println("Mismatch between package length and actual received length ",
                                      custom_utils::COLOR::RED);
                        self->println("Got " + std::to_string(bytes_read) + " instead of " +
                                          std::to_string(bpp.payload_length + bpp.padding_length),
                                      custom_utils::COLOR::RED);
                        self->close_connection();
                        return;
                    }

                    key_exchange_init_packet keip;

                    std::vector<uint8_t>::iterator received_buffer_iterator = self->m_receive_buffer.begin();

                    if (*received_buffer_iterator != keip.SSH_MSG_KEXINIT)
                    {
                        self->println("Client is not initiating key exchange", custom_utils::COLOR::RED);
                        self->close_connection();
                        return;
                    }
                    received_buffer_iterator++;

                    // store cookie for hash later
                    for (int i = 0; i < 16; i++)
                    {
                        keip.cookie[i] = *received_buffer_iterator;
                        received_buffer_iterator++;
                    }

                    // set kex_algorithms
                    {
                        uint32_t kex_algorithms_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.kex_algorithms =
                            std::string(received_buffer_iterator, received_buffer_iterator + kex_algorithms_length);
                        received_buffer_iterator += kex_algorithms_length;

                        if (keip.kex_algorithms.find(SUPPORTED_ALGORITHMS::kex_algorithms) == std::string::npos)
                        {
                            self->println("Client does not have supported kex_algorithms", custom_utils::COLOR::RED);
                            self->println(keip.kex_algorithms, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set server_host_key_algorithms
                    {
                        uint32_t server_host_key_algorithms_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.server_host_key_algorithms = std::string(
                            received_buffer_iterator, received_buffer_iterator + server_host_key_algorithms_length);
                        received_buffer_iterator += server_host_key_algorithms_length;

                        if (keip.server_host_key_algorithms.find(SUPPORTED_ALGORITHMS::server_host_key_algorithms) ==
                            std::string::npos)
                        {
                            self->println("Client does not have supported server_host_key_algorithms",
                                          custom_utils::COLOR::RED);
                            self->println(keip.server_host_key_algorithms, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set encryption_algorithms_client_to_server
                    {
                        uint32_t encryption_algorithms_client_to_server_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.encryption_algorithms_client_to_server =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + encryption_algorithms_client_to_server_length);
                        received_buffer_iterator += encryption_algorithms_client_to_server_length;

                        if (keip.encryption_algorithms_client_to_server.find(
                                SUPPORTED_ALGORITHMS::encryption_algorithms_client_to_server) == std::string::npos)
                        {
                            self->println("Client does not have supported encryption_algorithms_client_to_server",
                                          custom_utils::COLOR::RED);
                            self->println(keip.encryption_algorithms_client_to_server, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set encryption_algorithms_server_to_client
                    {
                        uint32_t encryption_algorithms_server_to_client_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.encryption_algorithms_server_to_client =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + encryption_algorithms_server_to_client_length);
                        received_buffer_iterator += encryption_algorithms_server_to_client_length;

                        if (keip.encryption_algorithms_server_to_client.find(
                                SUPPORTED_ALGORITHMS::encryption_algorithms_server_to_client) == std::string::npos)
                        {
                            self->println("Client does not have supported encryption_algorithms_server_to_client",
                                          custom_utils::COLOR::RED);
                            self->println(keip.encryption_algorithms_server_to_client, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set mac_algorithms_client_to_server
                    {
                        uint32_t mac_algorithms_client_to_server_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.mac_algorithms_client_to_server =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + mac_algorithms_client_to_server_length);
                        received_buffer_iterator += mac_algorithms_client_to_server_length;

                        if (keip.mac_algorithms_client_to_server.find(
                                SUPPORTED_ALGORITHMS::mac_algorithms_client_to_server) == std::string::npos)
                        {
                            self->println("Client does not have supported mac_algorithms_client_to_server",
                                          custom_utils::COLOR::RED);
                            self->println(keip.mac_algorithms_client_to_server, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set mac_algorithms_server_to_client
                    {
                        uint32_t mac_algorithms_server_to_client_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.mac_algorithms_server_to_client =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + mac_algorithms_server_to_client_length);
                        received_buffer_iterator += mac_algorithms_server_to_client_length;

                        if (keip.mac_algorithms_server_to_client.find(
                                SUPPORTED_ALGORITHMS::mac_algorithms_server_to_client) == std::string::npos)
                        {
                            self->println("Client does not have supported mac_algorithms_server_to_client",
                                          custom_utils::COLOR::RED);
                            self->println(keip.mac_algorithms_server_to_client, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set compression_algorithms_client_to_server
                    {
                        uint32_t compression_algorithms_client_to_server_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.compression_algorithms_client_to_server =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + compression_algorithms_client_to_server_length);
                        received_buffer_iterator += compression_algorithms_client_to_server_length;

                        if (keip.compression_algorithms_client_to_server.find(
                                SUPPORTED_ALGORITHMS::compression_algorithms_client_to_server) == std::string::npos)
                        {
                            self->println("Client does not have supported compression_algorithms_client_to_server",
                                          custom_utils::COLOR::RED);
                            self->println(keip.compression_algorithms_client_to_server, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set compression_algorithms_server_to_client
                    {
                        uint32_t compression_algorithms_server_to_client_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.compression_algorithms_server_to_client =
                            std::string(received_buffer_iterator,
                                        received_buffer_iterator + compression_algorithms_server_to_client_length);
                        received_buffer_iterator += compression_algorithms_server_to_client_length;

                        if (keip.compression_algorithms_server_to_client.find(
                                SUPPORTED_ALGORITHMS::compression_algorithms_server_to_client) == std::string::npos)
                        {
                            self->println("Client does not have supported compression_algorithms_server_to_client",
                                          custom_utils::COLOR::RED);
                            self->println(keip.compression_algorithms_server_to_client, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set languages_client_to_server
                    {
                        uint32_t languages_client_to_server_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.languages_client_to_server = std::string(
                            received_buffer_iterator, received_buffer_iterator + languages_client_to_server_length);
                        received_buffer_iterator += languages_client_to_server_length;

                        if (keip.languages_client_to_server.find(SUPPORTED_ALGORITHMS::languages_client_to_server) ==
                            std::string::npos)
                        {
                            self->println("Client does not have supported languages_client_to_server",
                                          custom_utils::COLOR::RED);
                            self->println(keip.languages_client_to_server, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set languages_server_to_client
                    {
                        uint32_t languages_server_to_client_length =
                            self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                  *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                        received_buffer_iterator += 4;

                        keip.languages_server_to_client = std::string(
                            received_buffer_iterator, received_buffer_iterator + languages_server_to_client_length);
                        received_buffer_iterator += languages_server_to_client_length;

                        if (keip.languages_server_to_client.find(SUPPORTED_ALGORITHMS::languages_server_to_client) ==
                            std::string::npos)
                        {
                            self->println("Client does not have supported languages_server_to_client",
                                          custom_utils::COLOR::RED);
                            self->println(keip.languages_server_to_client, custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                    }

                    // set first_kex_packet_follows
                    {
                        if (*received_buffer_iterator == 0)
                        {
                            keip.first_kex_packet_follows = false;
                        }
                        else
                        {
                            keip.first_kex_packet_follows = true;
                        }
                        received_buffer_iterator++;
                    }

                    // reserved protocol bytes
                    {
                        received_buffer_iterator += 4;
                    }

                    // padding bytes
                    {
                        received_buffer_iterator += bpp.padding_length;
                    }

                    // remaining unread bytes
                    if (received_buffer_iterator < self->m_receive_buffer.end())
                    {
                        self->println("Unprocessed bytes:", custom_utils::COLOR::YELLOW);
                        std::string temp;
                        while (received_buffer_iterator < self->m_receive_buffer.end())
                        {
                            temp += std::to_string(*received_buffer_iterator) + " ";
                            received_buffer_iterator++;
                        }
                        self->println(temp, custom_utils::COLOR::YELLOW);
                    }

                    // exchange diffie-hellmen key
                    self->key_exchange_ecdh_init();
                });
        });
    }

    void session::key_exchange_ecdh_init()
    {
        m_socket.async_read_some(
            boost::asio::buffer(m_receive_buffer, 5),
            [self = shared_from_this()](boost::system::error_code ec, size_t bytes_read) {
                if (ec)
                {
                    self->println("Unable to receive client binary packet protocol packet", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                binary_packet_protocol bpp = self->parse_bpp(self->m_receive_buffer);

                if (!self->verify_bpp(bpp))
                {
                    self->println("Invalid binary packet protocol", custom_utils::COLOR::RED);
                    self->close_connection();
                    return;
                }

                // pre-allocate space for payload and padding
                self->m_receive_buffer.resize(bpp.payload_length + bpp.padding_length);

                // read the payload and random padding
                self->m_socket.async_read_some(
                    boost::asio::buffer(self->m_receive_buffer, bpp.payload_length + bpp.padding_length),
                    [self, bpp](boost::system::error_code ec, size_t bytes_read) {
                        if (ec)
                        {
                            self->println("Unable to receive ecdh key exchange init", custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }

                        if (bytes_read != bpp.payload_length + bpp.padding_length)
                        {
                            self->println("Mismatch between package length and actual received length ",
                                          custom_utils::COLOR::RED);
                            self->println("Got " + std::to_string(bytes_read) + " instead of " +
                                              std::to_string(bpp.payload_length + bpp.padding_length),
                                          custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }

                        key_exchange_ecdh_init_packet keeip;

                        std::vector<uint8_t>::iterator received_buffer_iterator = self->m_receive_buffer.begin();

                        if (*received_buffer_iterator != keeip.SSH_MSG_KEX_ECDH_INIT)
                        {
                            self->println("Client is not initiating ecdh key exchange", custom_utils::COLOR::RED);
                            self->close_connection();
                            return;
                        }
                        received_buffer_iterator++;

                        // set Q_C (client ephemeral public key)
                        {
                            uint32_t Q_C_length =
                                self->uint8_to_uint32(*received_buffer_iterator, *(received_buffer_iterator + 1),
                                                      *(received_buffer_iterator + 2), *(received_buffer_iterator + 3));
                            received_buffer_iterator += 4;

                            keeip.Q_C =
                                std::vector<uint8_t>(received_buffer_iterator, received_buffer_iterator + Q_C_length);
                            received_buffer_iterator += Q_C_length;
                        }

                        received_buffer_iterator += bpp.padding_length;

                        // remaining unprocessed bytes
                        if (received_buffer_iterator < self->m_receive_buffer.end())
                        {
                            std::string temp;
                            while (received_buffer_iterator < self->m_receive_buffer.end())
                            {
                                temp += std::to_string(*received_buffer_iterator) + " ";
                                received_buffer_iterator++;
                            }
                            self->println(temp);
                        }

                        {
                            EC_GROUP *group = EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1);
                            EC_POINT *client_point = EC_POINT_new(group);

                            if (!EC_POINT_oct2point(group, client_point, keeip.Q_C.data(), keeip.Q_C.size(), nullptr))
                            {
                                self->println("Unable to parse client ec to point", custom_utils::COLOR::RED);
                                EC_POINT_free(client_point);
                                EC_GROUP_free(group);
                                self->close_connection();
                                return;
                            }

                            if (!EC_POINT_is_on_curve(group, client_point, nullptr))
                            {
                                self->println("Client ec point is not on curve", custom_utils::COLOR::RED);
                                EC_POINT_free(client_point);
                                EC_GROUP_free(group);
                                self->close_connection();
                                return;
                            }

                            // std::string temp;
                            // for (auto c : K)
                            // {
                            //     temp += std::to_string(c) + " ";
                            // }
                            // self->println(temp);

                            // EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(own_key, NULL);

                            {
                                // /* 1. Create an EVP_PKEY_CTX for ECDH */
                                // EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new(own_key, NULL);
                                // if (!ctx)
                                // { /* handle error */
                                // }

                                // /* 2. Initialise the context – key agreement operation */
                                // if (EVP_PKEY_derive_init(ctx) <= 0)
                                // { /* error */
                                // }

                                // /* 3. Set the peer public key in the context
                                //  *    (we need to convert EC_POINT → EVP_PKEY first)
                                //  */
                                // EC_KEY *peer_key = EC_KEY_new();
                                // EC_KEY_set_group(peer_key, group);
                                // EC_KEY_set_public_key(peer_key, client_point);

                                // EVP_PKEY *peer_pkey = EVP_PKEY_new();
                                // EVP_PKEY_assign_EC_KEY(peer_pkey, peer_key); /* ownership transferred */

                                // /* 4. Tell the context about the peer key */
                                // if (EVP_PKEY_derive_set_peer(ctx, peer_pkey) <= 0)
                                // { /* error */
                                // }

                                // /* 5. Determine buffer length for shared secret */
                                // size_t secret_len = 0;
                                // if (EVP_PKEY_derive(ctx, NULL, &secret_len) <= 0)
                                // { /* error */
                                // }

                                // unsigned char *secret = OPENSSL_malloc(secret_len);
                                // if (!secret)
                                // { /* error */
                                // }

                                // /* 6. Derive the shared secret */
                                // if (EVP_PKEY_derive(ctx, secret, &secret_len) <= 0)
                                // { /* error */
                                // }

                                /* ---------- use `secret` of length `secret_len` here ---------- */
                            }

                            // todo: shared secret

                            EC_POINT_free(client_point);
                            EC_GROUP_free(group);

                            // EC_POINT_

                            // todo:
                            // handle client Q_C

                            // send SSH_MSG_KEX_ECDH_REPLY back
                            {
                                key_exchange_ecdh_reply_packet keerp;

                                std::vector<uint8_t> send_buffer;

                                // binary packet protocol placeholder bytes
                                send_buffer.push_back(0); // packet length
                                send_buffer.push_back(0); // packet length
                                send_buffer.push_back(0); // packet length
                                send_buffer.push_back(0); // packet length
                                send_buffer.push_back(0); // padding length

                                send_buffer.push_back(keerp.SSH_MSG_KEX_ECDH_REPLY);
                            }
                        }
                    });
            });
    }

    void session::close_connection()
    {
        boost::system::error_code ec = m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        m_socket.close();
    }

    uint32_t session::uint8_to_uint32(uint8_t uint8_1, uint8_t uint8_2, uint8_t uint8_3, uint8_t uint8_4)
    {
        return (uint8_1 << 24) + (uint8_2 << 16) + (uint8_3 << 8) + uint8_4;
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