#pragma once

#include <expected>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace common::cli {

// 用于标识帮助请求的哨兵值
inline constexpr std::string_view k_help_sentinel = "__HELP__";

/// 命令行选项定义
struct option {
  std::string_view long_name;   // 如 "-max"
  char short_name{'\0'};        // 如 'm'，无短名则为 '\0'
  std::string_view description;
  bool has_argument{false};
};

/// 解析结果
struct parse_result {
  /// 选项名 -> 值（无参数选项值为空串）
  std::vector<std::pair<std::string_view, std::string>> options;
  /// 位置参数
  std::vector<std::string> positional;
};

/// 命令行解析器
class parser {
  std::string_view program_desc_;
  std::vector<option> options_;
  std::vector<std::string_view> positional_names_;

 public:
  explicit parser(std::string_view program_desc)
      : program_desc_(program_desc) {}

  /// 添加选项
  void add_option(option opt) { options_.push_back(std::move(opt)); }

  /// 添加位置参数说明
  void add_positional(std::string_view name, std::string_view /*desc*/) {
    positional_names_.push_back(name);
  }

  /// 解析命令行参数
  [[nodiscard]] std::expected<parse_result, std::string> parse(
      int argc, char* argv[]) const {
    parse_result result;

    for (int i = 1; i < argc; ++i) {
      const std::string_view arg = argv[i];

      if (arg == "-h" || arg == "--help") {
        return std::unexpected(std::string(k_help_sentinel));
      }

      // 检查是否匹配已知选项
      bool matched = false;
      for (const auto& opt : options_) {
        if (arg == opt.long_name ||
            (opt.short_name != '\0' && arg.size() == 2 &&
             arg[0] == '-' && arg[1] == opt.short_name)) {
          std::string value;
          if (opt.has_argument) {
            if (i + 1 >= argc) {
              return std::unexpected(std::string("Error: ") +
                                     std::string(opt.long_name) +
                                     " requires an argument\n");
            }
            ++i;
            value = argv[i];
          }
          result.options.emplace_back(opt.long_name, std::move(value));
          matched = true;
          break;
        }

        // 处理 -l5 这种合并格式（仅短选项+参数），排除长选项格式（--xxx）
        if (opt.short_name != '\0' && arg.size() > 2 && arg[0] == '-' &&
            arg[1] != '-' && arg[1] == opt.short_name && opt.has_argument) {
          std::string value(arg.substr(2));
          result.options.emplace_back(opt.long_name, std::move(value));
          matched = true;
          break;
        }
      }

      if (!matched) {
        if (arg[0] != '-') {
          result.positional.emplace_back(arg);
        } else {
          return std::unexpected("Error: Unknown option '" + std::string(arg) +
                                 "'\n");
        }
      }
    }

    return result;
  }

  /// 从解析结果中获取选项值
  [[nodiscard]] static std::string_view get_option(
      const parse_result& result,
      std::string_view name,
      std::string_view default_val = "") {
    for (const auto& [opt_name, value] : result.options) {
      if (opt_name == name) {
        return value;
      }
    }
    return default_val;
  }

  /// 检查选项是否存在
  [[nodiscard]] static bool has_option(const parse_result& result,
                                       std::string_view name) {
    for (const auto& [opt_name, value] : result.options) {
      if (opt_name == name) {
        return true;
      }
    }
    return false;
  }

  /// 打印用法信息
  void print_usage(const char* program_name) const {
    std::cout << "Usage: " << program_name << " [OPTIONS]";
    for (const auto& name : positional_names_) {
      std::cout << " <" << name << ">";
    }
    std::cout << "\n\nOptions:\n";

    for (const auto& opt : options_) {
      if (opt.short_name != '\0') {
        std::cout << "  -" << opt.short_name << ", " << opt.long_name;
      } else {
        std::cout << "  " << opt.long_name;
      }
      if (opt.has_argument) {
        std::cout << " <arg>";
      }
      std::cout << "  " << opt.description << "\n";
    }
    std::cout << "  -h              Show this help message\n";
    std::cout << "\n" << program_desc_ << "\n";
  }
};

}  // namespace common::cli
