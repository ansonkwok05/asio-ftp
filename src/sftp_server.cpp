#include "sftp_server.h"
#include "custom_utils.h"

#include <libssh/libssh.h>
#include <libssh/server.h>

#include <stdexcept>

namespace sftp_server
{
    using custom_utils::print;

    server::server()
    {
        ssh_init(); // initialize libssh cryptographic data structures

        createSSHBind();
        setBindOptions();
        startBinding(); // starts ssh server and listens to port
    }

    server::~server()
    {
        ssh_bind_free(bind);
    }

    void server::createSSHBind()
    {
        bind = ssh_bind_new();
        if (bind == NULL)
        {
            print("\n", "red");
            throw std::runtime_error("Failed to create ssh bind");
        }
        print("SSH bind created\n", "green");
    }

    void server::setBindOptions()
    {
        ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDADDR, BINDADDR.c_str());
        ssh_bind_options_set(bind, SSH_BIND_OPTIONS_BINDPORT_STR, BINDPORT.c_str());

        // todo: check for "ssh_key" folder, create one if not found
        // todo: check for rsa key file, exit program if not found

        ssh_bind_options_set(bind, SSH_BIND_OPTIONS_RSAKEY, "ssh_key/rsa.pem");

        int verbosity = SSH_LOG_FUNCTIONS;
        ssh_bind_options_set(bind, SSH_BIND_OPTIONS_LOG_VERBOSITY, &verbosity);

        print("Successfully set bind options\n", "green");
    }

    void server::startBinding()
    {
        if (ssh_bind_listen(bind) < 0)
        {
            ssh_bind_free(bind);

            print("Failed to listen to bind\n", "red");
            throw std::runtime_error("Target address -> " + BINDADDR + ":" + BINDPORT);
        }
        print("Set listen target to " + BINDADDR + ":" + BINDPORT + "\n", "green");
    }

    void server::startServerLoop()
    {
        print("Server loop started\n");
        print("\n");
        while (1)
        {
            ssh_session session = ssh_new();
            if (session == NULL)
            {
                print("Failed to preallocate ssh session\n", "red");
                custom_utils::sleep(1000);
                continue;
            }

            // Accept connection
            if (ssh_bind_accept(bind, session) != SSH_OK)
            {
                ssh_disconnect(session);
                ssh_free(session);

                print("Failed to accept ssh session\n", "red");
                custom_utils::sleep(1000);
                continue;
            }

            print("SSH connection received\n");

            ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD);
            // todo: add user authentication and use the sqlite database

            // // todo:
            // // Process the session
            // // handle authentication and SFTP operations here

            ssh_disconnect(session);
            ssh_free(session);
            print("SSH connect discarded\n");

            print("\n");

            custom_utils::sleep(1); // reduce cpu utilization when idle
        }
    }
}