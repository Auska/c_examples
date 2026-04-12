#define CATCH_CONFIG_MAIN
#include <filesystem>
#include <fstream>

#include "common/common.hpp"
#include "external/catch_amalgamated.hpp"

using Catch::Approx;

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

TEST_CASE("calculate_total_size_cached works correctly", "[common]") {
  SECTION("non-existent directory returns 0") {
    std::unordered_map<std::string, uintmax_t> cache;
    REQUIRE(common::calculate_total_size_cached("/non/existent/path/12345",
                                                cache) == 0);
  }

  SECTION("cache is populated") {
    std::unordered_map<std::string, uintmax_t> cache;
    (void)common::calculate_total_size_cached("/non/existent/path/12345",
                                              cache);
    REQUIRE(cache.size() == 1);
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
    // Note: UTF-8 encoding means each Chinese character is 3 bytes
    // So "你好" and "你们" differ by 3 bytes (one character)
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

TEST_CASE("levenshtein_similarity_cached works correctly", "[levenshtein]") {
  SECTION("returns same value as uncached") {
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        cache;
    double cached =
        common::levenshtein_similarity_cached(0, 1, "hello", "hallo", cache);
    double uncached = common::levenshtein_similarity("hello", "hallo");
    REQUIRE(cached == Approx(uncached).epsilon(0.001));
  }

  SECTION("cache is populated after first call") {
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        cache;
    (void)common::levenshtein_similarity_cached(0, 1, "hello", "hallo", cache);
    REQUIRE(cache.contains({0, 1}));
  }

  SECTION("cache returns same value on second call") {
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        cache;
    double first =
        common::levenshtein_similarity_cached(0, 1, "hello", "hallo", cache);
    double second =
        common::levenshtein_similarity_cached(0, 1, "hello", "hallo", cache);
    REQUIRE(first == second);
  }

  SECTION("indices are normalized") {
    std::unordered_map<std::pair<size_t, size_t>, double, common::PairHash>
        cache;
    (void)common::levenshtein_similarity_cached(5, 2, "hello", "hallo", cache);
    REQUIRE(cache.contains({2, 5}));
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
