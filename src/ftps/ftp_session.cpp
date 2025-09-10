#include "../custom_utils.h"
#include "ftp_session.h"
#include "ftps_session.h"

#include <boost/asio.hpp>

#include <string>
#include <vector>

namespace ftp_session
{
    using custom_utils::print;
    using custom_utils::println;

    session::session(boost::asio::ip::tcp::socket socket) : m_socket(std::move(socket))
    {
        println("FTP session created", "black");
    }

    session::~session()
    {
        println("FTP session destroyed", "black");
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
    }

    void session::send(std::string message)
    {
        message += "\r\n";
        boost::asio::async_write(m_socket, boost::asio::buffer(message),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_written) {
                                     if (ec)
                                     {
                                         println("Uncaught async_write error -> " + ec.message(), "yellow");
                                         return;
                                     }
                                     self->receive();
                                 });
    }

    void session::receive()
    {
        m_socket.async_read_some(boost::asio::buffer(m_buffer),
                                 [self = shared_from_this()](boost::system::error_code ec, size_t bytes_received) {
                                     if (ec)
                                     {
                                         if (ec.message() == "End of file")
                                         {
                                             // client disconnects
                                             return;
                                         }
                                         println("Uncaught read_some error -> " + ec.message(), "yellow");
                                         return;
                                     }

                                     // remove unwanted data

                                     println(std::to_string(bytes_received));

                                     //  self->handle_FTP_command();
                                 });
    }

    void session::handle_FTP_command()
    {
        println(m_received_string);

        std::vector<std::string> split_received_string = custom_utils::splitString(m_received_string, ' ');

        std::string FTP_command = split_received_string[0];
        std::string FTP_argument = custom_utils::vectorStrJoin(
            std::vector<std::string>(split_received_string.begin() + 1, split_received_string.end()), " ");

        if (FTP_command != "AUTH")
        {
            // client not starting a secure connection
            send("503 Not recognized.");
            return;
        }

        // AUTH command received, checking argument
        if (FTP_argument != "TLS" && FTP_argument != "SSL")
        {
            // unexpected security extensions
            send("502 Not supported.");
            return;
        }

        send("234 Proceed.");
        // std::make_shared<ftps_session::session>(std::move(m_socket))->start();
    }
} // namespace ftp_session