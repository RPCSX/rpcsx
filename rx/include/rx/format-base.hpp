#pragma once

#include <source_location>

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include(<print>)
#include <format>

namespace rx {
using std::format;
using std::format_args;
using std::format_context;
using std::format_parse_context;
using std::format_string;
using std::format_to;
using std::format_to_n;
using std::formatter;
using std::make_format_args;
using std::vformat;
using std::vformat_to;
} // namespace rx
#else
#include <fmt/format.h>
#include <type_traits>

namespace rx {
using fmt::format;
using fmt::format_args;
using fmt::format_context;
using fmt::format_parse_context;

namespace detail {
template <typename... Args>
struct format_string_impl : fmt::format_string<Args...> {
  using fmt::format_string<Args...>::format_string;

  constexpr std::string_view get() const noexcept {
    return std::string_view(this->str);
  }
};
} // namespace detail

template <typename... Args>
using format_string = std::type_identity_t<detail::format_string_impl<Args...>>;

template <typename... Args>
auto make_format_args(const Args &...args)
    -> decltype(fmt::make_format_args(const_cast<Args &>(args)...)) {
  return fmt::make_format_args(const_cast<Args &>(args)...);
}

using fmt::format_to;
using fmt::format_to_n;
using fmt::formatter;
using fmt::vformat;
using fmt::vformat_to;
} // namespace rx
#endif

namespace rx {
namespace detail {
template <typename... Args>
struct format_string_with_location_impl : format_string<Args...> {
  std::source_location location;

  template <typename T>
  constexpr format_string_with_location_impl(
      T message,
      std::source_location location = std::source_location::current())
      : format_string<Args...>(message), location(location) {}
};
} // namespace detail
template <typename... Args>
using format_string_with_location =
    std::type_identity_t<detail::format_string_with_location_impl<Args...>>;
} // namespace rx
