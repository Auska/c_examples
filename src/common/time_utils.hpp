#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace common {

// 格式化修改时间（线程安全版本）
[[nodiscard]] inline std::string format_time(std::filesystem::file_time_type ftime) {
  using namespace std::chrono;

  // 获取文件时间与当前文件时间的差值
  const auto file_now = std::filesystem::file_time_type::clock::now();
  const auto sys_now = system_clock::now();

  // 计算文件时间对应的系统时间
  const auto time_diff = ftime - file_now;
  const auto sys_time = sys_now + time_diff;

  const std::time_t tt = system_clock::to_time_t(sys_time);
  std::tm tm_buf{};
  localtime_r(&tt, &tm_buf);

  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d+%H:%M:%S");
  return oss.str();
}

}  // namespace common
