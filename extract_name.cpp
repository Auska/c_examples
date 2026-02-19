#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// 将字节数转换为人类可读格式
std::string format_size(uintmax_t bytes) {
  const char *units[] = {"B", "KB", "MB", "GB", "TB"};
  int unit_index = 0;
  double size = static_cast<double>(bytes);

  while (size >= 1024 && unit_index < 4) {
    size /= 1024;
    unit_index++;
  }

  char buffer[32];
  if (unit_index == 0) {
    snprintf(buffer, sizeof(buffer), "%.0f %s", size, units[unit_index]);
  } else {
    snprintf(buffer, sizeof(buffer), "%.2f %s", size, units[unit_index]);
  }
  return buffer;
}

// 格式化修改时间（参考 oldsort.cpp）
std::string format_time(fs::file_time_type ftime) {
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      ftime - fs::file_time_type::clock::now() +
      std::chrono::system_clock::now());
  std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
  std::tm *tm = std::localtime(&tt);

  char buffer[32];
  std::strftime(buffer, sizeof(buffer), "%Y-%m-%d+%H:%M:%S", tm);
  return buffer;
}

// 从文件夹名称中提取中括号内的中文名
std::string extract_chinese_name(const std::string &folder_name) {
  size_t start = folder_name.find('[');
  size_t end = folder_name.find(']');
  if (start != std::string::npos && end != std::string::npos && end > start) {
    return folder_name.substr(start + 1, end - start - 1);
  }
  return "";
}

// 计算目录下所有文件的总大小
uintmax_t calculate_total_size(const fs::path &dir_path) {
  uintmax_t total_size = 0;
  try {
    for (const auto &entry : fs::recursive_directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        try {
          total_size += entry.file_size();
        } catch (const fs::filesystem_error &) {
          // 跳过无法获取大小的文件
        }
      }
    }
  } catch (const fs::filesystem_error &) {
    // 跳过无法读取的目录
  }
  return total_size;
}

void print_usage(const char *program_name) {
  std::cout
      << "Usage: " << program_name << " [OPTIONS] <directory>\n"
      << "\nOptions:\n"
      << "  -h              Show this help message\n"
      << "  -min            Print the path with the minimum total size "
         "(default)\n"
      << "  -max            Print the path with the maximum total size\n"
      << "  -all            Print all duplicate folders (same Chinese name)\n"
      << "  -print0         Use null character as delimiter (for use with "
         "xargs -0)\n"
      << "\nDescription:\n"
      << "  Extract Chinese name from brackets [...] in folder names.\n"
      << "  For duplicate names, display the path with the smallest total file "
         "size.\n";
}

int main(int argc, char *argv[]) {
  std::string path_str = ".";
  bool print_max = false;  // 默认打印最小，-max 打印最大
  bool print_all = false;  // -all 显示所有重复文件夹
  bool use_print0 = false; // 默认不使用空字符分隔

  // 手动解析参数，支持 -min 和 -max
  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h") {
      print_usage(argv[0]);
      return 0;
    } else if (arg == "-min") {
      print_max = false;
    } else if (arg == "-max") {
      print_max = true;
    } else if (arg == "-all") {
      print_all = true;
    } else if (arg == "-print0") {
      use_print0 = true;
    } else if (arg[0] != '-') {
      // 非选项参数（目录路径）
      path_str = arg;
    } else {
      std::cerr << "Error: Unknown option '" << arg << "'\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  // 检查路径是否存在且是目录
  if (!fs::exists(path_str)) {
    std::cerr << "Error: Path does not exist: '" << path_str << "'\n";
    return 1;
  }

  if (!fs::is_directory(path_str)) {
    std::cerr << "Error: Path is not a directory: '" << path_str << "'\n";
    return 1;
  }

  // 存储每个中文名对应的所有路径及其大小、时间
  // Chinese name -> vector of (path, total_size, mtime)
  std::unordered_map<
      std::string,
      std::vector<std::tuple<fs::path, uintmax_t, fs::file_time_type>>>
      name_map;

  try {
    for (const auto &entry : fs::directory_iterator(path_str)) {
      if (entry.is_directory()) {
        fs::path dir_path = entry.path();
        std::string folder_name = dir_path.filename().string();
        std::string chinese_name = extract_chinese_name(folder_name);

        if (!chinese_name.empty()) {
          uintmax_t total_size = calculate_total_size(dir_path);
          fs::file_time_type mtime = fs::last_write_time(dir_path);
          name_map[chinese_name].push_back({dir_path, total_size, mtime});
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    std::cerr << "Error: Failed to read directory: " << e.what() << "\n";
    return 1;
  }

  // 对每个中文名，输出路径
  for (const auto &[chinese_name, entries] : name_map) {
    if (entries.size() > 1) {
      if (print_all) {
        // -all: 显示所有重复的文件夹
        for (const auto &[path, size, mtime] : entries) {
          if (use_print0) {
            std::cout << path.string();
            std::cout.put('\0');
          } else {
            std::cout << chinese_name << " -> '" << path.string() << "' "
                      << format_time(mtime) << " " << format_size(size) << "\n";
          }
        }
      } else {
        // 默认: 找出总大小最小或最大的路径
        auto extreme_entry =
            print_max
                ? std::max_element(entries.begin(), entries.end(),
                                   [](const auto &a, const auto &b) {
                                     return std::get<1>(a) < std::get<1>(b);
                                   })
                : std::min_element(entries.begin(), entries.end(),
                                   [](const auto &a, const auto &b) {
                                     return std::get<1>(a) < std::get<1>(b);
                                   });
        if (use_print0) {
          // 输出路径 + '\0'（便于与 xargs -0 配合）
          std::cout << std::get<0>(*extreme_entry).string();
          std::cout.put('\0');
        } else {
          std::cout << chinese_name << " -> '"
                    << std::get<0>(*extreme_entry).string() << "' "
                    << format_time(std::get<2>(*extreme_entry)) << " "
                    << format_size(std::get<1>(*extreme_entry)) << "\n";
        }
      }
    }
  }

  return 0;
}