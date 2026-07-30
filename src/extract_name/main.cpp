#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
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
    if (parse_result.error() == common::cli::k_help_sentinel) {
      parser.print_usage(argv[0]);
      return std::unexpected(std::string(common::cli::k_help_sentinel));
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

// ==================== 输出辅助函数 ====================

/// 从 name_map 中收集所有需要输出的 name_entry 列表
[[nodiscard]] std::vector<name_entry> collect_output_entries(
    const NameMap& name_map,
    bool print_max,
    bool print_all) {
  std::vector<name_entry> entries;

  for (const auto& [chinese_name, dir_entries] : name_map) {
    if (dir_entries.size() <= 1) continue;

    if (print_all) {
      for (const auto& e : dir_entries) {
        entries.push_back({chinese_name, e.path, e.size, e.mtime});
      }
    } else {
      const auto extreme =
          print_max
              ? std::ranges::max_element(
                    dir_entries,
                    [](const common::dir_entry& a, const common::dir_entry& b) {
                      return a.size < b.size;
                    })
              : std::ranges::min_element(
                    dir_entries,
                    [](const common::dir_entry& a, const common::dir_entry& b) {
                      return a.size < b.size;
                    });
      entries.push_back(
          {chinese_name, extreme->path, extreme->size, extreme->mtime});
    }
  }

  return entries;
}

/// 按时间排序输出条目
void sort_entries_by_time(std::vector<name_entry>& entries,
                          bool sort_time_desc) {
  std::ranges::sort(entries,
                    [sort_time_desc](const name_entry& a, const name_entry& b) {
                      return sort_time_desc ? a.mtime > b.mtime
                                            : a.mtime < b.mtime;
                    });
}

/// 输出格式化的对齐文本行
void print_aligned_output(std::ostream& os,
                          const std::vector<name_entry>& entries,
                          size_t max_out) {
  // 计算列宽
  size_t max_name_w = 0;
  size_t max_time_w = 0;
  size_t max_size_w = 0;
  for (size_t i = 0; i < max_out; ++i) {
    const auto& e = entries[i];
    max_name_w = std::max(max_name_w, common::display_width(e.chinese_name));
    max_time_w = std::max(
        max_time_w, common::display_width(common::format_time(e.mtime)));
    max_size_w = std::max(max_size_w,
                          common::display_width(common::format_size(e.size)));
  }

  // 输出对齐行
  for (size_t i = 0; i < max_out; ++i) {
    const auto& entry = entries[i];
    const auto time_str = common::format_time(entry.mtime);
    const auto size_str = common::format_size(entry.size);

    const size_t name_dw = common::display_width(entry.chinese_name);
    const size_t time_dw = common::display_width(time_str);

    os << std::string(max_name_w - name_dw, ' ')   // 名称 右对齐
       << entry.chinese_name << "  "
       << std::string(max_time_w - time_dw, ' ')   // 时间 右对齐
       << time_str << "  "
       << size_str                                   // 大小 左对齐
       << std::string(max_size_w - common::display_width(size_str), ' ')
       << "  '" << entry.path.string() << "'\n";    // 路径 末尾
  }
}

/// 输出空字符分隔的路径列表
void print_print0_output(std::ostream& os,
                         const std::vector<name_entry>& entries,
                         size_t max_out) {
  for (size_t i = 0; i < max_out; ++i) {
    os << entries[i].path.string();
    os.put('\0');
  }
}

void print_results(std::ostream& os,
                   const NameMap& name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0,
                   bool sort_time,
                   bool sort_time_desc,
                   const std::optional<int>& limit) {
  // 收集输出条目
  auto entries = collect_output_entries(name_map, print_max, print_all);

  // 按时间排序
  if (sort_time) {
    sort_entries_by_time(entries, sort_time_desc);
  }

  // 计算实际输出数量
  const size_t max_out = limit.has_value()
                             ? std::min<size_t>(*limit, entries.size())
                             : entries.size();

  // 输出
  if (use_print0) {
    print_print0_output(os, entries, max_out);
  } else {
    print_aligned_output(os, entries, max_out);
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  const auto config_result = parse_args(argc, argv);
  if (!config_result) {
    if (config_result.error() == common::cli::k_help_sentinel) {
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

  // 检查是否有重复名称
  bool found = false;
  for (const auto& [name, entries] : name_map) {
    if (entries.size() > 1) {
      found = true;
      break;
    }
  }

  if (found) {
    print_results(std::cout, name_map, config.print_max, config.print_all,
                  config.use_print0, config.sort_time, config.sort_time_desc,
                  config.limit);
  } else {
    std::cout << "No duplicate Chinese names found in: " << config.path << "\n";
  }

  return 0;
}
