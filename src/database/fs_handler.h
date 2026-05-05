#pragma once

#include <string>

namespace fs_handler
{
    bool directory_exists(const std::string &path_to_directory);
    bool file_exists(const std::string &file_name);
    bool create_directory(const std::string &path_to_directory);
    bool rename_file(const std::string &path_to_file, const std::string &new_path_to_file);
    bool remove_file(const std::string &path_to_file);
} // namespace fs_handler