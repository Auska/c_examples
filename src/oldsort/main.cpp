#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==================== 数据结构 ====================

enum class sort_mode { time, size_asc, size_desc };

// ==================== 函数声明 ====================

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

  if (sort_by == sort_mode::size_asc) {
    const size_t max_output =
        limit.has_value() ? std::min<size_t>(*limit, dirs.size()) : dirs.size();
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const common::dir_entry& a,
                                const common::dir_entry& b) {
                               return a.size < b.size;
                             });
  } else if (sort_by == sort_mode::size_desc) {
    const size_t max_output =
        limit.has_value() ? std::min<size_t>(*limit, dirs.size()) : dirs.size();
    std::ranges::stable_sort(dirs.begin(), dirs.begin() + max_output,
                             [](const common::dir_entry& a,
                                const common::dir_entry& b) {
                               return a.size > b.size;
                             });
  }
}

// ==================== 输出辅助函数 ====================

/// 输出格式化的对齐文本行
void print_aligned_output(std::ostream& os,
                          const std::vector<common::dir_entry>& dirs,
                          size_t max_output) {
  size_t max_time_w = 0;
  size_t max_size_w = 0;
  for (size_t i = 0; i < max_output; ++i) {
    common::output::update_max_width(
        max_time_w, common::format_time(dirs[i].mtime));
    common::output::update_max_width(
        max_size_w, common::format_size(dirs[i].size));
  }

  for (size_t i = 0; i < max_output; ++i) {
    const auto& dir = dirs[i];
    const auto time_str = common::format_time(dir.mtime);
    const auto size_str = common::format_size(dir.size);
    common::output::print_column_line(
        os, time_str, size_str, dir.path.string(), max_time_w, max_size_w);
  }
}

/// 输出空字符分隔的路径列表
void print_print0_output(std::ostream& os,
                         const std::vector<common::dir_entry>& dirs,
                         size_t max_output) {
  std::vector<std::string> paths;
  paths.reserve(max_output);
  for (size_t i = 0; i < max_output; ++i) {
    paths.push_back(dirs[i].path.string());
  }
  common::output::print_print0_paths(os, paths, max_output);
}

/// 输出摘要信息
void print_summary(std::ostream& os,
                   size_t count,
                   size_t max_output,
                   bool has_limit,
                   sort_mode sort_by) {
  if (has_limit) {
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

void print_results(std::ostream& os,
                   const std::vector<common::dir_entry>& dirs,
                   bool use_print0,
                   const std::optional<int>& limit,
                   sort_mode sort_by) {
  const size_t count = dirs.size();
  const size_t max_output =
      limit.has_value() ? std::min<size_t>(*limit, count) : count;

  if (use_print0) {
    print_print0_output(os, dirs, max_output);
  } else {
    print_aligned_output(os, dirs, max_output);
    print_summary(os, count, max_output, limit.has_value(), sort_by);
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  return common::args::run_tool(argc, argv,
      common::args::tool_descriptor{
          .description =
              "List all directories in the specified path, sorted by last\n"
              "modification time from oldest to newest. If no directory is\n"
              "specified, the current directory is used.",
          .has_size_sort = true,
          .has_time_sort = false,
          .has_limit = true,
          .has_print0 = true,
      },
      [](const common::args::parsed_args& args) -> int {
        const std::string path = args.directories[0].string();

        // Determine sort mode from args
        sort_mode sort_by = sort_mode::time;
        if (args.sort_size) {
          sort_by = args.sort_size_desc ? sort_mode::size_desc
                                        : sort_mode::size_asc;
        }

        std::optional<int> limit;
        if (args.limit.has_value()) {
          limit = static_cast<int>(*args.limit);
        }

        common::size_cache sc;
        auto directories = collect_directories(path, sc);

        sort_directories(directories, sort_by, limit);

        print_results(std::cout, directories, args.print0, limit, sort_by);

        return 0;
      });
}
