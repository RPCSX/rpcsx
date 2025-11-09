#pragma once

#include "FunctionRef.hpp"
#include <concepts>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rx {
std::wstring toWchar(std::string_view src);
std::string toUtf8(std::wstring_view src);
std::string toUtf8(std::u16string_view src);
std::u16string toUtf16(std::string_view src);

// Copy null-terminated string from a std::string or a char array to a char
// array with truncation
template <typename D, typename T> void strcpyTrunc(D &&dst, const T &src) {
  const std::size_t count = std::size(src) >= std::size(dst)
                                ? std::max<std::size_t>(std::size(dst), 1) - 1
                                : std::size(src);
  std::memcpy(std::data(dst), std::data(src), count);
  std::memset(std::data(dst) + count, 0, std::size(dst) - count);
}

std::string replaceAll(std::string_view src, std::string_view from,
                       std::string_view to, std::size_t count = -1);

template <std::size_t list_size>
std::string replaceAll(
    std::string src,
    const std::pair<std::string_view, std::string_view> (&list)[list_size]) {
  for (std::size_t pos = 0; pos < src.length(); ++pos) {
    for (std::size_t i = 0; i < list_size; ++i) {
      const std::size_t comp_length = list[i].first.length();

      if (src.length() - pos < comp_length) {
        continue;
      }

      if (std::string_view(src).substr(pos, comp_length) == list[i].first) {
        src.replace(pos, comp_length, list[i].second);
        pos += list[i].second.length() - 1;
        break;
      }
    }
  }

  return src;
}

template <std::size_t list_size>
std::string
replaceAll(std::string src,
           const std::pair<std::string_view, rx::FunctionRef<std::string()>> (
               &list)[list_size]) {
  for (std::size_t pos = 0; pos < src.length(); ++pos) {
    for (std::size_t i = 0; i < list_size; ++i) {
      const std::size_t comp_length = list[i].first.length();

      if (src.length() - pos < comp_length) {
        continue;
      }

      if (std::string_view(src).substr(pos, comp_length) == list[i].first) {
        auto replacement = list[i].second();
        src.replace(pos, comp_length, replacement);
        pos += replacement.length() - 1;
        break;
      }
    }
  }

  return src;
}

inline std::string replaceAll(
    std::string src,
    const std::vector<std::pair<std::string_view, std::string_view>> &list) {
  for (std::size_t pos = 0; pos < src.length(); ++pos) {
    for (const auto &i : list) {
      const std::size_t comp_length = i.first.length();

      if (src.length() - pos < comp_length) {
        continue;
      }

      if (std::string_view(src).substr(pos, comp_length) == i.first) {
        src.replace(pos, comp_length, i.second);
        pos += i.second.length() - 1;
        break;
      }
    }
  }

  return src;
}

constexpr std::pair<std::string_view, std::string_view>
splitPair(std::string_view source,
          std::initializer_list<std::string_view> separators) {
  std::size_t pos = std::string_view::npos;
  std::size_t sepLen = 0;

  for (auto separator : separators) {
    if (std::size_t sepPos = source.find(separator); sepPos < pos) {
      pos = sepPos;
      sepLen = separator.length();
    }
  }

  if (!sepLen) {
    return {source, {}};
  }

  return {source.substr(0, pos), source.substr(pos + sepLen)};
}

template <typename T>
  requires requires(T &container, std::string_view string) {
    container.emplace_back(string);
  }
constexpr T splitTo(std::string_view source,
                    std::initializer_list<std::string_view> separators,
                    bool skipEmpty = true) {
  T result;

  while (!source.empty()) {
    auto [piece, rest] = splitPair(source, separators);

    source = rest;

    if (!piece.empty() || !skipEmpty) {
      result.emplace_back(piece);
    }
  }

  if (result.empty() && !skipEmpty) {
    result.emplace_back();
  }

  return result;
}

struct Splitter {
  struct EndIterator {};
  struct iterator {
    constexpr iterator(std::string_view string,
                       std::initializer_list<std::string_view> separators,
                       bool skipEmpty)
        : mString(string), mSeparators(separators), mSkipEmpty(skipEmpty) {
      advance();
    }

    constexpr iterator &operator++() { advance(); return *this; }
    constexpr std::string_view operator*() const { return mPiece; }

    constexpr bool operator==(const EndIterator &) const {
      return mPiece.empty() && mString.empty();
    }

  private:
    constexpr void advance() {
      auto [piece, rest] = splitPair(mString, mSeparators);
      mString = rest;
      mPiece = piece;

      while (mSkipEmpty && mPiece.empty() && !mString.empty()) [[unlikely]] {
        auto [piece, rest] = splitPair(mString, mSeparators);
        mString = rest;
        mPiece = piece;
      }
    }
    std::string_view mString;
    std::string_view mPiece;
    std::initializer_list<std::string_view> mSeparators;
    bool mSkipEmpty;
  };

