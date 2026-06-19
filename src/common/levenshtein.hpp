#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace common {

// ==================== UTF-8 解码工具 ====================

/// 解码一个 UTF-8 码点，返回 {码点, 下一个字节位置}
[[nodiscard]] inline std::pair<char32_t, size_t> decode_utf8(
    std::string_view sv,
    size_t pos) noexcept {
  if (pos >= sv.size()) {
    return {0, pos + 1};
  }
  const auto b0 = static_cast<uint8_t>(sv[pos]);

  // ASCII
  if (b0 < 0x80) {
    return {static_cast<char32_t>(b0), pos + 1};
  }

  // 多字节序列
  char32_t cp = 0;
  size_t len = 0;

  if ((b0 & 0xE0) == 0xC0) {
    cp = b0 & 0x1F;
    len = 2;
  } else if ((b0 & 0xF0) == 0xE0) {
    cp = b0 & 0x0F;
    len = 3;
  } else if ((b0 & 0xF8) == 0xF0) {
    cp = b0 & 0x07;
    len = 4;
  } else {
    // 非法 UTF-8 前导字节，当作单字节处理
    return {static_cast<char32_t>(b0), pos + 1};
  }

  if (pos + len > sv.size()) {
    return {static_cast<char32_t>(b0), pos + 1};
  }

  for (size_t i = 1; i < len; ++i) {
    const auto b = static_cast<uint8_t>(sv[pos + i]);
    if ((b & 0xC0) != 0x80) {
      return {static_cast<char32_t>(b0), pos + 1};
    }
    cp = (cp << 6) | (b & 0x3F);
  }

  return {cp, pos + len};
}

/// 将 UTF-8 字符串解码为码点向量
[[nodiscard]] inline std::vector<char32_t> utf8_to_codepoints(
    std::string_view sv) {
  std::vector<char32_t> result;
  result.reserve(sv.size() / 2);  // 粗略估计
  size_t pos = 0;
  while (pos < sv.size()) {
    const auto [cp, next] = decode_utf8(sv, pos);
    result.push_back(cp);
    pos = next;
  }
  return result;
}

/// 计算 UTF-8 字符串中的码点数量（不分配内存）
[[nodiscard]] inline size_t utf8_codepoint_length(
    std::string_view sv) noexcept {
  size_t len = 0;
  size_t pos = 0;
  while (pos < sv.size()) {
    const auto [cp, next] = decode_utf8(sv, pos);
    pos = next;
    ++len;
  }
  return len;
}

// ==================== Levenshtein 距离 ====================

/// 计算两个码点向量之间的 Levenshtein 距离（内部实现）
/// 支持提前终止：当距离超过 max_distance 时立即返回
[[nodiscard]] inline size_t levenshtein_distance(
    const std::vector<char32_t>& cp1,
    const std::vector<char32_t>& cp2,
    size_t max_distance = SIZE_MAX) {
  const bool swap_needed = cp1.size() > cp2.size();
  const auto& shorter = swap_needed ? cp2 : cp1;
  const auto& longer = swap_needed ? cp1 : cp2;

  const size_t n = shorter.size();
  const size_t m = longer.size();

  // 长度差即为最小可能距离
  const size_t len_diff = m - n;
  if (len_diff > max_distance) {
    return len_diff;  // 必然超过阈值
  }

  std::vector<size_t> prev_row(n + 1);
  std::vector<size_t> curr_row(n + 1);

  for (size_t i = 0; i <= n; ++i) {
    prev_row[i] = i;
  }

  for (size_t j = 1; j <= m; ++j) {
    curr_row[0] = j;
    size_t row_min = j;  // 当前行最小值，用于提前终止

    for (size_t i = 1; i <= n; ++i) {
      if (shorter[i - 1] == longer[j - 1]) {
        curr_row[i] = prev_row[i - 1];
      } else {
        curr_row[i] =
            1 + std::min({prev_row[i], curr_row[i - 1], prev_row[i - 1]});
      }
      if (curr_row[i] < row_min) {
        row_min = curr_row[i];
      }
    }

    // 提前终止：如果当前行最小值已超过 max_distance，最终距离只会更大
    if (row_min > max_distance) {
      return row_min;
    }

    std::swap(prev_row, curr_row);
  }

  return prev_row[n];
}

