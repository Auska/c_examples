#define CATCH_CONFIG_MAIN

#include "common/common.hpp"
#include "catch_amalgamated.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

using Catch::Approx;
namespace fs = std::filesystem;

// ==================== common.hpp 测试 ====================

TEST_CASE("format_size formats bytes correctly", "[common]") {
  SECTION("bytes") {
    REQUIRE(common::format_size(0) == "0 B");
    REQUIRE(common::format_size(512) == "512 B");
    REQUIRE(common::format_size(1023) == "1023 B");
  }

  SECTION("kilobytes") {
    REQUIRE(common::format_size(1024) == "1.00 KB");
    REQUIRE(common::format_size(1536) == "1.50 KB");
    REQUIRE(common::format_size(2048) == "2.00 KB");
  }

  SECTION("megabytes") {
    REQUIRE(common::format_size(1048576) == "1.00 MB");
    REQUIRE(common::format_size(1572864) == "1.50 MB");
  }

  SECTION("gigabytes") {
    REQUIRE(common::format_size(1073741824ULL) == "1.00 GB");
  }

  SECTION("terabytes") {
    REQUIRE(common::format_size(1099511627776ULL) == "1.00 TB");
  }

  SECTION("large values") {
    REQUIRE(common::format_size(999ULL * 1024 * 1024 * 1024) != "");
  }
}

TEST_CASE("extract_bracket_content extracts content correctly", "[common]") {
  SECTION("simple bracket") {
    REQUIRE(common::extract_bracket_content("test[name]test") == "name");
  }

  SECTION("multiple brackets - returns last") {
    REQUIRE(common::extract_bracket_content("[first][second]test") == "second");
  }

  SECTION("nested brackets") {
    REQUIRE(common::extract_bracket_content("test[inner]end") == "inner");
  }

  SECTION("no brackets") {
    REQUIRE(common::extract_bracket_content("no brackets here").empty());
  }

  SECTION("only opening bracket") {
    REQUIRE(common::extract_bracket_content("test[noend").empty());
  }

  SECTION("only closing bracket") {
    REQUIRE(common::extract_bracket_content("test]noend").empty());
  }

  SECTION("empty brackets") {
    REQUIRE(common::extract_bracket_content("test[]end") == "");
  }

  SECTION("Chinese characters") {
    REQUIRE(common::extract_bracket_content("Game[游戏名称]v1.0") ==
            "游戏名称");
  }

  SECTION("complex filename") {
    REQUIRE(common::extract_bracket_content("SomeGame[中文名][v1.0][Author]") ==
            "Author");
  }

  SECTION("empty string") {
    REQUIRE(common::extract_bracket_content("").empty());
  }

  SECTION("only brackets") {
    REQUIRE(common::extract_bracket_content("[]") == "");
  }

  SECTION("reversed brackets") {
    REQUIRE(common::extract_bracket_content("]test[").empty());
  }
}

TEST_CASE("calculate_total_size handles directories", "[common]") {
  SECTION("non-existent directory returns 0") {
    REQUIRE(common::calculate_total_size("/non/existent/path/12345") == 0);
  }
}

TEST_CASE("size_cache works correctly", "[common]") {
  SECTION("non-existent directory returns 0") {
    common::size_cache sc;
    REQUIRE(sc.get("/non/existent/path/12345") == 0);
  }

  SECTION("cache returns same value on second call") {
    common::size_cache sc;
    const std::uintmax_t first = sc.get("/non/existent/path/12345");
    const std::uintmax_t second = sc.get("/non/existent/path/12345");
    REQUIRE(first == second);
  }
}

// ==================== Levenshtein 距离测试 ====================

TEST_CASE("levenshtein_distance calculates correctly", "[levenshtein]") {
  SECTION("identical strings") {
    REQUIRE(common::levenshtein_distance("hello", "hello") == 0);
    REQUIRE(common::levenshtein_distance("", "") == 0);
  }

  SECTION("empty string") {
    REQUIRE(common::levenshtein_distance("", "abc") == 3);
    REQUIRE(common::levenshtein_distance("abc", "") == 3);
  }

  SECTION("single character difference") {
    REQUIRE(common::levenshtein_distance("kitten", "sitten") == 1);
  }

  SECTION("multiple differences") {
    REQUIRE(common::levenshtein_distance("kitten", "sitting") == 3);
  }

  SECTION("completely different") {
    REQUIRE(common::levenshtein_distance("abc", "xyz") == 3);
  }

  SECTION("insertion") {
    REQUIRE(common::levenshtein_distance("ab", "abc") == 1);
  }

  SECTION("deletion") {
    REQUIRE(common::levenshtein_distance("abc", "ab") == 1);
  }

  SECTION("substitution") {
    REQUIRE(common::levenshtein_distance("abc", "axc") == 1);
  }

  SECTION("single character strings") {
    REQUIRE(common::levenshtein_distance("a", "b") == 1);
    REQUIRE(common::levenshtein_distance("a", "a") == 0);
  }

  SECTION("unicode strings") {
    REQUIRE(common::levenshtein_distance("你好", "你好") == 0);
    // 使用 RapidFuzz byte 级别计算：一个汉字 = 3 字节差异
    REQUIRE(common::levenshtein_distance("你好", "你们") == 3);
  }

  SECTION("long strings") {
    std::string s1(100, 'a');
    std::string s2(100, 'a');
    REQUIRE(common::levenshtein_distance(s1, s2) == 0);

    s2[50] = 'b';
    REQUIRE(common::levenshtein_distance(s1, s2) == 1);
  }
}

