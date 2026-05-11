#include "helpers.h"

#include <string>
#include <vector>

std::string get_basename(const std::string &path)
{
    if (path.length() == 0)
    {
        return "";
    }

    // not absolute path
    if (path.find('/') == std::string::npos)
    {
        return "";
    }

    // scan backwards for the last '/'
    size_t i = path.length() - 1;
    while (i >= 0)
    {
        if (path[i] == '/')
        {
            i++;
            break;
        }

        i--;
    }

    return path.substr(i, path.length() - i);
}

std::string get_parent_path(const std::string &path)
{
    // already at root directory
    if (path.length() <= 1)
    {
        return "/";
    }

    // not absolute path
    if (path[0] != '/')
    {
        return "/";
    }

    // scan backwards for the last '/'
    size_t i = path.length() - 1;
    while (i > 1)
    {
        if (path[i] == '/')
            break;

        i--;
    }

    return path.substr(0, i);
}

std::vector<std::string> string_split(const std::string &input_string, const std::string &delimiter)
{
    if (input_string.size() == 0)
    {
        return {};
    }

    if (delimiter.size() == 0 || input_string.length() < delimiter.length())
        return {input_string};

    std::vector<std::string> splitted;

    size_t find_start = 0;
    while (true)
    {
        size_t index = input_string.find(delimiter, find_start);

        if (index == std::string::npos)
            break;

        splitted.emplace_back(input_string.substr(find_start, index - find_start));
        find_start = index + delimiter.length();
    }
    splitted.emplace_back(input_string.substr(find_start, input_string.length() - find_start));

    return splitted;
}

std::string string_join(const std::vector<std::string> &input_vector, const std::string &separator)
{
    if (input_vector.size() == 0)
        return "";

    std::string joined;

    for (size_t i = 0; i < input_vector.size() - 1; i++)
    {
        joined += input_vector[i] + separator;
    }
    joined += input_vector[input_vector.size() - 1];

    return joined;
}

std::string string_to_uppercase(const std::string &input_string)
{
    std::string upper = "";

    size_t i = 0;
    while (i < input_string.size())
    {
        if (input_string[i] >= 'a' && input_string[i] <= 'z')
        {
            upper += input_string[i] - 32;
        }
        else
        {
            upper += input_string[i];
        }

        i++;
    }

    return upper;
}

bool string_starts_with(const std::string &input_string, const std::string &prefix)
{
    if (input_string.length() < prefix.length())
        return false;

    return input_string.substr(0, prefix.length()) == prefix;
}

std::string string_replace(const std::string &input_string, const std::string &search, const std::string &replace)
{
    if (input_string.length() == 0 || search.length() == 0)
        return "";

    std::string replaced;

    size_t find_start = 0;
    while (true)
    {
        size_t found = input_string.find(search, find_start);

        if (found == std::string::npos)
            break;

        replaced += input_string.substr(find_start, found - find_start) + replace;
        find_start = found + search.length();
    }
    replaced += input_string.substr(find_start, input_string.length() - find_start);

    return replaced;
}

std::string generate_uuid_string(size_t length)
{
    std::string uuid = "";

    for (size_t i = 0; i < length; i++)
    {
        int random_hex = std::rand() % 16;

        switch (random_hex)
        {
        case 10: {
            uuid += "A";
            break;
        }
        case 11: {
            uuid += "B";
            break;
        }
        case 12: {
            uuid += "C";
            break;
        }
        case 13: {
            uuid += "D";
            break;
        }
        case 14: {
            uuid += "E";
            break;
        }
        case 15: {
            uuid += "F";
            break;
        }
        default: {
            uuid += std::to_string(random_hex);
            break;
        }
        }
    }

    return uuid;
}