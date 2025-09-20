#pragma once

#include "Construct.hpp"

namespace shader::ir {
template <typename ImplT>
struct SelectionConstructWrapper : ConstructWrapper<ImplT> {
  using ConstructWrapper<ImplT>::ConstructWrapper;
  using ConstructWrapper<ImplT>::operator=;
};

struct SelectionConstructImpl;

struct SelectionConstruct : SelectionConstructWrapper<SelectionConstructImpl> {
  using SelectionConstructWrapper<
      SelectionConstructImpl>::SelectionConstructWrapper;
  using SelectionConstructWrapper<SelectionConstructImpl>::operator=;
};

struct SelectionConstructImpl : ConstructImpl {
  using ConstructImpl::ConstructImpl;

  Node clone(Context &context, CloneMap &map) const override;

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &opts) const override {
    os << '%' << ns.getNameOf(const_cast<SelectionConstructImpl *>(this));
    os << " = ";

    if (getOperands().size() > 2) {
      os << '[';
      for (bool first = true; auto &operand : getOperands().subspan(2)) {
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
    os << "selection (header = ";
    getOperand(0).print(os, ns);
    os << ", merge = ";
    getOperand(1).print(os, ns);
    os << ") {\n";

    for (auto child : children()) {
      bodyOpts.printIdent(os);

      child.print(os, ns, bodyOpts);
      os << "\n";
    }

    opts.printIdent(os);
    os << "}";
  }
};
} // namespace shader::ir
