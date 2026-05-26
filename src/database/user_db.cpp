#include "user_db.h"
#include "sqlite_wrapper.h"
#include "../helpers.h"
#include "../custom_utils.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace user_db
{
    user::user()
    {
        m_db.connect();
    }

    void user::initialize()
    {
        check_table_exists();
    }

    std::string user::get_id_by_name(const std::string &name)
    {
        std::vector<std::string> result_vector = m_db.run_param_query(GET_ID_BY_NAME_QUERY, {name});

        // return empty string if username not found
        if (result_vector.empty())
            return std::string();

        return result_vector[0];
    }

    std::vector<std::string> user::get_all_user_credentials()
    {
        return m_db.run_query(GET_ALL_USER_CREDENTIALS_QUERY);
    }

    bool user::check_password(const std::string &id, const std::string &password)
    {
        std::vector<std::string> result_vector = m_db.run_param_query(GET_PASSWORD_BY_ID_QUERY, {id});

        return !result_vector.empty() && (result_vector[0] == password);
    }

    void user::create_user(const std::string &name, const std::string &password)
    {
        m_db.run_param_query(CREATE_USER_QUERY, {generate_uuid_string(64), name, password});
    }

    void user::check_table_exists()
    {
        std::vector<std::string> result = m_db.run_query(GET_ALL_TABLE_NAMES_QUERY);

        bool exists = false;
        for (const std::string &table_name : result)
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
        std::vector<std::string> result = m_db.run_query(USER_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"users\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_table_structure();
    }

    void user::check_table_structure()
    {
        std::vector<std::string> result = m_db.run_query(CHECK_USER_TABLE_STRUCTURE_QUERY);

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