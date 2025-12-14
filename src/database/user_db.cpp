#include "user_db.h"
#include "sqlite_wrapper.h"

#include "../custom_utils.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace user_db
{
    user::user()
    {
        db.connect();
    }

    void user::initialize()
    {
        check_table_exists();
    }

    std::vector<std::string> user::get_id_by_name(std::string name)
    {
        return db.run_param_query("SELECT user_id FROM users WHERE name = ?", {name});
    }

    std::vector<std::string> user::get_name_by_id(std::string id)
    {
        return db.run_param_query("SELECT name FROM users WHERE user_id = ?", {id});
    }

    std::vector<std::string> user::get_password_by_id(std::string id)
    {
        return db.run_param_query("SELECT password FROM users WHERE user_id = ?", {id});
    }

    void user::create_user(std::string name, std::string password)
    {
        db.run_param_query("INSERT INTO users (user_id, name, password) VALUES (?, ?, ?)",
                           {custom_utils::generate_uuid_string(64), name, password});
    }

    void user::check_table_exists()
    {
        std::vector<std::string> result = db.run_query("SELECT name FROM sqlite_master WHERE type='table'");

        bool exists = false;
        for (auto table_name : result)
        {
            if (table_name == "users")
            {
                exists = true;
                break;
            }
        }

        if (!exists)
        {
            custom_utils::println("Missing \"users\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_table();
            return;
        }

        custom_utils::println("Found \"users\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_table_structure();
    }

    void user::create_table()
    {
        std::vector<std::string> result = db.run_query(USER_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"users\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_table_structure();
    }

    void user::check_table_structure()
    {
        std::vector<std::string> result = db.run_query("SELECT sql FROM sqlite_master WHERE name = 'users'");

        if (result.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (result[0] != USER_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"users\" table structure");
        }

        custom_utils::println("Verified \"users\" table structure", custom_utils::COLOR::GREEN);
    }
} // namespace user_db