#include "users.hpp"
#include "sqlite_wrapper.hpp"
#include "../helpers.hpp"
#include "../custom_utils.hpp"

#include <string>
#include <vector>
#include <utility>
#include <stdexcept>

namespace users
{
    users::users()
    {
        m_db.connect();
    }

    void users::initialize()
    {
        check_table_exists();
    }

    std::string users::get_id_by_name(const std::string &name)
    {
        std::vector<std::string> result_vector = m_db.run_param_query(GET_ID_BY_NAME_QUERY, {name});

        // return empty string if username not found
        if (result_vector.empty())
        {
            return std::string();
        }

        return result_vector[0];
    }

    std::vector<user> users::get_all_user_credentials()
    {
        std::vector<std::string> db_output = m_db.run_query(GET_ALL_USER_CREDENTIALS_QUERY);

        std::vector<user> all_user_credentials;
        all_user_credentials.reserve(db_output.size() / 2);

        for (size_t i = 0; i < db_output.size(); i += 2)
        {
            user user;
            user.name = std::move(db_output[i]);
            user.password = std::move(db_output[i + 1]);

            all_user_credentials.push_back(user);
        }

        return all_user_credentials;
    }

    bool users::check_password(const std::string &id, const std::string &password)
    {
        std::vector<std::string> result_vector = m_db.run_param_query(GET_PASSWORD_BY_ID_QUERY, {id});

        return !result_vector.empty() && (result_vector[0] == password);
    }

    void users::create_user(const std::string &name, const std::string &password)
    {
        m_db.run_param_query(CREATE_USER_QUERY, {generate_uuid_string(64), name, password});
    }

    void users::check_table_exists()
    {
        std::vector<std::string> db_output = m_db.run_query(GET_TABLE_NAME_QUERY);

        if (db_output.empty())
        {
            custom_utils::println("Missing \"users\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_table();
            return;
        }

        custom_utils::println("Found \"users\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_table_structure();
    }

    void users::create_table()
    {
        m_db.run_query(USER_TABLE_CREATION_QUERY);
        custom_utils::println("Created \"users\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_table_structure();
    }

    void users::check_table_structure()
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
} // namespace users
