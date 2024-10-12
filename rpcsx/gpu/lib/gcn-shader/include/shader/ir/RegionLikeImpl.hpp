#pragma once

#include "PreincNodeIterable.hpp"
#include "RegionLike.hpp"

namespace shader::ir {
struct RegionLikeImpl {
  Instruction first = nullptr;
  Instruction last = nullptr;

  virtual ~RegionLikeImpl() = default;

  template <typename T = Instruction> auto children() const {
    return PreincNodeIterable<T>{first, nullptr};
  }

  template <typename T = Instruction> auto revChildren() const {
    return RevPreincNodeIterable<T>{last, nullptr};
  }

  virtual void insertAfter(Instruction point, Instruction node);
  virtual void prependChild(Instruction node);
  virtual void addChild(Instruction node);
};
} // namespace shader::ir
