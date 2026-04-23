#include "common/common.hpp"

#include <celero/Celero.h>

#include <regex>
#include <string>
#include <string_view>
#include <vector>

// ==================== Fixture：Levenshtein 距离 ====================

class LevenshteinFixture : public celero::TestFixture {
 public:
  std::vector<std::pair<std::string, std::string>> pairs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    // 短 ASCII 字符串
    pairs.emplace_back("kitten", "sitting");
    pairs.emplace_back("hello", "hallo");
    pairs.emplace_back("testing", "test");
    // 长 ASCII 字符串
    pairs.emplace_back("Show.Name.S01.1080p.NF.WEB-DL.x264",
                       "Show.Name.S02.1080p.NF.WEB-DL.x264");
    pairs.emplace_back("Movie.Title.2020.1080p.BluRay",
                       "Movie.Title.2021.1080p.BluRay");
    // 中文字符串（UTF-8）
    pairs.emplace_back("[雪国列车].Snowpiercer.S01.2020",
                       "[雪国列车].Snowpiercer.S02.2021");
    pairs.emplace_back("[权力的游戏].Game.of.Thrones.Season.1",
                       "[权力的游戏].Game.of.Thrones.Season.2");
    // 完全不同
    pairs.emplace_back("abcdef", "xyzuvw");
  }
};

// ==================== Levenshtein 距离基准测试 ====================

BASELINE_F(LevenshteinDistance, AsciiShort, LevenshteinFixture, 30, 1000) {
  for (const auto& [a, b] : this->pairs) {
    if (a.size() < 20 && a[0] != '[') {
      celero::DoNotOptimizeAway(common::levenshtein_distance(a, b));
    }
  }
}

BENCHMARK_F(LevenshteinDistance, AsciiLong, LevenshteinFixture, 30, 1000) {
  for (const auto& [a, b] : this->pairs) {
    if (a.size() >= 20 && a[0] != '[') {
      celero::DoNotOptimizeAway(common::levenshtein_distance(a, b));
    }
  }
}

BENCHMARK_F(LevenshteinDistance, ChineseUtf8, LevenshteinFixture, 30, 1000) {
  for (const auto& [a, b] : this->pairs) {
    if (a[0] == '[') {
      celero::DoNotOptimizeAway(common::levenshtein_distance(a, b));
    }
  }
}

BENCHMARK_F(LevenshteinDistance, AllPairs, LevenshteinFixture, 30, 1000) {
  for (const auto& [a, b] : this->pairs) {
    celero::DoNotOptimizeAway(common::levenshtein_distance(a, b));
  }
}

// ==================== Levenshtein 相似度（带提前终止） ====================

class LevenshteinSimFixture : public celero::TestFixture {
 public:
  std::vector<std::pair<std::string, std::string>> similar_pairs;
  std::vector<std::pair<std::string, std::string>> dissimilar_pairs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    // 相似配对（相似度 > 0.9）
    similar_pairs.emplace_back("Show.S01.1080p", "Show.S02.1080p");
    similar_pairs.emplace_back("[雪国列车].S01", "[雪国列车].S02");
    similar_pairs.emplace_back("movie.2020.1080p", "movie.2021.1080p");

    // 不相似配对（相似度 < 0.5）
    dissimilar_pairs.emplace_back("abcdef", "xyzuvw");
    dissimilar_pairs.emplace_back("short", "a_very_long_completely_different_name");
    dissimilar_pairs.emplace_back("abc", "xyz123456789");
  }
};

BASELINE_F(LevenshteinSimilarity, NoEarlyExit, LevenshteinSimFixture, 30, 1000) {
  for (const auto& [a, b] : this->dissimilar_pairs) {
    celero::DoNotOptimizeAway(common::levenshtein_similarity(a, b, 0.0));
  }
}

BENCHMARK_F(LevenshteinSimilarity, EarlyExitThreshold09,
            LevenshteinSimFixture, 30, 1000) {
  for (const auto& [a, b] : this->dissimilar_pairs) {
    celero::DoNotOptimizeAway(common::levenshtein_similarity(a, b, 0.9));
  }
}

BENCHMARK_F(LevenshteinSimilarity, SimilarPairsExact,
            LevenshteinSimFixture, 30, 1000) {
  for (const auto& [a, b] : this->similar_pairs) {
    celero::DoNotOptimizeAway(common::levenshtein_similarity(a, b, 0.0));
  }
}

BENCHMARK_F(LevenshteinSimilarity, SimilarPairsThreshold09,
            LevenshteinSimFixture, 30, 1000) {
  for (const auto& [a, b] : this->similar_pairs) {
    celero::DoNotOptimizeAway(common::levenshtein_similarity(a, b, 0.9));
  }
}

// ==================== extract_season：手写扫描 vs std::regex ====================

class ExtractSeasonFixture : public celero::TestFixture {
 public:
  std::vector<std::string> inputs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    inputs = {
        "[雪国列车].Snowpiercer.S01.2020",
        "[权力的游戏].Game.of.Thrones.Season.1.1080p",
        "[绝命毒师].Breaking.Bad.第3季.1080p",
        "Show.Name.S12.1080p",
        "Movie.2020.1080p",           // 无季数
        "Another.Show.s05.720p",      // 小写 s
        "Series.Season.5.720p",       // Season 格式
        "测试.第10季.1080p",           // 中文季
    };
  }
};

