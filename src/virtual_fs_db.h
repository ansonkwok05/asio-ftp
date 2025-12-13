#pragma once

#include "sqlite/sqlite_wrapper.h"

namespace virtual_fs_db
{
    constexpr char OBJECT_TABLE_CREATION_QUERY[] =
        "CREATE TABLE objects (object_id CHAR(36) PRIMARY KEY NOT NULL, user_id "
        "CHAR(64) NOT NULL, FOREIGN KEY (user_id) REFERENCES users (user_id))";

    constexpr char OBJECT_METADATA_TABLE_CREATION_QUERY[] =
        "CREATE TABLE objects_metadata (name VARCHAR(255) NOT NULL, path VARCHAR(1024) NOT NULL, "
        "size BIGINT NOT NULL, modified_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, is_directory INTEGER "
        "NOT NULL, object_id CHAR(64) NOT NULL, FOREIGN KEY (object_id) REFERENCES files (object_id))";

    class virtual_fs
    {
    public:
        virtual_fs();
        void initialize();

        std::vector<std::string> get_object(std::string user_id, std::string object_name, std::string object_path);
        std::vector<std::string> get_object_list(std::string user_id);
        std::string create_virtual_object(std::string user_id, std::string object_name, std::string object_path,
                                          long long object_size, bool is_directory);
        std::string update_virtual_object(std::string user_id, std::string object_name, std::string object_path,
                                          long long object_size);
        std::string remove_virtual_object(std::string user_id, std::string object_name, std::string object_path);

    private:
        void check_table_exists();

        void create_objects_table();
        void create_objects_metadata_table();

        void check_objects_table_structure();
        void check_objects_metadata_table_structure();
        sqlite_wrapper::SQLiteDb db;
    };
} // namespace virtual_fs_db