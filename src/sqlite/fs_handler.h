#pragma once

#include <string>
#include <map>

namespace fs_handler
{
    const char HEX_CHARS[16] = {
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
    };

    /**
     * These are from wikipedia. This table suggests the most likely extension of the corresponding file signatures.
     */
    const std::map<std::string, std::string> FILE_SIGNATURE_DESCRIPTION = {
        {"4D 5A", "DOS MZ executable"},
        {"53 51 4C 69 74 65 20 66 6F 72 6D 61 74 20 33 00", "SQLite Database"},
        {"7F 45 4C 46", "Executable and Linkable Format"},
        {"FF D8 FF DB", "JPEG image"},
        {"FF D8 FF E0 00 10 4A 46 49 46 00 01", "JPEG image"},
        {"FF D8 FF EE", "JPEG image"},
        {"FF D8 FF E0", "JPEG image"},
        {"89 50 4E 47 0D 0A 1A 0A", "PNG image"},
        {"47 49 46 38 37 61", "GIF image"},
        {"47 49 46 38 39 61", "GIF image"},
        {"FF FB", "MP3 audio file"},
        {"FF F3", "MP3 audio file"},
        {"FF F2", "MP3 audio file"},
        {"49 44 33", "MP3 audio file"},
        {"66 74 79 70 69 73 6F 6D", "MP4 media file"},
        {"66 74 79 70 4D 53 4E 56", "MP4 media file"},
        {"7B 5C 72 74 66 31", "Rich Text Format"},
        {"37 7A BC AF 27 1C", "7-Zip archive"},
        {"52 61 72 21 1A 07 00", "RAR archive"},
        {"52 61 72 21 1A 07 01 00", "RAR archive"},
    };

    // one-liner filesystem wrapper functions
    bool directory_exists(std::string path_to_directory);
    bool file_exists(std::string file_name);
    void create_directory(std::string path_to_directory);

    /**
     * Returns the suggested file extension from its file signature
     * @param file_name File name / Path to file
     */
    std::string get_file_description(std::string file_name);

    /**
     * @param file_name File name / Path to file
     */
    unsigned long long read_file_size(std::string file_name);
} // namespace fs_handler