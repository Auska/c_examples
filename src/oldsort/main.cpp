#include "common/common.hpp"

#include <algorithm>
#include <charconv>
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

void print_usage(const char* program_name);
[[nodiscard]] bool parse_int(std::string_view s, int& result);
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

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg = argv[i];
    if (arg == "-h") {
      return std::unexpected("HELP");
    } else if (arg.starts_with("-l") && arg.length() > 2) {
      std::string_view num = arg.substr(2);
      int val = 0;
      if (!parse_int(num, val) || val <= 0) {
        return std::unexpected("Error: -l requires a positive integer\n");
      }
      config.limit = val;
    } else if (arg == "-l") {
      if (i + 1 < argc) {
        std::string_view next_arg = argv[i + 1];
        int val = 0;
        if (!next_arg.empty() &&
            std::isdigit(static_cast<unsigned char>(next_arg[0]))) {
          if (!parse_int(next_arg, val) || val <= 0) {
            return std::unexpected("Error: -l requires a positive integer\n");
          }
          config.limit = val;
          ++i;
        } else {
          return std::unexpected("Error: -l requires a positive integer\n");
        }
      } else {
        return std::unexpected("Error: -l requires an argument\n");
      }
    } else if (arg == "-print0") {
      config.use_print0 = true;
    } else if (arg == "-min") {
      config.sort_by = sort_mode::size_asc;
    } else if (arg == "-max") {
      config.sort_by = sort_mode::size_desc;
    } else if (arg[0] != '-') {
      config.path = arg;
    } else {
      return std::unexpected("Error: Unknown option '" + std::string(arg) +
                             "'\nUse '" + argv[0] + " -h' for help.\n");
    }
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

  for (size_t i = 0; i < max_output; ++i) {
    const auto& dir = dirs[i];
    if (use_print0) {
      os << dir.path.string();
      os.put('\0');
    } else {
      os << common::format_time(dir.mtime) << "  "
         << common::format_size(dir.size) << "  '" << dir.path.string()
         << "'\n";
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
  auto directories = collect_directories(config.path, sc);

  sort_directories(directories, config.sort_by, config.limit);

  print_results(std::cout, directories, config.use_print0, config.limit,
                config.sort_by);

  return 0;
}
