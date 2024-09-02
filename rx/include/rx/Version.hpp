#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace rx {
enum class VersionTag { Draft, RC, Release };

struct Version {
  std::uint32_t raw{};
  VersionTag tag{};
  std::uint32_t tagVersion{};
  std::uint32_t gitTag{};
  bool dirty{};

  std::string toString() const {
    std::string result = std::to_string(raw);

    if (tag == VersionTag::Draft && gitTag != 0) {
      result += '-';
      auto value = gitTag;
      char buf[7];
      for (int i = 0; i < 7; ++i) {
        auto digit = value & 0xf;
        value >>= 4;
        if (digit >= 10) {
          buf[i] = 'a' + (digit - 10);
        } else {
          buf[i] = '0' + digit;
        }
      }

      for (int i = 0; i < 7; ++i) {
        result += buf[6 - i];
      }
    }

    switch (tag) {
    case VersionTag::Draft:
      result += " Draft";
      break;
    case VersionTag::RC:
      result += " RC";
      break;
    case VersionTag::Release:
      break;
    }

    if (tagVersion) {
      result += std::to_string(tagVersion);
    }


    if (dirty) {
      result += '+';
    }

    return result;
  }
};

Version getVersion();
} // namespace rx
