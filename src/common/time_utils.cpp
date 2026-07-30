#include "time_utils.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <string>

namespace common {

std::string format_time(std::filesystem::file_time_type ftime) {
  const auto sys_time =
      std::chrono::clock_cast<std::chrono::system_clock>(ftime);
  const std::time_t tt = std::chrono::system_clock::to_time_t(sys_time);

  std::tm tm_buf{};
#if defined(_WIN32) || defined(_WIN64)
  localtime_s(&tm_buf, &tt);
#else
  localtime_r(&tt, &tm_buf);
#endif

  return std::format("{}-{:02d}-{:02d}+{:02d}:{:02d}:{:02d}",
                     tm_buf.tm_year + 1900,
                     tm_buf.tm_mon + 1,
                     tm_buf.tm_mday,
                     tm_buf.tm_hour,
                     tm_buf.tm_min,
                     tm_buf.tm_sec);
}

}  // namespace common
