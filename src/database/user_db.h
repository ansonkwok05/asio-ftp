#pragma once

#include "sqlite_wrapper.h"

#include <string>

namespace user_db
{
    constexpr char USER_TABLE_CREATION_QUERY[] = "CREATE TABLE users (user_id CHAR(64) PRIMARY KEY NOT NULL, name "
                                                 "VARCHAR(30) UNIQUE NOT NULL, password VARCHAR(255) NOT NULL)";
    class user
    {
    public:
        user();

        void initialize();

        std::string get_id_by_name(std::string name);
        bool check_password(std::string id, std::string password);
        void create_user(std::string name, std::string password);

    private:
        sqlite_wrapper::SQLiteDb db;

        void check_table_exists();
        void create_table();
        void check_table_structure();
    };
} // namespace user_db