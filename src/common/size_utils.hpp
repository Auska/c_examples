#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

namespace fs = std::filesystem;

namespace common {

// 将字节数转换为人类可读格式
[[nodiscard]] inline std::string format_size(uintmax_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024 && unit_index < 4) {
    size /= 1024;
    unit_index++;
  }

  char buffer[32];
  if (unit_index == 0) {
    snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unit_index]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
  }
  return buffer;
}

// 计算目录下所有文件的总大小
[[nodiscard]] inline uintmax_t calculate_total_size(const fs::path &dir_path) {
  uintmax_t total_size = 0;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        try {
          total_size += entry.file_size();
        } catch (const fs::filesystem_error &) {
          // 跳过无法获取大小的文件
        }
      }
    }
  } catch (const fs::filesystem_error &) {
    // 跳过无法读取的目录
  }
  return total_size;
}

// 计算目录大小（带缓存版本，避免重复计算）
[[nodiscard]] inline uintmax_t calculate_total_size_cached(
    const fs::path &dir_path,
    std::unordered_map<std::string, uintmax_t> &cache) {
  // 规范化路径作为缓存键
  std::string key;
  try {
    key = fs::canonical(dir_path).string();
  } catch (...) {
    key = dir_path.string();
  }

  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  uintmax_t total_size = calculate_total_size(dir_path);
  cache[key] = total_size;
  return total_size;
}

}  // namespace common
