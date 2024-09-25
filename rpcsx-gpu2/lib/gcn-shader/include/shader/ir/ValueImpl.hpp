#pragma once

#include "InstructionImpl.hpp"
#include "NameStorage.hpp"
#include "Node.hpp"
#include "Value.hpp"

namespace shader::ir {
struct ValueImpl : InstructionImpl {
  std::set<ValueUse> uses;

  ValueImpl(Location location, Kind kind, unsigned op,
            std::span<const Operand> operands = {})
      : InstructionImpl(location, kind, op, operands) {}

  void addUse(Instruction user, int operandIndex) {
    uses.insert({user, this, operandIndex});
  }

  void removeUse(Instruction user, int operandIndex) {
    uses.erase({user, this, operandIndex});
  }

  std::set<Node> getUserList() const {
    std::set<Node> list;
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

  void print(std::ostream &os, NameStorage &ns) const override {
    os << '%' << ns.getNameOf(const_cast<ValueImpl *>(this));
    os << " = ";
    InstructionImpl::print(os, ns);
  }

  Node clone(Context &context, CloneMap &map) const override;
};
} // namespace shader::ir
