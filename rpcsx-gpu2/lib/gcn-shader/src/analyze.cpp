#include "analyze.hpp"
#include "dialect.hpp"
#include "ir.hpp"
#include "rx/die.hpp"
#include "spv.hpp"
#include <iostream>
#include <print>

using namespace shader;

static std::unordered_set<ir::InstructionId> g_instsWithoutSideEffects = {
    ir::getInstructionId(ir::spv::OpAccessChain),
    ir::getInstructionId(ir::spv::OpInBoundsAccessChain),
    ir::getInstructionId(ir::spv::OpPtrAccessChain),
    ir::getInstructionId(ir::spv::OpArrayLength),
    ir::getInstructionId(ir::spv::OpInBoundsPtrAccessChain),
    ir::getInstructionId(ir::spv::OpVectorExtractDynamic),
    ir::getInstructionId(ir::spv::OpVectorInsertDynamic),
    ir::getInstructionId(ir::spv::OpVectorShuffle),
    ir::getInstructionId(ir::spv::OpCompositeConstruct),
    ir::getInstructionId(ir::spv::OpCompositeExtract),
    ir::getInstructionId(ir::spv::OpCompositeInsert),
    ir::getInstructionId(ir::spv::OpTranspose),
    ir::getInstructionId(ir::spv::OpConvertFToU),
    ir::getInstructionId(ir::spv::OpConvertFToS),
    ir::getInstructionId(ir::spv::OpConvertSToF),
    ir::getInstructionId(ir::spv::OpConvertUToF),
    ir::getInstructionId(ir::spv::OpUConvert),
    ir::getInstructionId(ir::spv::OpSConvert),
    ir::getInstructionId(ir::spv::OpFConvert),
    ir::getInstructionId(ir::spv::OpQuantizeToF16),
    ir::getInstructionId(ir::spv::OpConvertPtrToU),
    ir::getInstructionId(ir::spv::OpSatConvertSToU),
    ir::getInstructionId(ir::spv::OpSatConvertUToS),
    ir::getInstructionId(ir::spv::OpConvertUToPtr),
    ir::getInstructionId(ir::spv::OpPtrCastToGeneric),
    ir::getInstructionId(ir::spv::OpGenericCastToPtr),
    ir::getInstructionId(ir::spv::OpGenericCastToPtrExplicit),
    ir::getInstructionId(ir::spv::OpBitcast),
    ir::getInstructionId(ir::spv::OpSNegate),
    ir::getInstructionId(ir::spv::OpFNegate),
    ir::getInstructionId(ir::spv::OpIAdd),
    ir::getInstructionId(ir::spv::OpFAdd),
    ir::getInstructionId(ir::spv::OpISub),
    ir::getInstructionId(ir::spv::OpFSub),
    ir::getInstructionId(ir::spv::OpIMul),
    ir::getInstructionId(ir::spv::OpFMul),
    ir::getInstructionId(ir::spv::OpUDiv),
    ir::getInstructionId(ir::spv::OpSDiv),
    ir::getInstructionId(ir::spv::OpFDiv),
    ir::getInstructionId(ir::spv::OpUMod),
    ir::getInstructionId(ir::spv::OpSRem),
    ir::getInstructionId(ir::spv::OpSMod),
    ir::getInstructionId(ir::spv::OpFRem),
    ir::getInstructionId(ir::spv::OpFMod),
    ir::getInstructionId(ir::spv::OpVectorTimesScalar),
    ir::getInstructionId(ir::spv::OpMatrixTimesScalar),
    ir::getInstructionId(ir::spv::OpVectorTimesMatrix),
    ir::getInstructionId(ir::spv::OpMatrixTimesVector),
    ir::getInstructionId(ir::spv::OpMatrixTimesMatrix),
    ir::getInstructionId(ir::spv::OpOuterProduct),
    ir::getInstructionId(ir::spv::OpDot),
    ir::getInstructionId(ir::spv::OpIAddCarry),
    ir::getInstructionId(ir::spv::OpISubBorrow),
    ir::getInstructionId(ir::spv::OpUMulExtended),
    ir::getInstructionId(ir::spv::OpSMulExtended),
    ir::getInstructionId(ir::spv::OpAny),
    ir::getInstructionId(ir::spv::OpAll),
    ir::getInstructionId(ir::spv::OpIsNan),
    ir::getInstructionId(ir::spv::OpIsInf),
    ir::getInstructionId(ir::spv::OpIsFinite),
    ir::getInstructionId(ir::spv::OpIsNormal),
    ir::getInstructionId(ir::spv::OpSignBitSet),
    ir::getInstructionId(ir::spv::OpLessOrGreater),
    ir::getInstructionId(ir::spv::OpOrdered),
    ir::getInstructionId(ir::spv::OpUnordered),
    ir::getInstructionId(ir::spv::OpLogicalEqual),
    ir::getInstructionId(ir::spv::OpLogicalNotEqual),
    ir::getInstructionId(ir::spv::OpLogicalOr),
    ir::getInstructionId(ir::spv::OpLogicalAnd),
    ir::getInstructionId(ir::spv::OpLogicalNot),
    ir::getInstructionId(ir::spv::OpSelect),
    ir::getInstructionId(ir::spv::OpIEqual),
    ir::getInstructionId(ir::spv::OpINotEqual),
    ir::getInstructionId(ir::spv::OpUGreaterThan),
    ir::getInstructionId(ir::spv::OpSGreaterThan),
    ir::getInstructionId(ir::spv::OpUGreaterThanEqual),
    ir::getInstructionId(ir::spv::OpSGreaterThanEqual),
    ir::getInstructionId(ir::spv::OpULessThan),
    ir::getInstructionId(ir::spv::OpSLessThan),
    ir::getInstructionId(ir::spv::OpULessThanEqual),
    ir::getInstructionId(ir::spv::OpSLessThanEqual),
    ir::getInstructionId(ir::spv::OpFOrdEqual),
    ir::getInstructionId(ir::spv::OpFUnordEqual),
    ir::getInstructionId(ir::spv::OpFOrdNotEqual),
    ir::getInstructionId(ir::spv::OpFUnordNotEqual),
    ir::getInstructionId(ir::spv::OpFOrdLessThan),
    ir::getInstructionId(ir::spv::OpFUnordLessThan),
    ir::getInstructionId(ir::spv::OpFOrdGreaterThan),
    ir::getInstructionId(ir::spv::OpFUnordGreaterThan),
    ir::getInstructionId(ir::spv::OpFOrdLessThanEqual),
    ir::getInstructionId(ir::spv::OpFUnordLessThanEqual),
    ir::getInstructionId(ir::spv::OpFOrdGreaterThanEqual),
    ir::getInstructionId(ir::spv::OpFUnordGreaterThanEqual),
    ir::getInstructionId(ir::spv::OpShiftRightLogical),
    ir::getInstructionId(ir::spv::OpShiftRightArithmetic),
    ir::getInstructionId(ir::spv::OpShiftLeftLogical),
    ir::getInstructionId(ir::spv::OpBitwiseOr),
    ir::getInstructionId(ir::spv::OpBitwiseXor),
    ir::getInstructionId(ir::spv::OpBitwiseAnd),
    ir::getInstructionId(ir::spv::OpNot),
    ir::getInstructionId(ir::spv::OpBitFieldInsert),
    ir::getInstructionId(ir::spv::OpBitFieldSExtract),
    ir::getInstructionId(ir::spv::OpBitFieldUExtract),
    ir::getInstructionId(ir::spv::OpBitReverse),
    ir::getInstructionId(ir::spv::OpBitCount),
    ir::getInstructionId(ir::spv::OpDPdx),
    ir::getInstructionId(ir::spv::OpDPdy),
    ir::getInstructionId(ir::spv::OpFwidth),
    ir::getInstructionId(ir::spv::OpDPdxFine),
    ir::getInstructionId(ir::spv::OpDPdyFine),
    ir::getInstructionId(ir::spv::OpFwidthFine),
    ir::getInstructionId(ir::spv::OpDPdxCoarse),
    ir::getInstructionId(ir::spv::OpDPdyCoarse),
    ir::getInstructionId(ir::spv::OpFwidthCoarse),
    ir::getInstructionId(ir::spv::OpPhi),

    ir::getInstructionId(ir::amdgpu::IMM),
    ir::getInstructionId(ir::amdgpu::USER_SGPR),
    ir::getInstructionId(ir::amdgpu::NEG_ABS),
    ir::getInstructionId(ir::amdgpu::OMOD),
    ir::getInstructionId(ir::amdgpu::VBUFFER),
    ir::getInstructionId(ir::amdgpu::SAMPLER),
    ir::getInstructionId(ir::amdgpu::TBUFFER),
    ir::getInstructionId(ir::amdgpu::POINTER),
    ir::getInstructionId(ir::amdgpu::PS_INPUT_VGPR),
    ir::getInstructionId(ir::amdgpu::PS_COMP_SWAP),
};

