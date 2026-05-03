#include "parser.h"
#include "../custom_utils.h"
#include "../database/fs_handler.h"

#include <fstream>
#include <string>

#include <boost/json/serialize.hpp>
#include <boost/json/object.hpp>
#include <boost/json/stream_parser.hpp>
#include <boost/system/detail/error_code.hpp>
#include <boost/json/kind.hpp>
#include <boost/json/array.hpp>

namespace config
{
    using custom_utils::println;

    parsed_config make_default_json()
    {
        fs_handler::create_directory("config");

        std::string output_string = boost::json::serialize(DEFAULT_CONFIG);
        std::ofstream cfg_ofstream("config/config.json");

        // beautify json string then write to file
        int indent = 0;
        for (char c : output_string)
        {
            if (c == '{' || c == '[')
            {
                indent++;
                cfg_ofstream << c << '\n';
                for (int i = 0; i < indent; i++)
                {
                    cfg_ofstream << "    ";
                }
                continue;
            }

            if (c == '}' || c == ']')
            {
                indent--;
                cfg_ofstream << '\n';
                for (int i = 0; i < indent; i++)
                {
                    cfg_ofstream << "    ";
                }
                cfg_ofstream << c;
                continue;
            }

            if (c == ',')
            {
                cfg_ofstream << c << '\n';
                for (int i = 0; i < indent; i++)
                {
                    cfg_ofstream << "    ";
                }
                continue;
            }

            cfg_ofstream << c;

            if (c == ':')
            {
                cfg_ofstream << ' ';
            }
        }
        cfg_ofstream.close();

        println("Default config created", custom_utils::COLOR::GREEN);

        parsed_config parsed_config;

        parsed_config.secure = DEFAULT_CONFIG.find("secure")->value().as_bool();
        parsed_config.port = DEFAULT_CONFIG.find("port")->value().as_int64();

        for (auto i = DEFAULT_USERS.begin(); i < DEFAULT_USERS.end(); i++)
        {
            boost::json::object user_obj = i->as_object();

            std::string temp_name;
            std::string temp_password;
            for (auto j = user_obj.begin(); j < user_obj.end(); j++)
            {
                if (j->key() == "name")
                {
                    temp_name = j->value().as_string();
                    continue;
                }

                if (j->key() == "password")
                {
                    temp_password = j->value().as_string();
                    continue;
                }
            }
            parsed_config.users[temp_name] = temp_password;
        }

        return parsed_config;
    }

    parsed_config read_json_config()
    {
        if (!fs_handler::directory_exists("config"))
        {
            fs_handler::create_directory("config");
            return make_default_json();
        }

        if (!fs_handler::file_exists("config/config.json"))
        {
            return make_default_json();
        }

        boost::json::stream_parser json_sparser;
        boost::system::error_code ec;

        std::ifstream cfg_ifstream("config/config.json");
        std::string line;

        while (std::getline(cfg_ifstream, line))
        {
            json_sparser.write(line, ec);
            if (ec)
            {
                println("Error during JSON parsing: " + ec.message(), custom_utils::COLOR::RED);
                throw std::runtime_error("Invalid JSON config");
            }
        }
        cfg_ifstream.close();

        json_sparser.finish(ec);
        if (ec)
        {
            println("Error during JSON parsing: " + ec.message(), custom_utils::COLOR::RED);
            throw std::runtime_error("Invalid JSON config");
        }

        boost::json::value config = json_sparser.release();

        // first check if config file is one big JSON object
        if (!config.is_object())
        {
            println("Invalid config", custom_utils::COLOR::RED);
            throw std::runtime_error("Invalid JSON config");
        }

        parsed_config parsed_config;

        // check overall config value types
        boost::json::object config_firstobj = config.get_object();
        for (auto i = config_firstobj.begin(); i < config_firstobj.end(); i++)
        {
            // ignore unrecognized configs
            if (CONFIG_VALUE_TYPE_MAP.find(i->key()) == CONFIG_VALUE_TYPE_MAP.end())
            {
                continue;
            }

            if (i->value().kind() != CONFIG_VALUE_TYPE_MAP.at(i->key()))
            {
                println("Invalid data type: " + std::string(i->key()), custom_utils::COLOR::RED);
                throw std::runtime_error("Invalid JSON config");
            }

            // only use recognized configs
            switch (i->value().kind())
            {
            case boost::json::kind::array: {
                boost::json::array temp_arr = i->value().as_array();

                for (auto user_obj : temp_arr)
                {
                    // skip non-object users
                    if (!user_obj.is_object())
                        continue;

                    boost::json::object config_user_obj = user_obj.get_object();

                    std::string temp_name;
                    std::string temp_password;
                    for (auto j = config_user_obj.begin(); j < config_user_obj.end(); j++)
                    {
                        // only check "name" and "password" keys, other keys are ignored
                        if (j->key() == "name")
                        {
                            temp_name = j->value().as_string();
                            continue;
                        }

                        if (j->key() == "password")
                        {
                            temp_password = j->value().as_string();
                            continue;
                        }
                    }
                    parsed_config.users[temp_name] = temp_password;
                }
                break;
            }
            case boost::json::kind::bool_: {
                if (i->key() == "secure")
                {
                    parsed_config.secure = i->value().as_bool();
                    break;
                }

                break;
            }
            case boost::json::kind::int64:
            case boost::json::kind::uint64: {
                if (i->key() == "port")
                {
                    parsed_config.port = i->value().as_int64();
                    break;
                }
            }
            default:
                println("Something is wrong with the code, this shouldn't happen", custom_utils::COLOR::RED);
                throw std::runtime_error("Program error");
            }
        }

        return parsed_config;
    }
} // namespace config