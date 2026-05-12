#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <cstddef>

std::string get_basename(const std::string &path);

std::string get_parent_path(const std::string &path);

std::vector<std::string> string_split(const std::string &input_string, const std::string &delimiter);

std::string string_join(const std::vector<std::string> &input_vector, const std::string &separator);

std::string string_to_uppercase(const std::string &input_string);

bool string_starts_with(const std::string &input_string, const std::string &prefix);

std::string string_replace(std::string_view input_string, std::string_view search, std::string_view replace);

std::string generate_uuid_string(size_t length);