static bool isGlobal(ir::Instruction inst) {
  return inst == ir::spv::OpVariable || inst == ir::spv::OpConstantTrue ||
         inst == ir::spv::OpConstantFalse || inst == ir::spv::OpConstant ||
         inst == ir::spv::OpConstantComposite ||
         inst == ir::spv::OpConstantSampler ||
         inst == ir::spv::OpConstantNull ||
         inst == ir::spv::OpSpecConstantTrue ||
         inst == ir::spv::OpSpecConstantFalse ||
         inst == ir::spv::OpSpecConstant ||
         inst == ir::spv::OpSpecConstantComposite ||
         inst == ir::spv::OpSpecConstantOp;
}

bool shader::isTerminator(ir::Instruction inst) {
  return spv::isTerminatorInst(inst.getInstId());
}
bool shader::isBranch(ir::Instruction inst) {
  return inst == ir::spv::OpBranch || inst == ir::spv::OpBranchConditional ||
         inst == ir::spv::OpSwitch;
}
bool shader::isWithoutSideEffects(ir::InstructionId id) {
  return g_instsWithoutSideEffects.contains(id);
}

ir::Value shader::unwrapPointer(ir::Value pointer) {
  while (true) {
    if (pointer == ir::spv::OpAccessChain ||
        pointer == ir::spv::OpInBoundsAccessChain) {
      pointer = pointer.getOperand(1).getAsValue();
      continue;
    }

    return pointer;
  }
}

