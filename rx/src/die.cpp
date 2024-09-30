#include "die.hpp"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

void rx::die(const char *message, ...) {
  va_list args;
  va_start(args, message);
  std::vfprintf(stderr, message, args);
  std::fprintf(stderr, "\n");
  va_end(args);

  std::fflush(stdout);
  std::fflush(stderr);
  std::abort();
}

void rx::dieIf(bool condition, const char *message, ...) {
  if (condition) {
    va_list args;
    va_start(args, message);
    std::vfprintf(stderr, message, args);
    std::fprintf(stderr, "\n");
    va_end(args);

    std::fflush(stdout);
    std::fflush(stderr);
    std::abort();
  }
}
