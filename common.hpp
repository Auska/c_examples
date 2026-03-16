#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>

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

// 格式化修改时间（线程安全版本）
[[nodiscard]] inline std::string format_time(fs::file_time_type ftime) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - fs::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
  std::tm tm_buf;
  localtime_r(&tt, &tm_buf);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d+%H:%M:%S", &tm_buf);
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

}  // namespace common