graph::DomTree<ir::Value> shader::buildDomTree(CFG &cfg, ir::Value root) {
  if (root == nullptr) {
    root = cfg.getEntryLabel();
  }

  return graph::buildDomTree(root, [&](ir::Value region, const auto &cb) {
    for (auto succ : cfg.getSuccessors(region)) {
      cb(succ->getLabel());
    }
  });
}

graph::DomTree<ir::Value> shader::buildPostDomTree(CFG &cfg, ir::Value root) {
  return graph::buildDomTree(root, [&](ir::Value region, const auto &cb) {
    auto node = cfg.getNode(region);
    if (node == nullptr) {
      rx::die("failed to find node of predecessor!");
      return;
    }

    for (auto pred : node->getPredecessors()) {
      cb(pred->getLabel());
    }
  });
}

void CFG::print(std::ostream &os, ir::NameStorage &ns, bool subgraph,
                std::string_view nameSuffix) {
  if (subgraph) {
    os << "subgraph {\n";
  } else {
    os << "digraph {\n";
  }
  for (auto node : getPreorderNodes()) {
    for (auto succ : node->getSuccessors()) {
      os << "  ";
      os << ns.getNameOf(node->getLabel());
      os << nameSuffix;
      os << " -> ";
      os << ns.getNameOf(succ->getLabel());
      os << nameSuffix;
      os << ";\n";
    }
  }
  os << "}\n";
}

std::string CFG::genTest() {
  std::string result;
  result += "ir::Value genCfg(spv::Context &context) {\n";
  result += "  auto loc = context.getUnknownLocation();\n";
  result += "  auto boolT = context.getTypeBool();\n";
  result += "  auto trueV = context.getTrue();\n";
  result += "  auto builder = Builder::createAppend(context, "
            "context.layout.getOrCreateFunctions(context));\n";
  result += "  auto debugs = Builder::createAppend(context, "
            "context.layout.getOrCreateDebugs(context));\n";

  ir::NameStorage ns;

  for (auto node : getPreorderNodes()) {
    auto name = ns.getNameOf(node->getLabel());
    result += "  auto _" + name + " =  builder.createSpvLabel(loc);\n";
    result += "  context.ns.setNameOf(_" + name + ", \"" + name + "\");\n";
    result += "  debugs.createSpvName(loc, _" + name + ", \"" + name + "\");\n";
  }

  for (auto node : getPreorderNodes()) {
    auto name = ns.getNameOf(node->getLabel());
    result +=
        "  builder = Builder::createInsertAfter(context, _" + name + ");\n";
    if (node->getSuccessorCount() == 1) {
      result += "  builder.createSpvBranch(loc, _" +
                ns.getNameOf((*node->getSuccessors().begin())->getLabel()) +
                ");\n";
    } else if (node->getSuccessorCount() == 2) {
      auto firstIt = node->getSuccessors().begin();
      auto secondIt = std::next(firstIt);
      result += "  builder.createSpvBranchConditional(loc, trueV, _" +
                ns.getNameOf((*firstIt)->getLabel()) + ", _" +
                ns.getNameOf((*secondIt)->getLabel()) + ");\n";

    } else if (node->getSuccessorCount() == 0) {
      result += "  builder.createSpvReturn(loc);\n";
      result += "  auto returnBlock = _" + name + ";\n";
    }
  }

  result += "  return returnBlock;\n";
  result += "}\n";

  return result;
}

static void walkSuccessors(ir::Instruction terminator, auto &&cb) {
  if (terminator == ir::spv::OpBranch) {
    cb(terminator.getOperand(0).getAsValue());
    return;
  }

  if (terminator == ir::spv::OpBranchConditional) {
    cb(terminator.getOperand(1).getAsValue());
    cb(terminator.getOperand(2).getAsValue());
    return;
  }

  if (terminator == ir::spv::OpSwitch) {
    for (std::size_t i = 1, end = terminator.getOperandCount(); i < end;
         i += 2) {
      cb(terminator.getOperand(i).getAsValue());
    }
    return;
  }
}

