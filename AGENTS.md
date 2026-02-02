# C/C++ 示例项目 AGENTS.md

## 项目概述

这是一个 C++ 实用工具集合项目，包含两个独立的命令行工具程序：

1. **folder_similarity** - 文件夹名称相似度比较工具
   - 使用 Levenshtein 距离算法计算文件夹名称的相似度
   - 支持自定义相似度阈值
   - 可以比较多个目录中的子文件夹

2. **oldsort** - 按修改时间排序目录工具
   - 递归遍历目录并按最后修改时间排序（从旧到新）
   - 支持限制输出数量
   - 支持使用空字符分隔符输出，便于与其他工具管道结合

## 技术栈

- **语言**: C++23
- **标准库**:
  - `<filesystem>` - 文件系统操作
  - `<ranges>` - 范围操作
  - `<getopt.h>` - 命令行参数解析
- **构建工具**: Make

## 构建和运行

### 构建项目

```bash
# 使用 Make 构建
make

# 或者指定编译器（如果需要）
make CXX=g++
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

# 限制输出最老的 N 个文件夹
./oldsort -l 5
# 或使用 -lN 格式
./oldsort -l10

# 使用空字符分隔符输出（便于与 xargs 等工具管道结合）
./oldsort -0

# 指定目录
./oldsort /path/to/directory
```

## 开发约定

### 代码风格

- **命名约定**:
  - 函数名使用下划线命名法（snake_case）
  - 变量名使用下划线命名法
  - 常量使用大写字母

- **文件组织**:
  - 每个工具程序是独立的可执行文件
  - 使用 `namespace fs = std::filesystem;` 简化文件系统操作

- **错误处理**:
  - 使用 `try-catch` 捕获 `fs::filesystem_error` 异常
  - 向 `std::cerr` 输出错误信息
  - 使用适当的返回码（0 表示成功，非 0 表示失败）

### 命令行参数处理

- 使用 `getopt` 函数解析参数
- 提供清晰的使用说明（Usage 信息）
- 参数验证和错误处理（如阈值范围检查、数字验证等）

### 现代 C++ 特性

- 使用 `std::filesystem` 进行跨平台文件系统操作
- 使用 C++23 `std::ranges` 进行排序操作
- 使用 `std::chrono` 处理文件时间戳

## 项目结构

```
c_examples/
├── folder_similarity.cpp    # 文件夹相似度比较工具
├── oldsort.cpp              # 目录排序工具
├── .gitignore               # Git 忽略配置
└── Makefile                 # Make 构建配置
```

## 已知限制

- 项目使用 C++23 特性（`std::ranges`），需要支持 C++23 的编译器
- 目前没有单元测试