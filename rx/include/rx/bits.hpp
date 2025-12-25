#pragma once

namespace rx {
template <typename T>
inline constexpr T getBits(T value, unsigned end, unsigned begin) {
  return (value >> begin) & ((1ull << (end - begin + 1)) - 1);
}

template <typename T, typename U>
inline constexpr T setBits(T value, unsigned end, unsigned begin, U bits) {
  auto mask = ((1ull << (end - begin + 1)) - 1) << begin;
  return static_cast<T>((value & ~mask) |
                        ((static_cast<T>(bits) << begin) & mask));
}

template <typename T> inline constexpr T getBit(T value, unsigned bit) {
  return (value >> bit) & 1;
}
template <typename T, typename U>
inline constexpr T setBit(T value, unsigned bit, U bitValue) {
  auto mask = 1ull << bit;

  return static_cast<T>((value & ~mask) |
                        ((static_cast<T>(bitValue) << bit) & mask));
}
} // namespace rx
