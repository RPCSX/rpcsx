#pragma once

#include "Block.hpp"

namespace shader::ir {
template <typename ImplT>
struct ConstructWrapper : BlockWrapper<ImplT> {
  using BlockWrapper<ImplT>::BlockWrapper;
  using BlockWrapper<ImplT>::operator=;

  ir::Block getHeader() {
    return this->impl->getOperand(0)
        .getAsValue()
        .template staticCast<ir::Block>();
  }

  void setHeader(ir::Block block) {
    this->impl->replaceOperand(0, block);
  }

  ir::Block getMerge() {
    return this->impl->getOperand(1)
        .getAsValue()
        .template staticCast<ir::Block>();
  }

  void setMerge(ir::Block block) {
    this->impl->replaceOperand(1, block);
  }
};

struct ConstructImpl;

struct Construct : ConstructWrapper<ConstructImpl> {
  using ConstructWrapper<ConstructImpl>::ConstructWrapper;
  using ConstructWrapper<ConstructImpl>::operator=;
};

struct ConstructImpl : BlockImpl {
  using BlockImpl::BlockImpl;
};
} // namespace shader::ir
