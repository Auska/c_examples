#include "common/common.hpp"

#include <getopt.h>

#include <algorithm>
#include <filesystem>
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

void print_usage(const char* program_name);
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
    double threshold,
    common::size_cache& sc);

// ==================== 函数实现 ====================

void print_usage(const char* program_name) {
  std::cout << "Usage: " << program_name
            << " [OPTIONS] [directory1 [directory2 ...]]\n"
            << "\nOptions:\n"
            << "  -s <threshold>  Set similarity threshold (0.0 ~ 1.0, "
               "default: 0.9)\n"
            << "  -h              Show this help message\n"
            << "\nDescription:\n"
            << "  Compare folder names in the specified directories using "
               "Levenshtein distance.\n"
            << "  If no directories are specified, the current directory "
               "is used.\n"
            << "  Only pairs with similarity >= threshold are displayed.\n";
}

[[nodiscard]] std::expected<AppConfig, std::string> parse_args(int argc,
                                                               char* argv[]) {
  AppConfig config;
  int opt = 0;

  while ((opt = getopt(argc, argv, "s:h")) != -1) {
    if (opt == 's') {
      try {
        config.threshold = std::stod(optarg);
        if (config.threshold < 0.0 || config.threshold > 1.0) {
          return std::unexpected(
              "Error: Similarity threshold must be between 0.0 and 1.0\n");
        }
      } catch (const std::invalid_argument&) {
        return std::unexpected(
            "Error: Invalid threshold value: " + std::string(optarg) + "\n");
      } catch (const std::out_of_range&) {
        return std::unexpected(
            "Error: Threshold value out of range: " + std::string(optarg) +
            "\n");
      }
    } else if (opt == 'h') {
      return std::unexpected("HELP");
    } else {
      return std::unexpected(
          "Error: Unknown option '" + std::string(1, static_cast<char>(optopt)) +
          "'\nUse '" + argv[0] + " -h' for help.\n");
    }
  }

  for (int i = optind; i < argc; ++i) {
    config.directories.emplace_back(argv[i]);
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
    size_t len = 0;
    for (size_t pos = 0; pos < name.size();) {
      const auto b0 = static_cast<uint8_t>(name[pos]);
      if (b0 < 0x80) {
        pos += 1;
      } else if ((b0 & 0xE0) == 0xC0) {
        pos += 2;
      } else if ((b0 & 0xF0) == 0xE0) {
        pos += 3;
      } else if ((b0 & 0xF8) == 0xF0) {
        pos += 4;
      } else {
        pos += 1;
      }
      ++len;
    }
    cp_lengths.push_back(len);
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

      // 传入 min_similarity 以启用 Levenshtein 提前终止
      const double sim =
          sim_cache.get(i, j, unique_names[i], unique_names[j], threshold);
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
    double threshold,
    common::size_cache& sc) {
  bool found = false;

  for (const auto& group : std::views::values(groups)) {
    if (group.size() < 2) {
      continue;
    }

    found = true;
    double min_sim = 1.0;
    for (size_t i = 0; i < group.size(); ++i) {
      for (size_t j = i + 1; j < group.size(); ++j) {
        const size_t idx1 = name_to_index.at(group[i]);
        const size_t idx2 = name_to_index.at(group[j]);
        // 使用 find 避免双重查找
        const double* cached = sim_cache.find(idx1, idx2);
        const double sim =
            cached ? *cached
                   : common::levenshtein_similarity(group[i], group[j], 0.0);
        min_sim = std::min(min_sim, sim);
      }
    }
    os << "Group (min similarity: " << min_sim << "):\n";

    for (const auto& name : group) {
      os << "  \"" << name << "\":\n";
      for (const auto& path : name_to_paths.at(name)) {
        try {
          const auto mtime = fs::last_write_time(path);
          const std::uintmax_t size = sc.get(path);
          os << "    \"" << path.string() << "\" "
             << common::format_time(mtime) << " " << common::format_size(size)
             << "\n";
        } catch (const fs::filesystem_error& e) {
          os << "    \"" << path.string() << "\" (error: " << e.what()
             << ")\n";
        }
      }
    }
    os << "\n";
  }

  if (!found) {
    os << "No folder name groups with similarity >= " << threshold << "\n";
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
                 config.threshold, sc);
  } else {
    std::cout << "No folder name groups with similarity >= " << config.threshold
              << "\n";
  }

  return 0;
}
