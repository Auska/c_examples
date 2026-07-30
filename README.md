# C++ Examples

一个 C++23 实用工具集合项目，包含三个独立的命令行工具。

## 工具简介

| 工具 | 功能 |
|------|------|
| **folder_similarity** | 使用 Levenshtein 距离（RapidFuzz）比较文件夹名称相似度 |
| **oldsort** | 按最后修改时间/文件夹大小排序并列出目录 |
| **extract_name** | 提取文件夹中括号内的中文名并比较重复文件夹大小 |

## 构建要求

- C++23 支持的编译器（如 GCC 13+ 或 Clang 16+）
- CMake 3.27+

## 构建和测试

### 使用 CMake

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

比较文件夹名称的相似度，使用 Levenshtein 距离算法（RapidFuzz 字节级，UTF-8 语义正确）。

```bash
# 比较当前目录下的子文件夹（默认阈值 0.9）
./folder_similarity

# 设置自定义相似度阈值
./folder_similarity -s 0.8

# 时间排序（从旧到新）
./folder_similarity -t

# 时间排序（从新到旧）
./folder_similarity -T

# 体积排序（从小到大）
./folder_similarity -m

# 体积排序（从大到小）
./folder_similarity -M

# 比较指定目录
./folder_similarity /path/to/dir1 /path/to/dir2
```

**选项：**
- `-s <threshold>` 设置相似度阈值（0.0 ~ 1.0）
- `-t` 按修改时间升序排列
- `-T` 按修改时间降序排列
- `-m` 按文件夹体积升序排列
- `-M` 按文件夹体积降序排列
- `-h` 显示帮助

### oldsort

递归遍历目录并按最后修改时间/大小排序。

```bash
# 列出当前目录下所有文件夹（默认按时间排序）
./oldsort

# 只显示前 5 个文件夹
./oldsort -l 5

# 使用空字符分隔符输出（便于管道处理）
./oldsort -print0 /path/to/directory | xargs -0 ls -ld

# 按大小排序（对 -l 限制的结果）
./oldsort -l 10 -m /path/to/directory  # 升序
./oldsort -l 10 -M /path/to/directory  # 降序
```

**选项：**
- `-l <N>` 限制输出数量
- `-0` / `--print0` 空字符分隔
- `-m` / `--min` 按文件夹大小升序（先按时间筛选，再对前 N 个按大小排序）
- `-M` / `--max` 按文件夹大小降序

### extract_name

从文件夹名称中提取 `[...]` 内的中文名，比较相同中文名的文件夹大小。

```bash
# 显示总大小最小的路径（默认）
./extract_name /path/to/media

# 显示总大小最大的路径
./extract_name -M /path/to/media

# 显示所有重复的文件夹
./extract_name -a /path/to/media

# 按修改时间排序（从旧到新）
./extract_name -a -t /path/to/media

# 按修改时间排序（从新到旧）
./extract_name -a -T /path/to/media

# 按时间排序输出最小大小的路径
./extract_name -t /path/to/media

# 按时间排序输出最大大小的路径
./extract_name -M -t /path/to/media

# 限制每组输出行数
./extract_name -a -l 3 /path/to/media

# 使用空字符分隔输出
./extract_name -0 /path/to/media | xargs -0 -I{} ls -ld '{}'
```

**选项：**
- `-m` / `--min` 最小（默认）
- `-M` / `--max` 最大
- `-a` / `--all` 全部
- `-0` / `--print0` 空字符分隔
- `-t` / `--time` 按时间升序
- `-T` / `--time-reverse` 按时间降序
- `-l <N>` 限制每组的输出行数

## 项目结构

```
c_examples/
├── src/
│   ├── common/                # 公共模块（编译为 common_utils 静态库）
│   │   ├── common.hpp         # 聚合头文件
│   │   ├── cli_args.hpp       # 统一 CLI 解析（tool_descriptor / parsed_args / run_tool）
│   │   ├── cli_args.cpp
│   │   ├── cli_utils.hpp      # 基础命令行解析器
│   │   ├── dir_entry.hpp      # 目录条目结构体
│   │   ├── fs_utils.hpp / .cpp    # 文件系统校验
│   │   ├── levenshtein.hpp    # Levenshtein 算法（RapidFuzz 字节级）
│   │   ├── output_utils.hpp / .cpp # 格式化输出（列对齐、错误行）
│   │   ├── size_utils.hpp / .cpp   # 大小格式化和计算 + 缓存
│   │   ├── string_utils.hpp / .cpp # 字符串处理（手写扫描提取季数 + 显示宽度）
│   │   ├── time_utils.hpp / .cpp   # 时间格式化
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
│   ├── Celero-2.10.0/         # Celero 基准测试框架
│   └── rapidfuzz-cpp-3.3.3/   # RapidFuzz Levenshtein 库
├── CMakeLists.txt
├── .clang-format
├── .clang-tidy
└── README.md
```

