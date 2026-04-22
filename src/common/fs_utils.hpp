#pragma once

#include <filesystem>
#include <expected>
#include <string>

namespace common {

/// 校验路径是否为有效目录
/// 成功返回规范化的绝对路径，失败返回错误描述
[[nodiscard]] inline std::expected<std::filesystem::path, std::string>
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