/// 计算 Levenshtein 距离（码点级，UTF-8 语义正确）
/// 支持提前终止：当距离超过 max_distance 时立即返回
[[nodiscard]] inline size_t levenshtein_distance(
    std::string_view s1,
    std::string_view s2,
    size_t max_distance = SIZE_MAX) {
  return levenshtein_distance(utf8_to_codepoints(s1), utf8_to_codepoints(s2),
                              max_distance);
}

/// 计算 Levenshtein 相似度 (0.0 ~ 1.0)，支持提前终止
[[nodiscard]] inline double levenshtein_similarity(std::string_view a,
                                                   std::string_view b,
                                                   double min_similarity = 0.0) {
  if (a.empty() && b.empty()) {
    return 1.0;
  }
  if (a.empty() || b.empty()) {
    return 0.0;
  }

  // 解码为码点（一次解码，复用结果）
  const auto cp1 = utf8_to_codepoints(a);
  const auto cp2 = utf8_to_codepoints(b);
  const size_t max_len = std::max(cp1.size(), cp2.size());

  // 长度差预过滤：如果长度差已经导致相似度低于阈值，直接返回
  const size_t len_diff =
      cp1.size() > cp2.size() ? cp1.size() - cp2.size() : cp2.size() - cp1.size();
  const double worst_case_sim =
      1.0 - static_cast<double>(len_diff) / static_cast<double>(max_len);
  if (worst_case_sim < min_similarity) {
    return worst_case_sim;
  }

  // 计算允许的最大距离
  const size_t max_distance = static_cast<size_t>(
      (1.0 - min_similarity) * static_cast<double>(max_len));

  const size_t distance = levenshtein_distance(cp1, cp2, max_distance);
  return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

/// 用于缓存相似度计算的哈希函数（boost::hash_combine 惯用法）
struct PairHash {
  template <typename T1, typename T2>
  [[nodiscard]] size_t operator()(const std::pair<T1, T2>& p) const noexcept {
    const auto h1 = std::hash<T1>{}(p.first);
    const auto h2 = std::hash<T2>{}(p.second);
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};

/// 相似度缓存，封装索引对哈希和查找逻辑
class similarity_cache {
  std::unordered_map<std::pair<size_t, size_t>, double, PairHash> cache_;

 public:
  /// 获取相似度（带缓存），索引 i/j 自动归一化
  /// 注意：为避免缓存不精确值，始终计算精确相似度
  [[nodiscard]] double get(size_t i,
                           size_t j,
                           std::string_view a,
                           std::string_view b) {
    if (i > j) {
      std::swap(i, j);
    }
    const auto key = std::make_pair(i, j);

    const auto it = cache_.find(key);
    if (it != cache_.end()) {
      return it->second;
    }

    const double sim = levenshtein_similarity(a, b);
    cache_[key] = sim;
    return sim;
  }

  /// 查找缓存值，返回指针（不存在则 nullptr），避免双重查找
  [[nodiscard]] const double* find(size_t i, size_t j) const {
    if (i > j) {
      std::swap(i, j);
    }
    const auto it = cache_.find(std::make_pair(i, j));
    return it != cache_.end() ? &(it->second) : nullptr;
  }

  /// 检查缓存中是否包含指定索引对
  [[nodiscard]] bool contains(size_t i, size_t j) const {
    if (i > j) {
      std::swap(i, j);
    }
    return cache_.contains(std::make_pair(i, j));
  }

  /// 获取缓存中指定索引对的值（须先确认存在）
  [[nodiscard]] double at(size_t i, size_t j) const {
    if (i > j) {
      std::swap(i, j);
    }
    return cache_.at(std::make_pair(i, j));
  }
};

}  // namespace common
