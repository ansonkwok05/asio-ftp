#pragma once

#include "sqlite_wrapper.h"

namespace virtual_fs_db
{
    constexpr char VIRTUAL_OBJECTS_TABLE_CREATION_QUERY[] =
        "CREATE TABLE virtual_objects (object_id CHAR(64) PRIMARY KEY, name VARCHAR(255) NOT NULL, path VARCHAR(1024) "
        "NOT NULL, size BIGINT NOT NULL, modified_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, is_directory "
        "INTEGER NOT NULL, user_id CHAR(64) NOT NULL, FOREIGN KEY (user_id) REFERENCES users (user_id), UNIQUE (name, "
        "path) ON CONFLICT IGNORE)";

    class virtual_fs
    {
    public:
        virtual_fs();

        void initialize();

        std::vector<std::string> get_object(const std::string &user_id, const std::string &object_name,
                                            const std::string &object_path);
        std::vector<std::string> get_object_list(const std::string &user_id);
        std::string create_virtual_object(const std::string &user_id, const std::string &object_name,
                                          const std::string &object_path, long long object_size, bool is_directory);
        std::string update_virtual_object(const std::string &user_id, const std::string &object_name,
                                          const std::string &object_path, long long object_size);
        bool remove_virtual_object(const std::string &user_id, const std::string &object_name,
                                   const std::string &object_path);

    private:
        sqlite_wrapper::SQLiteDb db;

        void check_table_exists();

        void create_virtual_objects_table();

        void check_virtual_objects_table_structure();
    };
} // namespace virtual_fs_db