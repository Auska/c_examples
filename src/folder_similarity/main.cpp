#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==================== 配置结构 ====================

struct AppConfig {
  double threshold = 0.9;
  std::vector<std::string> directories;
  bool sort_time = false;
  bool sort_time_desc = false;   // 时间降序（默认升序）
  bool sort_size = false;
  bool sort_size_desc = false;   // 体积降序（默认升序）
};

// ==================== 函数声明 ====================

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]);
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

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;

  common::cli::parser parser("Compare folder names using Levenshtein distance.\n"
                             "If no directories are specified, the current directory is used.\n"
                             "Only pairs with similarity >= threshold are displayed.");
  parser.add_option({"--threshold", 's', "Set similarity threshold (0.0 ~ 1.0, default: 0.9)", true});
  parser.add_option({"--time", 't', "Sort by modification time (oldest first)", false});
  parser.add_option({"--time-reverse", 'T', "Sort by modification time (newest first)", false});
  parser.add_option({"--min", 'm', "Sort by folder size (smallest first)", false});
  parser.add_option({"--max", 'M', "Sort by folder size (largest first)", false});
  parser.add_positional("directory", "Directory to scan");

  auto parse_result = parser.parse(argc, argv);
  if (!parse_result) {
    if (parse_result.error() == common::cli::k_help_sentinel) {
      parser.print_usage(argv[0]);
      return std::unexpected(std::string(common::cli::k_help_sentinel));
    }
    return std::unexpected(parse_result.error());
  }

  // 处理阈值选项
  auto threshold_str = common::cli::parser::get_option(*parse_result, "--threshold");
  if (!threshold_str.empty()) {
    try {
      config.threshold = std::stod(std::string(threshold_str));
      if (config.threshold < 0.0 || config.threshold > 1.0) {
        return std::unexpected("Error: Similarity threshold must be between 0.0 and 1.0\n");
      }
    } catch (const std::invalid_argument&) {
      return std::unexpected("Error: Invalid threshold value: " + std::string(threshold_str) + "\n");
    } catch (const std::out_of_range&) {
      return std::unexpected("Error: Threshold value out of range: " + std::string(threshold_str) + "\n");
    }
  }

  // 处理排序选项
  if (common::cli::parser::has_option(*parse_result, "--time-reverse")) {
    config.sort_time = true;
    config.sort_time_desc = true;
  } else if (common::cli::parser::has_option(*parse_result, "--time")) {
    config.sort_time = true;
    config.sort_time_desc = false;
  }
  if (common::cli::parser::has_option(*parse_result, "--max")) {
    config.sort_size = true;
    config.sort_size_desc = true;
  } else if (common::cli::parser::has_option(*parse_result, "--min")) {
    config.sort_size = true;
    config.sort_size_desc = false;
  }

  // 处理位置参数
  for (const auto& dir : parse_result->positional) {
    config.directories.emplace_back(dir);
  }

  if (config.directories.empty()) {
    config.directories.emplace_back(".");
  }

  return config;
}

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
  // 预计算每个名称的字节长度，用于快速预过滤
  // （RapidFuzz 在字节级别计算距离，故使用 byte_len 做预过滤）
  std::vector<size_t> byte_lens;
  byte_lens.reserve(unique_names.size());
  for (const auto& name : unique_names) {
    byte_lens.push_back(name.size());
  }

  // 预过滤：计算长度差对应的最低相似度
  // 相似度 = 1 - distance/max_len，最低 distance = |len_diff|
  // 所以最低相似度 = 1 - |len_diff|/max_len
  // 如果最低相似度 < threshold，则这对不可能满足条件

  for (size_t i = 0; i < unique_names.size(); ++i) {
    for (size_t j = i + 1; j < unique_names.size(); ++j) {
      // 长度预过滤
      const size_t max_byte_len =
          std::max(byte_lens[i], byte_lens[j]);
      const size_t byte_diff =
          byte_lens[i] > byte_lens[j] ? byte_lens[i] - byte_lens[j]
                                      : byte_lens[j] - byte_lens[i];
      const double worst_sim =
          1.0 - static_cast<double>(byte_diff) / static_cast<double>(max_byte_len);
      if (worst_sim < threshold) {
        continue;  // 长度差太大，不可能相似
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
  std::vector<std::string> names;              // 层级模式
  std::vector<const path_info*> sorted;        // 扁平模式（已排序）
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

/// 对 group 的 path_info 进行排序（时间或体积）
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

/// 打印单行路径信息（带列对齐）
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

  // 输出
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
  const auto config_result = parse_args(argc, argv);
  if (!config_result) {
    if (config_result.error() == common::cli::k_help_sentinel) {
      return 0;
    }
    std::cerr << config_result.error();
    return 1;
  }
  const auto& config = *config_result;

  const auto name_to_paths = collect_folders(config.directories);

  std::vector<std::string> unique_names;
  for (const auto& name : std::views::keys(name_to_paths)) {
    unique_names.push_back(name);
  }

  if (unique_names.size() < 2) {
    std::cout << "Only " << unique_names.size()
              << " unique subfolder names found. Nothing to compare.\n";
    return 0;
  }

  std::ranges::sort(unique_names);

  // 构建名称到索引的映射，用于 O(1) 索引查找
  std::unordered_map<std::string, size_t> name_to_index;
  for (size_t i = 0; i < unique_names.size(); ++i) {
    name_to_index[unique_names[i]] = i;
  }

  common::similarity_cache sim_cache;
  common::UnionFind uf(unique_names.size());

  const auto groups = build_similarity_groups(
      unique_names, config.threshold, uf, sim_cache);

  bool found = false;
  for (const auto& group : std::views::values(groups)) {
    if (group.size() >= 2) {
      found = true;
      break;
    }
  }

  common::size_cache sc;
  if (found) {
    print_groups(std::cout, groups, name_to_index, name_to_paths, sim_cache,
                 sc, config.sort_time, config.sort_time_desc,
                 config.sort_size, config.sort_size_desc);
  } else {
    std::cout << "No folder name groups with similarity >= " << config.threshold
              << "\n";
  }

  return 0;
}
