#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ==================== 数据结构 ====================

struct AppConfig {
  std::string path = ".";
  bool print_max = false;
  bool print_all = false;
  bool use_print0 = false;
  bool sort_time = false;       // 按时间排序
  bool sort_time_desc = false;  // 时间降序（默认升序）
  std::optional<int> limit;     // 限制输出行数
};

/// 带中文名的目录条目（替代 std::tuple 提高可读性）
struct name_entry {
  std::string chinese_name;
  fs::path path;
  std::uintmax_t size;
  fs::file_time_type mtime;
};

using NameMap = std::unordered_map<std::string, std::vector<common::dir_entry>>;

// ==================== 函数声明 ====================

void print_usage(const char* program_name);
[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]);
NameMap collect_name_map(const std::string& path_str, common::size_cache& sc);
void print_results(std::ostream& os,
                   const NameMap& name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0,
                   bool sort_time,
                   bool sort_time_desc,
                   const std::optional<int>& limit);

// ==================== 函数实现 ====================

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS] <directory>\n"
            << "\nOptions:\n"
            << "  -h              Show this help message\n"
            << "  -min            Print the path with the minimum total size "
               "(default)\n"
            << "  -max            Print the path with the maximum total size\n"
            << "  -all            Print all duplicate folders (same Chinese name)\n"
            << "  -print0         Use null character as delimiter (for use with "
               "xargs -0)\n"
            << "  -t              Sort output by modification time (oldest first)\n"
            << "  -tr             Sort output by modification time (newest first)\n"
            << "  -l <N>          Limit output to N lines per group\n"
            << "\nDescription:\n"
            << "  Extract Chinese name from brackets [...] in folder names.\n"
            << "  For duplicate names, display the path with the smallest total file "
               "size.\n";
}

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "-h") {
      return std::unexpected("HELP");
    } else if (arg == "-min") {
      config.print_max = false;
    } else if (arg == "-max") {
      config.print_max = true;
    } else if (arg == "-all") {
      config.print_all = true;
    } else if (arg == "-print0") {
      config.use_print0 = true;
    } else if (arg == "-t") {
      config.sort_time = true;
      config.sort_time_desc = false;
    } else if (arg == "-tr") {
      config.sort_time = true;
      config.sort_time_desc = true;
    } else if (arg == "-l") {
      if (i + 1 >= argc) {
        return std::unexpected("Error: -l requires a number argument\n");
      }
      ++i;
      try {
        int val = std::stoi(argv[i]);
        if (val <= 0) {
          return std::unexpected("Error: -l requires a positive number\n");
        }
        config.limit = val;
      } catch (const std::exception&) {
        return std::unexpected("Error: -l requires a valid number\n");
      }
    } else if (arg[0] != '-') {
      config.path = arg;
    } else {
      return std::unexpected("Error: Unknown option '" + std::string(arg) +
                             "'\n");
    }
  }

  return config;
}

NameMap collect_name_map(const std::string& path_str, common::size_cache& sc) {
  NameMap name_map;

  std::error_code ec;
  for (const auto& entry : fs::directory_iterator(path_str, ec)) {
    if (ec) {
      std::cerr << "Warning: Error iterating directory: " << ec.message()
                << "\n";
      ec.clear();
      continue;
    }

    if (entry.is_directory()) {
      const fs::path dir_path = entry.path();
      const std::string folder_name = dir_path.filename().string();
      const std::string chinese_name =
          common::extract_name_with_season(folder_name);

      if (!chinese_name.empty()) {
        try {
          common::dir_entry info;
          info.path = dir_path;
          info.size = sc.get(dir_path);
          info.mtime = fs::last_write_time(dir_path);
          name_map[chinese_name].push_back(std::move(info));
        } catch (const fs::filesystem_error& e) {
          std::cerr << "Warning: Cannot access directory '" << dir_path
                    << "': " << e.what() << "\n";
        }
      }
    }
  }

  if (ec) {
    std::cerr << "Warning: Error iterating directory: " << ec.message() << "\n";
  }

  return name_map;
}

void print_results(std::ostream& os,
                   const NameMap& name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0,
                   bool sort_time,
                   bool sort_time_desc,
                   const std::optional<int>& limit) {
  // 收集所有要输出的条目
  std::vector<name_entry> output_entries;

  for (const auto& [chinese_name, entries] : name_map) {
    if (entries.size() > 1) {
      if (print_all) {
        for (const auto& e : entries) {
          output_entries.push_back(
              {chinese_name, e.path, e.size, e.mtime});
        }
      } else {
        const auto extreme_entry =
            print_max ? std::ranges::max_element(
                            entries, [](const common::dir_entry& a,
                                        const common::dir_entry& b) {
                              return a.size < b.size;
                            })
                      : std::ranges::min_element(
                            entries, [](const common::dir_entry& a,
                                        const common::dir_entry& b) {
                              return a.size < b.size;
                            });
        output_entries.push_back(
            {chinese_name,
             extreme_entry->path,
             extreme_entry->size,
             extreme_entry->mtime});
      }
    }
  }

  // 按时间排序
  if (sort_time) {
    std::ranges::sort(output_entries,
                       [sort_time_desc](const name_entry& a,
                                        const name_entry& b) {
                         return sort_time_desc ? a.mtime > b.mtime
                                               : a.mtime < b.mtime;
                       });
  }

  // 输出结果
  int count = 0;
  for (const auto& entry : output_entries) {
    if (limit.has_value() && count >= *limit) {
      break;
    }
    ++count;
    if (use_print0) {
      os << entry.path.string();
      os.put('\0');
    } else {
      os << entry.chinese_name << " -> '" << entry.path.string() << "' "
         << common::format_time(entry.mtime) << " "
         << common::format_size(entry.size) << "\n";
    }
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  const auto config_result = parse_args(argc, argv);
  if (!config_result) {
    if (config_result.error() == "HELP") {
      print_usage(argv[0]);
      return 0;
    }
    std::cerr << config_result.error();
    return 1;
  }
  const auto& config = *config_result;

  const auto dir_result = common::validate_directory(config.path);
  if (!dir_result) {
    std::cerr << "Error: " << dir_result.error() << "\n";
    return 1;
  }

  common::size_cache sc;
  NameMap name_map;
  try {
    name_map = collect_name_map(config.path, sc);
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Error: Failed to read directory: " << e.what() << "\n";
    return 1;
  }

  print_results(std::cout, name_map, config.print_max, config.print_all,
                config.use_print0, config.sort_time, config.sort_time_desc,
                config.limit);

  return 0;
}
