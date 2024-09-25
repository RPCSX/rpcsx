#pragma once

#include "dialect/spv.hpp"

namespace shader::spv {
struct TypeInfo {
  ir::spv::Op baseType = {};
  ir::spv::Op componentType = {};
  int componentWidth = 0;
  int componentsCount = 1;
  bool isSigned = false;

  int width() const { return componentWidth * componentsCount; }
  bool operator==(const TypeInfo &other) const = default;
};

TypeInfo getTypeInfo(ir::Value type);
} // namespace shader::spv
