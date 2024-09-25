#pragma once

#include "Instruction.hpp"

namespace shader::ir {
struct RegionLike;

template <typename ImplT, template <typename> typename BaseWrapper>
struct RegionLikeWrapper : BaseWrapper<ImplT> {
  using BaseWrapper<ImplT>::BaseWrapper;
  using BaseWrapper<ImplT>::operator=;

  void appendRegion(RegionLike other);

  auto getFirst() { return this->impl->first; }
  auto getLast() { return this->impl->last; }
  bool empty() { return this->impl->first == nullptr; }

  void insertAfter(Instruction point, Instruction node) {
    this->impl->insertAfter(point, node);
  }
  void prependChild(Instruction node) { this->impl->prependChild(node); }

  void addChild(Instruction node) { this->impl->addChild(node); }
  template <typename T = Instruction> auto children() {
    return this->impl->template children<T>();
  }
  template <typename T = Instruction> auto revChildren() {
    return this->impl->template revChildren<T>();
  }
};

struct RegionLikeImpl;
struct RegionLike : RegionLikeWrapper<RegionLikeImpl, PointerWrapper> {
  using RegionLikeWrapper::RegionLikeWrapper;
  using RegionLikeWrapper::operator=;
};
} // namespace shader::ir