TEST_CASE("levenshtein_similarity returns correct values", "[levenshtein]") {
  SECTION("identical strings") {
    REQUIRE(common::levenshtein_similarity("test", "test") ==
            Approx(1.0).epsilon(0.001));
  }

  SECTION("empty strings") {
    REQUIRE(common::levenshtein_similarity("", "") ==
            Approx(1.0).epsilon(0.001));
  }

  SECTION("one empty string") {
    REQUIRE(common::levenshtein_similarity("test", "") ==
            Approx(0.0).epsilon(0.001));
  }

  SECTION("half similar") {
    REQUIRE(common::levenshtein_similarity("ab", "cd") ==
            Approx(0.0).epsilon(0.001));
  }

  SECTION("mostly similar") {
    REQUIRE(common::levenshtein_similarity("testing", "test") > 0.5);
  }

  SECTION("similarity range") {
    std::string s1 = "hello";
    std::string s2 = "hallo";
    double sim = common::levenshtein_similarity(s1, s2);
    REQUIRE(sim >= 0.0);
    REQUIRE(sim <= 1.0);
  }
}

TEST_CASE("similarity_cache works correctly", "[levenshtein]") {
  SECTION("returns same value as uncached") {
    common::similarity_cache sc;
    double cached = sc.get(0, 1, "hello", "hallo");
    double uncached = common::levenshtein_similarity("hello", "hallo");
    REQUIRE(cached == Approx(uncached).epsilon(0.001));
  }

  SECTION("cache returns same value on second call") {
    common::similarity_cache sc;
    double first = sc.get(0, 1, "hello", "hallo");
    double second = sc.get(0, 1, "hello", "hallo");
    REQUIRE(first == second);
  }

  SECTION("indices are normalized") {
    common::similarity_cache sc;
    (void)sc.get(5, 2, "hello", "hallo");
    REQUIRE(sc.contains(2, 5));
  }
}

TEST_CASE("PairHash works correctly", "[common]") {
  common::PairHash hasher;

  SECTION("same pairs have same hash") {
    auto p1 = std::make_pair(1UL, 2UL);
    auto p2 = std::make_pair(1UL, 2UL);
    REQUIRE(hasher(p1) == hasher(p2));
  }

  SECTION("different pairs have different hash (usually)") {
    auto p1 = std::make_pair(1UL, 2UL);
    auto p2 = std::make_pair(2UL, 1UL);
    // Not guaranteed, but very likely
    REQUIRE(hasher(p1) != hasher(p2));
  }
}

// ==================== format_time 测试 ====================

TEST_CASE("format_time formats time correctly", "[common]") {
  SECTION("returns non-empty string") {
    auto now = fs::file_time_type::clock::now();
    std::string result = common::format_time(now);
    REQUIRE(!result.empty());
  }

  SECTION("format has correct pattern") {
    auto now = fs::file_time_type::clock::now();
    std::string result = common::format_time(now);
    // Format: YYYY-MM-DD+HH:MM:SS (length should be 19)
    REQUIRE(result.length() == 19);
    REQUIRE(result[4] == '-');
    REQUIRE(result[7] == '-');
    REQUIRE(result[10] == '+');
    REQUIRE(result[13] == ':');
    REQUIRE(result[16] == ':');
  }

  SECTION("past time") {
    auto past = fs::file_time_type::clock::now() - std::chrono::hours(24);
    std::string result = common::format_time(past);
    REQUIRE(!result.empty());
  }

  SECTION("future time") {
    auto future = fs::file_time_type::clock::now() + std::chrono::hours(24);
    std::string result = common::format_time(future);
    REQUIRE(!result.empty());
  }
}

// ==================== 文件系统集成测试 ====================

class TempDirectoryFixture {
 protected:
  fs::path temp_dir_;

  TempDirectoryFixture() {
    temp_dir_ = fs::temp_directory_path() / ("test_c_examples_" + std::to_string(std::time(nullptr)));
    fs::create_directories(temp_dir_);
  }

  ~TempDirectoryFixture() {
    std::error_code ec;
    fs::remove_all(temp_dir_, ec);
  }

  void create_file(const fs::path& path, std::string content = "test") {
    std::ofstream file(path);
    file << content;
    file.close();
  }
};

TEST_CASE_METHOD(TempDirectoryFixture, "calculate_total_size with real files", "[common]") {
  SECTION("empty directory has size 0") {
    REQUIRE(common::calculate_total_size(temp_dir_) == 0);
  }

  SECTION("single file size is calculated") {
    create_file(temp_dir_ / "test.txt", "hello");
    REQUIRE(common::calculate_total_size(temp_dir_) == 5);
  }

  SECTION("multiple files size is summed") {
    create_file(temp_dir_ / "file1.txt", "12345");  // 5 bytes
    create_file(temp_dir_ / "file2.txt", "abc");    // 3 bytes
    REQUIRE(common::calculate_total_size(temp_dir_) == 8);
  }

  SECTION("nested directory size is calculated") {
    fs::create_directories(temp_dir_ / "subdir");
    create_file(temp_dir_ / "subdir" / "nested.txt", "content");
    REQUIRE(common::calculate_total_size(temp_dir_) == 7);
  }

  SECTION("subdirectories are ignored in file count") {
    fs::create_directories(temp_dir_ / "empty_dir");
    REQUIRE(common::calculate_total_size(temp_dir_) == 0);
  }
}

