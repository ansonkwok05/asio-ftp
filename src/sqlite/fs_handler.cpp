#include "fs_handler.h"
#include "../custom_utils.h"

#include <stdexcept>
#include <filesystem>
#include <fstream>

namespace fs_handler
{
    using custom_utils::print;

    bool directory_exists(std::string path_to_directory)
    {
        return std::filesystem::is_directory(path_to_directory);
    }

    bool file_exists(std::string file_name)
    {
        return std::filesystem::exists(file_name);
    }

    void create_directory(std::string path_to_directory)
    {
        std::filesystem::create_directory(path_to_directory);
    }

    std::string get_file_description(std::string file_name)
    {
        if (!file_exists)
        {
            print("File doesn't exists.\n", "red");
            throw std::runtime_error("File path: " + file_name);
        }

        std::ifstream file(file_name, std::ios::binary);

        std::string file_signature = "";
        char byte;

        for (int i = 0; i < 15; i++) // 15 as the maximum length the key of FILE_SIGNATURE_TABLE
        {
            file.read(&byte, 1);
            file_signature += HEX_CHARS[(byte & 0xF0) >> 4];
            file_signature += HEX_CHARS[(byte & 0x0F) >> 0];

            if (FILE_SIGNATURE_DESCRIPTION.find(file_signature) != FILE_SIGNATURE_DESCRIPTION.end())
                return FILE_SIGNATURE_DESCRIPTION.at(file_signature);

            file_signature += " ";
        }

        // print("Unknown file signature -> " + file_signature + "from " + file_name + "\n", "blue");
        return "";
    }

    unsigned long long read_file_size(std::string file_name)
    {
        if (!file_exists)
        {
            print("File doesn't exists.\n", "red");
            throw std::runtime_error("File path: " + file_name);
        }

        return std::filesystem::file_size(std::filesystem::path(file_name));
    }
}