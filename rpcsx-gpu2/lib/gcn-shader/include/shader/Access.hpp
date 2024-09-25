#pragma once

namespace shader {
enum class Access {
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  ReadWrite = Read | Write
};

constexpr Access operator|(Access lhs, Access rhs) {
  return static_cast<Access>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
constexpr Access operator&(Access lhs, Access rhs) {
  return static_cast<Access>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
constexpr Access operator~(Access rhs) {
  return static_cast<Access>(~static_cast<int>(rhs));
}
constexpr Access &operator|=(Access &lhs, Access rhs) {
  return ((lhs = lhs | rhs));
}
constexpr Access &operator&=(Access &lhs, Access rhs) {
  return ((lhs = lhs & rhs));
}
} // namespace shader
