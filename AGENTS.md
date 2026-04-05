# C/C++ 示例项目 AGENTS.md

## 项目概述

这是一个 C++ 实用工具集合项目，包含三个独立的命令行工具程序：

1. **folder_similarity** - 文件夹名称相似度比较工具
   - 使用 Levenshtein 距离算法计算文件夹名称的相似度
   - 支持自定义相似度阈值
   - 可以比较多个目录中的子文件夹
   - 使用 Union-Find 数据结构合并相似名称组

2. **oldsort** - 按修改时间排序目录工具
   - 递归遍历目录并按最后修改时间排序（从旧到新）
   - 显示每个文件夹的总大小（人类可读格式）
   - 支持限制输出数量
   - 支持按文件夹大小排序（-min/-max）
   - 支持使用空字符分隔符输出，便于与其他工具管道结合

3. **extract_name** - 提取文件夹中括号名称并比较大小工具
   - 从文件夹名称中提取 `[...]` 内的中文名
   - 对于相同中文名的文件夹，比较目录内文件总大小
   - 支持输出最小/最大/所有重复文件夹

## 技术栈

- **语言**: C++23
- **标准库**:
  - `<filesystem>` - 文件系统操作
  - `<ranges>` - 范围操作
  - `<getopt.h>` - 命令行参数解析（部分工具使用手动解析）
- **构建工具**: CMake
- **测试框架**: Catch2

## 构建和运行

### 构建项目

```bash
# 创建构建目录并构建
cmake -B build
cmake --build build

# 或者分步执行
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 运行测试

```bash
# 使用 CTest 运行测试
cd build && ctest --output-on-failure

# 或直接运行测试可执行文件
./build/test_runner
```

### 安装

```bash
# 安装到默认路径（通常为 /usr/local/bin）
cmake --install build

# 自定义安装路径
cmake --install build --prefix /path/to/install
```

### 运行程序

#### folder_similarity

```bash
# 比较当前目录下的子文件夹名称相似度（默认阈值 0.9）
./folder_similarity

# 指定自定义相似度阈值（0.0 ~ 1.0）
./folder_similarity -s 0.8

# 比较指定目录
./folder_similarity /path/to/dir1 /path/to/dir2
```

#### oldsort

```bash
# 列出当前目录下所有文件夹，按修改时间从旧到新排序
./oldsort

# 限制输出前 N 个文件夹
./oldsort -l 5
# 或使用 -lN 格式
./oldsort -l5 /path/to/dir

# 使用空字符分隔符输出（便于与 xargs -0 配合）
./oldsort -print0 /path/to/directory

# -min: 对 -l 限制的结果按文件夹大小升序排序
./oldsort -l 10 -min /path/to/directory

# -max: 对 -l 限制的结果按文件夹大小降序排序
./oldsort -l 10 -max /path/to/directory

# 指定目录
./oldsort /path/to/directory
```

#### extract_name

```bash
# 提取中括号内的中文名，对比重复文件夹的文件总大小
./extract_name <directory>

# -min: 显示总大小最小的路径（默认）
./extract_name -min <directory>

# -max: 显示总大小最大的路径
./extract_name -max <directory>

# -all: 显示所有重复的文件夹
./extract_name -all <directory>

# -print0: 使用空字符分隔输出（便于与 xargs -0 配合）
./extract_name -print0 <directory>

# 组合使用：显示所有重复文件夹并使用空字符分隔
./extract_name -all -print0 <directory>
```

## 开发约定

### 代码风格

- **命名约定**:
  - 函数名使用下划线命名法（snake_case）
  - 变量名使用下划线命名法
  - 常量使用大写字母

- **文件组织**:
  - 每个工具程序是独立的可执行文件
  - 公共功能抽取到 `common.hpp` 头文件
  - 使用 `namespace fs = std::filesystem;` 简化文件系统操作

- **错误处理**:
  - 使用 `try-catch` 捕获 `fs::filesystem_error` 异常
  - 向 `std::cerr` 输出错误信息
  - 使用适当的返回码（0 表示成功，非 0 表示失败）

- **代码格式化**:
  - 使用 `.clang-format` 配置文件（Google 风格）
  - 2 空格缩进，switch case 缩进
  - 指针/引用类型右对齐
  - 运行 `clang-format -i <file>.cpp` 格式化代码

### 公共模块 (common.hpp)

项目使用 `common.hpp` 提供公共功能：

```cpp
#include "common.hpp"

// 可用函数：
// - common::format_size(bytes)         - 格式化文件大小
// - common::format_time(ftime)         - 格式化时间（线程安全）
// - common::calculate_total_size(path) - 计算目录总大小
// - common::calculate_total_size_cached(path, cache) - 带缓存计算目录大小
// - common::extract_bracket_content(str) - 提取最后一对中括号内容
// - common::levenshtein_distance(s1, s2) - 计算 Levenshtein 距离
// - common::levenshtein_similarity(a, b) - 计算 Levenshtein 相似度
```

### 命令行参数处理

- 使用 `getopt` 函数或手动解析参数
- 提供清晰的使用说明（Usage 信息）
- 参数验证和错误处理（如阈值范围检查、数字验证等）

### 现代 C++ 特性

- 使用 `std::filesystem` 进行跨平台文件系统操作
- 使用 C++23 `std::ranges` 进行排序操作
- 使用 `std::chrono` 处理文件时间戳
- 使用 `[[nodiscard]]` 属性标记返回值不应被忽略的函数

### 线程安全

- 时间格式化使用 `localtime_r` 替代非线程安全的 `localtime`

## 项目结构

```
c_examples/
├── common.hpp               # 公共头文件（格式化、大小计算、Levenshtein 等）
├── folder_similarity.cpp    # 文件夹相似度比较工具
├── oldsort.cpp              # 目录排序工具
├── extract_name.cpp         # 提取中括号名称并比较大小工具
├── external/                # 第三方库
│   ├── catch_amalgamated.hpp
│   └── catch_amalgamated.cpp
├── tests/
│   └── test_common.cpp      # 单元测试
├── CMakeLists.txt           # CMake 构建配置
├── .clang-format            # 代码格式化配置
├── .gitignore               # Git 忽略配置
└── AGENTS.md                # 项目说明文档
```

## 已知限制

- 项目使用 C++23 特性（`std::ranges`），需要支持 C++23 的编译器
