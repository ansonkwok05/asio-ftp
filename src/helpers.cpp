#include "helpers.h"

#include <string>
#include <vector>
#include <string_view>
#include <cstddef>

std::string get_basename(const std::string &path)
{
    if (path.empty())
        return std::string();

    if (path.find('/') == std::string::npos)
        return path;

    // scan backwards for the last '/'
    size_t i = path.length() - 1;
    while (true)
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
        return std::string("/");

    // not absolute path
    if (path[0] != '/')
        return std::string("/");

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
    if (input_string.empty())
        return std::vector<std::string>{};

    if (delimiter.empty() || input_string.length() < delimiter.length())
        return std::vector<std::string>{input_string};

    std::vector<std::string> splitted;
    splitted.reserve(std::min<size_t>(input_string.length() / delimiter.length(), 64));

    size_t find_start = 0;
    while (true)
    {
        size_t index = input_string.find(delimiter, find_start);

        if (index == std::string::npos)
        {
            splitted.emplace_back(input_string.substr(find_start, input_string.length() - find_start));
            break;
        }

        splitted.emplace_back(input_string.substr(find_start, index - find_start));
        find_start = index + delimiter.length();
    }

    return splitted;
}

std::string string_join(const std::vector<std::string> &input_vector, const std::string &separator)
{
    if (input_vector.size() == 0)
        return std::string();

    size_t total_length = 0;

    for (const std::string &substring : input_vector)
    {
        total_length += substring.length();
    }
    total_length += separator.length() * (input_vector.size() - 1);

    std::string joined;
    joined.reserve(total_length);

    for (size_t i = 0; i < input_vector.size() - 1; i++)
    {
        joined += input_vector[i];
        joined += separator;
    }
    joined += input_vector.back();

    return joined;
}

std::string string_to_uppercase(const std::string &input_string)
{
    std::string upper(input_string);

    for (char &c : upper)
    {
        if (c < 'a' || c > 'z')
            continue;
        c = c - 32;
    }

    return upper;
}

bool string_starts_with(const std::string &input_string, const std::string &prefix)
{
    if (input_string.length() < prefix.length())
        return false;

    return input_string.substr(0, prefix.length()) == prefix;
}

std::string string_replace(std::string_view input_string, std::string_view search, std::string_view replace)
{
    if (input_string.empty() || search.empty() || input_string.length() < search.length() || search == replace)
        return std::string(input_string);

    std::string replaced;
    replaced.reserve(input_string.length());

    size_t find_start = 0;
    while (true)
    {
        size_t found = input_string.find(search, find_start);

        if (found == std::string::npos)
        {
            replaced += input_string.substr(find_start, input_string.length() - find_start);
            break;
        }

        replaced.append(input_string.begin() + find_start, found - find_start);
        replaced.append(replace);

        find_start = found + search.length();
    }

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