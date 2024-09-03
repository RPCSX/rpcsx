#pragma once

#include <cstdint>

namespace rx {
inline constexpr std::uint64_t alignUp(std::uint64_t value,
                                       std::uint64_t alignment) {
  return (value + (alignment - 1)) & ~(alignment - 1);
}
inline constexpr std::uint64_t alignDown(std::uint64_t value,
                                         std::uint64_t alignment) {
  return value & ~(alignment - 1);
}
} // namespace rx