CFG CFG::buildView(CFG::Node *from, PostDomTree *domTree,
                   const std::unordered_set<ir::Value> &stopLabels,
                   ir::Value continueLabel) {
  struct Item {
    CFG::Node *node;
    std::vector<CFG::Node *> successors;
  };

  std::vector<CFG::Node *> workList;
  std::unordered_set<ir::Value> visited;

  workList.push_back(from);
  CFG result;
  result.mEntryNode = result.getOrCreateNode(from->getLabel());
  visited.insert(from->getLabel());

  // for (auto pred : from->getPredecessors()) {
  //   result.getOrCreateNode(pred->getLabel());
  // }

  auto createResultNode = [&](CFG::Node *node) {
    auto newNode = result.getOrCreateNode(node->getLabel());
    newNode->setTerminator(node->getTerminator());
    return newNode;
  };

  while (!workList.empty()) {
    auto item = workList.back();
    workList.pop_back();

    auto resultItem = createResultNode(item);
    result.addPreorderNode(resultItem);

    if (item != from) {
      if (item->getLabel() == continueLabel) {
        continue;
      }
      if (stopLabels.contains(item->getLabel())) {
        if (domTree == nullptr) {
          continue;
        }

        for (auto succ : item->getSuccessors()) {
          if (!domTree->dominates(item->getLabel(), succ->getLabel())) {
            continue;
          }

          auto resultSucc = createResultNode(succ);
          resultItem->addEdge(resultSucc);

          if (visited.insert(succ->getLabel()).second) {
            workList.push_back(succ);
          }
        }

        continue;
      }
    }

    for (auto succ : item->getSuccessors()) {
      auto resultSucc = createResultNode(succ);
      resultItem->addEdge(resultSucc);

      if (visited.insert(succ->getLabel()).second) {
        workList.push_back(succ);
      }
    }
  }

  if (domTree != nullptr) {
    return result;
  }

  for (auto exitLabel : stopLabels) {
    if (exitLabel == nullptr) {
      continue;
    }

    // collect internal branches from exitLabel. Need to collect all blocks
    // first to be able discard edges to not exists in this CFG target blocks
    if (auto from = result.getNode(exitLabel)) {
      for (auto succ : getNode(exitLabel)->getSuccessors()) {
        if (auto to = result.getNode(succ->getLabel())) {
          from->addEdge(to);
        }
      }
    }
  }

  return result;
}

void Construct::invalidateAll() {
  Construct *root = this;
  while (root->parent != nullptr) {
    root = root->parent;
  }

  std::vector<Construct *> workList;
  workList.push_back(root);

  while (!workList.empty()) {
    auto item = workList.back();
    workList.pop_back();
    item->analysis.invalidateAll();

    for (auto &child : item->children) {
      workList.push_back(&child);
    }
  }
}

void Construct::invalidate() {
  invalidateAll();
  // Construct *item = this;
  // while (item != nullptr) {
  //   item->analysis.invalidateAll();
  //   item = item->parent;
  // }
}

CFG shader::buildCFG(ir::Instruction firstInstruction,
                     const std::unordered_set<ir::Value> &exitLabels,
                     ir::Value continueLabel) {
  struct Item {
    CFG::Node *node;
    ir::Instruction iterator;
    std::vector<CFG::Node *> successors;
  };

  CFG result;

  std::vector<Item> workList;
  workList.push_back({.iterator = firstInstruction});

  std::unordered_set<CFG::Node *> visited;

  bool force = true;

  auto addSuccessor = [&](Item &from, ir::Value toLabel) {
    auto to = result.getOrCreateNode(toLabel);
    from.node->addEdge(to);

    if (!force && (exitLabels.contains(from.node->getLabel()) ||
                   from.node->getLabel() == continueLabel)) {
      return;
    }

    if (visited.insert(to).second) {
      result.addPreorderNode(to);
      from.successors.push_back(to);
    }
  };

  while (!workList.empty()) {
    Item &item = workList.back();

    if (item.iterator == nullptr) {
      if (!item.successors.empty()) {
        auto successor = item.successors.back();
        item.successors.pop_back();

        workList.push_back(
            {.node = successor, .iterator = successor->getLabel().getNext()});
        continue;
      }

      result.addPostorderNode(item.node);
      workList.pop_back();
      continue;
    }

    auto inst = std::exchange(item.iterator, item.iterator.getNext());

    if (inst == ir::spv::OpLabel) {
      if (result.getEntryNode() == nullptr) {
        item.node = result.getOrCreateNode(inst.staticCast<ir::Value>());
        result.addPreorderNode(item.node);
        result.setEntryNode(item.node);
        visited.insert(item.node);
      } else {
        item.iterator = nullptr;
        force = false;
      }

      continue;
    }

    if (isBranch(inst)) {
      item.node->setTerminator(inst);
      item.iterator = nullptr;

      walkSuccessors(inst, [&](ir::Value label) { addSuccessor(item, label); });
      continue;
    }

    if (isTerminator(inst)) {
      item.node->setTerminator(inst);
      item.iterator = nullptr;
      continue;
    }
  }

  for (auto exitLabel : exitLabels) {
    if (exitLabel == nullptr) {
      continue;
    }

    // collect internal branches from exitLabel. Need to collect all blocks
    // first to be able discard edges to not exists in this CFG target blocks
    if (auto from = result.getNode(exitLabel)) {
      walkSuccessors(from->getTerminator(), [&](ir::Value toLabel) {
        if (auto to = result.getNode(toLabel)) {
          from->addEdge(to);
        }
      });
    }
  }

  return result;
}

enum class VarSearchType { Root, Closest, Exact };

