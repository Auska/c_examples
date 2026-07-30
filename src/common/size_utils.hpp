#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

namespace common {

/// 将字节数转换为人类可读格式
[[nodiscard]] std::string format_size(std::uintmax_t bytes);

/// 计算目录下所有文件的总大小
[[nodiscard]] std::uintmax_t calculate_total_size(
    const std::filesystem::path& dir_path);

/// 目录大小缓存
class size_cache {
  std::unordered_map<std::string, std::uintmax_t> cache_;

 public:
  /// 获取目录大小（带缓存）
  [[nodiscard]] std::uintmax_t get(const std::filesystem::path& dir_path);
};

}  // namespace common
