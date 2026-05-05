#include "helpers.h"

#include "../custom_utils.h"

#include <string>
#include <vector>

std::pair<std::string, std::string> parse_buffer(const std::vector<uint8_t> &buffer, size_t bytes_received)
{
    std::string parsed_string;

    for (size_t i = 0; i < bytes_received - 2; i++)
    {
        parsed_string += buffer.at(i);
    }

    std::vector<std::string> split_string = custom_utils::splitString(parsed_string, ' ');

    std::string FTP_command = split_string[0];
    std::string FTP_argument = "";

    if (split_string.size() > 1)
    {
        // received message has argument(s)
        FTP_argument =
            custom_utils::vectorStrJoin(std::vector<std::string>(split_string.begin() + 1, split_string.end()), " ");
    }

    return {FTP_command, FTP_argument};
}

std::string return_parent_directory(const std::string &directory)
{
    // return root when already at root directory
    if (directory == "/")
        return "/";

    std::vector<std::string> split_directory = custom_utils::splitString(directory, '/');

    // input only have one child
    // such as "/test" or "/one"
    if (split_directory.size() <= 2)
    {
        return "/";
    }

    // combine split string except last, adding '/' in between
    std::string parent = "";
    for (int i = 0; i < split_directory.size() - 1; i++)
    {
        // ignore empty strings, they were "/" before splitting
        if (split_directory.at(i) == "")
            continue;

        parent += "/" + split_directory.at(i);
    }

    return parent;
}

namespace
{
    std::string parse_metadata_time(const std::string &time_str)
    {
        // format: 2025-08-28 05:12:43 -> Aug 28 05:12

        std::string parsedStr = "";

        switch (time_str.at(5))
        {
        // < 10
        case '0': {
            switch (time_str.at(6))
            {
            // 01
            case '1': {
                parsedStr += "Jan ";
                break;
            }
            // 02
            case '2': {
                parsedStr += "Feb ";
                break;
            }
            // 03
            case '3': {
                parsedStr += "Mar ";
                break;
            }
            // 04
            case '4': {
                parsedStr += "Apr ";
                break;
            }
            // 05
            case '5': {
                parsedStr += "May ";
                break;
            }
            // 06
            case '6': {
                parsedStr += "Jun ";
                break;
            }
            // 07
            case '7': {
                parsedStr += "July ";
                break;
            }
            // 08
            case '8': {
                parsedStr += "Aug ";
                break;
            }
            // 09
            case '9': {
                parsedStr += "Sep ";
                break;
            }
            }
            break;
        }
        // >= 10
        case '1': {
            switch (time_str.at(6))
            {
            // 10
            case '0': {
                parsedStr += "Oct ";
                break;
            }
            // 11
            case '1': {
                parsedStr += "Nov ";
                break;
            }
            // 12
            case '2': {
                parsedStr += "Dec ";
                break;
            }
            }
            break;
        }
        }

        // add day
        parsedStr += time_str.substr(8, 2) + " ";

        // add hour:minute
        parsedStr += time_str.substr(11, 5);

        return parsedStr;
    }
} // namespace

std::string create_directory_list(const std::vector<std::string> &virtual_object_list,
                                  const std::string &target_directory, std::string owner, bool include_special_entries)
{
    std::string directory_list = "";

    // also send "." and ".." entries
    if (include_special_entries)
    {
        directory_list += "drwxrwxrwx 1 " + owner + " " + owner + " 0 Jan 1 00:00 .\r\n";
        directory_list += "drwxrwxrwx 1 " + owner + " " + owner + " 0 Jan 1 00:00 ..\r\n";
    }

    size_t i = 2;
    while (i < virtual_object_list.size())
    {
        if (virtual_object_list[i] != target_directory)
        {
            i += 6;
            continue;
        }

        std::string listing_format = "";

        // is file
        if (virtual_object_list[i + 3] == "0")
        {
            listing_format += "-rwxrwxrwx 1 ";
        }
        else if (virtual_object_list[i + 3] == "1")
        {
            // is directory
            listing_format += "drwxrwxrwx 1 ";
        }

        // file owner
        listing_format += owner + " " + owner + " ";

        // file_size
        listing_format += virtual_object_list[i + 1] + " ";

        // modified_time
        listing_format += parse_metadata_time(virtual_object_list[i + 2]) + " ";

        // file_name
        listing_format += virtual_object_list[i - 1];

        directory_list += listing_format + "\r\n";

        i += 6;
    }

    return directory_list;
}