#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <algorithm>
#include <getopt.h>
#include <ranges>

namespace fs = std::filesystem;

// 计算 Levenshtein 距离
size_t levenshtein_distance(const std::string& s1, const std::string& s2) {
    // 修复 levenshtein_distance 函数中变量类型
    size_t n = s1.size();
    size_t m = s2.size();
    std::vector<std::vector<size_t>> dp(n + 1, std::vector<size_t>(m + 1, 0));

    for (size_t i = 0; i <= n; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= m; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= n; ++i) {
        for (size_t j = 1; j <= m; ++j) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }

    return dp[n][m];
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

    // 解析 -s 参数
    int opt;
    while ((opt = getopt(argc, argv, "s:")) != -1) {
        // 替换原来的 switch 语句部分
        if (opt == 's') {
            try {
                threshold = std::stod(optarg);
                if (threshold < 0.0 || threshold > 1.0) {
                    std::cerr << "Similarity threshold must be between 0.0 and 1.0\n";
                    return 1;
                }
            } catch (...) {
                std::cerr << "Invalid threshold value: " << optarg << std::endl;
                return 1;
            }
        } else {
            std::cerr << "Usage: " << argv[0] << " [-s similarity_threshold] directory1 [directory2 ...]\n";
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
    std::map<std::string, std::vector<fs::path>> name_to_paths;

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
                    std::string name = entry.path().filename().string();
                    fs::path full_path = abs_parent / name;  // 构造完整路径
                    name_to_paths[name].push_back(full_path);
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
