#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==================== 函数声明 ====================

std::unordered_map<std::string, std::vector<fs::path>> collect_folders(
    const std::vector<std::string>& dir_paths);
std::unordered_map<size_t, std::vector<std::string>> build_similarity_groups(
    const std::vector<std::string>& unique_names,
    double threshold,
    common::UnionFind& uf,
    common::similarity_cache& sim_cache);
void print_groups(
    std::ostream& os,
    const std::unordered_map<size_t, std::vector<std::string>>& groups,
    const std::unordered_map<std::string, size_t>& name_to_index,
    const std::unordered_map<std::string, std::vector<fs::path>>& name_to_paths,
    common::similarity_cache& sim_cache,
    common::size_cache& sc,
    bool sort_time,
    bool sort_time_desc,
    bool sort_size,
    bool sort_size_desc);

// ==================== 函数实现 ====================

std::unordered_map<std::string, std::vector<fs::path>> collect_folders(
    const std::vector<std::string>& dir_paths) {
  std::unordered_map<std::string, std::vector<fs::path>> name_to_paths;

  for (const auto& dir_path : dir_paths) {
    if (!fs::exists(dir_path)) {
      std::cerr << "Warning: Directory does not exist: " << dir_path << "\n";
      continue;
    }

    if (!fs::is_directory(dir_path)) {
      std::cerr << "Warning: Not a directory (skipped): " << dir_path << "\n";
      continue;
    }

    fs::path abs_parent;
    try {
      abs_parent = fs::absolute(dir_path);
    } catch (const fs::filesystem_error& e) {
      std::cerr << "Warning: Cannot get absolute path for: " << dir_path
                << " -> " << e.what() << "\n";
      continue;
    }

    try {
      for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_directory()) {
          const fs::path name = entry.path().filename();
          const fs::path full_path = abs_parent / name;
          name_to_paths[name.string()].push_back(full_path);
        }
      }
    } catch (const fs::filesystem_error& e) {
      std::cerr << "Warning: Error reading directory: " << dir_path << " -> "
                << e.what() << "\n";
    }
  }

  return name_to_paths;
}

std::unordered_map<size_t, std::vector<std::string>> build_similarity_groups(
    const std::vector<std::string>& unique_names,
    double threshold,
    common::UnionFind& uf,
    common::similarity_cache& sim_cache) {
  std::vector<size_t> byte_lens;
  byte_lens.reserve(unique_names.size());
  for (const auto& name : unique_names) {
    byte_lens.push_back(name.size());
  }

  for (size_t i = 0; i < unique_names.size(); ++i) {
    for (size_t j = i + 1; j < unique_names.size(); ++j) {
      const size_t max_byte_len =
          std::max(byte_lens[i], byte_lens[j]);
      const size_t byte_diff =
          byte_lens[i] > byte_lens[j] ? byte_lens[i] - byte_lens[j]
                                      : byte_lens[j] - byte_lens[i];
      const double worst_sim =
          1.0 - static_cast<double>(byte_diff) / static_cast<double>(max_byte_len);
      if (worst_sim < threshold) {
        continue;
      }

      const double sim =
          sim_cache.get(i, j, unique_names[i], unique_names[j]);
      if (sim >= threshold) {
        uf.unite(i, j);
      }
    }
  }

  std::unordered_map<size_t, std::vector<std::string>> groups;
  for (size_t i = 0; i < unique_names.size(); ++i) {
    const size_t root = uf.find(i);
    groups[root].push_back(unique_names[i]);
  }

  return groups;
}

// ==================== 输出结构与辅助函数 ====================

/// 单个路径的展示信息
struct path_info {
  std::string path_str;
  std::string time_str;
  std::string size_str;
  std::string error_msg;
  fs::file_time_type mtime;
  std::uintmax_t raw_size = 0;
};

/// 一个相似组的输出数据
struct group_out {
  double min_sim;
  std::vector<std::string> names;
  std::vector<const path_info*> sorted;
};

