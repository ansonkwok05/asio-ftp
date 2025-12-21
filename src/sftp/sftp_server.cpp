#include "sftp_server.h"
#include "ssh_session.h"
#include "../custom_utils.h"

namespace sftp_server
{
    using custom_utils::println;

    server::server()
        : m_acceptor(m_io_ctx, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), PORT)), m_socket(m_io_ctx)
    {
        start_accepting();

        println("SFTP server listening on port -> " + std::to_string(PORT), custom_utils::COLOR::GREEN);

        // std::thread io_ctx_thread1([&] { m_io_ctx.run(); });
        // std::thread io_ctx_thread2([&] { m_io_ctx.run(); });
        // std::thread io_ctx_thread3([&] { m_io_ctx.run(); });

        m_io_ctx.run();
        // io_ctx_thread1.join();
        // io_ctx_thread2.join();
        // io_ctx_thread3.join();
    }

    void server::start_accepting()
    {
        m_acceptor.async_accept(m_socket, [this](boost::system::error_code ec) {
            if (ec)
            {
                println("Failed to accept connection -> " + ec.message(), custom_utils::COLOR::RED);
                return;
            }

            std::make_shared<sftp_session::session>(std::move(m_socket))->start();

            this->start_accepting();
        });
    }
} // namespace sftp_server