#include "hexdump.hpp"
#include <cstdio>
#include <cstring>
#include <string>

void rx::hexdump(std::span<std::byte> bytes) {
  unsigned sizeWidth = 1;

  {
    std::size_t size = bytes.size() / 0x10;

    while (size > 0) {
      sizeWidth++;
      size /= 0x10;
    }
  }

  flockfile(stderr);
  std::fprintf(stderr, "%s ", std::string(sizeWidth, ' ').c_str());
  for (unsigned i = 0; i < 16; ++i) {
    std::fprintf(stderr, " %02x", i);
  }
  std::fprintf(stderr, "\n%s ", std::string(sizeWidth, ' ').c_str());
  for (unsigned i = 0; i < 16; ++i) {
    std::fprintf(stderr, " --");
  }

  std::byte zeros[16]{};

  bool wasAllZeros = true;
  bool dotsPrinted = false;
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i % 16 == 0) {
      if (i > 0) {
        if (wasAllZeros && bytes.size() - i > 16) {
          if (std::memcmp(bytes.data() + i, zeros, 16) == 0) {
            if (!dotsPrinted) {
              dotsPrinted = true;
              std::printf("\n...");
            }
            i += 15;
            continue;
          }
        }

        if (!dotsPrinted) {
          std::fprintf(stderr, " | ");

          for (std::size_t j = i - 16; j < i; ++j) {
            auto c = unsigned(bytes[j]);
            std::fprintf(stderr, "%c",
                         (std::isprint(c) && c != '\n') ? c : '.');
          }
        }
      }
      std::fprintf(stderr, "\n");
      std::fprintf(stderr, "%0*zx ", sizeWidth, i);
      wasAllZeros = true;
      dotsPrinted = false;
    }

    std::fprintf(stderr, " %02x", unsigned(bytes[i]));

    if (bytes[i] != std::byte{0}) {
      wasAllZeros = false;
    }
  }

  if (!bytes.empty()) {
    for (std::size_t i = 0; i < (16 - (bytes.size() % 16)) % 16; ++i) {
      std::fprintf(stderr, "   ");
    }
    std::fprintf(stderr, " | ");

    for (std::size_t j = bytes.size() -
                         std::min(bytes.size(),
                                  (bytes.size() % 16 ? bytes.size() % 16 : 16));
         j < bytes.size(); ++j) {
      auto c = unsigned(bytes[j]);
      std::fprintf(stderr, "%c", (std::isprint(c) && c != '\n') ? c : '.');
    }
  }
  std::fprintf(stderr, "\n");
  funlockfile(stderr);
}
