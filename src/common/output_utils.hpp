#pragma once

#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "string_utils.hpp"

namespace common::output {

/// 更新最大显示宽度
inline void update_max_width(size_t& current_max, std::string_view str) {
  const size_t dw = display_width(str);
  if (dw > current_max) {
    current_max = dw;
  }
}

/// 输出右对齐字符串（以空格填充左侧）
/// 使用 display_width 计算 CJK 字符宽度
inline void print_right_aligned(std::ostream& os,
                                std::string_view str,
                                size_t max_width) {
  const size_t padding = max_width > display_width(str)
                             ? max_width - display_width(str)
                             : 0;
  os << std::string(padding, ' ') << str;
}

/// 输出格式化的 "时间  大小  '路径'" 行
inline void print_column_line(std::ostream& os,
                              std::string_view time_str,
                              std::string_view size_str,
                              std::string_view path_str,
                              size_t max_time_w,
                              size_t max_size_w,
                              std::string_view indent = "    ") {
  os << indent;
  print_right_aligned(os, time_str, max_time_w);
  os << "  ";
  print_right_aligned(os, size_str, max_size_w);
  os << "  '" << path_str << "'\n";
}

/// 输出错误行（用空格填充时间/大小列）
inline void print_error_line(std::ostream& os,
                             std::string_view path_str,
                             std::string_view error_msg,
                             size_t max_time_w,
                             size_t max_size_w,
                             std::string_view indent = "    ") {
  os << indent
     << std::string(max_time_w, ' ') << "  "
     << std::string(max_size_w, ' ') << "  '"
     << path_str << "' (error: " << error_msg << ")\n";
}

/// 输出空字符分隔的路径
inline void print_print0_paths(std::ostream& os,
                               const std::vector<std::string>& paths,
                               size_t count) {
  for (size_t i = 0; i < count; ++i) {
    os << paths[i];
    os.put('\0');
  }
}

}  // namespace common::output
