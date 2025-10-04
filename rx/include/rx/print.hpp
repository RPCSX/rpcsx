#pragma once

#ifndef __has_include
#define __has_include(x) 0
#endif

#if __has_include(<print>)
#include <print>
namespace rx {
using std::print;
using std::println;
using std::vprint_nonunicode;
using std::vprint_unicode;
} // namespace rx
#else
#include <fmt/format.h>

namespace rx {
using fmt::print;
using fmt::println;

inline void vprint_nonunicode(FILE *stream, std::string_view fmt,
                              fmt::format_args args) {
  fmt::vprint(stream, fmt, args);
}

inline void vprint_unicode(FILE *stream, std::string_view fmt,
                           fmt::format_args args) {
  fmt::vprint(stream, fmt, args);
}
} // namespace rx
#endif
