#include "helpers.h"

#include "../custom_utils.h"

#include <string>
#include <vector>

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