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

///
/// Expand file path(e.g. expand `~` to `/home/muda`)
///
std::string expand_filepath(const std::string& filepath);

///
/// Check if a specified file path exists.
///
bool file_exists(const std::string& filepath);

/// Get the detected platform
std::string platform();

}  // namespace os_utils
