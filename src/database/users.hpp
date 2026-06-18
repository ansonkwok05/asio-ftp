#pragma once

#include "sqlite_wrapper.hpp"

#include <string>
#include <vector>

namespace users
{
    constexpr char GET_ID_BY_NAME_QUERY[] = "SELECT user_id FROM users WHERE name = ?";

    constexpr char GET_ALL_USER_CREDENTIALS_QUERY[] = "SELECT name, password FROM users";

    constexpr char GET_PASSWORD_BY_ID_QUERY[] = "SELECT password FROM users WHERE user_id = ?";

    constexpr char CREATE_USER_QUERY[] = "INSERT INTO users (user_id, name, password) VALUES (?, ?, ?)";

    constexpr char GET_TABLE_NAME_QUERY[] = "SELECT name FROM sqlite_master WHERE type='table' AND name='users'";

    constexpr char USER_TABLE_CREATION_QUERY[] = "CREATE TABLE users (user_id CHAR(64) PRIMARY KEY NOT NULL, name "
                                                 "VARCHAR(30) UNIQUE NOT NULL, password VARCHAR(255) NOT NULL)";

    constexpr char CHECK_USER_TABLE_STRUCTURE_QUERY[] = "SELECT sql FROM sqlite_master WHERE name = 'users'";

    struct user
    {
        std::string id;
        std::string name;
        std::string password;
    };

    class users
    {
    public:
        users();

        void initialize();

        std::string get_id_by_name(const std::string &name);
        std::vector<user> get_all_user_credentials();

        bool check_password(const std::string &id, const std::string &password);
        void create_user(const std::string &name, const std::string &password);

    private:
        sqlite_wrapper::SQLiteDb m_db;

        void check_table_exists();
        void create_table();
        void check_table_structure();
    };
} // namespace users
