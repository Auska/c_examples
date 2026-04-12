#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "common/common.hpp"

namespace fs = std::filesystem;

// ==================== 数据结构 ====================

struct AppConfig {
  std::string path = ".";
  bool print_max = false;
  bool print_all = false;
  bool use_print0 = false;
};

using NameEntry = std::tuple<fs::path, uintmax_t, fs::file_time_type>;
using NameMap = std::unordered_map<std::string, std::vector<NameEntry>>;

// ==================== 函数声明 ====================

void print_usage(const char *program_name);
AppConfig parse_args(int argc, char *argv[]);
NameMap collect_name_map(const std::string &path_str);
void print_results(const NameMap &name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0);

// ==================== 函数实现 ====================

void print_usage(const char *program_name) {
  std::cout << "Usage: " << program_name << " [OPTIONS] <directory>\n"
            << "\nOptions:\n"
            << "  -h              Show this help message\n"
            << "  -min            Print the path with the minimum total size "
               "(default)\n"
            << "  -max            Print the path with the maximum total size\n"
            << "  -all            Print all duplicate folders (same Chinese name)\n"
            << "  -print0         Use null character as delimiter (for use with "
               "xargs -0)\n"
            << "\nDescription:\n"
            << "  Extract Chinese name from brackets [...] in folder names.\n"
            << "  For duplicate names, display the path with the smallest total file "
               "size.\n";
}

AppConfig parse_args(int argc, char *argv[]) {
  AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h") {
      print_usage(argv[0]);
      std::exit(0);
    } else if (arg == "-min") {
      config.print_max = false;
    } else if (arg == "-max") {
      config.print_max = true;
    } else if (arg == "-all") {
      config.print_all = true;
    } else if (arg == "-print0") {
      config.use_print0 = true;
    } else if (arg[0] != '-') {
      config.path = arg;
    } else {
      std::cerr << "Error: Unknown option '" << arg << "'\n";
      print_usage(argv[0]);
      std::exit(1);
    }
  }

  return config;
}

NameMap collect_name_map(const std::string &path_str) {
  NameMap name_map;

  for (const auto &entry : fs::directory_iterator(path_str)) {
    if (entry.is_directory()) {
      fs::path dir_path = entry.path();
      std::string folder_name = dir_path.filename().string();
      std::string chinese_name = common::extract_bracket_content(folder_name);

      if (!chinese_name.empty()) {
        uintmax_t total_size = common::calculate_total_size(dir_path);
        fs::file_time_type mtime = fs::last_write_time(dir_path);
        name_map[chinese_name].emplace_back(dir_path, total_size, mtime);
      }
    }
  }

  return name_map;
}

void print_results(const NameMap &name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0) {
  for (const auto &[chinese_name, entries] : name_map) {
    if (entries.size() > 1) {
      if (print_all) {
        for (const auto &[path, size, mtime] : entries) {
          if (use_print0) {
            std::cout << path.string();
            std::cout.put('\0');
          } else {
            std::cout << chinese_name << " -> '" << path.string() << "' "
                      << common::format_time(mtime) << " "
                      << common::format_size(size) << "\n";
          }
        }
      } else {
        auto extreme_entry =
            print_max ? std::ranges::max_element(
                            entries,
                            [](const auto &a, const auto &b) {
                              return std::get<1>(a) < std::get<1>(b);
                            })
                      : std::ranges::min_element(
                            entries, [](const auto &a, const auto &b) {
                              return std::get<1>(a) < std::get<1>(b);
                            });
        if (use_print0) {
          std::cout << std::get<0>(*extreme_entry).string();
          std::cout.put('\0');
        } else {
          std::cout << chinese_name << " -> '"
                    << std::get<0>(*extreme_entry).string() << "' "
                    << common::format_time(std::get<2>(*extreme_entry)) << " "
                    << common::format_size(std::get<1>(*extreme_entry)) << "\n";
        }
      }
    }
  }
}

// ==================== main ====================

int main(int argc, char *argv[]) {
  AppConfig config = parse_args(argc, argv);

  if (!fs::exists(config.path)) {
    std::cerr << "Error: Path does not exist: '" << config.path << "'\n";
    return 1;
  }

  if (!fs::is_directory(config.path)) {
    std::cerr << "Error: Path is not a directory: '" << config.path << "'\n";
    return 1;
  }

  NameMap name_map;
  try {
    name_map = collect_name_map(config.path);
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error: Failed to read directory: " << e.what() << "\n";
    return 1;
  }

  print_results(name_map, config.print_max, config.print_all, config.use_print0);

  return 0;
}
