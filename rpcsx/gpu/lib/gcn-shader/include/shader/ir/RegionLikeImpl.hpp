#pragma once

#include "PreincNodeIterable.hpp"
#include "RegionLike.hpp"

namespace shader::ir {
struct RegionLikeImpl {
  Instruction first = nullptr;
  Instruction last = nullptr;

  virtual ~RegionLikeImpl() = default;

  template <typename T = Instruction> auto children() const {
    return Range<T>{first, nullptr};
  }

  template <typename T = Instruction> auto revChildren() const {
    return RevRange<T>{last, nullptr};
  }

  void insertAfter(Instruction point, Instruction node);
  void prependChild(Instruction node);
  void addChild(Instruction node);

  void printRegion(std::ostream &os, NameStorage &ns,
                   const PrintOptions &opts) const;

  auto getParent() const;
};
} // namespace shader::ir