static ir::memssa::Var getVarFromVariableImpl(ir::Value variable,
                                              std::span<const ir::Operand> path,
                                              VarSearchType searchType,
                                              auto &&getVarFn) {
  auto result = getVarFn(variable);

  if (searchType == VarSearchType::Root) {
    return result;
  }

  if (result == nullptr) {
    return nullptr;
  }

  for (auto &op : path) {
    auto indexOp = op.getAsValue();
    if (indexOp != ir::spv::OpConstant) {
      if (searchType == VarSearchType::Exact) {
        return {};
      }

      break;
    }

    auto pIndex = indexOp.getOperand(1).getAsInt32();

    if (pIndex == nullptr) {
      if (searchType == VarSearchType::Exact) {
        return {};
      }

      break;
    }

    auto index = *pIndex;

    if (index >= result.getOperandCount()) {
      if (searchType == VarSearchType::Exact) {
        return {};
      }

      break;
    }

    result = result.getOperand(index)
                 .getAsValue()
                 .template staticCast<ir::memssa::Var>();
  }

  return result;
}

template <typename GetVarFnT>
static ir::memssa::Var getVarFromPointerImpl(ir::Value pointer,
                                             VarSearchType searchType,
                                             GetVarFnT &&getVarFn) {
  std::vector<std::span<const ir::Operand>> pathStack;

  while (pointer != ir::spv::OpVariable) {
    if (pointer == ir::spv::OpAccessChain ||
        pointer == ir::spv::OpInBoundsAccessChain) {
      pathStack.push_back(pointer.getOperands().subspan(2));
      pointer = pointer.getOperand(1).getAsValue();
    } else {
      ir::NameStorage ns;
      pointer.print(std::cerr, ns);
      rx::die("memssa: failed to unwrap pointer to variable");
    }
  }

  if (pathStack.empty()) {
    return getVarFromVariableImpl(pointer, {}, searchType,
                                  std::forward<GetVarFnT>(getVarFn));
  }

  if (pathStack.size() == 1) {
    return getVarFromVariableImpl(pointer, pathStack.back(), searchType,
                                  std::forward<GetVarFnT>(getVarFn));
  }

  std::vector<ir::Operand> mergedPath;

  while (!pathStack.empty()) {
    auto span = pathStack.back();
    pathStack.pop_back();
    mergedPath.reserve(mergedPath.size() + span.size());

    for (auto &elem : span) {
      mergedPath.push_back(elem);
    }
  }

  return getVarFromVariableImpl(pointer, mergedPath, searchType,
                                std::forward<GetVarFnT>(getVarFn));
}

ir::memssa::Var MemorySSA::getVar(ir::Value variable,
                                  std::span<const ir::Operand> path) {
  return getVarFromVariableImpl(
      variable, path, VarSearchType::Exact,
      [this](ir::Value variable) { return getVarImpl(variable); });
}

ir::memssa::Var MemorySSA::getVar(ir::Value pointer) {
  return getVarFromPointerImpl(
      pointer, VarSearchType::Exact,
      [this](ir::Value variable) { return getVarImpl(variable); });
}

ir::memssa::Var MemorySSA::getVarImpl(ir::Value variable) {
  rx::dieIf(variable != ir::spv::OpVariable,
            "memssa: getVar: unexpected variable type");

  if (auto it = variableToVar.find(variable); it != variableToVar.end()) {
    return it->second;
  }

  return nullptr;
}

class MemorySSABuilder {
public:
  using IRBuilder = ir::Builder<ir::builtin::Builder, ir::memssa::Builder>;

private:
  MemorySSA memSSA;

  ir::memssa::Var getOrCreateVarImpl(ir::Value variable) {
    rx::dieIf(variable != ir::spv::OpVariable,
              "memssa-builder: getVar: unexpected variable type");

    auto &result = memSSA.variableToVar[variable];

    if (result == nullptr) {
      result = createVarWithLayout(variable);
    }

    return result;
  }

  ir::memssa::Var createVarWithLayout(ir::Value variable, ir::Value type) {
    auto builder = IRBuilder::createPrepend(memSSA.context, memSSA.region);

    if (type == ir::spv::OpTypeVector) {
      auto elementType = type.getOperand(0).getAsValue();
      auto count = *type.getOperand(1).getAsInt32();

      auto result = builder.createVar(variable);

      for (int i = 0; i < count; ++i) {
        result.addOperand(createVarWithLayout(variable, elementType));
      }

      return result;
    }

    if (type == ir::spv::OpTypeArray) {
      auto elementType = type.getOperand(0).getAsValue();
      auto count = *type.getOperand(1).getAsValue().getOperand(1).getAsInt32();

      auto result = builder.createVar(variable);

      for (int i = 0; i < count; ++i) {
        result.addOperand(createVarWithLayout(variable, elementType));
      }

      return result;
    }

    if (type == ir::spv::OpTypeStruct) {
      auto result = builder.createVar(variable);
      for (std::size_t i = 0; auto &op : type.getOperands()) {
        result.addOperand(createVarWithLayout(variable, op.getAsValue()));
      }
      return result;
    }

    return builder.createVar(variable);
  }

  ir::memssa::Var createVarWithLayout(ir::Value variable) {
    auto type = variable.getOperand(0).getAsValue().getOperand(1).getAsValue();
    return createVarWithLayout(variable, type);
  }

public:
  ir::Context &getContext() { return memSSA.context; }
  ir::RegionLike getRegion() { return memSSA.region; }