/// 旧版 std::regex 实现的 extract_season，用于对比
[[nodiscard]] static std::string extract_season_regex(
    std::string_view folder_name) {
  static const std::array<std::regex, 4> season_patterns = {
      std::regex{R"([._ -]S(\d{1,2})(?:[._ -]|$))", std::regex::icase},
      std::regex{R"([._ -]Season[._ ]*(\d{1,2})(?:[._ -]|$))",
                 std::regex::icase},
      std::regex{R"([._ -]第(\d{1,2})季(?:[._ -]|$))"},
      std::regex{R"([._ -](\d{1,2})st[._ -]*Season(?:[._ -]|$))",
                 std::regex::icase},
  };

  for (const auto& pattern : season_patterns) {
    std::match_results<std::string_view::const_iterator> match;
    if (std::regex_search(folder_name.begin(), folder_name.end(), match,
                          pattern)) {
      int season_num = std::stoi(match[1].str());
      if (season_num >= 0 && season_num < 100) {
        return std::format("S{:02d}", season_num);
      }
    }
  }
  return "";
}

BASELINE_F(ExtractSeason, RegexImpl, ExtractSeasonFixture, 30, 1000) {
  for (const auto& input : this->inputs) {
    celero::DoNotOptimizeAway(extract_season_regex(input));
  }
}

BENCHMARK_F(ExtractSeason, HandwrittenScan, ExtractSeasonFixture, 30, 1000) {
  for (const auto& input : this->inputs) {
    celero::DoNotOptimizeAway(common::extract_season(input));
  }
}

// ==================== extract_bracket_content ====================

class ExtractBracketFixture : public celero::TestFixture {
 public:
  std::vector<std::string> inputs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    inputs = {
        "[雪国列车].Snowpiercer.S01.2020",
        "SomeGame[中文名][v1.0][Author]",
        "Game[游戏名称]v1.0",
        "no_brackets_here",
        "[single]",
        "multiple[nested[brackets]here]",
    };
  }
};

BASELINE_F(ExtractBracket, AllInputs, ExtractBracketFixture, 30, 10000) {
  for (const auto& input : this->inputs) {
    celero::DoNotOptimizeAway(common::extract_bracket_content(input));
  }
}

// ==================== extract_name_with_season ====================

class ExtractNameFixture : public celero::TestFixture {
 public:
  std::vector<std::string> inputs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    inputs = {
        "[雪国列车].Snowpiercer.S01.2020",
        "[权力的游戏].Game.of.Thrones.Season.1",
        "[绝命毒师].Breaking.Bad.第3季",
        "[电影名].Some.Movie.2020.1080p",
        "NoBrackets.Show.S01.1080p",
        "[黑镜].Black.Mirror.S05.1080p",
    };
  }
};

BASELINE_F(ExtractName, RegexImpl, ExtractNameFixture, 30, 1000) {
  for (const auto& input : this->inputs) {
    // 使用 regex 版本的 extract_season + 相同的 extract_bracket_content
    const std::string chinese_name = common::extract_bracket_content(input);
    if (!chinese_name.empty()) {
      const std::string season = extract_season_regex(input);
      celero::DoNotOptimizeAway(season.empty() ? chinese_name
                                               : chinese_name + " " + season);
    }
  }
}

BENCHMARK_F(ExtractName, HandwrittenImpl, ExtractNameFixture, 30, 1000) {
  for (const auto& input : this->inputs) {
    celero::DoNotOptimizeAway(common::extract_name_with_season(input));
  }
}

// ==================== UTF-8 解码性能 ====================

class Utf8DecodeFixture : public celero::TestFixture {
 public:
  std::vector<std::string> inputs;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    inputs = {
        "Hello, World!",                                        // 纯 ASCII
        "[雪国列车].Snowpiercer.S01.2020",                      // 混合
        "测试中文字符串的UTF8解码性能",                           // 纯中文
        "[权力的游戏].Game.of.Thrones.Season.1.1080p.BluRay",   // 长混合
    };
  }
};

BASELINE_F(Utf8Decode, ToCodepoints, Utf8DecodeFixture, 30, 5000) {
  for (const auto& input : this->inputs) {
    celero::DoNotOptimizeAway(common::utf8_to_codepoints(input));
  }
}

// ==================== similarity_cache 性能 ====================

class SimCacheFixture : public celero::TestFixture {
 public:
  std::vector<std::string> names;
  common::similarity_cache cache;

  void setUp(const celero::TestFixture::ExperimentValue* const) override {
    names = {
        "Show.S01.1080p", "Show.S02.1080p", "Show.S03.1080p",
        "Movie.2020.1080p", "Movie.2021.1080p",
        "Series.Name.S01", "Series.Name.S02",
        "[雪国列车].S01", "[雪国列车].S02",
        "CompletelyDifferent", "TotallyUnrelated",
    };
    cache = common::similarity_cache();  // 清空缓存
  }
};

BASELINE_F(SimCache, ColdCache, SimCacheFixture, 30, 100) {
  common::similarity_cache cold_cache;
  for (size_t i = 0; i < this->names.size(); ++i) {
    for (size_t j = i + 1; j < this->names.size(); ++j) {
      celero::DoNotOptimizeAway(
          cold_cache.get(i, j, this->names[i], this->names[j]));
    }
  }
}

BENCHMARK_F(SimCache, WarmCache, SimCacheFixture, 30, 10000) {
  for (size_t i = 0; i < this->names.size(); ++i) {
    for (size_t j = i + 1; j < this->names.size(); ++j) {
      celero::DoNotOptimizeAway(
          this->cache.get(i, j, this->names[i], this->names[j]));
    }
  }
}

CELERO_MAIN
