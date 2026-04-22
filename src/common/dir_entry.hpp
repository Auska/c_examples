#pragma once

#include <cstdint>
#include <filesystem>

namespace common {

/// 目录条目，统一封装路径、修改时间和大小
struct dir_entry {
  std::filesystem::path path;
  std::filesystem::file_time_type mtime;
  std::uintmax_t size{0};
};

}  // namespace common