  ir::memssa::Var getOrCreateVar(ir::Value variable,
                                 std::span<const ir::Operand> path,
                                 VarSearchType searchType) {
    return getVarFromVariableImpl(
        variable, path, searchType,
        [this](ir::Value variable) { return getOrCreateVarImpl(variable); });
  }

  ir::memssa::Var getOrCreateVar(ir::Value pointer, VarSearchType searchType) {
    return getVarFromPointerImpl(
        pointer, searchType,
        [this](ir::Value variable) { return getOrCreateVarImpl(variable); });
  }

  ir::memssa::Def getOrCreatePointerDef(ir::memssa::Scope scope,
                                        ir::Value pointer,
                                        VarSearchType searchType) {
    rx::dieIf(searchType == VarSearchType::Root,
              "memssa-builder: getPointerDef: unexpected searchType");

    auto var = getOrCreateVar(pointer, searchType);

    if (auto varDef = scope.findVarDef(var)) {
      return varDef;
    }

    return IRBuilder::createPrepend(memSSA.context, scope).createPhi(var);
  }

  std::pair<ir::memssa::Def, bool> getOrCreateVarDef(ir::memssa::Scope scope,
                                                     ir::memssa::Var var) {
    if (auto varDef = scope.findVarDef(var)) {
      return {varDef, false};
    }

    return {IRBuilder::createPrepend(memSSA.context, scope).createPhi(var),
            true};
  }

  void createVarAccess(ir::Instruction inst, ir::memssa::Scope scope,
                       ir::memssa::Var var, Access access) {
    if ((access & Access::Read) == Access::Read) {
      auto [def, inserted] = getOrCreateVarDef(scope, var);
      IRBuilder::createAppend(getContext(), scope).createUse(inst, def);
    }

    if ((access & Access::Write) == Access::Write) {
      IRBuilder::createAppend(getContext(), scope).createDef(inst, var);
    }
  }

  void createPointerAccess(ir::Instruction inst, ir::memssa::Scope scope,
                           ir::Value pointer, VarSearchType searchType,
                           Access access) {
    if (access == Access::None) {
      return;
    }

    createVarAccess(inst, scope, getOrCreateVar(pointer, searchType), access);
  }

  MemorySSA build(CFG &cfg, auto &&handleInst);
};

