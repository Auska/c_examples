# C++ Examples

一个 C++23 实用工具集合项目，包含两个独立的命令行工具。

## 项目简介

本项目包含两个实用的命令行工具，用于文件系统操作和目录管理：

- **folder_similarity** - 使用 Levenshtein 距离算法比较文件夹名称相似度
- **oldsort** - 按最后修改时间排序并列出目录

## 构建要求

- C++23 支持的编译器（如 GCC 13+ 或 Clang 16+）
- Make 构建工具

## 构建项目

```bash
make
```

这将编译两个可执行文件：`folder_similarity` 和 `oldsort`。

## 工具说明

### folder_similarity

比较文件夹名称的相似度，使用 Levenshtein 距离算法计算相似度。

**用法：**

```bash
./folder_similarity [OPTIONS] [directory1 [directory2 ...]]
```

**选项：**

- `-s <threshold>` - 设置相似度阈值（0.0 ~ 1.0，默认：0.9）
- `-h` - 显示帮助信息

**示例：**

```bash
# 比较当前目录下的子文件夹（默认阈值 0.9）
./folder_similarity

# 设置自定义相似度阈值
./folder_similarity -s 0.8

# 比较指定目录
./folder_similarity /path/to/dir1 /path/to/dir2
```

**算法特性：**

- 使用优化的 Levenshtein 距离算法，空间复杂度为 O(min(n,m))
- 支持同时比较多个目录
- 只显示相似度 >= 阈值的文件夹对

### oldsort

递归遍历目录并按最后修改时间排序（从旧到新）。

**用法：**

```bash
./oldsort [OPTIONS] [directory]
```

**选项：**

- `-l <number>` - 限制输出最老的 N 个目录（默认：不限制）
- `-0` - 使用空字符作为分隔符（用于与 xargs 等工具管道结合）
- `-h` - 显示帮助信息

**示例：**

```bash
# 列出当前目录下所有文件夹，按修改时间从旧到新排序
./oldsort

# 只显示最老的 5 个文件夹
./oldsort -l 5

# 使用空字符分隔符输出（便于管道处理）
./oldsort -0 | xargs -0 ls -ld

# 指定目录
./oldsort /path/to/directory
```

**特性：**

- 预计算时间戳，避免重复文件系统调用
- 自动跳过无权限访问的目录
- 支持与其他工具管道结合

## 性能优化

两个工具都采用了以下优化策略：

- 使用现代 C++23 特性（`std::expected`、`std::ranges`、`std::filesystem`）
- 优化的算法实现（Levenshtein 距离空间优化）
- 使用 `std::from_chars` 替代异常抛出的字符串解析
- 使用 `std::unordered_map` 实现 O(1) 查找
- 预计算数据避免重复操作

## 清理构建文件

```bash
make clean
```

## 开发约定

### 代码风格

- 函数名使用下划线命名法（snake_case）
- 使用 `namespace fs = std::filesystem;` 简化文件系统操作
- 添加 `[[nodiscard]]` 属性到返回值重要的函数

### 错误处理

- 使用 `std::expected` 类型定义错误处理框架
- 统一的错误消息格式
- 向 `std::cerr` 输出错误信息
- 使用适当的返回码（0 表示成功，非 0 表示失败）

### 命令行参数

- 使用 `getopt` 函数解析参数
- 提供清晰的帮助信息（`-h` 选项）
- 参数验证和错误处理

## 许可证

本项目为示例项目，可自由使用和修改。