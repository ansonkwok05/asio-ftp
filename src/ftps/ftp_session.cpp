#include "../custom_utils.h"
#include "ftp_session.h"
#include "ftps_session.h"

#include <boost/asio.hpp>

#include <string>
#include <vector>

namespace ftp_session
{
    using custom_utils::print;

    // session::session() : m_socket(std::move
    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket))
    {
        print("ftp_session class construct.\n", "white");
    }

    session::~session()
    {
        print("ftp_session class destruct\n", "black");
    }

    void session::start()
    {
        custom_utils::stopwatch stopwatch;
        stopwatch.start();

        // Explicit/Implicit compatibility

        // expect implicit FTPS clients to send TLS handshake immediately
        while (stopwatch.lapMs() < IMPLICIT_TIMEOUT_MS)
        {
            // expect TLS handshake to be longer than 100 bytes
            if (m_socket.available() > 100)
            {
                // implicit mode
                std::make_shared<ftps_session::session>(std::move(m_socket))->start();
                return;
            }
            custom_utils::sleep(20);
        }

        // explicit mode
        send(FTP_WELCOMEMESSAGE);
        receive();
    }

    void session::send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_socket, boost::asio::buffer(message),
                                 [this](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         print("1" + ec.message());
                                         return;
                                     }
                                 });
    }

    void session::receive()
    {
        char data[256];
        m_socket.async_read_some(boost::asio::buffer(data, 256),
                                 [this, &data](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         if (ec.message() == "Operation canceled")
                                         {
                                             print("Client disconnects\n");
                                             m_socket.close();

                                             // bad file descriptor error
                                             //  m_socket.shutdown(boost::asio::socket_base::shutdown_both);
                                             return;
                                         }
                                         print("Uncaught read_some error -> " + ec.message(), "yellow");
                                         return;
                                     }

                                     this->m_received_string = "";
                                     for (size_t i = 0; i < bytes_written; i++)
                                     {
                                         this->m_received_string += data[i];
                                     }

                                     handle_FTP_command();
                                 });
    }

    void session::handle_FTP_command()
    {
        std::vector<std::string> split_received_string = custom_utils::splitString(m_received_string, ' ');

        std::string FTP_command = split_received_string[0];
        std::string FTP_argument = custom_utils::vectorStrJoin(split_received_string, " ");

        if (FTP_command != "AUTH")
        {
            // client not starting a secure connection
            send("503 Not recognized.");
        }

        // AUTH command received
        if (FTP_argument != "TLS" && FTP_argument != "SSL")
        {
            // unexpected security extensions
            send("502 Not supported.");
        }

        send("234 Proceed.");
    }
} // namespace ftp_session