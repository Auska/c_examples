#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <getopt.h>
#include <charconv>
#include <chrono>
#include <expected>

namespace fs = std::filesystem;

// 定义错误类型
enum class Error {
    None,
    PathNotFound,
    NotADirectory,
    FailedToReadDirectory,
    InvalidLimit,
    UnknownOption
};

// 错误消息映射
std::string error_to_string(Error err) {
    switch (err) {
        case Error::PathNotFound: return "Path does not exist";
        case Error::NotADirectory: return "Path is not a directory";
        case Error::FailedToReadDirectory: return "Failed to read directory";
        case Error::InvalidLimit: return "Invalid limit value";
        case Error::UnknownOption: return "Unknown option";
        default: return "Unknown error";
    }
}

// Result 类型别名
template<typename T>
using Result = std::expected<T, std::pair<Error, std::string>>;

// 存储目录信息和时间戳
struct DirInfo {
    fs::path path;
    fs::file_time_type time;
};

// 使用 std::from_chars 解析整数（性能优于 stoi，不抛异常）
[[nodiscard]] bool parse_int(const std::string& s, int& result) {
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), result);
    return ec == std::errc() && ptr == s.data() + s.size();
}

int main(int argc, char* argv[]) {
    std::string pathStr = ".";
    int limit = -1;           // -1 表示不限制
    bool use_null_delim = false; // 是否使用 \0 分隔符

    // 使用 getopt 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "l:0h")) != -1) {
        if (opt == 'l') {
            if (!parse_int(optarg, limit) || limit <= 0) {
                std::cerr << "Error: -l requires a positive integer\n";
                return 1;
            }
        } else if (opt == '0') {
            use_null_delim = true;
        } else if (opt == 'h') {
            std::cout << "Usage: " << argv[0] << " [OPTIONS] [directory]\n"
                      << "\nOptions:\n"
                      << "  -l <number>     Limit output to the oldest N directories (default: no limit)\n"
                      << "  -0              Use null character as delimiter (for use with xargs)\n"
                      << "  -h              Show this help message\n"
                      << "\nDescription:\n"
                      << "  List all directories in the specified path, sorted by last modification time\n"
                      << "  from oldest to newest. If no directory is specified, the current directory is used.\n";
            return 0;
        } else {
            std::cerr << "Error: Unknown option '" << static_cast<char>(optopt) << "'\n";
            std::cerr << "Use '" << argv[0] << " -h' for help.\n";
            return 1;
        }
    }

    // 处理剩余的非选项参数（目录路径）
    if (optind < argc) {
        pathStr = argv[optind];
    }

    // 检查路径是否存在且是目录
    if (!fs::exists(pathStr)) {
        std::cerr << "Error: " << error_to_string(Error::PathNotFound) << ": '" << pathStr << "'\n";
        return 1;
    }

    if (!fs::is_directory(pathStr)) {
        std::cerr << "Error: " << error_to_string(Error::NotADirectory) << ": '" << pathStr << "'\n";
        return 1;
    }

    std::vector<DirInfo> all_directories;

    try {
        // 递归遍历所有子目录，跳过无权限的目录
        auto opts = fs::directory_options::skip_permission_denied;
        for (const auto& entry : fs::recursive_directory_iterator(pathStr, opts)) {
            if (entry.is_directory()) {
                try {
                    DirInfo info;
                    info.path = entry.path();
                    info.time = fs::last_write_time(entry);
                    all_directories.push_back(info);
                } catch (const fs::filesystem_error&) {
                    // 跳过无法获取时间的目录
                }
            }
        }

        // 加入根目录本身
        try {
            DirInfo root_info;
            root_info.path = pathStr;
            root_info.time = fs::last_write_time(pathStr);
            all_directories.push_back(root_info);
        } catch (const fs::filesystem_error&) {
            // 根目录时间获取失败，跳过
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error: " << error_to_string(Error::FailedToReadDirectory) << ": " << e.what() << "\n";
        return 1;
    }

    // 按最后修改时间排序：从旧到新（时间戳已预计算，直接比较）
    std::ranges::sort(all_directories.begin(), all_directories.end(),
        [](const DirInfo& a, const DirInfo& b) {
            return a.time < b.time;
        });

    // 输出结果：使用 \0 或 \n 分隔
    size_t count = all_directories.size();
    size_t max_output = (limit == -1) ? count : std::min<size_t>(limit, count);

    for (size_t i = 0; i < max_output; ++i) {
        const auto& dir = all_directories[i];
        if (use_null_delim) {
            // 输出路径 + '\0'（不换行）
            std::cout << dir.path.string();
            std::cout.put('\0');
        } else {
            // 原始带时间格式输出（使用预计算的时间戳）
            auto ftime = dir.time;
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
            );
            std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
            std::tm* tm = std::localtime(&tt);

            std::cout << std::put_time(tm, "%Y-%m-%d+%H:%M:%S")
                      << "  " << dir.path.string() << '\n';
        }
    }

    // 只有在非 null 分隔时才输出统计信息（避免破坏 xargs 流）
    if (!use_null_delim) {
        if (limit != -1) {
            std::cout << "\n(Showing oldest " << max_output << " directories)\n";
        } else {
            std::cout << "\nFound " << count << " directories.\n";
        }
    }

    return 0;
}
