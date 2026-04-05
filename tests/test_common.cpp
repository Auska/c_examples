#define CATCH_CONFIG_MAIN
#include "external/catch_amalgamated.hpp"

#include "common.hpp"

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
    REQUIRE(common::extract_bracket_content("Game[游戏名称]v1.0") == "游戏名称");
  }

  SECTION("complex filename") {
    REQUIRE(common::extract_bracket_content(
                "SomeGame[中文名][v1.0][Author]") == "Author");
  }
}

TEST_CASE("calculate_total_size handles directories", "[common]") {
  SECTION("non-existent directory returns 0") {
    REQUIRE(common::calculate_total_size("/non/existent/path/12345") == 0);
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
}
