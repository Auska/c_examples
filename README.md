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
# 构建项目
cmake -B build
cmake --build build -j$(nproc)

# 运行测试
cd build && ctest --output-on-failure

# 安装
cmake --install build
```

## 工具使用

### folder_similarity

比较文件夹名称的相似度，使用 Levenshtein 距离算法。

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

# 使用空字符分隔输出
./extract_name -print0 /path/to/media | xargs -0 -I{} ls -ld '{}'
```

**选项：** `-min` 最小（默认），`-max` 最大，`-all` 全部，`-print0` 空字符分隔，`-t` 按时间升序，`-tr` 按时间降序

## 项目结构

```
c_examples/
├── src/
│   ├── common/                # 公共模块
│   │   ├── common.hpp         # 聚合头文件
│   │   ├── size_utils.hpp     # 大小格式化和计算
│   │   ├── time_utils.hpp     # 时间格式化
│   │   ├── string_utils.hpp   # 字符串处理
│   │   └── levenshtein.hpp    # Levenshtein 算法
│   ├── folder_similarity/     # 文件夹相似度工具
│   │   └── main.cpp
│   ├── oldsort/               # 目录排序工具
│   │   └── main.cpp
│   └── extract_name/          # 提取名称工具
│       └── main.cpp
├── tests/
│   └── test_common.cpp        # 单元测试
├── external/                  # 第三方库 (Catch2)
├── CMakeLists.txt
└── README.md
```

## 公共模块

```cpp
#include "common/common.hpp"

// 大小相关
common::format_size(bytes);                    // 格式化文件大小
common::calculate_total_size(path);            // 计算目录总大小
common::calculate_total_size_cached(path, cache); // 带缓存计算

// 时间相关
common::format_time(ftime);                    // 格式化时间

// 字符串相关
common::extract_bracket_content(str);          // 提取中括号内容

// Levenshtein 相关
common::levenshtein_distance(s1, s2);          // 计算距离
common::levenshtein_similarity(a, b);          // 计算相似度
```

## 开发约定

- **命名**：snake_case（函数、变量），UPPER_CASE（常量）
- **格式化**：使用 `.clang-format`（Google 风格，2 空格缩进）
- **错误处理**：`try-catch` 捕获 `filesystem_error`，`std::cerr` 输出错误
- **线程安全**：时间格式化使用 `localtime_r`

## 许可证

本项目为示例项目，可自由使用和修改。
