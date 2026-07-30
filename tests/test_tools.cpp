#include "common/common.hpp"
#include "external/catch_amalgamated.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ==================== 工具函数 ====================

class TempDirFixture {
 protected:
  fs::path temp_dir_;

  TempDirFixture() {
    temp_dir_ = fs::temp_directory_path() /
                ("test_integration_" + std::to_string(std::time(nullptr)));
    fs::create_directories(temp_dir_);
  }

  ~TempDirFixture() {
    std::error_code ec;
    fs::remove_all(temp_dir_, ec);
  }

  void create_file(const fs::path& path, std::string content = "test") {
    fs::create_directories(path.parent_path());
    std::ofstream file(path);
    file << content;
    file.close();
  }

  void create_dir(const fs::path& path) {
    fs::create_directories(path);
  }
};

// ==================== output_utils 集成测试 ====================

TEST_CASE_METHOD(TempDirFixture, "output::print_column_line with real data",
                 "[output][integration]") {
  SECTION("formats time and size from real paths") {
    create_dir(temp_dir_ / "test_folder");
    create_file(temp_dir_ / "test_folder" / "file.txt", "hello");

    auto ft = fs::last_write_time(temp_dir_ / "test_folder");
    const auto time_str = common::format_time(ft);
    const auto size_str = common::format_size(
        common::calculate_total_size(temp_dir_ / "test_folder"));

    std::ostringstream ss;
    common::output::print_column_line(
        ss, time_str, size_str,
        (temp_dir_ / "test_folder").string(),
        common::display_width(time_str),
        common::display_width(size_str));

    // 验证输出格式：时间 + 大小 + 路径
    const auto output = ss.str();
    REQUIRE(output.find(time_str) != std::string::npos);
    REQUIRE(output.find(size_str) != std::string::npos);
    REQUIRE(output.find("test_folder") != std::string::npos);
    REQUIRE(output.back() == '\n');
  }
}

TEST_CASE_METHOD(TempDirFixture, "output::print_print0_paths with real paths",
                 "[output][integration]") {
  SECTION("outputs null-delimited real paths") {
    create_dir(temp_dir_ / "a");
    create_dir(temp_dir_ / "b");

    std::vector<std::string> paths = {
        (temp_dir_ / "a").string(),
        (temp_dir_ / "b").string()
    };

    std::ostringstream ss;
    common::output::print_print0_paths(ss, paths, paths.size());

    std::string output = ss.str();
    REQUIRE(output.find('\0') != std::string::npos);
    REQUIRE(output.find((temp_dir_ / "a").string()) != std::string::npos);
    REQUIRE(output.find((temp_dir_ / "b").string()) != std::string::npos);
  }
}

// ==================== 工具处理流程集成测试 ====================

TEST_CASE_METHOD(TempDirFixture, "calculate_total_size with nested structure",
                 "[common][integration]") {
  SECTION("various file sizes are summed correctly") {
    create_file(temp_dir_ / "a" / "f1.txt", std::string(100, 'x'));
    create_file(temp_dir_ / "a" / "f2.txt", std::string(200, 'y'));
    create_file(temp_dir_ / "b" / "f3.txt", std::string(300, 'z'));
    create_dir(temp_dir_ / "empty");

    REQUIRE(common::calculate_total_size(temp_dir_ / "a") == 300);
    REQUIRE(common::calculate_total_size(temp_dir_ / "b") == 300);
    REQUIRE(common::calculate_total_size(temp_dir_ / "empty") == 0);
    REQUIRE(common::calculate_total_size(temp_dir_) == 600);
  }
}

TEST_CASE_METHOD(TempDirFixture, "size_cache with real directory changes",
                 "[common][integration]") {
  SECTION("cache returns same value for same directory") {
    create_file(temp_dir_ / "data.txt", "initial");

    common::size_cache sc;
    const auto first = sc.get(temp_dir_);
    const auto second = sc.get(temp_dir_);

    REQUIRE(first == second);
    REQUIRE(first > 0);
  }
}

// ==================== 工具 parse_args 测试 ====================

TEST_CASE("folder_similarity::parse_args processes help", "[tool][integration]") {
  // 这里测试 cli::parser 的 help 功能
  common::cli::parser parser("Test");
  parser.add_option({"--test", 't', "Test option", false});

  SECTION("-h returns help sentinel") {
    const char* argv[] = {"program", "-h"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == common::cli::k_help_sentinel);
  }

  SECTION("--help returns help sentinel") {
    const char* argv[] = {"program", "--help"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == common::cli::k_help_sentinel);
  }
}

TEST_CASE("extract_name::parse_args processes limit option", "[tool][integration]") {
  common::cli::parser parser("Test");
  parser.add_option({"--limit", 'l', "Limit", true});

  SECTION("valid limit") {
    const char* argv[] = {"program", "-l", "5"};
    auto result = parser.parse(3, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--limit") == "5");
  }

  SECTION("combined short option") {
    const char* argv[] = {"program", "-l10"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--limit") == "10");
  }
}

TEST_CASE("oldsort::parse_args processes size options", "[tool][integration]") {
  common::cli::parser parser("Test");
  parser.add_option({"--min", 'm', "Sort ascending", false});
  parser.add_option({"--max", 'M', "Sort descending", false});

  SECTION("--min") {
    const char* argv[] = {"program", "--min"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::has_option(*result, "--min"));
  }

  SECTION("--max") {
    const char* argv[] = {"program", "--max"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::has_option(*result, "--max"));
  }
}

// ==================== 文件系统交互集成测试 ====================

TEST_CASE_METHOD(TempDirFixture, "validate_directory with real paths",
                 "[fs][integration]") {
  SECTION("valid directory returns absolute path") {
    auto result = common::validate_directory(temp_dir_.string());
    REQUIRE(result.has_value());
    REQUIRE(result->is_absolute());
  }

  SECTION("non-existent path returns error") {
    auto result = common::validate_directory("/nonexistent_path_12345");
    REQUIRE(!result.has_value());
  }

  SECTION("file path returns error") {
    auto file_path = temp_dir_ / "not_a_dir.txt";
    create_file(file_path);
    auto result = common::validate_directory(file_path.string());
    REQUIRE(!result.has_value());
  }
}

TEST_CASE_METHOD(TempDirFixture, "format_time with real directory",
                 "[time][integration]") {
  SECTION("returns correctly formatted time") {
    auto ft = fs::last_write_time(temp_dir_);
    auto time_str = common::format_time(ft);
    REQUIRE(time_str.size() == 19);
    REQUIRE(time_str[4] == '-');
    REQUIRE(time_str[7] == '-');
    REQUIRE(time_str[10] == '+');
  }
}