MemorySSA MemorySSABuilder::build(CFG &cfg, auto &&handleInst) {
  memSSA.region =
      IRBuilder(memSSA.context).createRegion(cfg.getEntryLabel().getLocation());

  std::map<ir::Value, ir::memssa::Scope> labelToScope;
  ir::memssa::Scope entryScope;
  std::vector<ir::memssa::Barrier> barriers;

  for (auto node : cfg.getPreorderNodes()) {
    auto scope = IRBuilder::createAppend(memSSA.context, memSSA.region)
                     .createScope(node->getLabel());

    labelToScope[node->getLabel()] = scope;

    if (entryScope == nullptr) {
      entryScope = scope;
    }

    for (auto inst : node->rangeWithoutLabelAndTerminator()) {
      if (inst.getKind() == ir::Kind::Spv) {
        if (inst.getOp() == ir::spv::OpStore) {
          createPointerAccess(inst, scope, inst.getOperand(0).getAsValue(),
                              VarSearchType::Closest, Access::Write);
          continue;
        }

        if (inst.getOp() == ir::spv::OpLoad) {
          createPointerAccess(inst, scope, inst.getOperand(1).getAsValue(),
                              VarSearchType::Closest, Access::Read);
          continue;
        }
      }

      if (handleInst(*this, scope, inst)) {
        continue;
      }

      // if (isWithoutSideEffects(inst.getInstId())) {
      //   continue;
      // }

      if (inst == ir::amdgpu::BRANCH || (inst.getKind() != ir::Kind::Spv &&
                                         inst.getKind() != ir::Kind::AmdGpu)) {
        auto barrier =
            IRBuilder::createAppend(memSSA.context, scope).createBarrier(inst);
        barriers.push_back(barrier);
      }
    }
  }

  std::vector<ir::memssa::Scope> workList;
  for (auto [label, scope] : labelToScope) {
    auto successors = cfg.getSuccessors(label);

    auto builder = IRBuilder::createAppend(memSSA.context, scope);
    if (successors.empty()) {
      builder.createExit(label.getLocation());
    } else {
      auto jump = builder.createJump(label.getLocation());

      for (auto succLabel : successors) {
        auto succ = labelToScope.at(succLabel->getLabel());
        jump.addOperand(succ);

        for (auto child : succ.children()) {
          if (child != ir::memssa::OpPhi) {
            break;
          }

          auto phi = child.staticCast<ir::memssa::Phi>();
          auto [varDef, inserted] = getOrCreateVarDef(scope, phi.getVar());
          phi.addValue(scope, varDef);

          if (inserted) {
            workList.push_back(scope);
          }
        }
      }
    }
  }

  while (!workList.empty()) {
    auto scope = workList.back();
    workList.pop_back();

    for (auto pred : scope.getPredecessors()) {
      bool predChanges = false;

      for (auto child : scope.children()) {
        if (child != ir::memssa::OpPhi) {
          break;
        }

        auto phi = child.staticCast<ir::memssa::Phi>();
        auto [varDef, inserted] = getOrCreateVarDef(pred, phi.getVar());

        phi.setValue(pred, varDef);

        if (inserted) {
          predChanges = true;
        }
      }

      if (predChanges) {
        workList.push_back(pred);
      }
    }
  }

  for (auto scope : ir::range<ir::memssa::Scope>(entryScope)) {
    workList.push_back(scope);
  }

  while (!workList.empty()) {
    auto scope = workList.back();
    workList.pop_back();
    bool changes = false;

    for (auto child : scope.children()) {
      if (child != ir::memssa::OpPhi) {
        break;
      }

      auto phi = child.staticCast<ir::memssa::Phi>();
      auto uniqDef = phi.getUniqDef();

      if (uniqDef == nullptr) {
        continue;
      }

      phi.replaceAllUsesWith(uniqDef);
      phi.remove();
      changes = true;
    }

    auto succ = scope.getSingleSuccessor();
    if (succ && succ.getSinglePredecessor() == scope) {
      for (auto child : succ.children()) {
        if (child != ir::memssa::OpPhi) {
          break;
        }

        auto phi = child.staticCast<ir::memssa::Phi>();
        phi.replaceAllUsesWith(phi.getDef(scope));
        phi.remove();
      }

      // remove terminator from imm predecessor
      scope.getLast().remove();

      // merge regions and update phis
      scope.appendRegion(succ);
      succ.replaceAllUsesWith(scope);
      succ.remove();
      changes = true;
    }

    if (changes) {
      for (auto &succ : scope.getSuccessors()) {
        workList.push_back(succ);
      }
    }
  }

  // auto domTree = graph::DomTreeBuilder<ir::memssa::Scope>{}.build(
  //     entryScope, [&](ir::memssa::Scope scope, const auto &cb) {
  //       for (auto succ : scope.getSuccessors()) {
  //         cb(succ);
  //       }
  //     });

  for (auto scope : ir::range<ir::memssa::Scope>(entryScope)) {
    for (auto use : scope.children<ir::memssa::Use>()) {
      auto &user = memSSA.userDefs[use.getLinkedInst()];

      for (auto &op : use.getOperands()) {
        auto def = op.getAsValue().staticCast<ir::memssa::Def>();

        if (def == ir::memssa::OpPhi) {
          user[def.getRootVar()] = def;
          continue;
        }

        if (def == ir::memssa::OpBarrier) {
          user[nullptr] = def;
          continue;
        }

        for (auto &var : def.getOperands()) {
          user[var.getAsValue().staticCast<ir::memssa::Var>()] = def;
        }
      }
    }
  }

  return std::move(memSSA);
}

MemorySSA
shader::buildMemorySSA(CFG &cfg, const SemanticInfo &instructionSemantic,
                       std::function<ir::Value(int)> getRegisterVarCb) {
  return MemorySSABuilder{}.build(cfg, [&](MemorySSABuilder &builder,
                                           ir::memssa::Scope scope,
                                           ir::Instruction inst) {
    using IRBuilder = MemorySSABuilder::IRBuilder;
    auto semantic = instructionSemantic.findSemantic(inst.getInstId());
    if (semantic == nullptr) {
      return false;
    }

    for (auto [regId, access] : semantic->registerAccesses) {
      if (access == Access::None) {
        continue;
      }

      auto reg = getRegisterVarCb(regId);
      if (!reg) {
        continue;
      }

      builder.createPointerAccess(inst, scope, reg, VarSearchType::Root,
                                  access);
    }

    auto args = inst.getOperands();
    args = args.subspan(args.size() - semantic->parameters.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
      auto arg = args[i].getAsValue();
      auto param = semantic->parameters[i];

      if (param.access == Access::None) {
        continue;
      }

      builder.createPointerAccess(inst, scope, arg, VarSearchType::Root,
                                  param.access);
    }

    return true;
  });
}

MemorySSA shader::buildMemorySSA(CFG &cfg, ModuleInfo *moduleInfo) {
  return MemorySSABuilder{}.build(cfg, [&](MemorySSABuilder &builder,
                                           ir::memssa::Scope scope,
                                           ir::Instruction inst) {
    using IRBuilder = MemorySSABuilder::IRBuilder;

    if (moduleInfo == nullptr) {
      return false;
    }

    if (inst != ir::spv::OpFunctionCall) {
      return false;
    }

    auto callee = inst.getOperand(1).getAsValue();
    auto it = moduleInfo->functions.find(callee);
    auto fnInfo = it == moduleInfo->functions.end() ? nullptr : &it->second;

    if (fnInfo == nullptr) {
      return false;
    }

    for (auto [variable, access] : fnInfo->variables) {
      builder.createPointerAccess(inst, scope, variable, VarSearchType::Root,
                                  access);
    }

    auto args = inst.getOperands();
    args = args.subspan(args.size() - fnInfo->parameters.size());

    for (std::size_t i = 0; i < args.size(); ++i) {
      auto arg = args[i].getAsValue();
      auto param = fnInfo->parameters[i];

      if (param.access == Access::None) {
        continue;
      }

      builder.createPointerAccess(inst, scope, arg, VarSearchType::Root,
                                  param.access);
    }

    return true;
  });
}

