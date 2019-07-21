#pragma once

#include <string>

/// Lose collection of wrapping of operating system APIs
namespace os_utils {
/// Make a directory
bool mkdir(const std::string& dir_path);

/// Make the directory that should contain this file
bool mkdir_from_filepath(const std::string& file_name);

/// Open an URL
bool open_url(const std::string& url);

/// Get the detected platform
std::string platform();
}  // namespace os_utils
