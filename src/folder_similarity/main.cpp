#include <getopt.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/common.hpp"

namespace fs = std::filesystem;

// ==================== Union-Find 数据结构 ====================

class UnionFind {
  std::vector<size_t> parent_;
  std::vector<size_t> rank_;

 public:
  explicit UnionFind(size_t n) : parent_(n), rank_(n, 0) {
    for (size_t i = 0; i < n; ++i) {
      parent_[i] = i;
    }
  }

  [[nodiscard]] size_t find(size_t x) {
    if (parent_[x] != x) {
      parent_[x] = find(parent_[x]);
    }
    return parent_[x];
  }

  void unite(size_t x, size_t y) {
    size_t px = find(x);
    size_t py = find(y);
    if (px == py) {
      return;
    }

    if (rank_[px] < rank_[py]) {
      parent_[px] = py;
    } else if (rank_[px] > rank_[py]) {
      parent_[py] = px;
    } else {
      parent_[py] = px;
      rank_[px]++;
    }
  }
};

// ==================== 配置结构 ====================

struct AppConfig {
  double threshold = 0.9;
  std::vector<std::string> directories;
};

// ==================== 函数声明 ====================

void print_usage(const char *program_name);
AppConfig parse_args(int argc, char *argv[]);
std::unordered_map<std::string, std::vector<fs::path>> collect_folders(
    const std::vector<std::string> &dir_paths);
std::unordered_map<size_t, std::vector<std::string>> build_similarity_groups(
    const std::vector<std::string> &unique_names,
    double threshold,
    UnionFind &uf,
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        &similarity_cache);
void print_groups(
    const std::unordered_map<size_t, std::vector<std::string>> &groups,
    const std::unordered_map<std::string, size_t> &name_to_index,
    const std::unordered_map<std::string, std::vector<fs::path>> &name_to_paths,
    const std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        &similarity_cache,
    double threshold);

// ==================== 函数实现 ====================

void print_usage(const char *program_name) {
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

AppConfig parse_args(int argc, char *argv[]) {
  AppConfig config;
  int opt = 0;

  while ((opt = getopt(argc, argv, "s:h")) != -1) {
    if (opt == 's') {
      try {
        config.threshold = std::stod(optarg);
        if (config.threshold < 0.0 || config.threshold > 1.0) {
          std::cerr
              << "Error: Similarity threshold must be between 0.0 and 1.0\n";
          std::exit(1);
        }
      } catch (...) {
        std::cerr << "Error: Invalid threshold value: " << optarg << "\n";
        std::exit(1);
      }
    } else if (opt == 'h') {
      print_usage(argv[0]);
      std::exit(0);
    } else {
      std::cerr << "Error: Unknown option '" << static_cast<char>(optopt)
                << "'\n";
      std::cerr << "Use '" << argv[0] << " -h' for help.\n";
      std::exit(1);
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
    const std::vector<std::string> &dir_paths) {
  std::unordered_map<std::string, std::vector<fs::path>> name_to_paths;

  for (const auto &dir_path : dir_paths) {
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
    } catch (const fs::filesystem_error &e) {
      std::cerr << "Warning: Cannot get absolute path for: " << dir_path
                << " -> " << e.what() << "\n";
      continue;
    }

    try {
      for (const auto &entry : fs::directory_iterator(dir_path)) {
        if (entry.is_directory()) {
          fs::path name = entry.path().filename();
          fs::path full_path = abs_parent / name;
          name_to_paths[name.string()].push_back(full_path);
        }
      }
    } catch (const fs::filesystem_error &e) {
      std::cerr << "Warning: Error reading directory: " << dir_path << " -> "
                << e.what() << "\n";
    }
  }

  return name_to_paths;
}

std::unordered_map<size_t, std::vector<std::string>> build_similarity_groups(
    const std::vector<std::string> &unique_names,
    double threshold,
    UnionFind &uf,
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        &similarity_cache) {
  for (size_t i = 0; i < unique_names.size(); ++i) {
    for (size_t j = i + 1; j < unique_names.size(); ++j) {
      double sim = common::levenshtein_similarity_cached(
          i, j, unique_names[i], unique_names[j], similarity_cache);
      if (sim >= threshold) {
        uf.unite(i, j);
      }
    }
  }

  std::unordered_map<size_t, std::vector<std::string>> groups;
  for (size_t i = 0; i < unique_names.size(); ++i) {
    size_t root = uf.find(i);
    groups[root].push_back(unique_names[i]);
  }

  return groups;
}

void print_groups(
    const std::unordered_map<size_t, std::vector<std::string>> &groups,
    const std::unordered_map<std::string, size_t> &name_to_index,
    const std::unordered_map<std::string, std::vector<fs::path>> &name_to_paths,
    const std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        &similarity_cache,
    double threshold) {
  bool found = false;

  for (const auto &group : std::views::values(groups)) {
    if (group.size() < 2) {
      continue;
    }

    found = true;
    double min_sim = 1.0;
    for (size_t i = 0; i < group.size(); ++i) {
      for (size_t j = i + 1; j < group.size(); ++j) {
        size_t idx1 = name_to_index.at(group[i]);
        size_t idx2 = name_to_index.at(group[j]);
        size_t lo = std::min(idx1, idx2);
        size_t hi = std::max(idx1, idx2);
        double sim = similarity_cache.contains({lo, hi})
                         ? similarity_cache.at({lo, hi})
                         : common::levenshtein_similarity(group[i], group[j]);
        min_sim = std::min(min_sim, sim);
      }
    }
    std::cout << "Group (min similarity: " << min_sim << "):\n";

    for (const auto &name : group) {
      std::cout << "  \"" << name << "\":\n";
      for (const auto &path : name_to_paths.at(name)) {
        try {
          auto mtime = fs::last_write_time(path);
          std::uintmax_t size = common::calculate_total_size(path);
          std::cout << "    \"" << path.string() << "\" "
                    << common::format_time(mtime) << " "
                    << common::format_size(size) << "\n";
        } catch (const fs::filesystem_error &e) {
          std::cout << "    \"" << path.string() << "\" (error: " << e.what()
                    << ")\n";
        }
      }
    }
    std::cout << "\n";
  }

  if (!found) {
    std::cout << "No folder name groups with similarity >= " << threshold
              << "\n";
  }
}

// ==================== main ====================

int main(int argc, char *argv[]) {
  AppConfig config = parse_args(argc, argv);

  auto name_to_paths = collect_folders(config.directories);

  std::vector<std::string> unique_names;
  for (const auto &name : std::views::keys(name_to_paths)) {
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

  std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
      similarity_cache;
  UnionFind uf(unique_names.size());

  auto groups = build_similarity_groups(unique_names, config.threshold, uf,
                                         similarity_cache);

  bool found = false;
  for (const auto &group : std::views::values(groups)) {
    if (group.size() >= 2) {
      found = true;
      break;
    }
  }

  if (found) {
    print_groups(groups, name_to_index, name_to_paths, similarity_cache,
                 config.threshold);
  } else {
    std::cout << "No folder name groups with similarity >= " << config.threshold
              << "\n";
  }

  return 0;
}
