#pragma once

#include <expected>
#include <filesystem>
#include <string>

namespace common {

/// 校验路径是否为有效目录
[[nodiscard]] std::expected<std::filesystem::path, std::string>
validate_directory(const std::string& path_str);

}  // namespace common