TEST_CASE_METHOD(TempDirectoryFixture, "size_cache works with real files", "[common]") {
  SECTION("cache prevents recalculation") {
    create_file(temp_dir_ / "test.txt", "cached_content");

    common::size_cache sc;
    const std::uintmax_t first = sc.get(temp_dir_);
    const std::uintmax_t second = sc.get(temp_dir_);

    REQUIRE(first == second);
    REQUIRE(first >= 13);  // "cached_content" length (may vary by platform)
  }
}

// ==================== 边界条件测试 ====================

TEST_CASE("format_size boundary conditions", "[common]") {
  SECTION("maximum uintmax_t") {
    REQUIRE(!common::format_size(std::numeric_limits<std::uintmax_t>::max()).empty());
  }

  SECTION("size exactly at unit boundaries") {
    REQUIRE(common::format_size(1024) == "1.00 KB");
    REQUIRE(common::format_size(1024 * 1024) == "1.00 MB");
    REQUIRE(common::format_size(1024LL * 1024 * 1024) == "1.00 GB");
    REQUIRE(common::format_size(1024LL * 1024 * 1024 * 1024) == "1.00 TB");
  }

  SECTION("size just below unit boundaries") {
    REQUIRE(common::format_size(1023) == "1023 B");
    REQUIRE(common::format_size(1024 * 1024 - 1) != "");
  }
}

TEST_CASE("levenshtein_distance boundary conditions", "[levenshtein]") {
  SECTION("very long strings") {
    std::string long1(10000, 'a');
    std::string long2(10000, 'a');
    REQUIRE(common::levenshtein_distance(long1, long2) == 0);
  }

  SECTION("strings with null characters") {
    std::string with_null = std::string("hello\0world", 11);
    std::string without_null = "helloworld";
    REQUIRE(common::levenshtein_distance(with_null, without_null) == 1);
  }
}

TEST_CASE("extract_bracket_content edge cases", "[common]") {
  SECTION("brackets with special characters") {
    REQUIRE(common::extract_bracket_content("test[!@#$%^&*()]end") == "!@#$%^&*()");
  }

  SECTION("multiple nested brackets") {
    REQUIRE(common::extract_bracket_content("a[b[c]d]e") == "c]d");
  }

  SECTION("brackets with whitespace") {
    REQUIRE(common::extract_bracket_content("test[  spaces  ]end") == "  spaces  ");
  }

  SECTION("only opening brackets") {
    REQUIRE(common::extract_bracket_content("test[[[end").empty());
  }

  SECTION("only closing brackets") {
    REQUIRE(common::extract_bracket_content("test]]]end").empty());
  }

  SECTION("unicode in brackets") {
    REQUIRE(common::extract_bracket_content("prefix[中文测试]suffix") == "中文测试");
  }
}

TEST_CASE("levenshtein_similarity edge cases", "[levenshtein]") {
  SECTION("very long similar strings") {
    std::string base(1000, 'a');
    std::string modified = base;
    modified[500] = 'b';
    double sim = common::levenshtein_similarity(base, modified);
    REQUIRE(sim > 0.99);
  }

  SECTION("completely different long strings") {
    std::string s1(100, 'a');
    std::string s2(100, 'b');
    REQUIRE(common::levenshtein_similarity(s1, s2) == Approx(0.0).epsilon(0.001));
  }

  SECTION("one character difference in large strings") {
    std::string s1(1000, 'x');
    std::string s2(1000, 'x');
    s2[999] = 'y';
    double sim = common::levenshtein_similarity(s1, s2);
    REQUIRE(sim > 0.99);
    REQUIRE(sim < 1.0);
  }
}

TEST_CASE("PairHash collision resistance", "[common]") {
  common::PairHash hasher;

  SECTION("many pairs produce reasonable hash distribution") {
    std::set<size_t> hashes;
    for (size_t i = 0; i < 100; ++i) {
      for (size_t j = i + 1; j < 100; ++j) {
        hashes.insert(hasher(std::make_pair(i, j)));
      }
    }
    // Simple hash function has collisions, just verify basic distribution
    REQUIRE(hashes.size() > 2000);
  }
}

// ==================== extract_season 测试 ====================

