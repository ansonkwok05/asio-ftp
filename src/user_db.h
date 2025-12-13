#pragma once

#include "sqlite/sqlite_wrapper.h"

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
        std::vector<std::string> get_id_by_name(std::string name);
        std::vector<std::string> get_name_by_id(std::string id);
        std::vector<std::string> get_password_by_id(std::string id);
        void create_user(std::string name, std::string password);

    private:
        sqlite_wrapper::SQLiteDb db;

        void check_table_exists();
        void create_table();
        void check_table_structure();
    };
} // namespace user_db