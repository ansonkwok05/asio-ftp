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

    fs_object fs_objects::get_object(const std::string &user_id, const std::string &object_name,
                                     const std::string &object_path)
    {
        std::vector<std::string> db_output =
            m_db.run_param_query(GET_OBJECT_QUERY, {user_id, object_name, object_path});

        fs_object object;

        if (db_output.empty())
        {
            return object;
        }

        object.id = std::move(db_output[0]);
        object.name = std::move(db_output[1]);
        object.path = std::move(db_output[2]);
        object.size = std::move(db_output[3]);
        object.modified_time = std::move(db_output[4]);
        object.is_directory = db_output[5] == "1" ? true : false;

        return object;
    }

    std::vector<fs_object> fs_objects::get_all_objects(const std::string &user_id)
    {
        std::vector<std::string> db_output = m_db.run_param_query(GET_ALL_OBJECTS_QUERY, {user_id});

        std::vector<fs_object> objects;

        if (db_output.empty())
        {
            return objects;
        }

        objects.reserve(db_output.size() / 6);

        for (size_t i = 0; i < db_output.size(); i += 6)
        {
            fs_object object;
            object.id = std::move(db_output[i]);
            object.name = std::move(db_output[i + 1]);
            object.path = std::move(db_output[i + 2]);
            object.size = std::move(db_output[i + 3]);
            object.modified_time = std::move(db_output[i + 4]);
            object.is_directory = db_output[i + 5] == "1" ? true : false;

            objects.push_back(object);
        }

        return objects;
    }

    std::string fs_objects::create_object(const std::string &user_id, const std::string &object_name,
                                          const std::string &object_path, long long object_size, bool is_directory)
    {
        // check if object path is a directory that exists
        // not checking root because it always exists
        if (object_path != "/")
        {
            fs_object object = get_object(user_id, get_basename(object_path), get_parent_path(object_path));

            // does not exists
            if (object.id.empty())
            {
                return std::string();
            }

            // not a directory
            if (!object.is_directory)
            {
                return std::string();
            }
        }

        // check if object already exists to prevent recreating same object
        if (!get_object(user_id, object_name, object_path).id.empty())
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
        fs_object object = get_object(user_id, object_name, object_path);

        // object not found, not updating anything
        if (object.id.empty())
        {
            return std::string();
        }

        m_db.run_param_query(UPDATE_OBJECT_SIZE_QUERY, {std::to_string(object_size), object.id});

        return object.id;
    }

    bool fs_objects::remove_fs_object(const std::string &user_id, const std::string &object_name,
                                      const std::string &object_path)
    {
        fs_object object = get_object(user_id, object_name, object_path);

        if (object.id.empty())
        {
            return false;
        }

        // if object is a directory, delete all objects inside it
        if (object.is_directory)
        {
            std::string path_prefix = object_path;
            if (object_path != "/")
            {
                path_prefix += "/";
            }
            path_prefix += object_name + "%";

            std::vector<std::string> object_ids_for_removal =
                m_db.run_param_query(GET_ALL_OBJECTS_WITH_PREFIX_QUERY, {user_id, path_prefix});

            for (const std::string &remove_id : object_ids_for_removal)
            {
                fs_handler::remove_file("data/" + remove_id);
            }

            m_db.run_param_query(DELETE_ALL_OBJECTS_WITH_PREFIX_QUERY, {user_id, path_prefix});
        }

        // delete the object
        fs_handler::remove_file("data/" + object.id);
        m_db.run_param_query(DELETE_OBJECT_QUERY, {object.id});

        return true;
    }

    void fs_objects::check_table_exists()
    {
        std::vector<std::string> db_output = m_db.run_query(GET_TABLE_NAME_QUERY);

        if (db_output.empty())
        {
            custom_utils::println("Missing \"fs_objects\" table, creating a new one", custom_utils::COLOR::YELLOW);
            create_fs_objects_table();
            return;
        }

        custom_utils::println("Found \"fs_objects\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_fs_objects_table_structure();
    }

    void fs_objects::create_fs_objects_table()
    {
        m_db.run_query(FS_OBJECTS_TABLE_CREATION_QUERY);
        custom_utils::println("Created \"fs_objects\" table, checking table structure", custom_utils::COLOR::GREEN);
        check_fs_objects_table_structure();
    }

    void fs_objects::check_fs_objects_table_structure()
    {
        std::vector<std::string> db_output = m_db.run_query(CHECK_FS_OBJECTS_TABLE_STRUCTURE_QUERY);

        if (db_output.size() != 1)
        {
            throw std::runtime_error(
                "Unexpected result from SQLite, table structure check did not return result size of 1");
        }

        if (db_output[0] != FS_OBJECTS_TABLE_CREATION_QUERY)
        {
            throw std::runtime_error("Detected a different \"fs_objects\" table structure");
        }

        custom_utils::println("Verified \"fs_objects\" table structure", custom_utils::COLOR::GREEN);
    }

} // namespace fs_objects