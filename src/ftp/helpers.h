#pragma once

#include <string>
#include <vector>
#include <cstdint>

std::pair<std::string, std::string> parse_buffer(std::vector<uint8_t> buffer, size_t bytes_received);

std::string return_parent_directory(std::string directory);

std::string create_directory_list(std::vector<std::string> virtual_object_list, std::string target_directory,
                                  std::string owner, bool include_special_entries);