TEST_CASE("extract_season extracts season correctly", "[common]") {
  SECTION("S01 format") {
    REQUIRE(common::extract_season("[雪国列车].Snowpiercer.S01.2020") == "S01");
    REQUIRE(common::extract_season("[雪国列车].Snowpiercer.S02.2021") == "S02");
    REQUIRE(common::extract_season("Show.Name.S12.1080p") == "S12");
  }

  SECTION("s01 lowercase format") {
    REQUIRE(common::extract_season("Show.s01.1080p") == "S01");
    REQUIRE(common::extract_season("Show.s99.720p") == "S99");
  }

  SECTION("Season N format") {
    REQUIRE(common::extract_season("[权力的游戏].Game.of.Thrones.Season.1.1080p") == "S01");
    REQUIRE(common::extract_season("Show.Season.5.720p") == "S05");
    REQUIRE(common::extract_season("Show.Season5.720p") == "S05");
    REQUIRE(common::extract_season("Show.Season_3.720p") == "S03");
  }

  SECTION("第N季 format (Chinese)") {
    REQUIRE(common::extract_season("[绝命毒师].Breaking.Bad.第3季.1080p") == "S03");
    REQUIRE(common::extract_season("Show.第1季.720p") == "S01");
    REQUIRE(common::extract_season("Show.第12季.720p") == "S12");
  }

  SECTION("no season") {
    REQUIRE(common::extract_season("[无季数].Some.Movie.2020.1080p").empty());
    REQUIRE(common::extract_season("Movie.Name.2020.1080p").empty());
  }

  SECTION("single digit season") {
    REQUIRE(common::extract_season("Show.S1.1080p") == "S01");
    REQUIRE(common::extract_season("Show.Season.2.1080p") == "S02");
    REQUIRE(common::extract_season("Show.第5季.1080p") == "S05");
  }

  SECTION("double digit season") {
    REQUIRE(common::extract_season("Show.S10.1080p") == "S10");
    REQUIRE(common::extract_season("Show.Season.15.1080p") == "S15");
  }
}

// ==================== extract_name_with_season 测试 ====================

TEST_CASE("extract_name_with_season combines name and season", "[common]") {
  SECTION("name with season") {
    REQUIRE(common::extract_name_with_season("[雪国列车].Snowpiercer.S01.2020") == "雪国列车 S01");
    REQUIRE(common::extract_name_with_season("[雪国列车].Snowpiercer.S02.2021") == "雪国列车 S02");
    REQUIRE(common::extract_name_with_season("[雪国列车].Snowpiercer.S03.2022") == "雪国列车 S03");
  }

  SECTION("name without season") {
    REQUIRE(common::extract_name_with_season("[电影名].Some.Movie.2020.1080p") == "电影名");
  }

  SECTION("different season formats") {
    REQUIRE(common::extract_name_with_season("[权力的游戏].Game.of.Thrones.Season.1") == "权力的游戏 S01");
    REQUIRE(common::extract_name_with_season("[绝命毒师].Breaking.Bad.第3季") == "绝命毒师 S03");
  }

  SECTION("no brackets returns empty") {
    REQUIRE(common::extract_name_with_season("Show.Name.S01.1080p").empty());
  }

  SECTION("complex filenames") {
    REQUIRE(common::extract_name_with_season("[雪国列车].Snowpiercer.S01.2020.1080p.NF.WEB-DL.x264") == "雪国列车 S01");
    REQUIRE(common::extract_name_with_season("[黑镜].Black.Mirror.S05.1080p") == "黑镜 S05");
  }
}

// ==================== extract_season 边界测试 ====================

TEST_CASE("extract_season edge cases", "[common]") {
  SECTION("season number at boundaries") {
    REQUIRE(common::extract_season("Show.S01.1080p") == "S01");
    REQUIRE(common::extract_season("Show.S99.1080p") == "S99");
  }

  SECTION("season in middle of filename") {
    REQUIRE(common::extract_season("Prefix.S05.Middle.Suffix") == "S05");
  }

  SECTION("multiple season patterns - returns first match") {
    // 应该返回第一个匹配的模式
    REQUIRE_FALSE(common::extract_season("Show.S01.S02").empty());
  }

  SECTION("season with underscore separator") {
    REQUIRE(common::extract_season("Show_Name_S03_1080p") == "S03");
  }

  SECTION("season with dot separator") {
    REQUIRE(common::extract_season("Show.Name.S04.1080p") == "S04");
  }

  SECTION("season without separator before") {
    // S 必须前面有分隔符
    REQUIRE(common::extract_season("ShowS01.1080p").empty());
  }

  SECTION("season at start of string") {
    // 字符串开头没有分隔符
    REQUIRE(common::extract_season("S01.1080p").empty());
  }

  SECTION("invalid season formats") {
    REQUIRE(common::extract_season("Show.S.1080p").empty());
    REQUIRE(common::extract_season("Show.SABC.1080p").empty());
    REQUIRE(common::extract_season("Show.S00.1080p") == "S00");  // S00 是有效的
  }
}

// ==================== extract_name_with_season 边界测试 ====================

TEST_CASE("extract_name_with_season edge cases", "[common]") {
  SECTION("empty string") {
    REQUIRE(common::extract_name_with_season("").empty());
  }

  SECTION("only brackets") {
    REQUIRE(common::extract_name_with_season("[]").empty());
  }

  SECTION("brackets with empty content") {
    REQUIRE(common::extract_name_with_season("[].S01.1080p").empty());
  }

  SECTION("multiple brackets with season") {
    // 使用最后一对括号的内容
    auto result = common::extract_name_with_season("[前缀][中文名].S01");
    REQUIRE(result.find("S01") != std::string::npos);
  }

  SECTION("Chinese characters in season pattern") {
    REQUIRE(common::extract_name_with_season("[测试].Show.第1季.1080p") == "测试 S01");
  }
}

// ==================== cli::parser 测试 ====================

