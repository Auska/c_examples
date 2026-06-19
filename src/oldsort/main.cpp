#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

// ==================== 数据结构 ====================

enum class sort_mode { time, size_asc, size_desc };

struct AppConfig {
  std::string path = ".";
  std::optional<int> limit;
  bool use_print0 = false;
  sort_mode sort_by = sort_mode::time;
};

// ==================== 函数声明 ====================

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]);
std::vector<common::dir_entry> collect_directories(const std::string& path_str,
                                                    common::size_cache& sc);
void sort_directories(std::vector<common::dir_entry>& dirs,
                      sort_mode sort_by,
                      const std::optional<int>& limit);
void print_results(std::ostream& os,
                   const std::vector<common::dir_entry>& dirs,
                   bool use_print0,
                   const std::optional<int>& limit,
                   sort_mode sort_by);

// ==================== 函数实现 ====================

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;
  
  common::cli::parser parser("List all directories in the specified path, sorted by last modification time\n" 
                             "from oldest to newest. If no directory is specified, the current directory is used.");
  parser.add_option({"--limit", 'l', "Limit output to N directories", true});
  parser.add_option({"--print0", '0', "Use null character as delimiter", false});
  parser.add_option({"--min", 'm', "Sort by folder size (smallest first)", false});
  parser.add_option({"--max", 'M', "Sort by folder size (largest first)", false});
  parser.add_positional("directory", "Directory to scan");
  
  auto parse_result = parser.parse(argc, argv);
  if (!parse_result) {
    if (parse_result.error() == "HELP") {
      parser.print_usage(argv[0]);
      return std::unexpected("HELP");
    }
    return std::unexpected(parse_result.error());
  }
  
  // 处理 limit 选项
  auto limit_str = common::cli::parser::get_option(*parse_result, "--limit");
  if (!limit_str.empty()) {
    try {
      int val = std::stoi(std::string(limit_str));
      if (val <= 0) {
        return std::unexpected("Error: -l requires a positive integer\n");
      }
      config.limit = val;
    } catch (const std::exception&) {
      return std::unexpected("Error: -l requires a valid positive integer\n");
    }
  }
  
  // 处理 print0 选项
  if (common::cli::parser::has_option(*parse_result, "--print0")) {
    config.use_print0 = true;
  }
  
  // 处理排序选项
  if (common::cli::parser::has_option(*parse_result, "--min")) {
    config.sort_by = sort_mode::size_asc;
  } else if (common::cli::parser::has_option(*parse_result, "--max")) {
    config.sort_by = sort_mode::size_desc;
  }
  
  // 处理位置参数
  if (!parse_result->positional.empty()) {
    config.path = parse_result->positional[0];
  }
  
  return config;
}

std::vector<common::dir_entry> collect_directories(
    const std::string& path_str,
    common::size_cache& sc) {
  std::vector<common::dir_entry> all_directories;

  const auto opts = fs::directory_options::skip_permission_denied;
  for (const auto& entry : fs::recursive_directory_iterator(path_str, opts)) {
    if (entry.is_directory()) {
      try {
        common::dir_entry info;
        info.path = entry.path();
        info.mtime = fs::last_write_time(entry);
        info.size = sc.get(entry.path());
        all_directories.push_back(std::move(info));
      } catch (const fs::filesystem_error& e) {
        std::cerr << "Warning: Cannot access directory '" << entry.path()
                  << "': " << e.what() << "\n";
      }
    }
  }

  try {
    common::dir_entry root_info;
    root_info.path = path_str;
    root_info.mtime = fs::last_write_time(path_str);
    root_info.size = sc.get(path_str);
    all_directories.push_back(std::move(root_info));
  } catch (const fs::filesystem_error& e) {
    std::cerr << "Warning: Cannot access root directory '" << path_str
              << "': " << e.what() << "\n";
  }

  return all_directories;
}

void sort_directories(std::vector<common::dir_entry>& dirs,
                      sort_mode sort_by,
                      const std::optional<int>& limit) {
  std::ranges::sort(dirs, [](const common::dir_entry& a,
                             const common::dir_entry& b) {
    return a.mtime < b.mtime;
  });

  const size_t max_output =
      limit.has_value() ? std::min<size_t>(*limit, dirs.size()) : dirs.size();

  if (sort_by == sort_mode::size_asc) {
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const common::dir_entry& a,
                                const common::dir_entry& b) {
                               return a.size < b.size;
                             });
  } else if (sort_by == sort_mode::size_desc) {
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const common::dir_entry& a,
                                const common::dir_entry& b) {
                               return a.size > b.size;
                             });
  }
}

void print_results(std::ostream& os,
                   const std::vector<common::dir_entry>& dirs,
                   bool use_print0,
                   const std::optional<int>& limit,
                   sort_mode sort_by) {
  const size_t count = dirs.size();
  const size_t max_output =
      limit.has_value() ? std::min<size_t>(*limit, count) : count;

  if (!use_print0) {
    // 自动计算列宽（基于终端显示宽度）
    size_t max_time_w = 0;
    size_t max_size_w = 0;
    for (size_t i = 0; i < max_output; ++i) {
      max_time_w =
          std::max(max_time_w,
                   common::display_width(common::format_time(dirs[i].mtime)));
      max_size_w =
          std::max(max_size_w,
                   common::display_width(common::format_size(dirs[i].size)));
    }

    for (size_t i = 0; i < max_output; ++i) {
      const auto& dir = dirs[i];
      const auto time_str = common::format_time(dir.mtime);
      const auto size_str = common::format_size(dir.size);

      os << std::string(max_time_w - common::display_width(time_str), ' ')
         << time_str << "  "
         << std::string(max_size_w - common::display_width(size_str), ' ')
         << size_str << "  '" << dir.path.string() << "'\n";
    }
  } else {
    for (size_t i = 0; i < max_output; ++i) {
      os << dirs[i].path.string();
      os.put('\0');
    }
  }

  if (!use_print0) {
    if (limit.has_value()) {
      if (sort_by == sort_mode::size_desc) {
        os << "\n(Showing largest " << max_output << " directories)\n";
      } else if (sort_by == sort_mode::size_asc) {
        os << "\n(Showing smallest " << max_output << " directories)\n";
      } else {
        os << "\n(Showing oldest " << max_output << " directories)\n";
      }
    } else {
      os << "\nFound " << count << " directories.\n";
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
  auto directories = collect_directories(config.path, sc);

  sort_directories(directories, config.sort_by, config.limit);

  print_results(std::cout, directories, config.use_print0, config.limit,
                config.sort_by);

  return 0;
}
