#include "die.hpp"

#include <cstdio>
#include <cstdlib>
#include <rx/print.hpp>

void rx::detail::dieImpl(std::string_view fmt, format_args args,
                         std::source_location location) {
  rx::print(stderr, "\n");
  rx::print(stderr, "{}:{}:{}: ", location.file_name(), location.line(),
            location.column());
  rx::vprint_nonunicode(stderr, fmt, args);
  rx::print(stderr, "\n");

  std::fflush(stdout);
  std::fflush(stderr);
  std::abort();
}
