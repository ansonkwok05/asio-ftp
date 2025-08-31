#include "session_manager.h"
#include "../custom_utils.h"

namespace session_manager
{
    using custom_utils::print;

    void session_manager::start(session_ptr s, bool already_sent_welcome_message)
    {
        print("Session initializing\n", "green");

        std::string uuid = custom_utils::generate_uuid_string(16);
        try
        {
            sessions.insert(s);
            s->start([this](std::string s_uuid) { stop(s_uuid); }, uuid, already_sent_welcome_message);
        }
        catch (std::exception err)
        {
            print("Session Failed to initialize\n", "yellow");
            stop(uuid);
        }
    }

    void session_manager::stop(std::string s_uuid)
    {
        print("Stopping session\n", "green");

        // todo: there is an address boundary error that causes crash
        // crashes right on boost asio ssl part
        // possibly due to io_context is still running

        try
        {
            for (auto s : sessions)
            {
                if (s->s_id != s_uuid)
                {
                    continue;
                }

                // found session with s_uuid
                sessions.erase(s);
                print("Session stopped -> " + s_uuid + "\n", "green");
                return;
            }
        }
        catch (std::exception err)
        {
            print("Session Failed to stop\n", "yellow");
        }
    }
} // namespace session_manager