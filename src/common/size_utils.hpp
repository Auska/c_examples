#pragma once

#include <cstdint>
#include <filesystem>
#include <format>
#include <string>
#include <unordered_map>
#include <utility>

namespace common {

// 将字节数转换为人类可读格式
[[nodiscard]] inline std::string format_size(std::uintmax_t bytes) {
  constexpr std::string_view units[] = {"B", "KB", "MB", "GB", "TB"};
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

// 计算目录下所有文件的总大小
[[nodiscard]] inline std::uintmax_t calculate_total_size(
    const std::filesystem::path &dir_path) {
  std::uintmax_t total_size = 0;
  std::error_code ec;

  for (const auto &entry :
       std::filesystem::recursive_directory_iterator(dir_path, ec)) {
    if (entry.is_regular_file(ec)) {
      auto file_size = entry.file_size(ec);
      if (!ec) {
        total_size += file_size;
      }
    }
  }

  return total_size;
}

// 计算目录大小（带缓存版本，避免重复计算）
[[nodiscard]] inline std::uintmax_t calculate_total_size_cached(
    const std::filesystem::path &dir_path,
    std::unordered_map<std::string, std::uintmax_t> &cache) {
  // 规范化路径作为缓存键
  std::string key;
  std::error_code ec;
  auto canonical_path = std::filesystem::canonical(dir_path, ec);
  if (ec) {
    key = dir_path.string();
  } else {
    key = canonical_path.string();
  }

  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  std::uintmax_t total_size = calculate_total_size(dir_path);
  cache[key] = total_size;
  return total_size;
}

}  // namespace common
