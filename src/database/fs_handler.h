#pragma once

#include <string>

namespace fs_handler
{
    constexpr char HEX_CHARS[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };

    // one-liner filesystem wrapper functions
    bool directory_exists(std::string path_to_directory);
    bool file_exists(std::string file_name);
    bool create_directory(std::string path_to_directory);
    bool remove_file(std::string path_to_file);
} // namespace fs_handler