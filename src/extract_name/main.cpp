#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
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

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;
  
  common::cli::parser parser("Extract Chinese name from brackets [...] in folder names.\n" 
                             "For duplicate names, display the path with the smallest total file size.");
  parser.add_option({"--min", 'm', "Print the path with the minimum total size (default)", false});
  parser.add_option({"--max", 'M', "Print the path with the maximum total size", false});
  parser.add_option({"--all", 'a', "Print all duplicate folders (same Chinese name)", false});
  parser.add_option({"--print0", '0', "Use null character as delimiter", false});
  parser.add_option({"--time", 't', "Sort output by modification time (oldest first)", false});
  parser.add_option({"--time-reverse", 'T', "Sort output by modification time (newest first)", false});
  parser.add_option({"--limit", 'l', "Limit output to N lines per group", true});
  parser.add_positional("directory", "Directory to scan");
  
  auto parse_result = parser.parse(argc, argv);
  if (!parse_result) {
    if (parse_result.error() == "HELP") {
      parser.print_usage(argv[0]);
      return std::unexpected("HELP");
    }
    return std::unexpected(parse_result.error());
  }
  
  // 处理 min/max 选项
  if (common::cli::parser::has_option(*parse_result, "--max")) {
    config.print_max = true;
  } else if (common::cli::parser::has_option(*parse_result, "--min")) {
    config.print_max = false;
  }
  
  // 处理 all 选项
  if (common::cli::parser::has_option(*parse_result, "--all")) {
    config.print_all = true;
  }
  
  // 处理 print0 选项
  if (common::cli::parser::has_option(*parse_result, "--print0")) {
    config.use_print0 = true;
  }
  
  // 处理时间排序选项
  if (common::cli::parser::has_option(*parse_result, "--time-reverse")) {
    config.sort_time = true;
    config.sort_time_desc = true;
  } else if (common::cli::parser::has_option(*parse_result, "--time")) {
    config.sort_time = true;
    config.sort_time_desc = false;
  }
  
  // 处理 limit 选项
  auto limit_str = common::cli::parser::get_option(*parse_result, "--limit");
  if (!limit_str.empty()) {
    try {
      int val = std::stoi(std::string(limit_str));
      if (val <= 0) {
        return std::unexpected("Error: -l requires a positive number\n");
      }
      config.limit = val;
    } catch (const std::exception&) {
      return std::unexpected("Error: -l requires a valid number\n");
    }
  }
  
  // 处理位置参数
  if (!parse_result->positional.empty()) {
    config.path = parse_result->positional[0];
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
  const size_t max_out =
      limit.has_value()
          ? std::min<size_t>(*limit, output_entries.size())
          : output_entries.size();

  if (!use_print0) {
    // 自动计算列宽（仅扫描实际输出的行）
    size_t max_name_width = 0;
    size_t max_time_width = 0;
    size_t max_size_width = 0;
    for (size_t i = 0; i < max_out; ++i) {
      const auto& e = output_entries[i];
      max_name_width =
          std::max(max_name_width, e.chinese_name.size());
      max_time_width =
          std::max(max_time_width,
                   common::format_time(e.mtime).size());
      max_size_width =
          std::max(max_size_width,
                   common::format_size(e.size).size());
    }

    for (size_t i = 0; i < max_out; ++i) {
      const auto& entry = output_entries[i];
      os << std::right << std::setw(static_cast<int>(max_name_width))
         << entry.chinese_name << "  '" << entry.path.string() << "' "
         << std::right << std::setw(static_cast<int>(max_time_width))
         << common::format_time(entry.mtime) << " "
         << std::left << std::setw(static_cast<int>(max_size_width))
         << common::format_size(entry.size) << "\n";
    }
  } else {
    for (size_t i = 0; i < max_out; ++i) {
      os << output_entries[i].path.string();
      os.put('\0');
    }
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  const auto config_result = parse_args(argc, argv);
  if (!config_result) {
    if (config_result.error() == "HELP") {
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
