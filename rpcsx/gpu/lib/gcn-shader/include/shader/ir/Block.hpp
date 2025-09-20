#pragma once

#include "RegionLike.hpp"
#include "RegionLikeImpl.hpp"
#include "ValueImpl.hpp"

namespace shader::ir {
template <typename ImplT>
struct BlockWrapper : RegionLikeWrapper<ImplT, ValueWrapper> {
  using RegionLikeWrapper<ImplT, ValueWrapper>::RegionLikeWrapper;
  using RegionLikeWrapper<ImplT, ValueWrapper>::operator=;
};

struct BlockImpl;

struct Block : BlockWrapper<BlockImpl> {
  using BlockWrapper<BlockImpl>::BlockWrapper;
  using BlockWrapper<BlockImpl>::operator=;
};

struct BlockImpl : ValueImpl, RegionLikeImpl {
  using ValueImpl::ValueImpl;

  Node clone(Context &context, CloneMap &map) const override;

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &opts) const override {
    os << '%' << ns.getNameOf(const_cast<BlockImpl *>(this));
    os << " = ";

    if (!getOperands().empty()) {
      os << '[';
      for (bool first = true; auto &operand : getOperands()) {
        if (first) {
          first = false;
        } else {
          os << ", ";
        }

        operand.print(os, ns);
      }
      os << "] ";
    }

    os << "{\n";
    auto childOpts = opts.nextLevel();
    for (auto child : children()) {
      childOpts.printIdent(os);
      child.print(os, ns, childOpts);
      os << "\n";
    }
    opts.printIdent(os);
    os << "}";
  }
};
} // namespace shader::ir
