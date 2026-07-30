#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace common {

/// 从文件夹名称中提取最后一对中括号内的内容
[[nodiscard]] std::string extract_bracket_content(
    std::string_view folder_name);

// ==================== 季数提取（手写扫描，替代 std::regex） ====================

/// 检查字符是否为季节分隔符 [._ -]
[[nodiscard]] bool is_season_sep(char c) noexcept;

/// 在指定位置检查是否以 "season"（不区分大小写）开头
[[nodiscard]] bool starts_with_season(std::string_view sv,
                                       size_t pos) noexcept;

/// 从字符串视图指定位置解析 1-2 位数字，返回 {数字值, 数字字符数}
[[nodiscard]] std::pair<int, int> parse_season_number(
    std::string_view sv, size_t pos) noexcept;

/// 检查数字后面是否跟着分隔符或字符串结尾
[[nodiscard]] bool is_season_boundary(std::string_view sv,
                                       size_t pos) noexcept;

/// 模式1: S01 / s01
[[nodiscard]] std::string try_match_Sxx(std::string_view sv,
                                         size_t i) noexcept;

/// 模式2: Season 1 / Season.1 / season 1
[[nodiscard]] std::string try_match_Season(std::string_view sv,
                                            size_t i) noexcept;

/// 模式3: 第N季
[[nodiscard]] std::string try_match_chinese_season(
    std::string_view sv, size_t i) noexcept;

/// 检查序数后缀 (st/nd/rd/th)
[[nodiscard]] bool is_ordinal_suffix(std::string_view sv,
                                      size_t pos, int num) noexcept;

/// 模式4: 1st Season / 2nd Season / 3rd Season / 4th Season
[[nodiscard]] std::string try_match_ordinal_season(
    std::string_view sv, size_t i) noexcept;

/// 从文件夹名称中提取季数标识 (如 S01, S02, Season 1 等)
[[nodiscard]] std::string extract_season(std::string_view folder_name);

/// 从文件夹名称中提取名称和季数组合
[[nodiscard]] std::string extract_name_with_season(
    std::string_view folder_name);

/// 计算字符串的终端显示宽度
[[nodiscard]] size_t display_width(std::string_view sv);

}  // namespace common
