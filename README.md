# C++ Examples

一个 C++23 实用工具集合项目，包含三个独立的命令行工具。

## 工具简介

| 工具 | 功能 |
|------|------|
| **folder_similarity** | 使用 Levenshtein 距离算法比较文件夹名称相似度 |
| **oldsort** | 按最后修改时间排序并列出目录 |
| **extract_name** | 提取文件夹中括号名称并比较文件大小 |

## 构建要求

- C++23 支持的编译器（如 GCC 13+ 或 Clang 16+）
- CMake 3.16+

## 构建和测试

```bash
# 构建项目（默认包含测试和性能测试）
cmake -B build
cmake --build build -j$(nproc)

# 只构建工具，跳过测试和性能测试
cmake -B build -DENABLE_TESTS=OFF -DENABLE_BENCHMARKS=OFF
cmake --build build -j$(nproc)

# 运行单元测试
cd build && ctest --output-on-failure

# 运行性能测试
./build/benchmark_runner

# 安装
cmake --install build
```

### 构建选项

| 选项 | 默认 | 说明 |
|------|------|------|
| `ENABLE_TESTS` | ON | 构建单元测试 (Catch2) |
| `ENABLE_BENCHMARKS` | ON | 构建性能测试 (Celero) |

## 工具使用

### folder_similarity

比较文件夹名称的相似度，使用 Levenshtein 距离算法（UTF-8 码点级，中文语义正确）。

```bash
# 比较当前目录下的子文件夹（默认阈值 0.9）
./folder_similarity

# 设置自定义相似度阈值
./folder_similarity -s 0.8

# 比较指定目录
./folder_similarity /path/to/dir1 /path/to/dir2
```

**选项：** `-s <threshold>` 设置相似度阈值（0.0 ~ 1.0），`-h` 显示帮助

### oldsort

递归遍历目录并按最后修改时间排序（从旧到新）。

```bash
# 列出当前目录下所有文件夹
./oldsort

# 只显示前 5 个文件夹
./oldsort -l 5

# 使用空字符分隔符输出（便于管道处理）
./oldsort -print0 /path/to/directory | xargs -0 ls -ld

# 按大小排序（对 -l 限制的结果）
./oldsort -l 10 -min /path/to/directory  # 升序
./oldsort -l 10 -max /path/to/directory  # 降序
```

**选项：** `-l <N>` 限制输出数量，`-print0` 空字符分隔，`-min`/`-max` 按大小排序

### extract_name

从文件夹名称中提取 `[...]` 内的中文名，比较相同名称的文件夹大小。

```bash
# 显示总大小最小的路径
./extract_name /path/to/media

# 显示总大小最大的路径
./extract_name -max /path/to/media

# 显示所有重复的文件夹
./extract_name -all /path/to/media

# 按修改时间排序（从旧到新）
./extract_name -all -t /path/to/media

# 按修改时间排序（从新到旧）
./extract_name -all -tr /path/to/media

# 按时间排序输出最小大小的路径
./extract_name -t /path/to/media

# 按时间排序输出最大大小的路径
./extract_name -max -t /path/to/media

# 限制每组输出行数
./extract_name -all -l 3 /path/to/media

# 使用空字符分隔输出
./extract_name -print0 /path/to/media | xargs -0 -I{} ls -ld '{}'
```

**选项：** `-min` 最小（默认），`-max` 最大，`-all` 全部，`-print0` 空字符分隔，`-t` 按时间升序，`-tr` 按时间降序，`-l <N>` 限制每组的输出行数

## 项目结构

```
c_examples/
├── src/
│   ├── common/                # 公共模块
│   │   ├── common.hpp         # 聚合头文件
│   │   ├── cli_utils.hpp      # 命令行解析器
│   │   ├── dir_entry.hpp      # 目录条目结构体
│   │   ├── fs_utils.hpp       # 文件系统校验
│   │   ├── levenshtein.hpp    # Levenshtein 算法 (UTF-8 码点级)
│   │   ├── size_utils.hpp     # 大小格式化和计算 + 缓存
│   │   ├── string_utils.hpp   # 字符串处理 (手写扫描提取季数)
│   │   ├── time_utils.hpp     # 时间格式化
│   │   └── union_find.hpp     # 并查集
│   ├── folder_similarity/     # 文件夹相似度工具
│   │   └── main.cpp
│   ├── oldsort/               # 目录排序工具
│   │   └── main.cpp
│   └── extract_name/          # 提取名称工具
│       └── main.cpp
├── tests/
│   └── test_common.cpp        # 单元测试 (Catch2)
├── benchmarks/
│   └── benchmark_common.cpp   # 性能测试 (Celero)
├── external/                  # 第三方库
│   ├── catch_amalgamated.*    # Catch2
│   └── Celero-2.10.0/         # Celero 基准测试框架
├── CMakeLists.txt
└── README.md
```

## 公共模块

```cpp
#include "common/common.hpp"

// 大小相关
common::format_size(bytes);                    // 格式化文件大小
common::calculate_total_size(path);            // 计算目录总大小
common::size_cache sc;
sc.get(path);                                  // 带缓存计算目录大小

// 时间相关
common::format_time(ftime);                    // 格式化时间

// 字符串相关
common::extract_bracket_content(str);          // 提取中括号内容
common::extract_season(str);                   // 提取季数 (S01/Season 1/第N季)
common::extract_name_with_season(str);         // 提取中文名+季数

// Levenshtein 相关
common::levenshtein_distance(s1, s2);          // UTF-8 码点级距离
common::levenshtein_similarity(a, b);          // 相似度 (0.0 ~ 1.0)
common::levenshtein_similarity(a, b, 0.9);    // 带提前终止的相似度计算
common::similarity_cache cache;
cache.get(i, j, a, b);                        // 带缓存的相似度

// 并查集
common::UnionFind uf(n);
uf.unite(i, j);                                // 合并
uf.find(i);                                    // 查找根节点
```

## 性能优化

| 优化项 | 说明 | 效果 |
|--------|------|------|
| Levenshtein UTF-8 码点级 | 先解码为 Unicode 码点再计算距离 | 中文语义正确 |
| Levenshtein 提前终止 | 距离超过阈值时立即返回 | 不相似配对 ~3.5x 加速 |
| 长度差预过滤 | O(n^2) 比较中跳过不可能相似的配对 | 大目录场景显著减少计算量 |
| extract_season 手写扫描 | 替代 std::regex | ~12x 加速 |
| size_cache 去除 canonical | 直接用 path.string() 作缓存键 | 避免文件系统 I/O |

## 开发约定

- **命名**：snake_case（函数、变量），UPPER_CASE（常量）
- **格式化**：使用 `.clang-format`（Google 风格，2 空格缩进）
- **错误处理**：`try-catch` 捕获 `filesystem_error`，`std::cerr` 输出错误
- **线程安全**：时间格式化使用 `localtime_r`

## 许可证

本项目为示例项目，可自由使用和修改。
