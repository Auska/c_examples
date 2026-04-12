#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace common {

/// 计算 Levenshtein 距离（优化空间复杂度为 O(min(n,m))）
[[nodiscard]] inline size_t levenshtein_distance(std::string_view s1,
                                                 std::string_view s2) {
  const bool swap_needed = s1.size() > s2.size();
  const auto& shorter = swap_needed ? s2 : s1;
  const auto& longer = swap_needed ? s1 : s2;

  const size_t n = shorter.size();
  const size_t m = longer.size();

  std::vector<size_t> prev_row(n + 1);
  std::vector<size_t> curr_row(n + 1);

  for (size_t i = 0; i <= n; ++i) {
    prev_row[i] = i;
  }

  for (size_t j = 1; j <= m; ++j) {
    curr_row[0] = j;
    for (size_t i = 1; i <= n; ++i) {
      if (shorter[i - 1] == longer[j - 1]) {
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

/// 计算 Levenshtein 相似度 (0.0 ~ 1.0)
[[nodiscard]] inline double levenshtein_similarity(std::string_view a,
                                                   std::string_view b) {
  if (a.empty() && b.empty()) {
    return 1.0;
  }
  if (a.empty() || b.empty()) {
    return 0.0;
  }
  const size_t distance = levenshtein_distance(a, b);
  const size_t max_len = std::max(a.size(), b.size());
  return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

/// 用于缓存相似度计算的哈希函数
struct PairHash {
  [[nodiscard]] size_t operator()(
      const std::pair<size_t, size_t>& p) const noexcept {
    return (p.first * 31) + p.second;
  }
};

/// 带缓存的相似度计算
[[nodiscard]] inline double levenshtein_similarity_cached(
    size_t i,
    size_t j,
    std::string_view a,
    std::string_view b,
    std::unordered_map<std::pair<size_t, size_t>, double, PairHash>& cache) {
  if (i > j) {
    std::swap(i, j);
  }
  const auto key = std::make_pair(i, j);

  const auto it = cache.find(key);
  if (it != cache.end()) {
    return it->second;
  }

  const double sim = levenshtein_similarity(a, b);
  cache[key] = sim;
  return sim;
}

}  // namespace common
