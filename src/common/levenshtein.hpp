#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <rapidfuzz/distance/Levenshtein.hpp>

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

// ==================== Levenshtein 相似度 (RapidFuzz) ====================

/// 计算 Levenshtein 距离（byte 级别），支持 max_distance 提前终止
[[nodiscard]] inline size_t levenshtein_distance(std::string_view a,
                                                 std::string_view b,
                                                 size_t max_distance = SIZE_MAX) {
  return rapidfuzz::levenshtein_distance(a, b, {1, 1, 1}, max_distance);
}

/// 计算 Levenshtein 相似度 (0.0 ~ 1.0)，支持 score_cutoff 提前终止
/// 底层使用 RapidFuzz 的 byte 级别算法，对 UTF-8 字节序列直接计算
[[nodiscard]] inline double levenshtein_similarity(std::string_view a,
                                                   std::string_view b,
                                                   double min_similarity = 0.0) {
  // RapidFuzz 的 normalized_similarity 返回值范围 [0.0, 1.0]
  // score_cutoff < 1e-6 时关闭提前终止，与 our min_similarity=0.0 语义一致
  const double result =
      rapidfuzz::levenshtein_normalized_similarity(a, b, {1, 1, 1},
                                                    min_similarity);
  return result;
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