## 公共模块

所有工具通过 `common_utils` 静态库共享公共功能。

```cpp
#include "common/common.hpp"

// ==================== 统一 CLI 入口 ====================
// 每个工具通过 descriptor 声明支持哪些选项，run_tool 自动处理解析/校验/异常
common::args::run_tool(argc, argv,
    {.description = "My tool",
     .has_threshold = true,
     .has_time_sort = true,
     .has_size_sort = true,
     .has_limit = true,
     .has_show_mode = false,
     .has_print0 = true},
    [](const common::args::parsed_args& args) -> int {
        args.directories;           // 目录列表
        args.threshold;             // 相似度阈值
        args.sort_time;             // 时间排序
        args.limit;                 // 限制行数
        args.print0;                // null 分隔输出
        return 0;
    });

// ==================== 命令行解析 ====================
common::cli::parser p("program description");
p.add_option({"--threshold", 's', "Set threshold", true});
p.add_positional("directory", "Dir to scan");
auto result = p.parse(argc, argv);
common::cli::parser::get_option(*result, "--threshold");     // 获取选项值
common::cli::parser::has_option(*result, "--verbose");       // 检查选项存在
p.print_usage(argv[0]);                                      // 打印帮助

// 大小相关
common::format_size(bytes);                    // 格式化文件大小
common::calculate_total_size(path);            // 计算目录总大小
common::size_cache sc;
sc.get(path);                                  // 带缓存计算目录大小

// 时间相关
common::format_time(ftime);                    // 格式化修改时间

// 字符串相关
common::extract_bracket_content(str);          // 提取中括号内容
common::extract_season(str);                   // 提取季数 (S01/Season 1/第N季/1st Season)
common::extract_name_with_season(str);         // 提取中文名+季数
common::display_width(str);                    // 终端显示宽度（CJK 计 2 列）

// Levenshtein 相关（RapidFuzz 字节级，UTF-8 语义正确）
common::levenshtein_distance(s1, s2);          // 字节级 Levenshtein 距离
common::levenshtein_similarity(a, b);          // 相似度 (0.0 ~ 1.0)
common::levenshtein_similarity(a, b, 0.9);     // 带提前终止的相似度计算
common::similarity_cache cache;
cache.get(i, j, a, b);                         // 带缓存的相似度

// 并查集
common::UnionFind uf(n);
uf.unite(i, j);                                // 合并
uf.find(i);                                    // 查找根节点

// 目录条目（统一封装）
common::dir_entry entry;
entry.path;                                    // 文件路径
entry.mtime;                                   // 修改时间
entry.size;                                    // 大小
```

## 性能优化

| 优化项 | 说明 | 效果 |
|--------|------|------|
| Levenshtein RapidFuzz 字节级 | 使用 `rapidfuzz::levenshtein_distance` 直接对 UTF-8 字节序列计算 | 避免编解码开销，兼容 RapidFuzz SIMD 优化 |
| Levenshtein 提前终止 | `score_cutoff` 参数使距离超过阈值时立即返回 | 不相似配对 ~3.5x 加速 |
| 长度差预过滤 | O(n²) 比较中跳过不可能相似的配对 | 大目录场景显著减少计算量 |
| extract_season 手写扫描 | 替代 std::regex | ~12x 加速 vs regex |
| size_cache 去 canonical | 直接用 path.string() 作缓存键 | 避免文件系统 I/O |
| display_width 手写解析 | 替代 wcwidth 调用，按 UTF-8 字节前缀分类 | 无需 locale/ICU 依赖，零分配 |

## 开发约定

- **命名**：snake_case（函数、变量），UPPER_CASE（常量）
- **格式化**：使用 `.clang-format`（Google 风格，2 空格缩进）
- **静态分析**：`.clang-tidy` 配置（modernize-* 系列 + 性能检查）
- **错误处理**：`try-catch` 捕获 `filesystem_error`，`std::cerr` 输出错误
- **线程安全**：时间格式化使用 `localtime_r`（POSIX）/ `localtime_s`（Windows）

## 许可证

本项目为示例项目，可自由使用和修改。
