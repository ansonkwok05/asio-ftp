#include "fs_handler.h"

#include <filesystem>

namespace fs_handler
{
    bool directory_exists(const std::string &path_to_directory)
    {
        return std::filesystem::is_directory(path_to_directory);
    }

    bool file_exists(const std::string &file_name)
    {
        return std::filesystem::exists(file_name);
    }

    bool create_directory(const std::string &path_to_directory)
    {
        try
        {
            if (std::filesystem::create_directory(path_to_directory))
                return true;
            else
                return false;
        }
        catch (const std::filesystem::filesystem_error &err)
        {
            return false;
        }

        return true;
    }

    bool rename_file(const std::string &path_to_file, const std::string &new_path_to_file)
    {
        try
        {
            std::filesystem::rename(path_to_file, new_path_to_file);
            return true;
        }
        catch (const std::filesystem::filesystem_error &err)
        {
            return false;
        }
    }

    bool remove_file(const std::string &path_to_file)
    {
        try
        {
            if (std::filesystem::remove(path_to_file))
                return true;
            else
                return false;
        }
        catch (const std::filesystem::filesystem_error &err)
        {
            return false;
        }
    }
} // namespace fs_handler