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

/// 带中文名的目录条目
struct name_entry {
  std::string chinese_name;
  fs::path path;
  std::uintmax_t size;
  fs::file_time_type mtime;
};

using NameMap = std::unordered_map<std::string, std::vector<common::dir_entry>>;

// ==================== 函数声明 ====================

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
  size_t max_name_w = 0;
  size_t max_time_w = 0;
  size_t max_size_w = 0;
  for (size_t i = 0; i < max_out; ++i) {
    const auto& e = entries[i];
    common::output::update_max_width(max_name_w, e.chinese_name);
    common::output::update_max_width(
        max_time_w, common::format_time(e.mtime));
    common::output::update_max_width(
        max_size_w, common::format_size(e.size));
  }

  for (size_t i = 0; i < max_out; ++i) {
    const auto& entry = entries[i];
    const auto time_str = common::format_time(entry.mtime);
    const auto size_str = common::format_size(entry.size);

    common::output::print_right_aligned(os, entry.chinese_name, max_name_w);
    os << "  ";
    common::output::print_right_aligned(os, time_str, max_time_w);
    os << "  " << size_str
       << std::string(max_size_w > common::display_width(size_str)
                        ? max_size_w - common::display_width(size_str)
                        : 0, ' ')
       << "  '" << entry.path.string() << "'\n";
  }
}

/// 输出空字符分隔的路径列表
void print_print0_output(std::ostream& os,
                         const std::vector<name_entry>& entries,
                         size_t max_out) {
  std::vector<std::string> paths;
  paths.reserve(max_out);
  for (size_t i = 0; i < max_out; ++i) {
    paths.push_back(entries[i].path.string());
  }
  common::output::print_print0_paths(os, paths, max_out);
}

void print_results(std::ostream& os,
                   const NameMap& name_map,
                   bool print_max,
                   bool print_all,
                   bool use_print0,
                   bool sort_time,
                   bool sort_time_desc,
                   const std::optional<int>& limit) {
  auto entries = collect_output_entries(name_map, print_max, print_all);

  if (sort_time) {
    sort_entries_by_time(entries, sort_time_desc);
  }

  const size_t max_out = limit.has_value()
                             ? std::min<size_t>(*limit, entries.size())
                             : entries.size();

  if (use_print0) {
    print_print0_output(os, entries, max_out);
  } else {
    print_aligned_output(os, entries, max_out);
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  return common::args::run_tool(argc, argv,
      common::args::tool_descriptor{
          .description =
              "Extract Chinese name from brackets [...] in folder names.\n"
              "For duplicate names, display the path with the smallest\n"
              "total file size.",
          .has_time_sort = true,
          .has_limit = true,
          .has_show_mode = true,
          .has_print0 = true,
      },
      [](const common::args::parsed_args& args) -> int {
        const std::string path = args.directories[0].string();

        common::size_cache sc;
        NameMap name_map;
        try {
          name_map = collect_name_map(path, sc);
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
          print_results(std::cout, name_map,
                        args.show_max, args.show_all,
                        args.print0, args.sort_time, args.sort_time_desc,
                        args.limit);
        } else {
          std::cout << "No duplicate Chinese names found in: " << path << "\n";
        }

        return 0;
      });
}
