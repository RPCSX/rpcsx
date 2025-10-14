#include "transform/replace.hpp"
#include "dialect.hpp"
#include <rx/die.hpp>

using namespace shader;
using namespace shader::transform;

void shader::transform::replaceTerminatorTarget(ir::Instruction terminator,
                                    int operandIndex, ir::Value newTarget) {
  auto prevTarget = terminator.getOperand(operandIndex).getAsValue();
  terminator.replaceOperand(operandIndex, newTarget);
  auto selection = terminator.getPrev();

  if (selection == ir::spv::OpSelectionMerge ||
      selection == ir::spv::OpLoopMerge) {
    for (std::size_t i = 0, end = selection.getOperandCount(); i < end; ++i) {
      if (selection.getOperand(i) == prevTarget) {
        selection.replaceOperand(i, newTarget);
        break;
      }
    }
  }
}

bool shader::transform::replaceTerminatorTarget(ir::Instruction terminator,
                                    ir::Value oldTarget, ir::Value newTarget) {
  bool changes = false;
  for (std::size_t i = 0, end = terminator.getOperandCount(); i < end; ++i) {
    if (terminator.getOperand(i) == oldTarget) {
      replaceTerminatorTarget(terminator, i, newTarget);
      changes = true;
    }
  }

  return changes;
}