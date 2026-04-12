#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>

namespace common {

/// 格式化修改时间（线程安全跨平台版本）
[[nodiscard]] inline std::string format_time(
    std::filesystem::file_time_type ftime) {
  // C++20: 使用 clock_cast 直接转换为系统时钟时间
  const auto sys_time =
      std::chrono::clock_cast<std::chrono::system_clock>(ftime);
  const std::time_t tt = std::chrono::system_clock::to_time_t(sys_time);

  std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(&tm_buf, &tt);
#else
  localtime_r(&tt, &tm_buf);
#endif

  std::ostringstream oss;
  oss << std::put_time(&tm_buf, "%Y-%m-%d+%H:%M:%S");
  return oss.str();
}

}  // namespace common