void MemorySSA::print(std::ostream &os, ir::Region irRegion,
                      ir::NameStorage &ns) {
  std::map<ir::Instruction, std::vector<ir::memssa::Def>> instDefs;
  std::map<ir::Instruction, std::vector<ir::memssa::Phi>> phis;

  for (auto scope : region.children<ir::memssa::Scope>()) {
    for (auto def : scope.children<ir::memssa::Def>()) {
      if (auto linked = def.getLinkedInst()) {
        instDefs[linked].push_back(def);
      } else if (auto phi = def.cast<ir::memssa::Phi>()) {
        phis[phi.getParent().staticCast<ir::memssa::Scope>().getLinkedInst()]
            .push_back(phi);
      }
    }
  }

  for (auto child : irRegion.children()) {
    child.print(os, ns);

    if (auto it = instDefs.find(child); it != instDefs.end()) {
      for (auto def : it->second) {
        os << " def(@" << ns.getNameOf(def) << ")";
      }
    }

    if (auto it = phis.find(child); it != phis.end()) {
      for (auto phi : it->second) {
        os << " phi(@" << ns.getNameOf(phi);

        for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
          os << ", use(@" << ns.getNameOf(phi.getOperand(i).getAsValue())
             << ")";
        }

        os << ")";
      }
    }

    if (auto it = userDefs.find(child); it != userDefs.end()) {
      for (auto [var, def] : it->second) {
        os << " use(@" << ns.getNameOf(def) << ", ";
        if (var == nullptr) {
          os << "barrier ";
        }

        if (auto link = def.getLinkedInst()) {
          link.print(os, ns);
        } else {
          os << "phi";
        }

        os << ")";
      }
    }

    os << '\n';
  }
}
void MemorySSA::print(std::ostream &os, ir::NameStorage &ns) {
  region.print(os, ns);
}

void MemorySSA::dump() {
  ir::NameStorage ns;
  print(std::cerr, ns);
}

bool shader::dominates(ir::Instruction a, ir::Instruction b, bool isPostDom,
                       graph::DomTree<ir::Value> &domTree) {
  if (a == b) {
    return true;
  }

  if (isGlobal(a)) {
    return true;
  }

  if (isGlobal(b)) {
    return false;
  }

  auto origA = a;

  while (a != ir::spv::OpLabel) {
    if (a == b) {
      return isPostDom;
    }

    a = a.getPrev();
  }

  while (b != ir::spv::OpLabel) {
    if (b == origA) {
      return !isPostDom;
    }

    b = b.getPrev();
  }

  return domTree.dominates(a.staticCast<ir::Value>(),
                           b.staticCast<ir::Value>());
}

ir::Value
shader::findNearestCommonDominator(ir::Instruction a, ir::Instruction b,
                                   graph::DomTree<ir::Value> &domTree) {
  if (a == nullptr || b == nullptr || isGlobal(a) || isGlobal(b)) {
    std::abort();
  }

  while (a != ir::spv::OpLabel) {
    a = a.getPrev();
  }

  while (b != ir::spv::OpLabel) {
    b = b.getPrev();
  }

  return domTree.findNearestCommonDominator(a.staticCast<ir::Value>(),
                                            b.staticCast<ir::Value>());
}

BackEdgeStorage::BackEdgeStorage(CFG &cfg) {
  struct Entry {
    ir::Value bb;
    CFG::Node::Iterator successorsIt;
    CFG::Node::Iterator successorsEnd;
  };

  std::vector<Entry> workList;
  std::unordered_set<ir::Value> inWorkList;
  // std::unordered_set<ir::Value> viewed;
  workList.reserve(cfg.getPostorderNodes().size());
  inWorkList.reserve(cfg.getPostorderNodes().size());

  auto addToWorkList = [&](CFG::Node *node) {
    if (inWorkList.insert(node->getLabel()).second) {
      workList.push_back({
          .bb = node->getLabel(),
          .successorsIt = node->getSuccessors().begin(),
          .successorsEnd = node->getSuccessors().end(),
      });
      return true;
    }

    return false;
  };

  addToWorkList(cfg.getEntryNode());

  while (!workList.empty()) {
    auto &entry = workList.back();

    if (entry.successorsIt == entry.successorsEnd) {
      // viewed.insert(inWorkList.extract(entry.bb));
      workList.pop_back();
      continue;
    }

    auto label = entry.bb;
    auto it = entry.successorsIt;
    ++entry.successorsIt;

    auto successor = *it;

    // if (viewed.contains(successor->getLabel())) {
    //   continue;
    // }

    if (!addToWorkList(successor)) {
      backEdges[successor->getLabel()].insert(label);
    }
  }
}