TEST_CASE("cli::parser parses basic options", "[cli]") {
  common::cli::parser parser("Test program");
  parser.add_option({"--threshold", 't', "Set threshold", true});
  parser.add_option({"--verbose", 'v', "Verbose mode", false});
  parser.add_positional("input", "Input file");

  SECTION("parses option with argument") {
    const char* argv[] = {"program", "--threshold", "0.5"};
    auto result = parser.parse(3, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--threshold") == "0.5");
  }

  SECTION("parses short option with argument") {
    const char* argv[] = {"program", "-t", "0.8"};
    auto result = parser.parse(3, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--threshold") == "0.8");
  }

  SECTION("parses short option combined with argument") {
    const char* argv[] = {"program", "-t0.9"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--threshold") == "0.9");
  }

  SECTION("parses boolean option") {
    const char* argv[] = {"program", "--verbose"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::has_option(*result, "--verbose"));
  }

  SECTION("parses short boolean option") {
    const char* argv[] = {"program", "-v"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::has_option(*result, "--verbose"));
  }

  SECTION("parses positional argument") {
    const char* argv[] = {"program", "input.txt"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(result->positional.size() == 1);
    REQUIRE(result->positional[0] == "input.txt");
  }

  SECTION("parses multiple arguments") {
    const char* argv[] = {"program", "--threshold", "0.5", "--verbose", "file1", "file2"};
    auto result = parser.parse(6, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(result->positional.size() == 2);
    REQUIRE(result->positional[0] == "file1");
    REQUIRE(result->positional[1] == "file2");
  }
}

TEST_CASE("cli::parser handles errors", "[cli]") {
  common::cli::parser parser("Test program");
  parser.add_option({"--threshold", 't', "Set threshold", true});

  SECTION("returns error for unknown option") {
    const char* argv[] = {"program", "--unknown"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
  }

  SECTION("returns error for option without required argument") {
    const char* argv[] = {"program", "--threshold"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
  }

  SECTION("returns error for short option without argument") {
    const char* argv[] = {"program", "-t"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
  }

  SECTION("returns HELP for -h") {
    const char* argv[] = {"program", "-h"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == common::cli::k_help_sentinel);
  }

  SECTION("returns HELP for --help") {
    const char* argv[] = {"program", "--help"};
    auto result = parser.parse(2, const_cast<char**>(argv));
    REQUIRE(!result.has_value());
    REQUIRE(result.error() == common::cli::k_help_sentinel);
  }
}

TEST_CASE("cli::parser get_option default value", "[cli]") {
  common::cli::parser parser("Test program");
  parser.add_option({"--threshold", 't', "Set threshold", true});

  const char* argv[] = {"program"};
  auto result = parser.parse(1, const_cast<char**>(argv));
  REQUIRE(result.has_value());
  REQUIRE(common::cli::parser::get_option(*result, "--threshold").empty());
  REQUIRE(common::cli::parser::get_option(*result, "--threshold", "default") == "default");
}

TEST_CASE("cli::parser handles mixed options", "[cli]") {
  common::cli::parser parser("Test program");
  parser.add_option({"--input", 'i', "Input file", true});
  parser.add_option({"--output", 'o', "Output file", true});
  parser.add_option({"--verbose", 'v', "Verbose", false});
  parser.add_option({"--debug", 'd', "Debug mode", false});
  parser.add_positional("arg1", "First argument");
  parser.add_positional("arg2", "Second argument");

  SECTION("parses complex command line") {
    const char* argv[] = {"program", "-i", "input.txt", "--output", "out.txt", "-v", "pos1", "pos2"};
    auto result = parser.parse(8, const_cast<char**>(argv));
    REQUIRE(result.has_value());
    REQUIRE(common::cli::parser::get_option(*result, "--input") == "input.txt");
    REQUIRE(common::cli::parser::get_option(*result, "--output") == "out.txt");
    REQUIRE(common::cli::parser::has_option(*result, "--verbose"));
    REQUIRE(result->positional.size() == 2);
  }
}

// ==================== validate_directory 测试 ====================

TEST_CASE("validate_directory validates paths", "[fs]") {
  SECTION("returns error for non-existent path") {
    auto result = common::validate_directory("/non/existent/path/12345");
    REQUIRE(!result.has_value());
    REQUIRE(result.error().find("does not exist") != std::string::npos);
  }

  SECTION("returns error for file instead of directory") {
    fs::path temp_file = fs::temp_directory_path() / "test_file.txt";
    std::ofstream file(temp_file);
    file << "test";
    file.close();

    auto result = common::validate_directory(temp_file.string());
    REQUIRE(!result.has_value());
    REQUIRE(result.error().find("not a directory") != std::string::npos);

    std::error_code ec;
    fs::remove(temp_file, ec);
  }

  SECTION("returns valid path for existing directory") {
    fs::path temp_dir = fs::temp_directory_path() / "test_validate_dir";
    fs::create_directories(temp_dir);

    auto result = common::validate_directory(temp_dir.string());
    REQUIRE(result.has_value());
    REQUIRE(result->string().find("test_validate_dir") != std::string::npos);

    std::error_code ec;
    fs::remove(temp_dir, ec);
  }
}

// ==================== UnionFind 测试 ====================

TEST_CASE("UnionFind basic operations", "[unionfind]") {
  common::UnionFind uf(5);

  SECTION("find returns self for initial state") {
    REQUIRE(uf.find(0) == 0);
    REQUIRE(uf.find(1) == 1);
    REQUIRE(uf.find(2) == 2);
  }

  SECTION("unite merges sets") {
    uf.unite(0, 1);
    REQUIRE(uf.find(0) == uf.find(1));
  }

  SECTION("unite does nothing for same set") {
    uf.unite(0, 1);
    uf.unite(0, 1);
    REQUIRE(uf.find(0) == uf.find(1));
  }

  SECTION("transitive unification works") {
    uf.unite(0, 1);
    uf.unite(1, 2);
    uf.unite(2, 3);
    REQUIRE(uf.find(0) == uf.find(1));
    REQUIRE(uf.find(1) == uf.find(2));
    REQUIRE(uf.find(2) == uf.find(3));
    REQUIRE(uf.find(0) == uf.find(3));
  }

  SECTION("unrelated elements stay separate") {
    uf.unite(0, 1);
    uf.unite(2, 3);
    REQUIRE(uf.find(0) != uf.find(2));
    REQUIRE(uf.find(0) != uf.find(3));
    REQUIRE(uf.find(1) != uf.find(2));
    REQUIRE(uf.find(1) != uf.find(3));
  }
}

TEST_CASE("UnionFind path compression", "[unionfind]") {
  common::UnionFind uf(10);

  SECTION("path compression works") {
    uf.unite(0, 1);
    uf.unite(1, 2);
    uf.unite(2, 3);
    uf.unite(3, 4);

    (void)uf.find(0);
    (void)uf.find(4);

    REQUIRE(uf.find(0) == uf.find(4));
  }
}

TEST_CASE("UnionFind rank merging", "[unionfind]") {
  common::UnionFind uf(100);

  SECTION("many unions work correctly") {
    for (size_t i = 0; i < 99; ++i) {
      uf.unite(i, i + 1);
    }

    for (size_t i = 0; i < 100; ++i) {
      REQUIRE(uf.find(0) == uf.find(i));
    }
  }
}

// ==================== UTF-8 解码测试 ====================

TEST_CASE("utf8_to_codepoints decodes correctly", "[utf8]") {
  SECTION("empty string") {
    auto result = common::utf8_to_codepoints("");
    REQUIRE(result.empty());
  }

  SECTION("ASCII string") {
    auto result = common::utf8_to_codepoints("hello");
    REQUIRE(result.size() == 5);
    REQUIRE(result[0] == 'h');
    REQUIRE(result[4] == 'o');
  }

  SECTION("Chinese characters") {
    auto result = common::utf8_to_codepoints("你好");
    REQUIRE(result.size() == 2);
    REQUIRE(result[0] == 0x4F60);  // 你
    REQUIRE(result[1] == 0x597D);   // 好
  }

  SECTION("mixed ASCII and Chinese") {
    auto result = common::utf8_to_codepoints("hello你好world");
    REQUIRE(result.size() == 12);  // 5 + 2 + 5
  }

  SECTION("emoji characters") {
    auto result = common::utf8_to_codepoints("👋");
    REQUIRE(result.size() == 1);
    REQUIRE(result[0] == 0x1F44B);  // 👋
  }
}

TEST_CASE("decode_utf8 handles edge cases", "[utf8]") {
  SECTION("empty string") {
    auto [cp, next] = common::decode_utf8("", 0);
    REQUIRE(cp == 0);
    REQUIRE(next == 1);
  }

  SECTION("out of bounds position") {
    auto [cp, next] = common::decode_utf8("hello", 10);
    REQUIRE(cp == 0);
    REQUIRE(next == 11);
  }

  SECTION("ASCII characters") {
    auto [cp, next] = common::decode_utf8("abc", 0);
    REQUIRE(cp == 'a');
    REQUIRE(next == 1);
  }

  SECTION("2-byte UTF-8 sequence") {
    auto [cp, next] = common::decode_utf8("你好", 0);
    REQUIRE(cp == 0x4F60);  // 你
    REQUIRE(next == 3);     // 3 bytes for 你
  }
}

// ==================== 字符串辅助函数测试 ====================

TEST_CASE("is_season_sep identifies separators", "[string]") {
  REQUIRE(common::is_season_sep('.'));
  REQUIRE(common::is_season_sep('_'));
  REQUIRE(common::is_season_sep(' '));
  REQUIRE(common::is_season_sep('-'));
  REQUIRE(!common::is_season_sep('a'));
  REQUIRE(!common::is_season_sep('1'));
  REQUIRE(!common::is_season_sep('S'));
}

TEST_CASE("parse_season_number parses correctly", "[string]") {
  SECTION("single digit") {
    auto [num, digits] = common::parse_season_number("S01", 1);
    REQUIRE(num == 1);
    REQUIRE(digits == 2);
  }

  SECTION("two digits") {
    auto [num, digits] = common::parse_season_number("S12", 1);
    REQUIRE(num == 12);
    REQUIRE(digits == 2);
  }

  SECTION("single digit only") {
    auto [num, digits] = common::parse_season_number("S5", 1);
    REQUIRE(num == 5);
    REQUIRE(digits == 1);
  }

  SECTION("no digits at position") {
    auto [num, digits] = common::parse_season_number("SAB", 1);
    REQUIRE(num == 0);
    REQUIRE(digits == 0);
  }

  SECTION("out of bounds") {
    auto [num, digits] = common::parse_season_number("S1", 5);
    REQUIRE(num == 0);
    REQUIRE(digits == 0);
  }
}

TEST_CASE("is_season_boundary identifies boundaries", "[string]") {
  REQUIRE(common::is_season_boundary("S01.", 3));
  REQUIRE(common::is_season_boundary("S01_", 3));
  REQUIRE(common::is_season_boundary("S01 ", 3));
  REQUIRE(common::is_season_boundary("S01-", 3));
  REQUIRE(common::is_season_boundary("S01", 3));  // End of string
  REQUIRE(!common::is_season_boundary("S01X", 3));
}

// ==================== similarity_cache 详细测试 ====================

TEST_CASE("similarity_cache find and contains", "[levenshtein]") {
  common::similarity_cache sc;
  (void)sc.get(1, 2, "hello", "hallo");

  SECTION("find returns pointer to cached value") {
    const double* ptr = sc.find(1, 2);
    REQUIRE(ptr != nullptr);
    REQUIRE(*ptr == Approx(sc.get(1, 2, "hello", "hallo")).epsilon(0.001));
  }

  SECTION("find returns nullptr for missing entry") {
    const double* ptr = sc.find(99, 100);
    REQUIRE(ptr == nullptr);
  }

  SECTION("contains returns true for cached entry") {
    REQUIRE(sc.contains(1, 2));
    REQUIRE(sc.contains(2, 1));  // Normalized
  }

  SECTION("contains returns false for missing entry") {
    REQUIRE(!sc.contains(99, 100));
  }

  SECTION("at returns cached value") {
    double val = sc.at(1, 2);
    REQUIRE(val == Approx(common::levenshtein_similarity("hello", "hallo")).epsilon(0.001));
  }

  SECTION("indices are normalized") {
    (void)sc.get(5, 3, "test", "best");
    REQUIRE(sc.contains(3, 5));
    REQUIRE(sc.find(5, 3) != nullptr);
  }
}

// ==================== extract_season 序数格式测试 ====================

TEST_CASE("extract_season handles ordinal formats", "[common]") {
  SECTION("1st Season") {
    REQUIRE(common::extract_season("Show.1st.Season.1080p") == "S01");
    REQUIRE(common::extract_season("Show.21st.Season.1080p") == "S21");
  }

  SECTION("2nd Season") {
    REQUIRE(common::extract_season("Show.2nd.Season.1080p") == "S02");
    REQUIRE(common::extract_season("Show.22nd.Season.1080p") == "S22");
  }

  SECTION("3rd Season") {
    REQUIRE(common::extract_season("Show.3rd.Season.1080p") == "S03");
    REQUIRE(common::extract_season("Show.23rd.Season.1080p") == "S23");
  }

  SECTION("4th Season and others") {
    REQUIRE(common::extract_season("Show.4th.Season.1080p") == "S04");
    REQUIRE(common::extract_season("Show.11th.Season.1080p") == "S11");
    REQUIRE(common::extract_season("Show.12th.Season.1080p") == "S12");
    REQUIRE(common::extract_season("Show.13th.Season.1080p") == "S13");
  }

  SECTION("invalid ordinals are not matched") {
    REQUIRE(common::extract_season("Show.11st.Season.1080p").empty());
    REQUIRE(common::extract_season("Show.12nd.Season.1080p").empty());
    REQUIRE(common::extract_season("Show.13rd.Season.1080p").empty());
  }
}

// ==================== 文件系统错误处理测试 ====================

TEST_CASE("calculate_total_size handles errors gracefully", "[common]") {
  SECTION("does not throw on accessible directory") {
    REQUIRE_NOTHROW(common::calculate_total_size(fs::temp_directory_path()));
  }
}

TEST_CASE("size_cache handles invalid paths gracefully", "[common]") {
  common::size_cache sc;

  SECTION("non-existent path returns 0") {
    REQUIRE(sc.get("/non/existent/path") == 0);
  }
}

// ==================== display_width 测试 ====================

TEST_CASE("display_width calculates terminal display width", "[common]") {
  SECTION("pure ASCII") {
    REQUIRE(common::display_width("") == 0);
    REQUIRE(common::display_width("hello") == 5);
    REQUIRE(common::display_width("2026-06-19+18:14:39") == 19);
  }

  SECTION("pure CJK (3-byte UTF-8, 2 columns each)") {
    REQUIRE(common::display_width("鬼灭之刃") == 8);       // 4 chars × 2
    REQUIRE(common::display_width("耀眼") == 4);           // 2 chars × 2
    REQUIRE(common::display_width("香港探秘地图") == 12);  // 6 chars × 2
  }

  SECTION("mixed CJK + ASCII") {
    REQUIRE(common::display_width("太行谣 S01") == 10);         // (3×2) + 1 + 3
    REQUIRE(common::display_width("香港探秘地图 S01") == 16);   // (6×2) + 1 + 3
    REQUIRE(common::display_width("耀眼 S01") == 8);            // (2×2) + 1 + 3
    REQUIRE(common::display_width("神墓 S03") == 8);            // (2×2) + 1 + 3
  }

  SECTION("2-byte UTF-8 (Latin extensions, width 1)") {
    // £ (U+00A3) = 0xC2 0xA3, width 1
    REQUIRE(common::display_width("£100") == 4);
  }

  SECTION("4-byte UTF-8 (emoji, width 2)") {
    // 😀 (U+1F600) = 0xF0 0x9F 0x98 0x80, width 2
    REQUIRE(common::display_width("😀") == 2);
    REQUIRE(common::display_width("🎉") == 2);
  }

  SECTION("size format strings") {
    REQUIRE(common::display_width(common::format_size(0)) == 3);          // "0 B"
    REQUIRE(common::display_width(common::format_size(1024)) == 7);       // "1.00 KB"
    REQUIRE(common::display_width(common::format_size(1048576)) == 7);    // "1.00 MB"
    REQUIRE(common::display_width(common::format_size(1073741824)) == 7); // "1.00 GB"
  }

  SECTION("time format strings") {
    // "YYYY-MM-DD+HH:MM:SS" = 19 ASCII chars
    auto ft = fs::last_write_time(fs::current_path());
    REQUIRE(common::display_width(common::format_time(ft)) == 19);
  }
}

// ==================== output_utils 测试 ====================

TEST_CASE("output::update_max_width updates correctly", "[output]") {
  SECTION("starts at 0") {
    size_t w = 0;
    common::output::update_max_width(w, "hello");
    REQUIRE(w == 5);
  }

  SECTION("takes max") {
    size_t w = 0;
    common::output::update_max_width(w, "hi");
    common::output::update_max_width(w, "hello");
    REQUIRE(w == 5);
  }

  SECTION("CJK is double width") {
    size_t w = 0;
    common::output::update_max_width(w, "测试");
    REQUIRE(w == 4);
  }

  SECTION("mixed widths") {
    size_t w = 0;
    common::output::update_max_width(w, "测试");
    common::output::update_max_width(w, "hello");
    REQUIRE(w == 5);  // "hello" is longer in display width
  }
}

TEST_CASE("output::print_right_aligned pads correctly", "[output]") {
  SECTION("shorter string gets padded") {
    std::ostringstream ss;
    common::output::print_right_aligned(ss, "hi", 5);
    REQUIRE(ss.str() == "   hi");
  }

  SECTION("exact width has no padding") {
    std::ostringstream ss;
    common::output::print_right_aligned(ss, "hello", 5);
    REQUIRE(ss.str() == "hello");
  }

  SECTION("longer string has no padding") {
    std::ostringstream ss;
    common::output::print_right_aligned(ss, "hello", 3);
    REQUIRE(ss.str() == "hello");
  }

  SECTION("CJK characters") {
    std::ostringstream ss;
    common::output::print_right_aligned(ss, "测", 4);
    REQUIRE(ss.str() == "  测");  // 3 spaces + 2-col char = 4 display width? No wait...
    // "测" has display_width 2, so padding = 4 - 2 = 2
    REQUIRE(ss.str() == "  测");
  }

  SECTION("empty string") {
    std::ostringstream ss;
    common::output::print_right_aligned(ss, "", 3);
    REQUIRE(ss.str() == "   ");
  }
}

TEST_CASE("output::print_column_line formats correctly", "[output]") {
  SECTION("standard line") {
    std::ostringstream ss;
    common::output::print_column_line(ss, "12:00:00", "1.50 KB", "/tmp/test", 8, 7);
    // indent(4) + time(8) + sep(2) + size(7) + sep(2) + path
    REQUIRE(ss.str() == "    12:00:00  1.50 KB  '/tmp/test'\n");
  }

  SECTION("custom indent") {
    std::ostringstream ss;
    common::output::print_column_line(ss, "12:00", "1KB", "/tmp", 6, 3, ">>");
    // ">>" + pad_to_6("12:00") + "  " + pad_to_3("1KB") + "  '/tmp'"
    REQUIRE(ss.str() == ">> 12:00  1KB  '/tmp'\n");
  }
}

TEST_CASE("output::print_error_line formats correctly", "[output]") {
  SECTION("error line") {
    std::ostringstream ss;
    common::output::print_error_line(ss, "/bad/path", "permission denied", 8, 7);
    // indent(4) + time_pad(8) + sep(2) + size_pad(7) + sep(2) = 23 prefix
    REQUIRE(ss.str() == "                       '/bad/path' (error: permission denied)\n");
  }

  SECTION("error line with custom indent") {
    std::ostringstream ss;
    common::output::print_error_line(ss, "/bad", "err", 3, 3, ">>");
    // indent(2) + time_pad(3) + sep(2) + size_pad(3) + sep(2) = 12 prefix
    REQUIRE(ss.str() == ">>          '/bad' (error: err)\n");
  }
}

TEST_CASE("output::print_print0_paths outputs null delimiters", "[output]") {
  SECTION("multiple paths") {
    std::ostringstream ss;
    std::vector<std::string> paths = {"/a", "/b", "/c"};
    common::output::print_print0_paths(ss, paths, 3);
    REQUIRE(ss.str() == std::string("/a\0/b\0/c\0", 9));
  }

  SECTION("empty list") {
    std::ostringstream ss;
    std::vector<std::string> paths;
    common::output::print_print0_paths(ss, paths, 0);
    REQUIRE(ss.str().empty());
  }
}
