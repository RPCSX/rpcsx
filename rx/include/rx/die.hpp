#pragma once

#include "format-base.hpp"

namespace rx {
namespace detail {
[[noreturn]] void dieImpl(std::string_view fmt, format_args args,
                          std::source_location location);
}

template <typename... Args>
[[noreturn]] void die(rx::format_string_with_location<Args...> fmt,
                      const Args &...args) {
  detail::dieImpl(fmt.get(), make_format_args(const_cast<Args &>(args)...),
                  fmt.location);
}

template <typename... Args>
void dieIf(bool condition, rx::format_string_with_location<Args...> fmt,
           const Args &...args) {
  if (condition) {
    detail::dieImpl(fmt.get(), make_format_args(const_cast<Args &>(args)...),
                    fmt.location);
  }
}
} // namespace rx
