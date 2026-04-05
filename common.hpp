#pragma once

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace common {

// 将字节数转换为人类可读格式
[[nodiscard]] inline std::string format_size(uintmax_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024 && unit_index < 4) {
    size /= 1024;
    unit_index++;
  }

  char buffer[32];
  if (unit_index == 0) {
    snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unit_index]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
  }
  return buffer;
}

// 格式化修改时间（线程安全版本）
// 使用更可靠的时间转换方法
[[nodiscard]] inline std::string format_time(fs::file_time_type ftime) {
  // 方法：使用 system_clock 的 to_time_t 转换
  // 先将 file_time_type 转换为 system_clock::time_point
  using namespace std::chrono;

  // 获取文件时间相对于 file_time_type epoch 的时长
  auto file_duration = ftime.time_since_epoch();

  // 获取当前两个时钟的差值并进行转换
  auto file_now = fs::file_time_type::clock::now();
  auto sys_now = system_clock::now();

  // 计算偏移量并转换
  auto elapsed = ftime - file_now;
  auto sys_time = sys_now + elapsed;

  std::time_t tt = system_clock::to_time_t(sys_time);
  std::tm tm_buf{};
  localtime_r(&tt, &tm_buf);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d+%H:%M:%S", &tm_buf);
  return buffer;
}

// 计算目录下所有文件的总大小
[[nodiscard]] inline uintmax_t calculate_total_size(const fs::path &dir_path) {
  uintmax_t total_size = 0;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        try {
          total_size += entry.file_size();
        } catch (const fs::filesystem_error &) {
          // 跳过无法获取大小的文件
        }
      }
    }
  } catch (const fs::filesystem_error &) {
    // 跳过无法读取的目录
  }
  return total_size;
}

// 计算目录大小（带缓存版本，避免重复计算）
// 使用方法：传入一个静态的 unordered_map 作为缓存
[[nodiscard]] inline uintmax_t calculate_total_size_cached(
    const fs::path &dir_path,
    std::unordered_map<std::string, uintmax_t> &cache) {
  // 规范化路径作为缓存键
  std::string key;
  try {
    key = fs::canonical(dir_path).string();
  } catch (...) {
    key = dir_path.string();
  }

  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  uintmax_t total_size = calculate_total_size(dir_path);
  cache[key] = total_size;
  return total_size;
}

// 从文件夹名称中提取最后一对中括号内的内容
[[nodiscard]] inline std::string extract_bracket_content(
    const std::string &folder_name) {
  size_t end = folder_name.rfind(']');
  if (end == std::string::npos) {
    return "";
  }
  size_t start = folder_name.rfind('[', end);
  if (start == std::string::npos) {
    return "";
  }
  return folder_name.substr(start + 1, end - start - 1);
}

// ==================== Levenshtein 距离算法 ====================

// 计算 Levenshtein 距离（优化空间复杂度为 O(min(n,m))）
[[nodiscard]] inline size_t levenshtein_distance(const std::string &s1,
                                                 const std::string &s2) {
  // 确保 s1 是较短的字符串，以最小化空间使用
  const std::string *shorter = &s1;
  const std::string *longer = &s2;
  if (s1.size() > s2.size()) {
    std::swap(shorter, longer);
  }

  size_t n = shorter->size();
  size_t m = longer->size();

  // 只使用两行 DP 数组
  std::vector<size_t> prev_row(n + 1);
  std::vector<size_t> curr_row(n + 1);

  // 初始化第一行
  for (size_t i = 0; i <= n; ++i) {
    prev_row[i] = i;
  }

  // 填充 DP 表
  for (size_t j = 1; j <= m; ++j) {
    curr_row[0] = j;
    for (size_t i = 1; i <= n; ++i) {
      if ((*shorter)[i - 1] == (*longer)[j - 1]) {
        curr_row[i] = prev_row[i - 1];
      } else {
        curr_row[i] =
            1 + std::min({prev_row[i], curr_row[i - 1], prev_row[i - 1]});
      }
    }
    std::swap(prev_row, curr_row);
  }

  return prev_row[n];
}

// 计算 Levenshtein 相似度 (0.0 ~ 1.0)
[[nodiscard]] inline double levenshtein_similarity(const std::string &a,
                                                   const std::string &b) {
  if (a.empty() && b.empty()) {
    return 1.0;
  }
  if (a.empty() || b.empty()) {
    return 0.0;
  }
  size_t distance = levenshtein_distance(a, b);
  size_t max_len = std::max(a.size(), b.size());
  return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

// 用于缓存相似度计算的哈希函数
struct PairHash {
  size_t operator()(const std::pair<size_t, size_t> &p) const {
    return (p.first * 31) + p.second;
  }
};

// 带缓存的相似度计算
[[nodiscard]] inline double levenshtein_similarity_cached(
    size_t i, size_t j, const std::string &a, const std::string &b,
    std::unordered_map<std::pair<size_t, size_t>, double, PairHash> &cache) {
  // 确保 i < j 以便缓存键一致
  if (i > j) {
    std::swap(i, j);
  }
  auto key = std::make_pair(i, j);

  auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  double sim = levenshtein_similarity(a, b);
  cache[key] = sim;
  return sim;
}

}  // namespace common
