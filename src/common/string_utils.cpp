#include "string_utils.hpp"

#include <cctype>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace common {

std::string extract_bracket_content(std::string_view folder_name) {
  const size_t end = folder_name.rfind(']');
  if (end == std::string_view::npos) {
    return "";
  }
  const size_t start = folder_name.rfind('[', end);
  if (start == std::string_view::npos) {
    return "";
  }
  return std::string(folder_name.substr(start + 1, end - start - 1));
}

bool is_season_sep(char c) noexcept {
  return c == '.' || c == '_' || c == ' ' || c == '-';
}

bool starts_with_season(std::string_view sv, size_t pos) noexcept {
  constexpr std::string_view k_season = "season";
  if (pos + k_season.size() > sv.size()) {
    return false;
  }
  for (size_t k = 0; k < k_season.size(); ++k) {
    if (std::tolower(static_cast<unsigned char>(sv[pos + k])) != k_season[k]) {
      return false;
    }
  }
  return true;
}

std::pair<int, int> parse_season_number(std::string_view sv,
                                         size_t pos) noexcept {
  if (pos >= sv.size() || !std::isdigit(static_cast<unsigned char>(sv[pos]))) {
    return {0, 0};
  }
  int num = 0;
  int digits = 0;
  while (pos + digits < sv.size() &&
         digits < 2 &&
         std::isdigit(static_cast<unsigned char>(sv[pos + digits]))) {
    num = num * 10 + (sv[pos + digits] - '0');
    ++digits;
  }
  return {num, digits};
}

bool is_season_boundary(std::string_view sv, size_t pos) noexcept {
  return pos >= sv.size() || is_season_sep(sv[pos]);
}

std::string try_match_Sxx(std::string_view sv, size_t i) noexcept {
  const auto [num, digits] = parse_season_number(sv, i + 1);
  if (digits > 0 && is_season_boundary(sv, i + 1 + digits)) {
    return std::format("S{:02d}", num);
  }
  return {};
}

std::string try_match_Season(std::string_view sv, size_t i) noexcept {
  size_t pos = i + 6;
  while (pos < sv.size() && is_season_sep(sv[pos])) {
    ++pos;
  }
  const auto [num, digits] = parse_season_number(sv, pos);
  if (digits > 0 && is_season_boundary(sv, pos + digits)) {
    return std::format("S{:02d}", num);
  }
  return {};
}

std::string try_match_chinese_season(std::string_view sv, size_t i) noexcept {
  const size_t len = sv.size();
  size_t pos = i + 3;
  const auto [num, digits] = parse_season_number(sv, pos);
  if (digits > 0) {
    const size_t after_num = pos + digits;
    if (after_num + 2 < len &&
        static_cast<uint8_t>(sv[after_num]) == 0xE5 &&
        static_cast<uint8_t>(sv[after_num + 1]) == 0xAD &&
        static_cast<uint8_t>(sv[after_num + 2]) == 0xA3) {
      if (after_num + 3 >= len ||
          is_season_sep(sv[after_num + 3])) {
        return std::format("S{:02d}", num);
      }
    }
  }
  return {};
}

bool is_ordinal_suffix(std::string_view sv, size_t pos, int num) noexcept {
  if (pos + 1 >= sv.size()) return false;
  const char s1 = std::tolower(static_cast<unsigned char>(sv[pos]));
  const char s2 = std::tolower(static_cast<unsigned char>(sv[pos + 1]));
  if (s1 == 's' && s2 == 't') {
    return num == 1 || (num > 10 && num % 10 == 1 && num / 10 % 10 != 1);
  }
  if (s1 == 'n' && s2 == 'd') {
    return num == 2 || (num > 10 && num % 10 == 2 && num / 10 % 10 != 1);
  }
  if (s1 == 'r' && s2 == 'd') {
    return num == 3 || (num > 10 && num % 10 == 3 && num / 10 % 10 != 1);
  }
  if (s1 == 't' && s2 == 'h') {
    return true;
  }
  return false;
}

std::string try_match_ordinal_season(std::string_view sv,
                                      size_t i) noexcept {
  const auto [num, digits] = parse_season_number(sv, i);
  if (digits == 0) return {};

  size_t pos = i + digits;
  if (!is_ordinal_suffix(sv, pos, num)) return {};
  pos += 2;

  while (pos < sv.size() && is_season_sep(sv[pos])) {
    ++pos;
  }
  if (!starts_with_season(sv, pos)) return {};
  const size_t after_season = pos + 6;
  if (is_season_boundary(sv, after_season)) {
    return std::format("S{:02d}", num);
  }
  return {};
}

std::string extract_season(std::string_view folder_name) {
  const size_t len = folder_name.size();

  for (size_t i = 0; i < len; ++i) {
    if (i == 0 || !is_season_sep(folder_name[i - 1])) {
      continue;
    }

    const char c = folder_name[i];

    if ((c == 'S' || c == 's') && i + 1 < len) {
      auto result = try_match_Sxx(folder_name, i);
      if (!result.empty()) return result;
    }

    if ((c == 'S' || c == 's') && starts_with_season(folder_name, i)) {
      auto result = try_match_Season(folder_name, i);
      if (!result.empty()) return result;
    }

    if (static_cast<uint8_t>(c) == 0xE7 && i + 2 < len &&
        static_cast<uint8_t>(folder_name[i + 1]) == 0xAC &&
        static_cast<uint8_t>(folder_name[i + 2]) == 0xAC) {
      auto result = try_match_chinese_season(folder_name, i);
      if (!result.empty()) return result;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
      auto result = try_match_ordinal_season(folder_name, i);
      if (!result.empty()) return result;
    }
  }

  return "";
}

std::string extract_name_with_season(std::string_view folder_name) {
  const std::string chinese_name = extract_bracket_content(folder_name);
  if (chinese_name.empty()) {
    return "";
  }

  const std::string season = extract_season(folder_name);
  if (season.empty()) {
    return chinese_name;
  }

  return chinese_name + " " + season;
}

size_t display_width(std::string_view sv) {
  size_t w = 0;
  for (size_t i = 0; i < sv.size();) {
    const auto c = static_cast<unsigned char>(sv[i]);
    if (c < 0x80) {
      w += 1;
      i += 1;
    } else if (c < 0xE0) {
      w += 1;
      i += 2;
    } else if (c < 0xF0) {
      w += 2;
      i += 3;
    } else {
      w += 2;
      i += 4;
    }
  }
  return w;
}

}  // namespace common
