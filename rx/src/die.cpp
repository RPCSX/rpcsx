#include "die.hpp"

#include "print.hpp"
#include "format-base.hpp"

#include <cstdio>
#include <cstdlib>


static rx::DieHandler g_dieHandler;

void rx::setDieHandler(DieHandler handler) {
  g_dieHandler = std::move(handler);
}

void rx::detail::dieImpl(std::string_view fmt, format_args args,
                         std::source_location location) {
  if (g_dieHandler != nullptr) {
    rx::print(stderr, "\n");
    std::fflush(stdout);
    std::fflush(stderr);

    g_dieHandler(rx::vformat(fmt, args), location);
  } else {
    rx::print(stderr, "\n");
    rx::print(stderr, "{}:{}:{}: ", location.file_name(), location.line(),
              location.column());
    rx::vprint_nonunicode(stderr, fmt, args);
    rx::print(stderr, "\n");
    std::fflush(stdout);
    std::fflush(stderr);
  }

  std::abort();
}
