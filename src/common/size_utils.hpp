#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace common {

/// 将字节数转换为人类可读格式
[[nodiscard]] inline std::string format_size(std::uintmax_t bytes) {
  static constexpr std::array<std::string_view, 5> units{"B", "KB", "MB", "GB",
                                                          "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024.0 && unit_index < 4) {
    size /= 1024.0;
    ++unit_index;
  }

  if (unit_index == 0) {
    return std::format("{:.0f} {}", size, units[unit_index]);
  }
  return std::format("{:.2f} {}", size, units[unit_index]);
}

/// 计算目录下所有文件的总大小
[[nodiscard]] inline std::uintmax_t calculate_total_size(
    const std::filesystem::path& dir_path) {
  std::uintmax_t total_size = 0;
  std::error_code ec;

  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           dir_path, std::filesystem::directory_options::skip_permission_denied,
           ec)) {
    if (entry.is_regular_file(ec)) {
      const auto file_size = entry.file_size(ec);
      if (!ec) {
        total_size += file_size;
      }
    }
  }

  return total_size;
}

/// 目录大小缓存，封装缓存键生成和查找逻辑
class size_cache {
  std::unordered_map<std::string, std::uintmax_t> cache_;

 public:
  /// 获取目录大小（带缓存）
  [[nodiscard]] std::uintmax_t get(const std::filesystem::path& dir_path) {
    std::string key;
    std::error_code ec;
    const auto canonical_path = std::filesystem::canonical(dir_path, ec);
    key = ec ? dir_path.string() : canonical_path.string();

    const auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }

    const std::uintmax_t total_size = calculate_total_size(dir_path);
    cache_[key] = total_size;
    return total_size;
  }
};

}  // namespace common
