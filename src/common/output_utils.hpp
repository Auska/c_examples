#pragma once

#include <cstddef>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace common::output {

/// 更新最大显示宽度
void update_max_width(size_t& current_max, std::string_view str);

/// 输出右对齐字符串（以空格填充左侧）
void print_right_aligned(std::ostream& os,
                          std::string_view str,
                          size_t max_width);

/// 输出格式化的 "时间  大小  '路径'" 行
void print_column_line(std::ostream& os,
                        std::string_view time_str,
                        std::string_view size_str,
                        std::string_view path_str,
                        size_t max_time_w,
                        size_t max_size_w,
                        std::string_view indent = "    ");

/// 输出错误行（用空格填充时间/大小列）
void print_error_line(std::ostream& os,
                       std::string_view path_str,
                       std::string_view error_msg,
                       size_t max_time_w,
                       size_t max_size_w,
                       std::string_view indent = "    ");

/// 输出空字符分隔的路径
void print_print0_paths(std::ostream& os,
                         const std::vector<std::string>& paths,
                         size_t count);

}  // namespace common::output
