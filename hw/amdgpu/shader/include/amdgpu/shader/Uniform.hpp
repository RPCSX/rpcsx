#pragma once

#include "AccessOp.hpp"
#include "TypeId.hpp"
#include "spirv/spirv-builder.hpp"

#include <cstdint>
#include <set>

namespace amdgpu::shader {
struct UniformInfo {
  std::uint32_t buffer[8];
  int index;
  TypeId typeId;
  spirv::PointerType type;
  spirv::VariableValue variable;
  AccessOp accessOp = AccessOp::None;
  bool isBuffer;
};
} // namespace amdgpu::shader
