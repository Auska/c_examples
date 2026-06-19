#include "common/common.hpp"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

// ==================== 配置结构 ====================

struct AppConfig {
  double threshold = 0.9;
  std::vector<std::string> directories;
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
    common::size_cache& sc);

// ==================== 函数实现 ====================

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;
  
  common::cli::parser parser("Compare folder names using Levenshtein distance.\n" 
                             "If no directories are specified, the current directory is used.\n" 
                             "Only pairs with similarity >= threshold are displayed.");
  parser.add_option({"--threshold", 's', "Set similarity threshold (0.0 ~ 1.0, default: 0.9)", true});
  parser.add_positional("directory", "Directory to scan");
  
  auto parse_result = parser.parse(argc, argv);
  if (!parse_result) {
    if (parse_result.error() == "HELP") {
      parser.print_usage(argv[0]);
      return std::unexpected("HELP");
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
  // 预计算每个名称的 UTF-8 码点长度，用于快速预过滤
  std::vector<size_t> cp_lengths;
  cp_lengths.reserve(unique_names.size());
  for (const auto& name : unique_names) {
    cp_lengths.push_back(common::utf8_codepoint_length(name));
  }

  // 预过滤：计算长度差对应的最低相似度
  // 相似度 = 1 - distance/max_len，最低 distance = |len_diff|
  // 所以最低相似度 = 1 - |len_diff|/max_len
  // 如果最低相似度 < threshold，则这对不可能满足条件

  for (size_t i = 0; i < unique_names.size(); ++i) {
    for (size_t j = i + 1; j < unique_names.size(); ++j) {
      // 长度预过滤
      const size_t max_cp_len =
          std::max(cp_lengths[i], cp_lengths[j]);
      const size_t cp_diff =
          cp_lengths[i] > cp_lengths[j] ? cp_lengths[i] - cp_lengths[j]
                                        : cp_lengths[j] - cp_lengths[i];
      const double worst_sim =
          1.0 - static_cast<double>(cp_diff) / static_cast<double>(max_cp_len);
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

void print_groups(
    std::ostream& os,
    const std::unordered_map<size_t, std::vector<std::string>>& groups,
    const std::unordered_map<std::string, size_t>& name_to_index,
    const std::unordered_map<std::string, std::vector<fs::path>>& name_to_paths,
    common::similarity_cache& sim_cache,
    common::size_cache& sc) {
  // 预格式化路径信息，缓存 name → 路径行 的映射
  struct path_info {
    std::string path_str;
    std::string time_str;
    std::string size_str;
    std::string error_msg;  // 非空表示错误
  };
  std::unordered_map<std::string, std::vector<path_info>> name_to_info;

  struct group_out {
    double min_sim;
    std::vector<std::string> names;
  };
  std::vector<group_out> printable;

  size_t max_time_w = 0;
  size_t max_size_w = 0;

  for (const auto& group : std::views::values(groups)) {
    if (group.size() < 2) continue;

    // 计算组内最低相似度
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

    printable.push_back({min_sim, group});

    // 收集路径信息并计算列宽（每个 name 只处理一次）
    for (const auto& name : group) {
      if (name_to_info.contains(name)) continue;

      auto& infos = name_to_info[name];
      for (const auto& path : name_to_paths.at(name)) {
        path_info pi;
        pi.path_str = path.string();
        try {
          const auto mtime = fs::last_write_time(path);
          const std::uintmax_t size = sc.get(path);
          pi.time_str = common::format_time(mtime);
          pi.size_str = common::format_size(size);
          max_time_w = std::max(max_time_w, pi.time_str.size());
          max_size_w = std::max(max_size_w, pi.size_str.size());
        } catch (const fs::filesystem_error& e) {
          pi.error_msg = e.what();
        }
        infos.push_back(std::move(pi));
      }
    }
  }

  // 对齐输出
  for (const auto& g : printable) {
    os << "Group (min similarity: " << std::fixed << std::setprecision(2)
       << g.min_sim << "):\n";
    for (const auto& name : g.names) {
      os << "  \"" << name << "\":\n";
      for (const auto& pi : name_to_info[name]) {
        if (!pi.error_msg.empty()) {
          os << "    \"" << pi.path_str << "\" (error: " << pi.error_msg
             << ")\n";
        } else {
          os << "    \"" << pi.path_str << "\" "
             << std::right << std::setw(static_cast<int>(max_time_w))
             << pi.time_str << " "
             << std::right << std::setw(static_cast<int>(max_size_w))
             << pi.size_str << "\n";
        }
      }
    }
    os << "\n";
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
                 sc);
  } else {
    std::cout << "No folder name groups with similarity >= " << config.threshold
              << "\n";
  }

  return 0;
}
