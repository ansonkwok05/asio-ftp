#include "virtual_fs_db.h"
#include "sqlite_wrapper.h"

#include "../custom_utils.h"

#include <stdexcept>
#include <vector>
#include <string>

namespace virtual_fs_db
{
    virtual_fs::virtual_fs()
    {
        db.connect();
    }

    void virtual_fs::initialize()
    {
        check_table_exists();
    }

    std::vector<std::string> virtual_fs::get_object(std::string user_id, std::string object_name,
                                                    std::string object_path)
    {
        return db.run_param_query(
            "SELECT object_id, name, path, size, modified_time, is_directory FROM virtual_objects WHERE "
            "user_id = ? AND name = ? AND path = ?",
            {user_id, object_name, object_path});
    }

    std::vector<std::string> virtual_fs::get_object_list(std::string user_id)
    {
        return db.run_param_query(
            "SELECT object_id, name, path, size, modified_time, is_directory FROM virtual_objects WHERE user_id = ?",
            {user_id});
    }

    std::string virtual_fs::create_virtual_object(std::string user_id, std::string object_name, std::string object_path,
                                                  long long object_size, bool is_directory)
    {
        std::string object_id = custom_utils::generate_uuid_string(64);

        db.run_param_query(
            "INSERT INTO virtual_objects (object_id, name, path, size, is_directory, user_id) VALUES (?, ?, ?, ?, ?, "
            "?)",
            {object_id, object_name, object_path, std::to_string(object_size), std::to_string(is_directory), user_id});

        return object_id;
    }

    std::string virtual_fs::update_virtual_object(std::string user_id, std::string object_name, std::string object_path,
                                                  long long object_size)
    {
        std::vector<std::string> v_obj = get_object(user_id, object_name, object_path);

        if (v_obj.size() == 0)
        {
            return "";
        }

        std::string object_id = v_obj[0];

        db.run_param_query("UPDATE virtual_objects SET size = ?, modified_time = CURRENT_TIMESTAMP WHERE object_id = ?",
                           {std::to_string(object_size), object_id});

        return object_id;
    }

    std::string virtual_fs::remove_virtual_object(std::string user_id, std::string object_name, std::string object_path)
    {
        std::vector<std::string> v_obj = get_object(user_id, object_name, object_path);

        if (v_obj.size() == 0)
        {
            return "";
        }

        std::string object_id = v_obj[0];

        std::vector<std::string> deleted =
            db.run_param_query("DELETE FROM virtual_objects WHERE object_id = ?", {object_id});

        for (auto col : deleted)
        {
            custom_utils::println(col);
        }

        return object_id;
    }

    void virtual_fs::check_table_exists()
    {
        std::vector<std::string> result = db.run_query("SELECT name FROM sqlite_master WHERE type='table'");

        bool virtual_objects_table_exists = false;

        for (auto table_name : result)
        {
            if (table_name == "virtual_objects")
            {
                virtual_objects_table_exists = true;
                break;
            }
        }

        if (!virtual_objects_table_exists)
        {
            custom_utils::println("Missing \"virtual_objects\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_virtual_objects_table();
        }
        else
        {
            custom_utils::println("Found \"virtual_objects\" table, checking table structure",
                                  custom_utils::COLOR::GREEN);
            check_virtual_objects_table_structure();
        }
    }

    void virtual_fs::create_virtual_objects_table()
    {
        std::vector<std::string> result = db.run_query(VIRTUAL_OBJECTS_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"virtual_objects\" table, checking table structure",
                              custom_utils::COLOR::GREEN);

        check_virtual_objects_table_structure();
    }

    void virtual_fs::check_virtual_objects_table_structure()
    {
        std::vector<std::string> result = db.run_query("SELECT sql FROM sqlite_master WHERE name = 'virtual_objects'");

        if (result.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (result[0] != VIRTUAL_OBJECTS_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"virtual_objects\" table structure");
        }

        custom_utils::println("Verified \"virtual_objects\" table structure", custom_utils::COLOR::GREEN);
    }

} // namespace virtual_fs_db