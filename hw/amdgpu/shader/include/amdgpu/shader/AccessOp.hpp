#pragma once

namespace amdgpu::shader {
enum class AccessOp { None = 0, Load = 1 << 0, Store = 1 << 1 };

constexpr AccessOp operator|(AccessOp lhs, AccessOp rhs) {
  return static_cast<AccessOp>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
constexpr AccessOp operator&(AccessOp lhs, AccessOp rhs) {
  return static_cast<AccessOp>(static_cast<int>(lhs) & static_cast<int>(rhs));
}
constexpr AccessOp operator~(AccessOp rhs) {
  return static_cast<AccessOp>(~static_cast<int>(rhs));
}
constexpr AccessOp &operator|=(AccessOp &lhs, AccessOp rhs) {
  return ((lhs = lhs | rhs));
}
constexpr AccessOp &operator&=(AccessOp &lhs, AccessOp rhs) {
  return ((lhs = lhs & rhs));
}
} // namespace amdgpu::shader
