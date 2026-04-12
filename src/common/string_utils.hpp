#pragma once

#include <string>

namespace common {

// 从文件夹名称中提取最后一对中括号内的内容
[[nodiscard]] inline std::string extract_bracket_content(
    const std::string &folder_name) {
  size_t end = folder_name.rfind(']');
  if (end == std::string::npos) {
    return "";
  }
  size_t start = folder_name.rfind('[', end);
  if (start == std::string::npos) {
    return "";
  }
  return folder_name.substr(start + 1, end - start - 1);
}

}  // namespace common
