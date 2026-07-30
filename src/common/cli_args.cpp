#include "cli_args.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "cli_utils.hpp"
#include "fs_utils.hpp"

namespace common::args {

// ==================== 内部辅助函数 ====================

/// 根据 descriptor 构建统一的 CLI parser 并添加选项
[[nodiscard]] static common::cli::parser build_parser(
    const tool_descriptor& desc) {
  common::cli::parser parser(desc.description);

  if (desc.has_threshold) {
    parser.add_option({"--threshold", 's',
                       "Set similarity threshold (0.0 ~ 1.0)", true});
  }
  if (desc.has_time_sort) {
    parser.add_option({"--time", 't',
                       "Sort by modification time (oldest first)", false});
    parser.add_option({"--time-reverse", 'T',
                       "Sort by modification time (newest first)", false});
  }
  if (desc.has_size_sort) {
    parser.add_option({"--min", 'm',
                       "Sort by folder size (smallest first)", false});
    parser.add_option({"--max", 'M',
                       "Sort by folder size (largest first)", false});
  }
  if (desc.has_show_mode) {
    parser.add_option({"--min", 'm',
                       "Print the path with the minimum total size", false});
    parser.add_option({"--max", 'M',
                       "Print the path with the maximum total size", false});
    parser.add_option({"--all", 'a',
                       "Print all duplicate folders", false});
  }
  if (desc.has_limit) {
    parser.add_option({"--limit", 'l', "Limit output to N entries", true});
  }
  if (desc.has_print0) {
    parser.add_option({"--print0", '0',
                       "Use null character as delimiter", false});
  }

  parser.add_positional("directory", "Directory to scan");
  return parser;
}

/// 从解析结果中提取字符串选项值
[[nodiscard]] static std::string_view get_opt(
    const common::cli::parse_result& result,
    std::string_view name) {
  return common::cli::parser::get_option(result, name);
}

/// 检查选项是否存在
[[nodiscard]] static bool has_opt(
    const common::cli::parse_result& result,
    std::string_view name) {
  return common::cli::parser::has_option(result, name);
}

// ==================== parse() ====================

std::expected<parsed_args, std::string> parse(
    int argc, char* argv[], const tool_descriptor& desc) {
  auto parser = build_parser(desc);
  auto parse_result = parser.parse(argc, argv);

  if (!parse_result) {
    if (parse_result.error() == common::cli::k_help_sentinel) {
      parser.print_usage(argv[0]);
      return std::unexpected(std::string(common::cli::k_help_sentinel));
    }
    return std::unexpected(parse_result.error());
  }

  parsed_args result;
  const auto& pr = *parse_result;

  // 处理阈值
  if (desc.has_threshold) {
    auto val = get_opt(pr, "--threshold");
    if (!val.empty()) {
      try {
        double t = std::stod(std::string(val));
        if (t < 0.0 || t > 1.0) {
          return std::unexpected(
              "Error: Similarity threshold must be between 0.0 and 1.0\n");
        }
        result.threshold = t;
      } catch (const std::invalid_argument&) {
        return std::unexpected("Error: Invalid threshold value: " +
                               std::string(val) + "\n");
      } catch (const std::out_of_range&) {
        return std::unexpected("Error: Threshold value out of range: " +
                               std::string(val) + "\n");
      }
    }
  }

  // 处理时间排序
  if (desc.has_time_sort) {
    if (has_opt(pr, "--time-reverse")) {
      result.sort_time = true;
      result.sort_time_desc = true;
    } else if (has_opt(pr, "--time")) {
      result.sort_time = true;
      result.sort_time_desc = false;
    }
  }

  // 处理大小排序 (folder_similarity / oldsort)
  if (desc.has_size_sort) {
    if (has_opt(pr, "--max")) {
      result.sort_size = true;
      result.sort_size_desc = true;
    } else if (has_opt(pr, "--min")) {
      result.sort_size = true;
      result.sort_size_desc = false;
    }
  }

  // 处理显示模式 (extract_name)
  if (desc.has_show_mode) {
    if (has_opt(pr, "--all")) {
      result.show_all = true;
    } else if (has_opt(pr, "--max")) {
      result.show_max = true;
    } else {
      // 默认显示最小值
      result.show_min = true;
    }
  }

  // 处理 limit
  if (desc.has_limit) {
    auto val = get_opt(pr, "--limit");
    if (!val.empty()) {
      try {
        int v = std::stoi(std::string(val));
        if (v <= 0) {
          return std::unexpected("Error: -l requires a positive number\n");
        }
        result.limit = static_cast<size_t>(v);
      } catch (const std::exception&) {
        return std::unexpected("Error: -l requires a valid number\n");
      }
    }
  }

  // 处理 print0
  if (desc.has_print0) {
    result.print0 = has_opt(pr, "--print0");
  }

  // 处理位置参数（目录）
  if (desc.supports_multiple_dirs) {
    if (pr.positional.empty()) {
      result.directories.emplace_back(".");
    } else {
      for (const auto& dir : pr.positional) {
        result.directories.emplace_back(dir);
      }
    }
  } else {
    const auto& dir = pr.positional.empty()
                          ? std::string_view(".")
                          : pr.positional[0];
    result.directories.emplace_back(dir);
  }

  return result;
}

// ==================== print_usage() ====================

void print_usage(const char* program_name, const tool_descriptor& desc) {
  auto parser = build_parser(desc);
  parser.print_usage(program_name);
}

// ==================== run_tool() ====================

int run_tool(int argc, char* argv[],
             tool_descriptor desc,
             const std::function<int(const parsed_args&)>& tool_fn) {
  const auto parse_result = parse(argc, argv, desc);
  if (!parse_result) {
    if (parse_result.error() == common::cli::k_help_sentinel) {
      return 0;
    }
    std::cerr << parse_result.error();
    return 1;
  }

  const auto& args = *parse_result;

  // 对于单目录工具，自动验证目录有效性
  if (!desc.supports_multiple_dirs && !args.directories.empty()) {
    const auto dir_result = common::validate_directory(
        args.directories[0].string());
    if (!dir_result) {
      std::cerr << "Error: " << dir_result.error() << "\n";
      return 1;
    }
  }

  try {
    return tool_fn(args);
  } catch (const std::filesystem::filesystem_error& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}

}  // namespace common::args
