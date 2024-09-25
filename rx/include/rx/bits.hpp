#pragma once

namespace rx {
template <typename T>
inline constexpr T getBits(T value, unsigned end, unsigned begin) {
  return (value >> begin) & ((1ull << (end - begin + 1)) - 1);
}

template <typename T> inline constexpr T getBit(T value, unsigned bit) {
  return (value >> bit) & 1;
}
} // namespace rx
