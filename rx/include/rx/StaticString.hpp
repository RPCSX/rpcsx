#pragma once
#include "format-base.hpp"
#include "rx/die.hpp"
#include <cassert>
#include <compare>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <type_traits>

namespace rx {
template <std::size_t N> class StaticCString {
private:
  char m_data[N]{};

public:
  // Reserve space for null terminator
  static constexpr std::size_t capacity = N - 1;

  constexpr StaticCString() noexcept = default;
  constexpr StaticCString(std::string_view sv) noexcept { assign(sv); }

  template <std::size_t OtherN>
    requires(OtherN <= N)
  constexpr StaticCString(const StaticCString<OtherN> &other) noexcept {
    std::memcpy(m_data, other.m_data,
                other.size() + 1); // +1 for null terminator
  }

  template <std::size_t OtherN>
    requires(OtherN <= N)
  constexpr StaticCString &
  operator=(const StaticCString<OtherN> &other) noexcept {
    if (this != &other) {
      std::memcpy(m_data, other.m_data, other.size() + 1);
    }
    return *this;
  }

  constexpr StaticCString &operator=(std::string_view sv) noexcept {
    assign(sv);
    return *this;
  }

  constexpr void assign(std::string_view sv) noexcept {
    if (sv.size() > capacity) {
      // Truncate if too long
      sv = sv.substr(0, capacity);
    }
    auto size = sv.size();
    sv.copy(m_data, size);
    m_data[size] = '\0';
  }

  constexpr void append(std::string_view sv) noexcept {
    auto selfSize = size();
    std::size_t available = capacity - selfSize;
    if (sv.size() > available) {
      sv = sv.substr(0, available);
    }
    sv.copy(m_data + selfSize, sv.size());
    selfSize += sv.size();
    m_data[selfSize] = '\0';
  }

  constexpr StaticCString &operator+=(std::string_view sv) noexcept {
    append(sv);
    return *this;
  }

  constexpr void remove_suffix(std::size_t n) { m_data[size() - n] = 0; }

  constexpr void clear() noexcept { m_data[0] = '\0'; }

  [[nodiscard]] constexpr const char *data() const noexcept { return m_data; }
  [[nodiscard]] constexpr char *data() noexcept { return m_data; }
  [[nodiscard]] constexpr const char *c_str() const noexcept { return m_data; }
  [[nodiscard]] constexpr std::size_t size() const noexcept {
    return std::strlen(m_data);
  }
  [[nodiscard]] constexpr std::size_t length() const noexcept { return size(); }
  [[nodiscard]] constexpr bool empty() const noexcept {
    return m_data[0] == '\0';
  }
  [[nodiscard]] constexpr std::size_t max_size() const noexcept {
    return capacity;
  }

  constexpr operator std::string_view() const noexcept {
    return std::string_view(m_data, size());
  }

  [[nodiscard]] constexpr const char *begin() const noexcept { return m_data; }
  [[nodiscard]] constexpr const char *end() const noexcept {
    return m_data + size();
  }
  [[nodiscard]] constexpr char *begin() noexcept { return m_data; }
  [[nodiscard]] constexpr char *end() noexcept { return m_data + size(); }
  [[nodiscard]] constexpr const char *cbegin() const noexcept { return m_data; }
  [[nodiscard]] constexpr const char *cend() const noexcept {
    return m_data + size();
  }

  constexpr char &operator[](std::size_t pos) noexcept { return m_data[pos]; }

  constexpr const char &operator[](std::size_t pos) const noexcept {
    return m_data[pos];
  }

  [[nodiscard]] char &at(std::size_t pos) {
    dieIf(pos >= size(), "StaticCString::at: index out of range");
    return m_data[pos];
  }

  [[nodiscard]] constexpr char &at(std::size_t pos) const {
    dieIf(pos >= size(), "StaticCString::at: index out of range");
    return m_data[pos];
  }

  constexpr std::size_t find(char c, std::size_t pos = 0) const noexcept {
    return std::string_view(*this).find(c, pos);
  }

  constexpr std::size_t find(std::string_view sv,
                             std::size_t pos = 0) const noexcept {
    return std::string_view(*this).find(sv, pos);
  }

  [[nodiscard]] constexpr std::string_view
  substr(std::size_t pos = 0, std::size_t len = std::string_view::npos) const {
    return std::string_view(*this).substr(pos, len);
  }

  void resize(std::size_t size, char c = ' ') {
    assert(size <= capacity);

    auto selfSize = this->size();
    if (size < selfSize) {
      m_data[size] = '\0';
      return;
    }

    for (std::size_t i = selfSize; i < size; ++i) {
      m_data[i] = c;
    }

    m_data[size] = '\0';
  }

  template <typename... Args>
  [[nodiscard]] constexpr static StaticCString
  format(format_string<Args...> fmt, Args &&...args) {
    StaticCString result;
    auto [ptr, size] = format_to_n(result.m_data, result.capacity, fmt,
                                   std::forward<Args>(args)...);
    result.m_data[std::min<std::size_t>(size, capacity)] = '\0';
    return result;
  }

  [[nodiscard]] constexpr static StaticCString vformat(std::string_view fmt,
                                                       format_args args) {
    StaticCString result;
    auto [ptr, size] =
        vformat_to(result.m_data, result.capacity, fmt, std::move(args));
    result.m_data[std::min<std::size_t>(size, capacity)] = '\0';
    return result;
  }

  template <typename... Args>
  constexpr void assignFormat(format_string<Args...> fmt, Args &&...args) {
    auto [ptr, size] =
        format_to_n(m_data, capacity, fmt, std::forward<Args>(args)...);
    m_data[std::min<std::size_t>(size, capacity)] = '\0';
  }

  constexpr void assignVFormat(std::string_view fmt, format_args args) {
    auto [ptr, size] = vformat_to(m_data, capacity, fmt, std::move(args));
    m_data[std::min<std::size_t>(size, capacity)] = '\0';
  }
};

template <std::size_t N> class StaticString {
private:
  char m_data[N]{};
  std::size_t m_size = 0;

public:
  static constexpr std::size_t capacity = N;

  constexpr StaticString() noexcept = default;
  constexpr StaticString(std::string_view sv) noexcept { assign(sv); }

  template <std::size_t OtherN>
    requires(OtherN <= N)
  constexpr StaticString(const StaticString<OtherN> &other) noexcept
      : m_size(other.m_size) {
    std::memcpy(m_data, other.m_data, m_size);
  }

  template <std::size_t OtherN>
    requires(OtherN <= N)
  constexpr StaticString &
  operator=(const StaticString<OtherN> &other) noexcept {
    if (this != &other) {
      m_size = other.m_size;
      std::memcpy(m_data, other.m_data, m_size);
    }
    return *this;
  }

  constexpr StaticString &operator=(std::string_view sv) noexcept {
    assign(sv);
    return *this;
  }

  constexpr void assign(std::string_view sv) noexcept {
    if (sv.size() > capacity) {
      // Truncate if too long
      sv = sv.substr(0, capacity);
    }
    m_size = sv.size();
    sv.copy(m_data, m_size);
  }

  constexpr void append(std::string_view sv) noexcept {
    std::size_t available = capacity - m_size;
    if (sv.size() > available) {
      sv = sv.substr(0, available);
    }
    sv.copy(m_data + m_size, sv.size());
    m_size += sv.size();
  }

  constexpr StaticString &operator+=(std::string_view sv) noexcept {
    append(sv);
    return *this;
  }

  constexpr void remove_suffix(std::size_t n) {
    m_size -= n;
    m_data[m_size] = 0;
  }

  constexpr void clear() noexcept {
    m_size = 0;
    m_data[0] = '\0';
  }

  [[nodiscard]] constexpr const char *data() const noexcept { return m_data; }
  [[nodiscard]] constexpr char *data() noexcept { return m_data; }
  [[nodiscard]] constexpr std::size_t size() const noexcept { return m_size; }
  [[nodiscard]] constexpr std::size_t length() const noexcept { return m_size; }
  [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }
  [[nodiscard]] constexpr std::size_t max_size() const noexcept {
    return capacity;
  }

  constexpr operator std::string_view() const noexcept {
    return std::string_view(m_data, m_size);
  }

  [[nodiscard]] constexpr const char *begin() const noexcept { return m_data; }
  [[nodiscard]] constexpr const char *end() const noexcept {
    return m_data + m_size;
  }
  [[nodiscard]] constexpr char *begin() noexcept { return m_data; }
  [[nodiscard]] constexpr char *end() noexcept { return m_data + m_size; }
  [[nodiscard]] constexpr const char *cbegin() const noexcept { return m_data; }
  [[nodiscard]] constexpr const char *cend() const noexcept {
    return m_data + m_size;
  }

  constexpr char &operator[](std::size_t pos) noexcept { return m_data[pos]; }

  constexpr const char &operator[](std::size_t pos) const noexcept {
    return m_data[pos];
  }

  [[nodiscard]] char &at(std::size_t pos) {
    dieIf(pos >= m_size, "StaticString::at: index out of range");
    return m_data[pos];
  }

  [[nodiscard]] constexpr char &at(std::size_t pos) const {
    dieIf(pos >= m_size, "StaticString::at: index out of range");
    return m_data[pos];
  }

  constexpr std::size_t find(char c, std::size_t pos = 0) const noexcept {
    return std::string_view(*this).find(c, pos);
  }

  constexpr std::size_t find(std::string_view sv,
                             std::size_t pos = 0) const noexcept {
    return std::string_view(*this).find(sv, pos);
  }

  [[nodiscard]] constexpr std::string_view
  substr(std::size_t pos = 0, std::size_t len = std::string_view::npos) const {
    return std::string_view(*this).substr(pos, len);
  }

  void resize(std::size_t size, char c = ' ') {
    assert(size <= capacity);

    if (size < m_size) {
      m_size = size;
      return;
    }

    for (std::size_t i = m_size; i < size; ++i) {
      m_data[i] = c;
    }

    m_size = size;
  }

  template <typename... Args>
  [[nodiscard]] constexpr static StaticString format(format_string<Args...> fmt,
                                                     Args &&...args) {
    StaticString result;
    auto [ptr, size] = format_to_n(result.m_data, result.capacity, fmt,
                                   std::forward<Args>(args)...);
    result.m_size = std::min<std::size_t>(size, capacity);
    return result;
  }

  [[nodiscard]] constexpr static StaticString vformat(std::string_view fmt,
                                                      format_args args) {
    StaticString result;
    auto [ptr, size] =
        vformat_to(result.m_data, result.capacity, fmt, std::move(args));
    result.m_size = std::min<std::size_t>(size, capacity);
    return result;
  }

  template <typename... Args>
  constexpr void assignFormat(format_string<Args...> fmt, Args &&...args) {
    auto [ptr, size] =
        format_to_n(m_data, capacity, fmt, std::forward<Args>(args)...);
    m_size = std::min<std::size_t>(size, capacity);
  }

  constexpr void assignVFormat(std::string_view fmt, format_args args) {
    auto [ptr, size] = vformat_to(m_data, capacity, fmt, std::move(args));
    m_size = std::min<std::size_t>(size, capacity);
  }
};

template <std::size_t LN, std::size_t RN>
constexpr bool operator==(const StaticString<LN> &lhs,
                          const StaticString<RN> &rhs) noexcept {
  return std::string_view(lhs) == std::string_view(rhs);
}

template <std::size_t N>
constexpr bool operator==(const StaticString<N> &lhs,
                          std::string_view sv) noexcept {
  return std::string_view(lhs) == sv;
}

template <std::size_t LN, std::size_t RN>
constexpr auto operator<=>(const StaticString<LN> &lhs,
                           const StaticString<RN> &rhs) noexcept {
  return std::string_view(lhs) <=> std::string_view(rhs);
}

template <std::size_t N>
constexpr auto operator<=>(const StaticString<N> &lhs,
                           std::string_view sv) noexcept {
  return std::string_view(lhs) <=> sv;
}

template <std::size_t N> StaticString(const char (&)[N]) -> StaticString<N>;

template <std::size_t LN, std::size_t RN>
constexpr bool operator==(const StaticCString<LN> &lhs,
                          const StaticCString<RN> &rhs) noexcept {
  return std::strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

template <std::size_t N>
constexpr bool operator==(const StaticCString<N> &lhs,
                          std::string_view sv) noexcept {
  return lhs.c_str() == sv;
}

template <std::size_t LN, std::size_t RN>
constexpr auto operator<=>(const StaticCString<LN> &lhs,
                           const StaticCString<RN> &rhs) noexcept {
  return std::strcmp(lhs.c_str(), rhs.c_str());
}

template <std::size_t N>
constexpr auto operator<=>(const StaticCString<N> &lhs,
                           std::string_view sv) noexcept {
  return lhs.c_str() <=> sv;
}

template <std::size_t N> StaticCString(const char (&)[N]) -> StaticCString<N>;

template <std::size_t N = 32, typename... Args>
auto formatStatic(format_string<Args...> fmt, Args &&...args) {
  return StaticString<N>::format(fmt, std::forward<Args>(args)...);
}
} // namespace rx

template <std::size_t N>
struct rx::formatter<rx::StaticString<N>> : rx::formatter<std::string_view> {
  auto format(const rx::StaticString<N> &str, rx::format_context &ctx) const {
    return rx::formatter<std::string_view>::format(std::string_view(str), ctx);
  }
};

template <std::size_t N>
struct rx::formatter<rx::StaticCString<N>> : rx::formatter<const char *> {
  auto format(const rx::StaticCString<N> &str, rx::format_context &ctx) const {
    return rx::formatter<const char *>::format(str.c_str(), ctx);
  }
};
