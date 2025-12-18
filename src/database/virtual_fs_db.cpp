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
            "SELECT objects_metadata.* FROM objects, objects_metadata WHERE objects.user_id = ? AND "
            "objects.object_id = objects_metadata.object_id AND objects_metadata.name = ? AND objects_metadata.path = "
            "?",
            {user_id, object_name, object_path});
    }

    std::vector<std::string> virtual_fs::get_object_list(std::string user_id)
    {
        return db.run_param_query(
            "SELECT objects_metadata.* FROM objects, objects_metadata WHERE objects.user_id = ? AND "
            "objects.object_id = objects_metadata.object_id",
            {user_id});
    }

    std::string virtual_fs::create_virtual_object(std::string user_id, std::string object_name, std::string object_path,
                                                  long long object_size, bool is_directory)
    {
        std::string object_id = custom_utils::generate_uuid_string(64);
        db.run_param_query("INSERT INTO objects (object_id, user_id) VALUES (?, ?)", {object_id, user_id});
        db.run_param_query(
            "INSERT INTO objects_metadata (name, path, size, is_directory, object_id) VALUES (?, ?, ?, ?, ?)",
            {object_name, object_path, std::to_string(object_size), std::to_string(is_directory), object_id});

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

        std::string object_id = v_obj[5];

        db.run_param_query(
            "UPDATE objects_metadata SET size = ?, modified_time = CURRENT_TIMESTAMP WHERE object_id = ?",
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

        std::string object_id = v_obj[5];
        db.run_param_query("DELETE FROM objects WHERE object_id = ?", {object_id});
        db.run_param_query("DELETE FROM objects_metadata WHERE object_id = ?", {object_id});

        return object_id;
    }

    void virtual_fs::check_table_exists()
    {
        std::vector<std::string> result = db.run_query("SELECT name FROM sqlite_master WHERE type='table'");

        bool objects_exists = false;
        bool objects_metadata_exists = false;
        for (auto table_name : result)
        {
            if (table_name == "objects")
            {
                objects_exists = true;
                if (objects_exists && objects_metadata_exists)
                {
                    break;
                }
            }
            if (table_name == "objects_metadata")
            {
                objects_metadata_exists = true;
                if (objects_exists && objects_metadata_exists)
                {
                    break;
                }
            }
        }

        if (!objects_exists)
        {
            custom_utils::println("Missing \"objects\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_objects_table();
        }
        else
        {
            custom_utils::println("Found \"objects\" table, checking table structure", custom_utils::COLOR::GREEN);
            check_objects_table_structure();
        }

        if (!objects_metadata_exists)
        {
            custom_utils::println("Missing \"objects_metadata\" table, creating a new one",
                                  custom_utils::COLOR::YELLOW);
            create_objects_metadata_table();
        }
        else
        {
            custom_utils::println("Found \"objects_metadata\" table, checking table structure",
                                  custom_utils::COLOR::GREEN);
            check_objects_metadata_table_structure();
        }
    }

    void virtual_fs::create_objects_table()
    {
        std::vector<std::string> result = db.run_query(OBJECT_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"objects\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_objects_table_structure();
    }

    void virtual_fs::create_objects_metadata_table()
    {
        std::vector<std::string> result = db.run_query(OBJECT_METADATA_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"objects_metadata\" table, checking table structure",
                              custom_utils::COLOR::GREEN);
        check_objects_metadata_table_structure();
    }

    void virtual_fs::check_objects_table_structure()
    {
        std::vector<std::string> result = db.run_query("SELECT sql FROM sqlite_master WHERE name = 'objects'");

        if (result.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (result[0] != OBJECT_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"objects\" table structure");
        }

        custom_utils::println("Verified \"objects\" table structure", custom_utils::COLOR::GREEN);
    }

    void virtual_fs::check_objects_metadata_table_structure()
    {
        std::vector<std::string> result = db.run_query("SELECT sql FROM sqlite_master WHERE name = 'objects_metadata'");

        if (result.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (result[0] != OBJECT_METADATA_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"objects_metadata\" table structure");
        }

        custom_utils::println("Verified \"objects_metadata\" table structure", custom_utils::COLOR::GREEN);
    }

} // namespace virtual_fs_db