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
  BlockImpl(Location loc);
  Node clone(Context &context, CloneMap &map) const override;

  void print(std::ostream &os, NameStorage &ns) const override {
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
    for (auto child : children()) {
      os << "  ";
      child.print(os, ns);
      os << "\n";
    }
    os << "}";
  }
};
} // namespace ir
