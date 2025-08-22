#include "ftp_server.h"
#include "custom_utils.h"

#include <boost/asio.hpp>

#include <string>
#include <vector>

namespace ftp_server
{
    using custom_utils::print;
    using custom_utils::printNum;

    server::server()
    {
        boost::asio::io_context io_ctx;
        boost::asio::ip::tcp::acceptor acceptor(io_ctx,
                                                boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT));

        accept_connection(acceptor);

        // blocks until all async operation is finished
        io_ctx.run();

        print("FTP server listening on port -> " + std::to_string(PORT) + "\n");
    }

    void server::accept_connection(boost::asio::ip::tcp::acceptor &acceptor)
    {
        acceptor.async_accept([this, &acceptor](boost::system::error_code err, boost::asio::ip::tcp::socket socket) {
            if (err)
            {
                print("Failed to accept connection\n");
            }

            // create new session for new connection
            manager.start(std::make_shared<session>(std::move(socket)));

            // write_welcome_message(socket);

            accept_connection(acceptor);
        });
    }

    void session_manager::start(session_ptr s)
    {
        sessions.insert(s);
        s->start();
    }

    void session_manager::stop(session_ptr s)
    {
        print("stopping a session\n");
        sessions.erase(s);
        s->stop();
    }

    void session_manager::stop_all(session_ptr s)
    {
        print("stopping all session\n");
        for (auto s : sessions)
        {
            s->stop();
        }
        sessions.clear();
    }

    session::session(boost::asio::ip::tcp::socket s) : socket(std::move(s))
    {
        print("\nSession initializing\n", "green");
    }

    void session::start()
    {
        send_message(FTP_WELCOMEMESSAGE);
        read_incoming_message();
    }

    void session::stop()
    {
        socket.close();
    }

    void session::send_message(std::string message)
    {
        message += "\r\n"; // all messages need to end in \r\n
        // boost::asio::async_write(socket, boost::asio::buffer(message),
        //                          [this](boost::system::error_code err, size_t bytes_transferred) {
        //                              if (err)
        //                              {
        //                                  print("yo error -> ");
        //                                  print(err.message());
        //                                  print("\n");
        //                                  return;
        //                              }
        //                              print("Sent message\n");

        //                              read_incoming_message();
        //                          });

        boost::asio::write(socket, boost::asio::buffer(message));
        print("Sent message\n");
    }

    void session::read_incoming_message()
    {
        char data[1024] = {0};
        socket.async_read_some(boost::asio::buffer(data, 1024),
                               [this, &data](boost::system::error_code err, size_t bytes_transferred) {
                                   if (err)
                                   {
                                       print("async_read_some error -> ");
                                       print(err.message());
                                       print("\n");
                                       return;
                                   }

                                   // remove unwanted data at the end of buffer
                                   // size of valid data is bytes_transferred
                                   // - 2 just to remove "\n"
                                   std::string sanitizedData = "";
                                   for (size_t i = 0; i < bytes_transferred - 2; i++)
                                   {
                                       sanitizedData += data[i];
                                   }

                                   //    print("\"");
                                   //    print(sanitizedData);
                                   //    print("\"\n");

                                   handle_FTP_command(sanitizedData);
                                   read_incoming_message();
                               });
    }

    void session::handle_FTP_command(std::string &data)
    {
        print("Handling FTP command\n");

        std::vector<std::string> splitStr = custom_utils::splitString(std::string(data), ' ');

        // print("Command received -> \"");
        // print(splitStr.at(0));
        // print("\"\n");

        bool found = false;

        for (size_t i = 0; i < FTP_COMMANDS.size(); i++)
        {
            if (splitStr.at(0) == FTP_COMMANDS.at(i))
            {
                found = true;
                break;
            }
        }

        if (!found)
        {
            print("FTP command not found -> ");
            print(splitStr.at(0));
            print("\n");

            send_message("502 Command not recognized");
            return;
        }

        if (splitStr.at(0) == "AUTH")
        {
            if (splitStr.at(1) == "TLS")
            {
                // todo: suppor tls
                send_message("503 TLS not supported.");
                return;
            }
            if (splitStr.at(1) == "SSL")
            {
                // todo: suppor ssl
                send_message("503 SSL not supported.");
                return;
            }

            send_message("503 Bad command syntax");
            return;
        }

        if (splitStr.at(0) == "USER")
        {
            print("Current user -> ");
            print(splitStr.at(1));
            print("\n");

            username = splitStr.at(1);

            send_message("230 User " + splitStr.at(1) + " logged in.");

            return;
        }

        // if (splitStr.at(0) == "USER")
        // {
        //     print("Processing USER arg -> ");
        //     print(splitStr.at(1));
        //     print("\n");

        //     send_message("503 Not yet implemented");
        //     return;
        // }

        print("Unrecognized command -> ", "blue");
        print(splitStr.at(0) + "," + splitStr.at(1));
        print("\n");

        return;
    }

    // void server::read_incoming_message(boost::asio::ip::tcp::socket &socket)
    // {
    //     char data[1024];
    //     socket.async_read_some(boost::asio::buffer(data, 1024),
    //                            [this, &socket, &data](boost::system::error_code err, size_t bytes_transferred) {
    //                                if (err)
    //                                {
    //                                    print("async_read_some error -> ");
    //                                    print(err.message());
    //                                    print("\n");
    //                                    return;
    //                                }
    //                                print("async_read_some received -> \"" + std::string(data) + "\"\n");
    //                                handle_FTP_command(socket, data);
    //                            });
    // }

    // void server::handle_FTP_command(boost::asio::ip::tcp::socket &socket, std::string command)
    // {
    //     if (command == "AUTH")
    //     {
    //         print("Known command :D\r\n");
    //         return;
    //     }
    //     print("Unknown command D: -> " + command + "\n");
    // }

} // namespace ftp_server