#pragma once

#include <unordered_map>

#include <boost/json/kind.hpp>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>

namespace config
{
    // data type lookup table for each user
    const std::unordered_map<std::string, boost::json::kind> CONFIG_USERS_TYPE_MAP = {
        {"name", boost::json::kind::string},
        {"password", boost::json::kind::string},
    };

    // data type lookup table
    const std::unordered_map<std::string, boost::json::kind> CONFIG_VALUE_TYPE_MAP = {
        {"secure", boost::json::kind::bool_},
        {"multi_threading", boost::json::kind::bool_},
        {"port", boost::json::kind::int64},
        {"users", boost::json::kind::array},
    };

    const boost::json::array DEFAULT_USERS({
        boost::json::object({
            {"name", "guest"},
            {"password", "12345"},
        }),
    });

    const boost::json::object DEFAULT_CONFIG = {
        {"secure", false},
        {"multi_threading", false},
        {"port", 6921},
        {"users", DEFAULT_USERS},
    };

    struct parsed_config
    {
        bool secure = false;
        bool multi_threading = false;
        int port = 6921;
        std::unordered_map<std::string, std::string> users;
    };

    parsed_config make_default_json();

    parsed_config read_json_config();
} // namespace config