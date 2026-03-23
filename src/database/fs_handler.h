#pragma once

#include <string>

namespace fs_handler
{
    bool directory_exists(std::string path_to_directory);
    bool file_exists(std::string file_name);
    bool create_directory(std::string path_to_directory);
    bool rename_file(std::string path_to_file, std::string new_path_to_file);
    bool remove_file(std::string path_to_file);
} // namespace fs_handler