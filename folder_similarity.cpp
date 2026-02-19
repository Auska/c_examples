#include <iostream>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <algorithm>
#include <getopt.h>
#include <ranges>
#include <expected>
#include <variant>
#include <chrono>
#include <iomanip>

namespace fs = std::filesystem;

// 定义错误类型
enum class Error {
    None,
    DirectoryNotFound,
    NotADirectory,
    CannotGetAbsolutePath,
    CannotReadDirectory,
    InvalidThreshold,
    UnknownOption
};

// 错误消息映射
std::string error_to_string(Error err) {
    switch (err) {
        case Error::DirectoryNotFound: return "Directory does not exist";
        case Error::NotADirectory: return "Not a directory";
        case Error::CannotGetAbsolutePath: return "Cannot get absolute path";
        case Error::CannotReadDirectory: return "Cannot read directory";
        case Error::InvalidThreshold: return "Invalid threshold value";
        case Error::UnknownOption: return "Unknown option";
        default: return "Unknown error";
    }
}

// Result 类型别名
template<typename T>
using Result = std::expected<T, std::pair<Error, std::string>>;

// 计算 Levenshtein 距离（优化空间复杂度为 O(min(n,m))）
[[nodiscard]] size_t levenshtein_distance(const std::string& s1, const std::string& s2) {
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
[[nodiscard]] double levenshtein_similarity(const std::string& a, const std::string& b) {
    if (a.empty() && b.empty()) return 1.0;
    if (a.empty() || b.empty()) return 0.0;
    size_t distance = levenshtein_distance(a, b);
    size_t max_len = std::max(a.size(), b.size());
    return 1.0 - (static_cast<double>(distance) / static_cast<double>(max_len));
}

// Union-Find 数据结构
class UnionFind {
    std::vector<size_t> parent_;
    std::vector<size_t> rank_;

public:
    explicit UnionFind(size_t n) : parent_(n), rank_(n, 0) {
        for (size_t i = 0; i < n; ++i) {
            parent_[i] = i;
        }
    }

    size_t find(size_t x) {
        if (parent_[x] != x) {
            parent_[x] = find(parent_[x]);  // 路径压缩
        }
        return parent_[x];
    }

    void unite(size_t x, size_t y) {
        size_t px = find(x);
        size_t py = find(y);
        if (px == py) return;

        // 按秩合并
        if (rank_[px] < rank_[py]) {
            parent_[px] = py;
        } else if (rank_[px] > rank_[py]) {
            parent_[py] = px;
        } else {
            parent_[py] = px;
            rank_[px]++;
        }
    }
};

// 递归计算文件夹大小
[[nodiscard]] uintmax_t get_folder_size(const fs::path& dir_path) {
    uintmax_t size = 0;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                size += entry.file_size();
            }
        }
    } catch (const fs::filesystem_error&) {
        // 忽略权限错误
    }
    return size;
}

// 格式化文件大小
[[nodiscard]] std::string format_size(uintmax_t size) {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit_idx = 0;
    double s = static_cast<double>(size);
    while (s >= 1024.0 && unit_idx < 4) {
        s /= 1024.0;
        unit_idx++;
    }
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << s << units[unit_idx];
    return oss.str();
}

// 格式化修改时间（参考 oldsort.cpp）
[[nodiscard]] std::string format_time(fs::file_time_type ftime) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
    );
    std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
    std::tm* tm = std::localtime(&tt);
    
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d+%H:%M:%S", tm);
    return buffer;
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
                std::cerr << "Error: Invalid threshold value: " << optarg << "\n";
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
        // 检查目录是否存在
        if (!fs::exists(dir_path)) {
            std::cerr << "Warning: Directory does not exist: " << dir_path << "\n";
            continue;
        }
        
        // 检查是否为目录
        if (!fs::is_directory(dir_path)) {
            std::cerr << "Warning: Not a directory (skipped): " << dir_path << "\n";
            continue;
        }

        // 获取当前目录的绝对路径
        fs::path abs_parent;
        try {
            abs_parent = fs::absolute(dir_path);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Warning: Cannot get absolute path for: " << dir_path << " -> " << e.what() << "\n";
            continue;
        }

        // 遍历目录
        try {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                if (entry.is_directory()) {
                    fs::path name = entry.path().filename();
                    fs::path full_path = abs_parent / name;  // 构造完整路径
                    name_to_paths[name.string()].push_back(full_path);
                }
            }
        } catch (const fs::filesystem_error& e) {
            std::cerr << "Warning: Error reading directory: " << dir_path << " -> " << e.what() << "\n";
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

    // 使用 Union-Find 将相似的名字合并到同一组
    UnionFind uf(unique_names.size());

    for (size_t i = 0; i < unique_names.size(); ++i) {
        for (size_t j = i + 1; j < unique_names.size(); ++j) {
            const std::string& name1 = unique_names[i];
            const std::string& name2 = unique_names[j];

            double sim = levenshtein_similarity(name1, name2);
            if (sim >= threshold) {
                uf.unite(i, j);
            }
        }
    }

    // 按根节点分组
    std::unordered_map<size_t, std::vector<std::string>> groups;
    for (size_t i = 0; i < unique_names.size(); ++i) {
        size_t root = uf.find(i);
        groups[root].push_back(unique_names[i]);
    }

    // 输出至少有2个成员的组
    bool found = false;
    for (const auto& group : std::views::values(groups)) {
        if (group.size() < 2) continue;

        found = true;
        // 计算组内所有对的最小相似度
        double min_sim = 1.0;
        for (size_t i = 0; i < group.size(); ++i) {
            for (size_t j = i + 1; j < group.size(); ++j) {
                double sim = levenshtein_similarity(group[i], group[j]);
                min_sim = std::min(min_sim, sim);
            }
        }
        std::cout << "Group (min similarity: " << min_sim << "):\n";

        // 打印组内每个名字的所有绝对路径
        for (const auto& name : group) {
            std::cout << "  \"" << name << "\":\n";
            for (const auto& path : name_to_paths[name]) {
                try {
                    auto mtime = fs::last_write_time(path);
                    uintmax_t size = get_folder_size(path);
                    std::cout << "    \"" << path.string() << "\" "
                              << format_time(mtime) << " " << format_size(size) << "\n";
                } catch (const fs::filesystem_error& e) {
                    std::cout << "    \"" << path.string() << "\" (error: " << e.what() << ")\n";
                }
            }
        }
        std::cout << "\n";
    }

    if (!found) {
        std::cout << "No folder name groups with similarity >= " << threshold << "\n";
    }

    return 0;
}
