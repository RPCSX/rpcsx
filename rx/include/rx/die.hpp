#pragma once

#include "format-base.hpp"
#include <functional>
#include <utility>

namespace rx {
using DieHandler =
    std::function<void(std::string_view text, std::source_location location)>;

void setDieHandler(DieHandler handler);

namespace detail {
[[noreturn]] void dieImpl(std::string_view fmt, format_args args,
                          std::source_location location);
}

template <typename... Args>
[[noreturn]] void die(format_string_with_location<Args...> fmt,
                      const Args &...args) {
  detail::dieImpl(fmt.get(), rx::make_format_args(args...), fmt.location);
}

template <typename... Args>
void dieIf(bool condition, format_string_with_location<Args...> fmt,
           const Args &...args) {
  if (condition) {
    detail::dieImpl(fmt.get(), rx::make_format_args(args...), fmt.location);
  }
}
} // namespace rx
