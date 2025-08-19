#include <libssh/libssh.h>
#include <libssh/server.h>

#include <string>

namespace sftp_server
{
    class server
    {
    public:
        server();
        ~server();

        void startServerLoop();

    private:
        // todo: reading a config file for these constants would be good
        std::string BINDADDR = "127.0.0.1";
        std::string BINDPORT = "2222"; // ssh port

        ssh_bind bind;

        void createSSHBind();
        void setBindOptions();
        void startBinding();
    };
}