#define CATCH_CONFIG_MAIN

#include "common/common.hpp"
#include "external/catch_amalgamated.hpp"

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
    // UTF-8 码点级计算：一个汉字差异 = 距离 1
    REQUIRE(common::levenshtein_distance("你好", "你们") == 1);
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
