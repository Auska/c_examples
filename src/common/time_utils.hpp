#pragma once

#include <filesystem>
#include <string>

namespace common {

/// 格式化修改时间（线程安全跨平台版本）
[[nodiscard]] std::string format_time(
    std::filesystem::file_time_type ftime);

}  // namespace common
