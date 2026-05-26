#include "fs_objects.h"
#include "sqlite_wrapper.h"
#include "fs_handler.h"
#include "../custom_utils.h"
#include "../helpers.h"

#include <stdexcept>
#include <vector>
#include <string>

namespace fs_objects
{
    fs_objects::fs_objects()
    {
        m_db.connect();
    }

    void fs_objects::initialize()
    {
        check_table_exists();
    }

    // todo make struct for object with all its properties
    // return struct instead of vector string

    std::vector<std::string> fs_objects::get_object(const std::string &user_id, const std::string &object_name,
                                                    const std::string &object_path)
    {
        return m_db.run_param_query(GET_OBJECT_QUERY, {user_id, object_name, object_path});
    }

    std::vector<std::string> fs_objects::get_all_objects(const std::string &user_id)
    {
        return m_db.run_param_query(GET_ALL_OBJECTS_QUERY, {user_id});
    }

    std::string fs_objects::create_object(const std::string &user_id, const std::string &object_name,
                                          const std::string &object_path, long long object_size, bool is_directory)
    {
        // check if object path is a directory that exists
        // not checking root because it always exists
        if (object_path != "/")
        {
            std::vector<std::string> object =
                get_object(user_id, get_basename(object_path), get_parent_path(object_path));

            // return empty string if not found
            if (object.empty())
            {
                return std::string();
            }

            // found, but not a directory
            if (object[5] == "0")
            {
                return std::string();
            }
        }

        // check if object already exists to prevent recreating same object
        if (!get_object(user_id, object_name, object_path).empty())
        {
            return std::string();
        }

        std::string object_id = generate_uuid_string(64);

        m_db.run_param_query(CREATE_OBJECT_QUERY, {object_id, object_name, object_path, std::to_string(object_size),
                                                   std::to_string(is_directory), user_id});

        return object_id;
    }

    std::string fs_objects::update_object_size(const std::string &user_id, const std::string &object_name,
                                               const std::string &object_path, long long object_size)
    {
        std::vector<std::string> object = get_object(user_id, object_name, object_path);

        // object not found, not updating anything
        if (object.empty())
        {
            return std::string();
        }

        std::string object_id = object[0];

        m_db.run_param_query(UPDATE_OBJECT_SIZE_QUERY, {std::to_string(object_size), object_id});

        return object_id;
    }

    bool fs_objects::remove_fs_object(const std::string &user_id, const std::string &object_name,
                                      const std::string &object_path)
    {
        std::vector<std::string> object = get_object(user_id, object_name, object_path);

        if (object.size() == 0)
        {
            return false;
        }

        std::string object_id = object[0];

        // if object is a directory, recursively delete all objects inside
        if (object[5] == "1")
        {
            std::string match;
            if (object_path == "/")
            {
                match = object_path + object_name + "%";
            }
            else
            {
                match = object_path + "/" + object_name + "%";
            }

            std::vector<std::string> obj_idlist = m_db.run_param_query(
                "SELECT object_id FROM fs_objects WHERE user_id = ? AND path LIKE ?", {user_id, match});

            for (std::string obj_id : obj_idlist)
            {
                fs_handler::remove_file("data/" + obj_id);
            }

            m_db.run_param_query("DELETE FROM fs_objects WHERE user_id = ? AND path LIKE ?", {user_id, match});
        }

        fs_handler::remove_file("data/" + object_id);
        m_db.run_param_query("DELETE FROM fs_objects WHERE object_id = ?", {object_id});

        return true;
    }

    void fs_objects::check_table_exists()
    {
        std::vector<std::string> result = m_db.run_query("SELECT name FROM sqlite_master WHERE type='table'");

        bool fs_objects_table_exists = false;

        for (auto table_name : result)
        {
            if (table_name == "fs_objects")
            {
                fs_objects_table_exists = true;
                break;
            }
        }

        if (!fs_objects_table_exists)
        {
            custom_utils::println("Missing \"fs_objects\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_fs_objects_table();
        }
        else
        {
            custom_utils::println("Found \"fs_objects\" table, checking table structure", custom_utils::COLOR::GREEN);
            check_fs_objects_table_structure();
        }
    }

    void fs_objects::create_fs_objects_table()
    {
        std::vector<std::string> result = m_db.run_query(VIRTUAL_OBJECTS_TABLE_CREATION_QUERY);

        custom_utils::println("Created \"fs_objects\" table, checking table structure", custom_utils::COLOR::GREEN);

        check_fs_objects_table_structure();
    }

    void fs_objects::check_fs_objects_table_structure()
    {
        std::vector<std::string> result = m_db.run_query("SELECT sql FROM sqlite_master WHERE name = 'fs_objects'");

        if (result.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (result[0] != VIRTUAL_OBJECTS_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"fs_objects\" table structure");
        }

        custom_utils::println("Verified \"fs_objects\" table structure", custom_utils::COLOR::GREEN);
    }

} // namespace fs_objects