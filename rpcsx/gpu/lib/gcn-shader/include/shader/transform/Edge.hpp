#pragma once

#include "analyze.hpp"
#include "ir.hpp"
#include "replace.hpp"

namespace shader::transform {
class Edge {
  ir::Block mFromBlock;
  int mToOperandIndex;

public:
  Edge(ir::Block fromBlock, int toOperandIndex)
      : mFromBlock(fromBlock), mToOperandIndex(toOperandIndex) {}

  [[nodiscard]] ir::Block from() const { return mFromBlock; }
  [[nodiscard]] ir::Block to() const {
    return getTerminator(mFromBlock)
        .getOperand(mToOperandIndex)
        .getAsValue()
        .staticCast<ir::Block>();
  }

  [[nodiscard]] int operandIndex() const { return mToOperandIndex; }

  void replaceSuccessor(ir::Value newSuccessor) {
    replaceTerminatorTarget(getTerminator(mFromBlock), mToOperandIndex,
                            newSuccessor);
  }

  bool operator==(const Edge &) const = default;
};
}