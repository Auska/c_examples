#include "output_utils.hpp"

#include <ostream>
#include <string>
#include <string_view>
#include <vector>

#include "string_utils.hpp"

namespace common::output {

void update_max_width(size_t& current_max, std::string_view str) {
  const size_t dw = display_width(str);
  if (dw > current_max) {
    current_max = dw;
  }
}

void print_right_aligned(std::ostream& os,
                          std::string_view str,
                          size_t max_width) {
  const size_t padding = max_width > display_width(str)
                             ? max_width - display_width(str)
                             : 0;
  os << std::string(padding, ' ') << str;
}

void print_column_line(std::ostream& os,
                        std::string_view time_str,
                        std::string_view size_str,
                        std::string_view path_str,
                        size_t max_time_w,
                        size_t max_size_w,
                        std::string_view indent) {
  os << indent;
  print_right_aligned(os, time_str, max_time_w);
  os << "  ";
  print_right_aligned(os, size_str, max_size_w);
  os << "  '" << path_str << "'\n";
}

void print_error_line(std::ostream& os,
                       std::string_view path_str,
                       std::string_view error_msg,
                       size_t max_time_w,
                       size_t max_size_w,
                       std::string_view indent) {
  os << indent
     << std::string(max_time_w, ' ') << "  "
     << std::string(max_size_w, ' ') << "  '"
     << path_str << "' (error: " << error_msg << ")\n";
}

void print_print0_paths(std::ostream& os,
                         const std::vector<std::string>& paths,
                         size_t count) {
  for (size_t i = 0; i < count; ++i) {
    os << paths[i];
    os.put('\0');
  }
}

}  // namespace common::output
