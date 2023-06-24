#pragma once

#include "SourceLocation.hpp"
#include <cstdio>
#include <cstdarg>

namespace util {
[[noreturn]] inline void unreachable_impl() { std::fflush(stdout); __builtin_trap(); }

[[noreturn]] inline void unreachable(SourceLocation location = {}) {
  std::printf("\n");
  std::fflush(stdout);
  std::fprintf(stderr, "Unreachable at %s:%u:%u %s\n", location.file_name(),
               location.line(), location.column(), location.function_name());
  unreachable_impl();
}

[[noreturn]] inline void unreachable(const char *fmt, ...) {
  std::printf("\n");
  std::fflush(stdout);
  va_list list;
  va_start(list, fmt);
  std::vfprintf(stderr, fmt, list);
  va_end(list);
  std::fprintf(stderr, "\n");

  unreachable_impl();
}
} // namespace util