  constexpr Splitter(std::string_view string,
                     std::initializer_list<std::string_view> separators,
                     bool skipEmpty)
      : mString(string), mSeparators(separators), mSkipEmpty(skipEmpty) {}

  constexpr iterator begin() const {
    return {mString, mSeparators, mSkipEmpty};
  }
  constexpr EndIterator end() const { return {}; }

private:
  std::string_view mString;
  std::initializer_list<std::string_view> mSeparators;
  bool mSkipEmpty;
};

constexpr Splitter
split(std::string_view string,
      std::initializer_list<std::string_view> separators = {" ", "\t", "\v",
                                                            "\n", "\r"},
      bool skipEmpty = true) {
  return {string, separators, skipEmpty};
}

constexpr std::string_view trimPrefix(std::string_view source,
                                      std::string_view values = " \t\v\n\r") {
  const auto begin = source.find_first_not_of(values);

  if (begin == source.npos)
    return {};

  return source.substr(begin);
}

constexpr std::string_view trimSuffix(std::string_view source,
                                      std::string_view values = " \t\v\n\r") {
  const std::size_t index = source.find_last_not_of(values);
  source.remove_suffix(source.size() - (index + 1));
  return source;
}

constexpr std::string_view trim(std::string_view source,
                                std::string_view values = " \t\v\n\r") {
  return trimSuffix(trimPrefix(source, values), values);
}

template <typename T>
constexpr std::string join(const T &source, std::string_view separator)
  requires requires {
    { source.empty() } -> std::convertible_to<bool>;
    ++source.begin();
    --source.end();
    std::string{*source.begin()};
    std::string{source.back()};
  }
{
  if (source.empty()) {
    return {};
  }

  std::string result;

  auto it = source.begin();
  auto end = source.end();
  for (--end; it != end; ++it) {
    if constexpr (requires { result += *it; }) {
      result += *it;
    } else {
      result += std::string{*it};
    }
    result += separator;
  }

  if constexpr (requires { result += source.back(); }) {
    result += source.back();
  } else {
    result += std::string{source.back()};
  }
  return result;
}

template <typename T>
constexpr std::string join(std::span<T> sources, std::string_view separator)
  requires requires { join(sources.front(), separator); }
{
  if (sources.empty()) {
    return {};
  }

  std::string result;
  bool first = true;

  for (const auto &v : sources) {
    if (first) {
      result = join(v, separator);
      first = false;
    } else {
      result += separator;
      result += join(v, separator);
    }
  }

  return result;
}

std::string toUpper(std::string_view string);
std::string toLower(std::string_view string);
std::string truncateString(std::string_view src, std::size_t length);
bool matchString(std::string_view source, std::string_view mask);

struct StringHash {
  using hash_type = std::hash<std::string_view>;
  using is_transparent = void;

  std::size_t operator()(const char *str) const { return hash_type{}(str); }
  std::size_t operator()(std::string_view str) const {
    return hash_type{}(str);
  }
  std::size_t operator()(std::string const &str) const {
    return hash_type{}(str);
  }
};

struct StringLess {
  using is_transparent = void;

  template <typename CharT, typename Traits>
  constexpr bool operator()(
      std::basic_string_view<CharT, Traits> lhs,
      std::type_identity_t<std::basic_string_view<CharT, Traits>> rhs) const {
    if (lhs.size() < rhs.size()) {
      return true;
    }

    if (lhs.size() > rhs.size()) {
      return false;
    }

    return Traits::compare(lhs.data(), rhs.data(), lhs.size()) < 0;
  }

  constexpr bool operator()(std::string_view lhs, std::string_view rhs) const {
    if (lhs.size() < rhs.size()) {
      return true;
    }

    if (lhs.size() > rhs.size()) {
      return false;
    }

    return std::char_traits<char>::compare(lhs.data(), rhs.data(), lhs.size()) <
           0;
  }
};

struct StringGreater {
  using is_transparent = void;

  template <typename CharT, typename Traits>
  constexpr bool operator()(
      std::basic_string_view<CharT, Traits> lhs,
      std::type_identity_t<std::basic_string_view<CharT, Traits>> rhs) const {
    if (lhs.size() > rhs.size()) {
      return true;
    }

    if (lhs.size() < rhs.size()) {
      return false;
    }

    return Traits::compare(lhs.data(), rhs.data(), lhs.size()) > 0;
  }

  constexpr bool operator()(std::string_view lhs, std::string_view rhs) const {
    if (lhs.size() > rhs.size()) {
      return true;
    }

    if (lhs.size() < rhs.size()) {
      return false;
    }

    return std::char_traits<char>::compare(lhs.data(), rhs.data(), lhs.size()) >
           0;
  }
};
} // namespace rx
