#include "fs_handler.h"

#include <filesystem>

namespace fs_handler
{
    bool directory_exists(std::string path_to_directory)
    {
        return std::filesystem::is_directory(path_to_directory);
    }

    bool file_exists(std::string file_name)
    {
        return std::filesystem::exists(file_name);
    }

    bool create_directory(std::string path_to_directory)
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

    bool remove_file(std::string path_to_file)
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