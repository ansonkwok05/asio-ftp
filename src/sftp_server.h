#include <libssh/server.h>
#include <libssh/callbacks.h>

#include <string>

// delelte later
// #include "custom_utils.h"
// using custom_utils::print;

namespace sftp_server
{
    class server
    {
    public:
        server();
        ~server();

        void startServerLoop();

    private:
        // todo: reading a config file for listen address and ports

        std::string BINDADDR = "127.0.0.1"; // currently set to listen locally
        std::string BINDPORT = "2222";      // custom port for dev / testing

        ssh_bind bind;

        // init functions
        void createSSHBind();
        void setBindOptions();
        void startBinding();

        void handle_session(ssh_session session);
    };
}