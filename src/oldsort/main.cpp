#include "common/common.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ==================== 数据结构 ====================

struct DirInfo {
  fs::path path;
  fs::file_time_type time;
  std::uintmax_t size{};
};

struct AppConfig {
  std::string path = ".";
  int limit = -1;
  bool use_print0 = false;
  int sort_mode = 0;  // 0: time, 1: size asc, 2: size desc
};

// ==================== 函数声明 ====================

void print_usage(const char* program_name);
[[nodiscard]] bool parse_int(std::string_view s, int& result);
AppConfig parse_args(int argc, char* argv[]);
std::vector<DirInfo> collect_directories(
    const std::string& path_str,
    std::unordered_map<std::string, std::uintmax_t>& size_cache);
void sort_directories(std::vector<DirInfo>& dirs, int sort_mode, int limit);
void print_results(const std::vector<DirInfo>& dirs,
                   bool use_print0,
                   int limit,
                   int sort_mode);

// ==================== 函数实现 ====================

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS] [directory]\n"
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
}

[[nodiscard]] bool parse_int(std::string_view s, int& result) {
  if (s.empty() || s[0] == '-') {
    return false;
  }
  const auto [ptr, ec] =
      std::from_chars(s.data(), s.data() + s.size(), result);
  return ec == std::errc() && ptr == s.data() + s.size();
}

AppConfig parse_args(int argc, char* argv[]) {
  AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "-h") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (arg.starts_with("-l") && arg.length() > 2) {
      std::string_view num = arg.substr(2);
      if (!parse_int(num, config.limit) || config.limit <= 0) {
        std::cerr << "Error: -l requires a positive integer\n";
        std::exit(1);
      }
    } else if (arg == "-l") {
      if (i + 1 < argc) {
        std::string_view next_arg = argv[i + 1];
        if (!next_arg.empty() && std::isdigit(
                                     static_cast<unsigned char>(next_arg[0]))) {
          if (!parse_int(next_arg, config.limit) || config.limit <= 0) {
            std::cerr << "Error: -l requires a positive integer\n";
            std::exit(1);
          }
          ++i;
        } else {
          std::cerr << "Error: -l requires a positive integer\n";
          std::exit(1);
        }
      } else {
        std::cerr << "Error: -l requires an argument\n";
        std::exit(1);
      }
    } else if (arg == "-print0") {
      config.use_print0 = true;
    } else if (arg == "-min") {
      config.sort_mode = 1;
    } else if (arg == "-max") {
      config.sort_mode = 2;
    } else if (arg[0] != '-') {
      config.path = arg;
    } else {
      std::cerr << "Error: Unknown option '" << arg << "'\n";
      std::cerr << "Use '" << argv[0] << " -h' for help.\n";
      std::exit(1);
    }
  }

  return config;
}

std::vector<DirInfo> collect_directories(
    const std::string& path_str,
    std::unordered_map<std::string, std::uintmax_t>& size_cache) {
  std::vector<DirInfo> all_directories;

  const auto opts = fs::directory_options::skip_permission_denied;
  for (const auto& entry : fs::recursive_directory_iterator(path_str, opts)) {
    if (entry.is_directory()) {
      try {
        DirInfo info;
        info.path = entry.path();
        info.time = fs::last_write_time(entry);
        info.size =
            common::calculate_total_size_cached(entry.path(), size_cache);
        all_directories.push_back(std::move(info));
      } catch (const fs::filesystem_error& e) {
        std::cerr << "Warning: Cannot access directory '" << entry.path()
                  << "': " << e.what() << "\n";
      }
    }
  }

  try {
    DirInfo root_info;
    root_info.path = path_str;
    root_info.time = fs::last_write_time(path_str);
    root_info.size = common::calculate_total_size_cached(path_str, size_cache);
    all_directories.push_back(std::move(root_info));
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Warning: Cannot access root directory '" << path_str
              << "': " << e.what() << "\n";
  }

  return all_directories;
}

void sort_directories(std::vector<DirInfo>& dirs, int sort_mode, int limit) {
  std::ranges::sort(dirs, [](const DirInfo& a, const DirInfo& b) {
    return a.time < b.time;
  });

  const size_t max_output =
      (limit == -1) ? dirs.size() : std::min<size_t>(limit, dirs.size());

  if (sort_mode == 1) {
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const DirInfo& a, const DirInfo& b) {
                               return a.size < b.size;
                             });
  } else if (sort_mode == 2) {
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const DirInfo& a, const DirInfo& b) {
                               return a.size > b.size;
                             });
  }
}

void print_results(const std::vector<DirInfo>& dirs,
                   bool use_print0,
                   int limit,
                   int sort_mode) {
  const size_t count = dirs.size();
  const size_t max_output =
      (limit == -1) ? count : std::min<size_t>(limit, count);

  for (size_t i = 0; i < max_output; ++i) {
    const auto& dir = dirs[i];
    if (use_print0) {
      std::cout << dir.path.string();
      std::cout.put('\0');
    } else {
      std::cout << common::format_time(dir.time) << "  "
                << common::format_size(dir.size) << "  '" << dir.path.string()
                << "'\n";
    }
  }

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
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  const AppConfig config = parse_args(argc, argv);

  if (!fs::exists(config.path)) {
    std::cerr << "Error: Path does not exist: '" << config.path << "'\n";
    return 1;
  }

  if (!fs::is_directory(config.path)) {
    std::cerr << "Error: Path is not a directory: '" << config.path << "'\n";
    return 1;
  }

  std::unordered_map<std::string, std::uintmax_t> size_cache;
  auto directories = collect_directories(config.path, size_cache);

  sort_directories(directories, config.sort_mode, config.limit);

  print_results(directories, config.use_print0, config.limit, config.sort_mode);

  return 0;
}
