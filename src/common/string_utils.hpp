#pragma once

#include <array>
#include <cctype>
#include <format>
#include <regex>
#include <string>
#include <string_view>

namespace common {

/// 从文件夹名称中提取最后一对中括号内的内容
[[nodiscard]] inline std::string extract_bracket_content(
    std::string_view folder_name) {
  const size_t end = folder_name.rfind(']');
  if (end == std::string_view::npos) {
    return "";
  }
  const size_t start = folder_name.rfind('[', end);
  if (start == std::string_view::npos) {
    return "";
  }
  return std::string(folder_name.substr(start + 1, end - start - 1));
}

/// 从文件夹名称中提取季数标识 (如 S01, S02, Season 1 等)
/// 返回格式化的季数字符串 (如 "S01", "S02")
/// 注意：正则中仅使用 ASCII 分隔符 [._ -]，避免 \s 误匹配 UTF-8 多字节字节
[[nodiscard]] inline std::string extract_season(std::string_view folder_name) {
  // 常见的季数模式，按优先级排序
  // 仅使用 ASCII 分隔符 [._ -] 替代 \s，避免 UTF-8 多字节字符的误匹配
  static const std::array<std::regex, 4> season_patterns = {
      std::regex{R"([._ -]S(\d{1,2})(?:[._ -]|$))", std::regex::icase},  // S01, S02
      std::regex{R"([._ -]Season[._ ]*(\d{1,2})(?:[._ -]|$))",
                 std::regex::icase},                                       // Season 1, Season.1
      std::regex{R"([._ -]第(\d{1,2})季(?:[._ -]|$))"},                  // 第1季, 第2季
      std::regex{R"([._ -](\d{1,2})st[._ -]*Season(?:[._ -]|$))",
                 std::regex::icase},  // 1st Season, 2nd Season
  };

  for (const auto& pattern : season_patterns) {
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(folder_name.begin(), folder_name.end(), match,
                          pattern)) {
      int season_num = std::stoi(match[1].str());
      // 格式化为 S01, S02, ..., S99
      return std::format("S{:02d}", season_num);
    }
  }

  return "";
}

/// 从文件夹名称中提取名称和季数组合
/// 例如: "[雪国列车].Snowpiercer.S01..." -> "雪国列车 S01"
[[nodiscard]] inline std::string extract_name_with_season(
    std::string_view folder_name) {
  const std::string chinese_name = extract_bracket_content(folder_name);
  if (chinese_name.empty()) {
    return "";
  }

  const std::string season = extract_season(folder_name);
  if (season.empty()) {
    return chinese_name;
  }

  return chinese_name + " " + season;
}

}  // namespace common