/// 收集组内路径信息到 name_to_info 缓存，并更新最大列宽
void collect_group_path_infos(
    const std::vector<std::string>& group,
    const std::unordered_map<std::string, std::vector<fs::path>>& name_to_paths,
    std::unordered_map<std::string, std::vector<path_info>>& name_to_info,
    size_t& max_time_w,
    size_t& max_size_w,
    common::size_cache& sc) {
  for (const auto& name : group) {
    if (name_to_info.contains(name)) continue;

    auto& infos = name_to_info[name];
    for (const auto& path : name_to_paths.at(name)) {
      path_info pi;
      pi.path_str = path.string();
      try {
        pi.mtime = fs::last_write_time(path);
        pi.raw_size = sc.get(path);
        pi.time_str = common::format_time(pi.mtime);
        pi.size_str = common::format_size(pi.raw_size);
        common::output::update_max_width(max_time_w, pi.time_str);
        common::output::update_max_width(max_size_w, pi.size_str);
      } catch (const fs::filesystem_error& e) {
        pi.error_msg = e.what();
      }
      infos.push_back(std::move(pi));
    }
  }
}

/// 计算组内最低相似度
[[nodiscard]] double compute_min_similarity(
    const std::vector<std::string>& group,
    const std::unordered_map<std::string, size_t>& name_to_index,
    common::similarity_cache& sim_cache) {
  double min_sim = 1.0;
  for (size_t i = 0; i < group.size(); ++i) {
    for (size_t j = i + 1; j < group.size(); ++j) {
      const size_t idx1 = name_to_index.at(group[i]);
      const size_t idx2 = name_to_index.at(group[j]);
      const double* cached = sim_cache.find(idx1, idx2);
      const double sim =
          cached ? *cached
                 : common::levenshtein_similarity(group[i], group[j], 0.0);
      min_sim = std::min(min_sim, sim);
    }
  }
  return min_sim;
}

/// 对 group 的 path_info 进行排序
void sort_group_entries(
    std::vector<const path_info*>& sorted,
    bool sort_time,
    bool sort_time_desc,
    bool sort_size_desc) {
  if (sort_time) {
    std::ranges::sort(sorted,
                      [sort_time_desc](const path_info* a,
                                       const path_info* b) {
                        return sort_time_desc
                                   ? a->mtime > b->mtime
                                   : a->mtime < b->mtime;
                      });
  } else {
    std::ranges::sort(sorted,
                      [sort_size_desc](const path_info* a,
                                       const path_info* b) {
                        return sort_size_desc
                                   ? a->raw_size > b->raw_size
                                   : a->raw_size < b->raw_size;
                      });
  }
}

/// 打印单行路径信息
void print_path_line(std::ostream& os,
                     const path_info& pi,
                     size_t max_time_w,
                     size_t max_size_w) {
  if (!pi.error_msg.empty()) {
    common::output::print_error_line(os, pi.path_str, pi.error_msg,
                                     max_time_w, max_size_w);
  } else {
    common::output::print_column_line(os, pi.time_str, pi.size_str,
                                      pi.path_str, max_time_w, max_size_w);
  }
}

/// 以扁平模式输出组
void print_group_flat(std::ostream& os,
                      const group_out& g,
                      size_t max_time_w,
                      size_t max_size_w) {
  os << "Group (min similarity: " << std::fixed << std::setprecision(2)
     << g.min_sim << "):\n";
  for (const auto* pi : g.sorted) {
    print_path_line(os, *pi, max_time_w, max_size_w);
  }
  os << "\n";
}

/// 以层级模式输出组
void print_group_hierarchical(
    std::ostream& os,
    const group_out& g,
    const std::unordered_map<std::string, std::vector<path_info>>& name_to_info,
    size_t max_time_w,
    size_t max_size_w) {
  os << "Group (min similarity: " << std::fixed << std::setprecision(2)
     << g.min_sim << "):\n";
  for (const auto& name : g.names) {
    os << "  \"" << name << "\":\n";
    for (const auto& pi : name_to_info.at(name)) {
      print_path_line(os, pi, max_time_w, max_size_w);
    }
  }
  os << "\n";
}

