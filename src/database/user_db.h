#pragma once

#include "sqlite_wrapper.h"

#include <string>
#include <vector>

namespace user_db
{
    constexpr char USER_TABLE_CREATION_QUERY[] = "CREATE TABLE users (user_id CHAR(64) PRIMARY KEY NOT NULL, name "
                                                 "VARCHAR(30) UNIQUE NOT NULL, password VARCHAR(255) NOT NULL)";
    class user
    {
    public:
        user();

        void initialize();

        std::string get_id_by_name(const std::string &name);
        std::vector<std::string> get_username_list();

        bool check_password(const std::string &id, const std::string &password);
        void create_user(const std::string &name, const std::string &password);

    private:
        sqlite_wrapper::SQLiteDb db;

        void check_table_exists();
        void create_table();
        void check_table_structure();
    };
} // namespace user_db