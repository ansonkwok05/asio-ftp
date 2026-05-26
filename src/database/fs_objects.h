#pragma once

#include "sqlite_wrapper.h"

namespace fs_objects
{
    constexpr char GET_OBJECT_QUERY[] = "SELECT object_id, name, path, size, modified_time, is_directory FROM "
                                        "fs_objects WHERE user_id = ? AND name = ? AND path = ?";

    constexpr char GET_ALL_OBJECTS_QUERY[] =
        "SELECT object_id, name, path, size, modified_time, is_directory FROM fs_objects WHERE user_id = ?";

    constexpr char CREATE_OBJECT_QUERY[] =
        "INSERT INTO fs_objects (object_id, name, path, size, is_directory, user_id) VALUES (?, ?, ?, ?, ?, ?)";

    constexpr char UPDATE_OBJECT_SIZE_QUERY[] =
        "UPDATE fs_objects SET size = ?, modified_time = CURRENT_TIMESTAMP WHERE object_id = ?";

    constexpr char GET_ALL_OBJECTS_WITH_PREFIX_QUERY[] =
        "SELECT object_id FROM fs_objects WHERE user_id = ? AND path LIKE ?";

    constexpr char DELETE_ALL_OBJECTS_WITH_PREFIX_QUERY[] = "DELETE FROM fs_objects WHERE user_id = ? AND path LIKE ?";

    constexpr char DELETE_OBJECT_QUERY[] = "DELETE FROM fs_objects WHERE object_id = ?";

    constexpr char GET_TABLE_NAME_QUERY[] = "SELECT name FROM sqlite_master WHERE type='table' AND name='fs_objects'";

    constexpr char FS_OBJECTS_TABLE_CREATION_QUERY[] =
        "CREATE TABLE fs_objects (object_id CHAR(64) PRIMARY KEY, name VARCHAR(255) NOT NULL, path VARCHAR(1024) "
        "NOT NULL, size BIGINT NOT NULL, modified_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP, is_directory "
        "INTEGER NOT NULL, user_id CHAR(64) NOT NULL, FOREIGN KEY (user_id) REFERENCES users (user_id), UNIQUE (name, "
        "path) ON CONFLICT IGNORE)";

    constexpr char CHECK_FS_OBJECTS_TABLE_STRUCTURE_QUERY[] = "SELECT sql FROM sqlite_master WHERE name = 'fs_objects'";

    struct fs_object
    {
        std::string id;
        std::string name;
        std::string path;
        std::string size;
        std::string modified_time;
        bool is_directory;
    };

    class fs_objects
    {
    public:
        fs_objects();

        void initialize();

        fs_object get_object(const std::string &user_id, const std::string &object_name,
                             const std::string &object_path);
        std::vector<fs_object> get_all_objects(const std::string &user_id);
        std::string create_object(const std::string &user_id, const std::string &object_name,
                                  const std::string &object_path, long long object_size, bool is_directory);
        std::string update_object_size(const std::string &user_id, const std::string &object_name,
                                       const std::string &object_path, long long object_size);
        bool remove_fs_object(const std::string &user_id, const std::string &object_name,
                              const std::string &object_path);

    private:
        sqlite_wrapper::SQLiteDb m_db;

        void check_table_exists();

        void create_fs_objects_table();

        void check_fs_objects_table_structure();
    };
} // namespace fs_objects