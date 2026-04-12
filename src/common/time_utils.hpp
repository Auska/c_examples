#pragma once

#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace common {

// 格式化修改时间（线程安全版本）
[[nodiscard]] inline std::string format_time(fs::file_time_type ftime) {
  using namespace std::chrono;

  auto file_duration = ftime.time_since_epoch();
  auto file_now = fs::file_time_type::clock::now();
  auto sys_now = system_clock::now();

  auto elapsed = ftime - file_now;
  auto sys_time = sys_now + elapsed;

  std::time_t tt = system_clock::to_time_t(sys_time);
  std::tm tm_buf{};
  localtime_r(&tt, &tm_buf);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d+%H:%M:%S", &tm_buf);
  return buffer;
}

}  // namespace common
