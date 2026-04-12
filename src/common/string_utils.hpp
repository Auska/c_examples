#pragma once

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

}  // namespace common
