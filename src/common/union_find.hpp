#pragma once

#include <cstddef>
#include <vector>

namespace common {

/// Union-Find (并查集) 数据结构，支持路径压缩和按秩合并
class UnionFind {
  std::vector<size_t> parent_;
  std::vector<size_t> rank_;

 public:
  explicit UnionFind(size_t n) : parent_(n), rank_(n, 0) {
    for (size_t i = 0; i < n; ++i) {
      parent_[i] = i;
    }
  }

  /// 查找根节点（迭代实现，带路径压缩）
  [[nodiscard]] size_t find(size_t x) noexcept {
    while (parent_[x] != x) {
      parent_[x] = parent_[parent_[x]];  // 路径压缩
      x = parent_[x];
    }
    return x;
  }

  /// 合并两个集合
  void unite(size_t x, size_t y) noexcept {
    const size_t px = find(x);
    const size_t py = find(y);
    if (px == py) {
      return;
    }

    if (rank_[px] < rank_[py]) {
      parent_[px] = py;
    } else if (rank_[px] > rank_[py]) {
      parent_[py] = px;
    } else {
      parent_[py] = px;
      ++rank_[px];
    }
  }
};

}  // namespace common
