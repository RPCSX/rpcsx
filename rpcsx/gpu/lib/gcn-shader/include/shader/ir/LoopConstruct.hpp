#pragma once

#include "Construct.hpp"

namespace shader::ir {
template <typename ImplT>
struct ContinueConstructWrapper : ConstructWrapper<ImplT> {
  using ConstructWrapper<ImplT>::ConstructWrapper;
  using ConstructWrapper<ImplT>::operator=;
};

struct ContinueConstructImpl;
struct ContinueConstruct : ContinueConstructWrapper<ContinueConstructImpl> {
  using ContinueConstructWrapper<
      ContinueConstructImpl>::ContinueConstructWrapper;
  using ContinueConstructWrapper<ContinueConstructImpl>::operator=;
};

template <typename ImplT>
struct LoopConstructWrapper : ConstructWrapper<ImplT> {
  using ConstructWrapper<ImplT>::ConstructWrapper;
  using ConstructWrapper<ImplT>::operator=;

  Block getLatch() { return this->impl->last.template staticCast<Block>(); }

  ContinueConstruct getContinue() {
    return this->impl->getOperand(2)
        .getAsValue()
        .template staticCast<ContinueConstruct>();
  }
};

struct LoopConstructImpl;
struct LoopConstruct : LoopConstructWrapper<LoopConstructImpl> {
  using LoopConstructWrapper<LoopConstructImpl>::LoopConstructWrapper;
  using LoopConstructWrapper<LoopConstructImpl>::operator=;
};

struct LoopConstructImpl : ConstructImpl {
  using ConstructImpl::ConstructImpl;

  Node clone(Context &context, CloneMap &map) const override;

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &opts) const override {
    os << '%' << ns.getNameOf(const_cast<LoopConstructImpl *>(this));
    os << " = ";

    if (getOperands().size() > 3) {
      os << '[';
      for (bool first = true; auto &operand : getOperands().subspan(3)) {
        if (first) {
          first = false;
        } else {
          os << ", ";
        }

        operand.print(os, ns);
      }
      os << "] ";
    }

    auto bodyOpts = opts.nextLevel();
    os << "loop (header = ";
    getOperand(0).print(os, ns);
    os << ", merge = ";
    getOperand(1).print(os, ns);
    os << ", latch = ";
    os << "%" << ns.getNameOf(last);
    os << ") {\n";

    {
      bodyOpts.printIdent(os);
      os << "body {\n";
      for (auto childOpts = bodyOpts.nextLevel(); auto child : children()) {
        childOpts.printIdent(os);
        child.print(os, ns, childOpts);
        os << "\n";
      }

      bodyOpts.printIdent(os);
      os << "}\n";
    }

    {
      bodyOpts.printIdent(os);
      os << "continue {\n";
      bodyOpts.printIdent(os, 1);
      getOperand(2).getAsValue().print(os, ns, bodyOpts.nextLevel());
      os << "\n";
      bodyOpts.printIdent(os);
      os << "}\n";
    }

    opts.printIdent(os);
    os << "}";
  }
};

struct ContinueConstructImpl : ConstructImpl {
  using ConstructImpl::ConstructImpl;
  Node clone(Context &context, CloneMap &map) const override;
};

} // namespace shader::ir
