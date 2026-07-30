#include "fs_utils.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace common {

std::expected<std::filesystem::path, std::string>
validate_directory(const std::string& path_str) {
  std::error_code ec;
  if (!std::filesystem::exists(path_str, ec)) {
    return std::unexpected("Path does not exist: '" + path_str + "'");
  }
  if (!std::filesystem::is_directory(path_str, ec)) {
    return std::unexpected("Path is not a directory: '" + path_str + "'");
  }
  return std::filesystem::absolute(path_str, ec);
}

}  // namespace common
