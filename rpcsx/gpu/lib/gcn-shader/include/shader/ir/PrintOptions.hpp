#pragma once

#include <ostream>
#include <string>

namespace shader {
struct PrintOptions {
  int identLevel = 0;
  int identCount = 2;
  char identChar = ' ';

  [[nodiscard]] PrintOptions nextLevel() const {
    auto result = *this;
    result.identLevel++;
    return result;
  }

  void printIdent(std::ostream &os, int offset = 0) const {
    os << std::string((identLevel + offset) * identCount, identChar);
  }
};
} // namespace shader
