#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include <ranges>
#include <cctype>

namespace fs = std::filesystem;

// 检查字符串是否为纯数字
bool is_number(const std::string& s) {
    if (s.empty()) return false;
    for (char c : s) {
        if (!std::isdigit(c)) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
    std::string pathStr = ".";
    int limit = -1;           // -1 表示不限制
    bool use_null_delim = false; // 是否使用 \0 分隔符

        // 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-l") {
            if (limit == -1 && i + 1 < argc && is_number(argv[i + 1])) {
                limit = std::stoi(argv[++i]); // 消费下一个参数
            } else {
                // -l 单独出现，默认为 1（或你可以改为报错）
                limit = 1;
            }
        } else if (arg.size() > 2 && arg.substr(0, 2) == "-l" && arg.size() > 2) {
            // 支持 -l10
            std::string numStr = arg.substr(2);
            if (is_number(numStr)) {
                limit = std::stoi(numStr);
                if (limit <= 0) {
                    std::cerr << "错误：-l 后面的数字必须大于 0\n";
                    return 1;
                }
            } else {
                std::cerr << "错误：-l 后面必须跟一个正整数，例如 -l10\n";
                return 1;
            }
        } else if (arg == "-0") {
            use_null_delim = true;
        } else {
            pathStr = arg; // 认为是路径
        }
    }

    // 检查路径是否存在且是目录
    if (!fs::exists(pathStr)) {
        std::cerr << "错误：路径不存在 '" << pathStr << "'\n";
        return 1;
    }

    if (!fs::is_directory(pathStr)) {
        std::cerr << "错误：提供的路径不是目录 '" << pathStr << "'\n";
        return 1;
    }

    std::vector<fs::path> all_directories;

    // 递归遍历所有子目录
    for (const auto& entry : fs::recursive_directory_iterator(pathStr)) {
        if (entry.is_directory()) {
            all_directories.push_back(entry.path());
        }
    }

    // 加入根目录本身
    all_directories.push_back(pathStr);

    // 按最后修改时间排序：从旧到新
    std::ranges::sort(all_directories.begin(), all_directories.end(),
        [](const fs::path& a, const fs::path& b) {
            return fs::last_write_time(a) < fs::last_write_time(b);
        });

    // 输出结果：使用 \0 或 \n 分隔
    size_t count = all_directories.size();
    size_t max_output = (limit == -1) ? count : std::min<size_t>(limit, count);

    for (size_t i = 0; i < max_output; ++i) {
        const auto& dir = all_directories[i];
        try {
            if (use_null_delim) {
                // 输出路径 + '\0'（不换行）
                std::cout << dir.string();
                std::cout.put('\0');
            } else {
                // 原始带时间格式输出
                auto ftime = fs::last_write_time(dir);
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
                );
                std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
                std::tm* tm = std::localtime(&tt);

                std::cout << std::put_time(tm, "%Y-%m-%d+%H:%M:%S")
                          << "  " << dir.string() << '\n';
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "无法获取时间: " << dir << " (" << e.what() << ")\n";
        }
    }

    // 只有在非 null 分隔时才输出统计信息（避免破坏 xargs 流）
    if (!use_null_delim) {
        if (limit != -1) {
            std::cout << "\n(仅显示最老的 " << max_output << " 个文件夹)\n";
        } else {
            std::cout << "\n共找到 " << count << " 个文件夹。\n";
        }
    }

    return 0;
}
