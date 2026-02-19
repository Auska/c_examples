#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// 计算目录下所有文件的总大小
uintmax_t calculate_total_size(const fs::path &dir_path) {
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

// 将字节数转换为人类可读格式
std::string format_size(uintmax_t bytes) {
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

// 定义错误类型
enum class Error {
  None,
  PathNotFound,
  NotADirectory,
  FailedToReadDirectory,
  InvalidLimit,
  UnknownOption
};

// 错误消息映射
std::string error_to_string(Error err) {
  switch (err) {
  case Error::PathNotFound:
    return "Path does not exist";
  case Error::NotADirectory:
    return "Path is not a directory";
  case Error::FailedToReadDirectory:
    return "Failed to read directory";
  case Error::InvalidLimit:
    return "Invalid limit value";
  case Error::UnknownOption:
    return "Unknown option";
  default:
    return "Unknown error";
  }
}

// Result 类型别名
template <typename T>
using Result = std::expected<T, std::pair<Error, std::string>>;

// 存储目录信息和时间戳
struct DirInfo {
  fs::path path;
  fs::file_time_type time;
  uintmax_t size;
};

// 使用 std::from_chars 解析整数（性能优于 stoi，不抛异常）
[[nodiscard]] bool parse_int(const std::string &s, int &result) {
  // 检查字符串是否完全由数字组成
  if (s.empty() || !std::isdigit(s[0])) {
    return false;
  }
  auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
  return ec == std::errc() && ptr == s.data() + s.size();
}

int main(int argc, char *argv[]) {
  std::string pathStr = ".";
  int limit = -1;          // -1 表示不限制
  bool use_print0 = false; // 是否使用 \0 分隔符
  int sort_mode =
      0; // 0: 按时间排序（默认），1: 按大小升序(-min)，2: 按大小降序(-max)

  // 手动解析参数，支持 -min 和 -max
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h") {
      std::cout << "Usage: " << argv[0] << " [OPTIONS] [directory]\n"
                << "\nOptions:\n"
                << "  -l <number>     Limit output to N directories (default: "
                   "no limit)\n"
                << "  -print0         Use null character as delimiter (for use "
                   "with xargs -0)\n"
                << "  -min            Sort by folder size (smallest first)\n"
                << "  -max            Sort by folder size (largest first)\n"
                << "  -h              Show this help message\n"
                << "\nDescription:\n"
                << "  List all directories in the specified path, sorted by "
                   "last modification time\n"
                << "  from oldest to newest. If no directory is specified, the "
                   "current directory is used.\n";
      return 0;
    } else if (arg.rfind("-l", 0) == 0 && arg.length() > 2) {
      // 支持 -lN 格式（如 -l20 或 -l1max）
      std::string num = arg.substr(2);
      if (!parse_int(num, limit) || limit <= 0) {
        std::cerr << "Error: -l requires a positive integer\n";
        return 1;
      }
    } else if (arg == "-l") {
      // 需要下一个参数
      if (i + 1 < argc) {
        std::string next_arg = argv[i + 1];
        // 检查下一个参数是否是数字（而不是另一个选项）
        if (!next_arg.empty() && std::isdigit(next_arg[0])) {
          if (!parse_int(next_arg, limit) || limit <= 0) {
            std::cerr << "Error: -l requires a positive integer\n";
            return 1;
          }
          i++; // 跳过下一个参数
        } else {
          std::cerr << "Error: -l requires a positive integer\n";
          return 1;
        }
      } else {
        std::cerr << "Error: -l requires an argument\n";
        return 1;
      }
    } else if (arg == "-print0") {
      use_print0 = true;
    } else if (arg == "-min") {
      sort_mode = 1;
    } else if (arg == "-max") {
      sort_mode = 2;
    } else if (arg[0] != '-') {
      // 非选项参数（目录路径）
      pathStr = arg;
    } else {
      std::cerr << "Error: Unknown option '" << arg << "'\n";
      std::cerr << "Use '" << argv[0] << " -h' for help.\n";
      return 1;
    }
  }

  // 检查路径是否存在且是目录
  if (!fs::exists(pathStr)) {
    std::cerr << "Error: " << error_to_string(Error::PathNotFound) << ": '"
              << pathStr << "'\n";
    return 1;
  }

  if (!fs::is_directory(pathStr)) {
    std::cerr << "Error: " << error_to_string(Error::NotADirectory) << ": '"
              << pathStr << "'\n";
    return 1;
  }

  std::vector<DirInfo> all_directories;

  try {
    // 递归遍历所有子目录，跳过无权限的目录
    auto opts = fs::directory_options::skip_permission_denied;
    for (const auto &entry : fs::recursive_directory_iterator(pathStr, opts)) {
      if (entry.is_directory()) {
        try {
          DirInfo info;
          info.path = entry.path();
          info.time = fs::last_write_time(entry);
          info.size = calculate_total_size(entry.path());
          all_directories.push_back(info);
        } catch (const fs::filesystem_error &) {
          // 跳过无法获取时间的目录
        }
      }
    }

    // 加入根目录本身
    try {
      DirInfo root_info;
      root_info.path = pathStr;
      root_info.time = fs::last_write_time(pathStr);
      root_info.size = calculate_total_size(pathStr);
      all_directories.push_back(root_info);
    } catch (const fs::filesystem_error &) {
      // 根目录时间获取失败，跳过
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error: " << error_to_string(Error::FailedToReadDirectory)
              << ": " << e.what() << "\n";
    return 1;
  }

  // 首先按最后修改时间排序（从旧到新）
  std::ranges::sort(
      all_directories.begin(), all_directories.end(),
      [](const DirInfo &a, const DirInfo &b) { return a.time < b.time; });

  // 如果有 -l 限制，先截取前 N 个
  size_t count = all_directories.size();
  size_t max_output = (limit == -1) ? count : std::min<size_t>(limit, count);

  // -min/-max: 对 -l 限制后的结果按大小排序（如果没有 -l 则对所有结果排序）
  if (sort_mode == 1) {
    std::ranges::stable_sort(
        all_directories.begin(), all_directories.begin() + max_output,
        [](const DirInfo &a, const DirInfo &b) { return a.size < b.size; });
  } else if (sort_mode == 2) {
    std::ranges::stable_sort(
        all_directories.begin(), all_directories.begin() + max_output,
        [](const DirInfo &a, const DirInfo &b) { return a.size > b.size; });
  }

  // 输出结果

  for (size_t i = 0; i < max_output; ++i) {
    const auto &dir = all_directories[i];
    if (use_print0) {
      // 输出路径 + '\0'（不换行）
      std::cout << dir.path.string();
      std::cout.put('\0');
    } else {
      // 原始带时间格式输出（使用预计算的时间戳）
      auto ftime = dir.time;
      auto sctp =
          std::chrono::time_point_cast<std::chrono::system_clock::duration>(
              ftime - fs::file_time_type::clock::now() +
              std::chrono::system_clock::now());
      std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
      std::tm *tm = std::localtime(&tt);

      std::cout << std::put_time(tm, "%Y-%m-%d+%H:%M:%S") << "  "
                << format_size(dir.size) << "  '" << dir.path.string() << "'\n";
    }
  }

  // 只有在非 null 分隔时才输出统计信息（避免破坏 xargs 流）
  if (!use_print0) {
    if (limit != -1) {
      if (sort_mode == 2) {
        std::cout << "\n(Showing largest " << max_output << " directories)\n";
      } else if (sort_mode == 1) {
        std::cout << "\n(Showing smallest " << max_output << " directories)\n";
      } else {
        std::cout << "\n(Showing oldest " << max_output << " directories)\n";
      }
    } else {
      std::cout << "\nFound " << count << " directories.\n";
    }
  }

  return 0;
}
