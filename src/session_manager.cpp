#include "session_manager.h"
#include "custom_utils.h"

namespace session_manager
{
    using custom_utils::print;

    void session_manager::start(session_ptr s)
    {
        print("\nSession initializing\n", "green");
        try
        {
            sessions.insert(s);
            s->start();
        }
        catch (std::exception err)
        {
            print("Session Failed to initialize\n", "yellow");
            stop(s);
        }
    }

    void session_manager::stop(session_ptr s)
    {
        print("Stopping a session\n", "green");
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
} // namespace session_manager