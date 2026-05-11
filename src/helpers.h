#pragma once

#include <string>
#include <vector>

std::string get_basename(const std::string &path);

std::string get_parent_path(const std::string &path);

std::vector<std::string> string_split(const std::string &input_string, const std::string &delimiter);

std::string string_join(const std::vector<std::string> &input_vector, const std::string &separator);

std::string string_to_uppercase(const std::string &input_string);

bool string_starts_with(const std::string &input_string, const std::string &prefix);

std::string string_replace(const std::string &input_string, const std::string &search, const std::string &replace);

std::string generate_uuid_string(size_t length);