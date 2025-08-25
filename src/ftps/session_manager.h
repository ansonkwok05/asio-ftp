#pragma once

#include <set>

#include "session.h"

namespace session_manager
{

    typedef std::shared_ptr<session::session> session_ptr;

    class session_manager
    {
    public:
        void start(session_ptr);
        void stop(session_ptr);
        void stop_all(session_ptr);

    private:
        std::set<session_ptr> sessions;
    };
} // namespace session_manager