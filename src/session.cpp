#include "session.h"
#include "custom_utils.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <string>
#include <vector>

namespace session
{
    using custom_utils::print;

    session::session(boost::asio::ip::tcp::socket s, boost::asio::ssl::context &s_ctx) : socket(std::move(s), s_ctx)
    {
    }

    void session::start()
    {
        // // read first 3 bytes to see if client is implicit (immediate secure connection)
        // try
        // {
        //     char data[3];
        //     // size_t bytes_read = socket.read_some(boost::asio::buffer(data, 3));
        //     size_t bytes_read = socket.next_layer().read_some(boost::asio::buffer(data, 3));

        //     print("FIRST THREE BYTES -> ", "red");
        //     print(std::string(data) + "\n");
        // }
        // catch (boost::system::error_code err)
        // {
        //     print("FAILED READING FIRST THREE BYTES\n");
        //     print(std::string(err.what()) + "\n");
        // }
        // return; // remove later

        try
        {
            socket.handshake(boost::asio::ssl::stream_base::server);
            print("TLS handshake completed successfully\n");
        }
        catch (std::exception err)
        {
            print("TLS handshake failed -> " + std::string(err.what()) + "\n");
            // print("Possible non secure FTP client\n");

            stop();
            return;
        }

        send_message(FTP_WELCOMEMESSAGE);
        read_incoming_message();
    }

    void session::stop()
    {
        print("Session stopping\n", "green");
        try
        {
            socket.lowest_layer().close();
        }
        catch (std::exception err)
        {
            print("Failed to close -> " + std::string(err.what()) + "\n");
        }
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
                                   // - 2 just to remove "\n" at the end of the message
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
        std::vector<std::string> splitStr = custom_utils::splitString(std::string(data), ' ');

        bool found = false;
        for (size_t i = 0; i < FTP_COMMANDS.size(); i++)
        {
            if (splitStr.at(0) == FTP_COMMANDS.at(i))
            {
                print("Known FTP command -> ", "blue");
                print(splitStr.at(0));
                print("\n");
                found = true;
                break;
            }
        }

        if (!found)
        {
            print("FTP command not found -> " + splitStr.at(0) + "\n", "yellow");

            send_message("502 Command not recognized");
            return;
        }

        if (splitStr.at(0) == "AUTH")
        {
            if (splitStr.at(1) == "TLS")
            {
                send_message("234 AUTH TLS successful");
                return;
            }
            if (splitStr.at(1) == "SSL")
            {
                send_message("234 AUTH SSL successful");
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

            // todo: use sqlite db to get username, reply accordingly
            // send_message("230 User " + splitStr.at(1) + " logged in.");
            send_message("331 Password required.");

            return;
        }

        if (splitStr.at(0) == "PASS")
        {
            print("Password received -> ");
            print(splitStr.at(1));
            print("\n");

            if (username == "")
            {
                send_message("332 Account required.");
                return;
            }

            password = splitStr.at(1);

            // todo: use sqlite db to authenticate the password
            send_message("230 User " + username + " logged in.");
            // send_message("530 Wrong password.");
            return;
        }

        if (splitStr.at(0) == "SYST")
        {
            // client requests info about the type/version of FTP server
            // ignore for security

            send_message("215 FileZilla Server 1.4.3"); // spoof, its fine for now
            return;
        }

        if (splitStr.at(0) == "FEAT")
        {
            // client requests info about all supported features of this FTP server

            // todo: check this
            send_message("201 Feature: SIZE ; 1048576"); // not sure if this is true
            return;
        }

        if (splitStr.at(0) == "PBSZ")
        {
            // client suggest a preferred maximum size (in bytes)

            print("Client suggest a maximum size of " + splitStr.at(1) + " in bytes\n");
            send_message("200 PBSZ accepted, using 8192"); // not sure (not even implemented)
            // todo: check this
            return;
        }

        if (splitStr.at(0) == "PROT")
        {
            // client issued maximum protected buffer size in decimal integer
            //  C - Clear
            //  S - Safe
            //  E - Confidential
            //  P - Private
            print("Client suggest protection level of \"" + splitStr.at(1) + "\"\n");
            send_message("201 Protection mode set to " + splitStr.at(1)); // spoof
            // todo: actually implement the protection feature
            return;
        }

        if (splitStr.at(0) == "PWD")
        {
            // reply the current working directory
            // example: 257 "/home/joe"
            send_message("257 \"/home\"");
            return;
        }

        if (splitStr.at(0) == "TYPE")
        {
            // binary flag, reply 200 to accept
            // A: Turn the binary flag off.
            // A N: Turn the binary flag off.
            // I: Turn the binary flag on.
            // L 8: Turn the binary flag on.

            send_message("200 Success"); // todo: actually check and implement this feature
            return;
        }

        if (splitStr.at(0) == "PASV")
        {
            // client requests server to listen on data port
            // which means a new port needs to be created
            // reply format: 227 =h1,h2,h3,h4,p1,p2
            // ip address -> h1.h2.h3.h4
            // port -> p1*256 + p2

            // todo: listen a new data port, somehow
            // send_message("227 =127,0,0,1,123,123");
            send_message("502 Not implemented");
            return;
        }

        if (splitStr.at(0) == "PORT")
        {
            // client requests server to listen on data port
            // which means a new port needs to be created

            // todo: listen a new data port, somehow
            // send_message("200 Okay");
            send_message("502 Not implemented");
            return;
        }

        print("Unprocessed command -> " + splitStr.at(0) + "\n", "yellow");
    }
} // namespace session