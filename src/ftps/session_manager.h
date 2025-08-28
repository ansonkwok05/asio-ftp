#pragma once

#include <memory>
#include <set>

#include "session.h"

namespace session_manager
{
    typedef std::shared_ptr<session::session> session_ptr;

    class session_manager
    {
    public:
        void start(session_ptr s, bool already_sent_welcome_message);
        void stop(std::string s_uuid);

    private:
        std::set<session_ptr> sessions;
    };
} // namespace session_manager