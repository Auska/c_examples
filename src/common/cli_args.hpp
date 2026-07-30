#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace common::args {

/// 描述一个工具支持哪些命令行选项
struct tool_descriptor {
  std::string description;
  bool has_threshold = false;          // -s <double>
  bool has_size_sort = false;          // -m/-M (sort by size)
  bool has_time_sort = false;          // -t/-T
  bool has_limit = false;              // -l <size_t>
  bool has_show_mode = false;          // -a/-m/-M (extract_name 模式)
  bool has_print0 = false;             // -0/--print0
  bool supports_multiple_dirs = false; // folder_similarity 支持多目录
};

/// 统一解析结果，所有工具共用
struct parsed_args {
  /// 位置参数目录列表（单目录工具也放在 vector 中保持一致性）
  std::vector<std::filesystem::path> directories;
  /// 相似度阈值 [0.0, 1.0]
  std::optional<double> threshold;
  /// 时间排序及方向
  bool sort_time = false;
  bool sort_time_desc = false;
  /// 大小排序及方向
  bool sort_size = false;
  bool sort_size_desc = false;
  /// 输出行数限制
  std::optional<size_t> limit;
  /// null 分隔输出
  bool print0 = false;
  /// extract_name 显示模式
  bool show_all = false;
  bool show_min = false;
  bool show_max = false;
};

/// 统一解析入口
/// 成功返回 parsed_args，失败返回错误信息
/// help 请求通过 k_help_sentinel 标识
[[nodiscard]] std::expected<parsed_args, std::string> parse(
    int argc, char* argv[], const tool_descriptor& desc);

/// 打印工具用法信息
void print_usage(const char* program_name, const tool_descriptor& desc);

/// 统一工具入口包装器
/// 自动处理：parse → help 检查 → 目录校验 → 执行 → 异常捕获
/// 返回适合 main() 的退出码
[[nodiscard]] int run_tool(
    int argc, char* argv[],
    tool_descriptor desc,
    const std::function<int(const parsed_args&)>& tool_fn);

}  // namespace common::args
