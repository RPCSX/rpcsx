#pragma once

#include "InstructionImpl.hpp"
#include "NameStorage.hpp"
#include "Node.hpp"
#include "Value.hpp"
#include "rx/FunctionRef.hpp"

namespace shader::ir {
struct ValueImpl : InstructionImpl {
  std::set<ValueUse> uses;

  using InstructionImpl::InstructionImpl;

  void addUse(Instruction user, int operandIndex) {
    uses.insert({.user=user, .node=this, .operandIndex=operandIndex});
  }

  void removeUse(Instruction user, int operandIndex) {
    uses.erase({.user=user, .node=this, .operandIndex=operandIndex});
  }

  std::set<Instruction> getUserList() const {
    std::set<Instruction> list;
    for (auto use : uses) {
      list.insert(use.user);
    }
    return list;
  }

  void replaceAllUsesWith(Value other) {
    if (other == this) {
      std::abort();
    }

    while (!uses.empty()) {
      auto use = *uses.begin();
      if (other == nullptr) {
        use.user.replaceOperand(use.operandIndex, nullptr);
      } else {
        use.user.replaceOperand(use.operandIndex, other);
      }
    }
  }

  void replaceUsesIf(Value other, rx::FunctionRef<bool(ValueUse)> cond) {
    if (other == this) {
      std::abort();
    }

    auto savedUses = uses;

    for (auto &use : savedUses) {
      if (cond(use)) {
        if (other == nullptr) {
          use.user.replaceOperand(use.operandIndex, nullptr);
        } else {
          use.user.replaceOperand(use.operandIndex, other);
        }
      }
    }
  }

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &opts) const override {
    os << '%' << ns.getNameOf(const_cast<ValueImpl *>(this));
    os << " = ";
    InstructionImpl::print(os, ns, opts);
  }

  Node clone(Context &context, CloneMap &map) const override;
};
} // namespace shader::ir
