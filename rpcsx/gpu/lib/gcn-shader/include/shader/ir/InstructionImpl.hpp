#pragma once

#include "Instruction.hpp"
#include "Kind.hpp"
#include "Location.hpp"
#include "NodeImpl.hpp"
#include "PrintableWrapper.hpp"
#include "RegionLike.hpp"
#include <ostream>
#include <span>

namespace shader::ir {
struct InstructionImpl : NodeImpl {
  Kind kind;
  unsigned op;

  RegionLike parent;
  Instruction prev;
  Instruction next;
  OperandList operands;

  InstructionImpl(Location location, Kind kind, unsigned op,
                  std::span<const Operand> operands = {})
      : kind(kind), op(op) {
    setLocation(location);

    for (auto &&op : operands) {
      addOperand(std::move(op));
    }
  }

  template <typename T>
    requires std::is_enum_v<T>
  void addOperand(T enumValue) {
    addOperand(std::to_underlying(enumValue));
  }

  void addOperand(Operand operand);
  Operand replaceOperand(int index, Operand operand);
  Operand eraseOperand(int index, int count);
  void remove();
  void erase();

  decltype(auto) getOperand(std::size_t i) const {
    return operands.getOperand(i);
  }

  decltype(auto) getOperands() const { return std::span(operands); }

  void print(std::ostream &os, NameStorage &ns,
             const PrintOptions &) const override {
    os << getInstructionName(kind, op);

    if (!operands.empty()) {
      os << "(";
      for (bool first = true; auto &operand : operands) {
        if (first) {
          first = false;
        } else {
          os << ", ";
        }
        operand.print(os, ns);
      }
      os << ")";
    }
  }

  Node clone(Context &context, CloneMap &map) const override;
};
} // namespace shader::ir
