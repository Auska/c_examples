#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <getopt.h>
#include <ranges>

namespace fs = std::filesystem;

// 计算 Levenshtein 距离（优化空间复杂度为 O(min(n,m))）
size_t levenshtein_distance(const std::string& s1, const std::string& s2) {
    // 确保 s1 是较短的字符串，以最小化空间使用
    const std::string* shorter = &s1;
    const std::string* longer = &s2;
    if (s1.size() > s2.size()) {
        std::swap(shorter, longer);
    }

    size_t n = shorter->size();
    size_t m = longer->size();

    // 只使用两行 DP 数组
    std::vector<size_t> prev_row(n + 1);
    std::vector<size_t> curr_row(n + 1);

    // 初始化第一行
    for (size_t i = 0; i <= n; ++i) {
        prev_row[i] = i;
    }

    // 填充 DP 表
    for (size_t j = 1; j <= m; ++j) {
        curr_row[0] = j;
        for (size_t i = 1; i <= n; ++i) {
            if ((*shorter)[i - 1] == (*longer)[j - 1]) {
                curr_row[i] = prev_row[i - 1];
            } else {
                curr_row[i] = 1 + std::min({prev_row[i], curr_row[i - 1], prev_row[i - 1]});
            }
        }
        std::swap(prev_row, curr_row);
    }

    return prev_row[n];
}

// 计算 Levenshtein 相似度 (0.0 ~ 1.0)
double levenshtein_similarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;
    size_t distance = levenshtein_distance(a, b);
    size_t max_len = std::max(a.size(), b.size());
    return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

int main(int argc, char* argv[]) {
    double threshold = 0.9;

    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "s:h")) != -1) {
        if (opt == 's') {
            try {
                threshold = std::stod(optarg);
                if (threshold < 0.0 || threshold > 1.0) {
                    std::cerr << "Error: Similarity threshold must be between 0.0 and 1.0\n";
                    return 1;
                }
            } catch (...) {
                std::cerr << "Error: Invalid threshold value: " << optarg << std::endl;
                return 1;
            }
        } else if (opt == 'h') {
            std::cout << "Usage: " << argv[0] << " [OPTIONS] [directory1 [directory2 ...]]\n"
                      << "\nOptions:\n"
                      << "  -s <threshold>  Set similarity threshold (0.0 ~ 1.0, default: 0.9)\n"
                      << "  -h              Show this help message\n"
                      << "\nDescription:\n"
                      << "  Compare folder names in the specified directories using Levenshtein distance.\n"
                      << "  If no directories are specified, the current directory is used.\n"
                      << "  Only pairs with similarity >= threshold are displayed.\n";
            return 0;
        } else {
            std::cerr << "Error: Unknown option '" << static_cast<char>(optopt) << "'\n";
            std::cerr << "Use '" << argv[0] << " -h' for help.\n";
            return 1;
        }
    }

    // 剩余参数都是目录路径
    std::vector<std::string> dir_paths;
    for (int i = optind; i < argc; ++i) {
        dir_paths.emplace_back(argv[i]);
    }

    // 如果没有传目录，默认使用当前目录
    if (dir_paths.empty()) {
        dir_paths.emplace_back(".");
    }

    // 存储每个文件夹名 -> 它的完整绝对路径列表（来自不同父目录）
    std::unordered_map<std::string, std::vector<fs::path>> name_to_paths;

    for (const auto& dir_path : dir_paths) {
        if (!fs::exists(dir_path)) {
            std::cerr << "Warning: Directory does not exist: " << dir_path << std::endl;
            continue;
        }
        if (!fs::is_directory(dir_path)) {
            std::cerr << "Warning: Not a directory (skipped): " << dir_path << std::endl;
            continue;
        }

        // 获取当前目录的绝对路径
        fs::path abs_parent;
        try {
            abs_parent = fs::absolute(dir_path);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Cannot get absolute path for: " << dir_path << " -> " << e.what() << std::endl;
            continue;
        }

        try {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (entry.is_directory()) {
                    fs::path name = entry.path().filename();
                    fs::path full_path = abs_parent / name;  // 构造完整路径
                    name_to_paths[name.string()].push_back(full_path);
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Error reading directory: " << dir_path << " -> " << e.what() << std::endl;
        }
    }

    // 提取所有唯一的文件夹名
    std::vector<std::string> unique_names;
    for (const auto& name : std::views::keys(name_to_paths)) {
        unique_names.push_back(name);
    }

    if (unique_names.size() < 2) {
        std::cout << "Only " << unique_names.size() << " unique subfolder names found. Nothing to compare.\n";
        return 0;
    }

    // 排序以便输出一致
    std::ranges::sort(unique_names.begin(), unique_names.end());

    // 比较所有名字对
    bool found = false;
    for (size_t i = 0; i < unique_names.size(); ++i) {
        for (size_t j = i + 1; j < unique_names.size(); ++j) {
            const std::string& name1 = unique_names[i];
            const std::string& name2 = unique_names[j];

            double sim = levenshtein_similarity(name1, name2);
            if (sim >= threshold) {
                std::cout << "Similarity: " << sim << "\n";

                // 打印 name1 的所有绝对路径
                for (const auto& path : name_to_paths[name1]) {
                    std::cout << "\t\"" << path.string() << "\"\n";
                }
                // 打印 name2 的所有绝对路径
                for (const auto& path : name_to_paths[name2]) {
                    std::cout << "\t\"" << path.string() << "\"\n";
                }
                std::cout << "\n";

                found = true;
            }
        }
    }

    if (!found) {
        std::cout << "No folder name pairs with similarity >= " << threshold << "\n";
    }

    return 0;
}