void print_groups(
    std::ostream& os,
    const std::unordered_map<size_t, std::vector<std::string>>& groups,
    const std::unordered_map<std::string, size_t>& name_to_index,
    const std::unordered_map<std::string, std::vector<fs::path>>& name_to_paths,
    common::similarity_cache& sim_cache,
    common::size_cache& sc,
    bool sort_time,
    bool sort_time_desc,
    bool sort_size,
    bool sort_size_desc) {
  const bool use_flat = (sort_time || sort_size);
  size_t max_time_w = 0;
  size_t max_size_w = 0;
  std::unordered_map<std::string, std::vector<path_info>> name_to_info;
  std::vector<group_out> printable;

  for (const auto& group : std::views::values(groups)) {
    if (group.size() < 2) continue;

    group_out go;
    go.min_sim = compute_min_similarity(group, name_to_index, sim_cache);
    collect_group_path_infos(group, name_to_paths, name_to_info,
                             max_time_w, max_size_w, sc);

    if (use_flat) {
      for (const auto& name : group) {
        for (const auto& pi : name_to_info[name]) {
          go.sorted.push_back(&pi);
        }
      }
      sort_group_entries(go.sorted, sort_time, sort_time_desc,
                         sort_size_desc);
    } else {
      go.names = group;
    }

    printable.push_back(std::move(go));
  }

  for (const auto& g : printable) {
    if (use_flat) {
      print_group_flat(os, g, max_time_w, max_size_w);
    } else {
      print_group_hierarchical(os, g, name_to_info, max_time_w, max_size_w);
    }
  }
}

// ==================== main ====================

int main(int argc, char* argv[]) {
  return common::args::run_tool(argc, argv,
      common::args::tool_descriptor{
          .description = "Compare folder names using Levenshtein distance.\n"
                         "If no directories are specified, the current "
                         "directory is used.\n"
                         "Only pairs with similarity >= threshold are "
                         "displayed.",
          .has_threshold = true,
          .has_size_sort = true,
          .has_time_sort = true,
          .supports_multiple_dirs = true,
      },
      [](const common::args::parsed_args& args) -> int {
        const double threshold = args.threshold.value_or(0.9);

        // Convert paths to strings for the collector
        std::vector<std::string> dir_strs;
        dir_strs.reserve(args.directories.size());
        for (const auto& p : args.directories) {
          dir_strs.push_back(p.string());
        }

        const auto name_to_paths = collect_folders(dir_strs);

        std::vector<std::string> unique_names;
        for (const auto& name : std::views::keys(name_to_paths)) {
          unique_names.push_back(name);
        }

        if (unique_names.size() < 2) {
          std::cout << "Only " << unique_names.size()
                    << " unique subfolder names found. "
                       "Nothing to compare.\n";
          return 0;
        }

        std::ranges::sort(unique_names);

        std::unordered_map<std::string, size_t> name_to_index;
        for (size_t i = 0; i < unique_names.size(); ++i) {
          name_to_index[unique_names[i]] = i;
        }

        common::similarity_cache sim_cache;
        common::UnionFind uf(unique_names.size());

        const auto groups = build_similarity_groups(
            unique_names, threshold, uf, sim_cache);

        bool found = false;
        for (const auto& group : std::views::values(groups)) {
          if (group.size() >= 2) {
            found = true;
            break;
          }
        }

        common::size_cache sc;
        if (found) {
          print_groups(std::cout, groups, name_to_index, name_to_paths,
                       sim_cache, sc,
                       args.sort_time, args.sort_time_desc,
                       args.sort_size, args.sort_size_desc);
        } else {
          std::cout << "No folder name groups with similarity >= "
                    << threshold << "\n";
        }

        return 0;
      });
}
