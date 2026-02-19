# C++ Examples

一个 C++23 实用工具集合项目，包含三个独立的命令行工具。

## 项目简介

本项目包含三个实用的命令行工具，用于文件系统操作和目录管理：

- **folder_similarity** - 使用 Levenshtein 距离算法比较文件夹名称相似度
- **oldsort** - 按最后修改时间排序并列出目录
- **extract_name** - 提取文件夹中括号名称并比较文件大小

## 构建要求

- C++23 支持的编译器（如 GCC 13+ 或 Clang 16+）
- Make 构建工具

## 构建项目

```bash
make
```

这将编译三个可执行文件：`folder_similarity`、`oldsort` 和 `extract_name`。

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

### oldsort

递归遍历目录并按最后修改时间排序（从旧到新），显示每个文件夹的总大小。

**用法：**

```bash
./oldsort [OPTIONS] [directory]
```

**选项：**

- `-l <number>` - 限制输出前 N 个目录（默认：不限制）
- `-print0` - 使用空字符作为分隔符（用于与 xargs -0 管道结合）
- `-min` - 对 -l 限制的结果按文件夹大小升序排序
- `-max` - 对 -l 限制的结果按文件夹大小降序排序
- `-h` - 显示帮助信息

**示例：**

```bash
# 列出当前目录下所有文件夹，按修改时间从旧到新排序
./oldsort

# 只显示前 5 个文件夹
./oldsort -l 5

# 使用空字符分隔符输出（便于管道处理）
./oldsort -print0 /path/to/directory | xargs -0 ls -ld

# -l 限制结果中按大小排序
./oldsort -l 10 -min /path/to/directory
./oldsort -l 10 -max /path/to/directory

# 指定目录
./oldsort /path/to/directory
```

**特性：**

- 显示文件夹总大小（人类可读格式：B, KB, MB, GB, TB）
- 按最后修改时间排序（从旧到新）
- 支持对限制结果按大小二次排序

### extract_name

从文件夹名称中提取 `[...]` 内的中文名，对于相同中文名的文件夹，比较目录内文件总大小。

**用法：**

```bash
./extract_name [OPTIONS] <directory>
```

**选项：**

- `-min` - 显示总大小最小的路径（默认）
- `-max` - 显示总大小最大的路径
- `-all` - 显示所有重复的文件夹
- `-print0` - 使用空字符作为分隔符（便于与 xargs -0 配合）
- `-h` - 显示帮助信息

**示例：**

```bash
# 提取中括号内的中文名，对比重复文件夹的文件总大小
./extract_name /path/to/media

# -max: 显示总大小最大的路径
./extract_name -max /path/to/media

# -all: 显示所有重复的文件夹
./extract_name -all /path/to/media

# -print0: 使用空字符分隔输出
./extract_name -print0 /path/to/media | xargs -0 -I{} ls -ld '{}'

# 组合使用：显示所有重复文件夹并使用空字符分隔
./extract_name -all -print0 /path/to/media
```

**特性：**

- 自动提取文件夹名中 `[...]` 内的中文名
- 计算每个文件夹内所有文件的总大小
- 支持人类可读的文件大小输出（B, KB, MB, GB, TB）

## 性能优化

所有工具都采用了以下优化策略：

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

- 使用 `getopt` 函数或手动解析参数
- 提供清晰的帮助信息（`-h` 选项）
- 参数验证和错误处理

### 代码格式化

项目使用 `.clang-format` 配置文件（Google 风格）进行代码格式化：

- 2 空格缩进
- switch case 缩进
- 指针/引用类型右对齐
- 80 字符行宽限制

格式化代码：

```bash
clang-format -i <file>.cpp
```

## 许可证

本项目为示例项目，可自由使用和修改。
