#include "opt.hpp"
#include "analyze.hpp"
#include "ir.hpp"
#include <unordered_map>

using namespace shader;

namespace {
bool isEqOperands(ir::Instruction a, ir::Instruction b) {
  auto opCount = a.getOperandCount();
  if (opCount != b.getOperandCount()) {
    return false;
  }

  for (std::size_t i = 0; i < opCount; ++i) {
    if (a.getOperand(i) != b.getOperand(i)) {
      return false;
    }
  }

  return true;
}
} // namespace

static bool combineInstructions(CFG &cfg, ir::Region region) {
  auto domTree = buildDomTree(cfg);

  std::unordered_map<ir::InstructionId, std::vector<ir::Instruction>>
      instructions;
  auto findPrevInst = [&](ir::Instruction inst) -> ir::Instruction {
    for (auto prevInst : instructions[inst.getInstId()]) {
      if (!isEqOperands(inst, prevInst)) {
        continue;
      }

      if (!dominates(prevInst, inst, false, domTree)) {
        continue;
      }

      return prevInst;
    }

    return nullptr;
  };

  std::size_t changes = 0;

  for (auto bb : cfg.getPreorderNodes()) {
    for (auto inst : bb->rangeWithoutLabelAndTerminator()) {
      if (!shader::isWithoutSideEffects(inst.getInstId())) {
        continue;
      }

      if (auto prev = findPrevInst(inst)) {
        if (auto value = inst.cast<ir::Value>()) {
          value.replaceAllUsesWith(prev.staticCast<ir::Value>());
        }
        inst.remove();
        changes++;
      } else {
        instructions[inst.getInstId()].push_back(inst);
      }
    }
  }

  // std::cerr << "combined instructions: " << changes << "\n";
  return changes != 0;
}

bool shader::optimize(ir::Context &context, ir::Region region) {
  auto cfg = buildCFG(region.getFirst());
  return combineInstructions(cfg, region);
}