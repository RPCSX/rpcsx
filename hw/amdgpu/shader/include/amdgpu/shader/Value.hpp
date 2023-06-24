#pragma once
#include <spirv/spirv-builder.hpp>

namespace amdgpu::shader {
struct Value {
  spirv::Type type;
  spirv::Value value;

  Value() = default;
  Value(spirv::Type type, spirv::Value value) : type(type), value(value) {}

  explicit operator bool() const { return static_cast<bool>(value); }
  bool operator==(Value other) const { return value == other.value; }
};
} // namespace amdgpu::shader
