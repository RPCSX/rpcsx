#include "transform.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <rx/die.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace shader;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

static bool isConstruct(ir::Instruction block) {
  return block == ir::builtin::LOOP_CONSTRUCT ||
         block == ir::builtin::SELECTION_CONSTRUCT ||
         block == ir::builtin::CONTINUE_CONSTRUCT;
}

static ir::Block getConstructOf(ir::Instruction inst) {
  auto block = inst.cast<ir::Block>();
  if (block && isConstruct(block)) {
    block = block.getParent().cast<ir::Block>();
  }

  while (block) {
    if (isConstruct(block)) {
      return block;
    }

    block = block.getParent().cast<ir::Block>();
  }

  return {};
}

static ir::Instruction skipPhis(ir::Instruction inst) {
  while (inst && inst == ir::spv::OpPhi) {
    inst = inst.getNext();
  }

  return inst;
}

static ir::Block getConstructMergeBlock(ir::Block block) {
  if (auto construct = block.cast<ir::Construct>()) {
    return construct.getMerge();
  }

  return {};
}

/**
 * Tarjan's algorithm for finding strongly connected components (SCCs).
 * This finds all cycles in the CFG
 */
static std::vector<std::unordered_set<ir::Block>>
findSCCs(ir::Range<ir::Block> nodes) {
  std::unordered_map<ir::Block, std::size_t> indices;
  std::unordered_map<ir::Block, std::size_t> lowlinks;
  std::unordered_set<ir::Block> onStack;
  std::vector<ir::Block> stack;
  std::vector<std::unordered_set<ir::Block>> sccs;
  std::size_t index = 0;

  auto rootParent = (*nodes.begin()).getParent();

  std::function<void(ir::Block)> strongConnect = [&](ir::Block node) {
    indices[node] = index;
    lowlinks[node] = index;
    index++;
    stack.push_back(node);
    onStack.insert(node);

    // Consider successors of node
    for (auto successor : getSuccessors(node)) {
      if (successor.getParent() != rootParent) {
        continue;
      }

      if (!indices.contains(successor)) {
        // Successor has not yet been visited; recurse on it
        strongConnect(successor);
        lowlinks[node] = std::min(lowlinks[node], lowlinks[successor]);
      } else if (onStack.contains(successor)) {
        // Successor is in stack and hence in the current SCC
        lowlinks[node] = std::min(lowlinks[node], indices[successor]);
      }
    }

    // If node is a root node, pop the stack and create an SCC
    if (lowlinks[node] == indices[node]) {
      std::unordered_set<ir::Block> scc;
      scc.reserve(stack.size());
      ir::Block w;
      do {
        w = stack.back();
        stack.pop_back();
        onStack.erase(w);
        scc.insert(w);
      } while (w != node);

      // keep cycles only
      if (!scc.empty()) {
        auto isLoop = scc.size() > 1;

        if (!isLoop) {
          // single node can contain branch to self
          isLoop = hasSuccessor(w, w);
        }

        if (isLoop) {
          sccs.push_back(std::move(scc));
        }
      }
    }
  };

  for (auto node : nodes) {
    if (node.getParent() != rootParent) {
      continue;
    }

    if (!indices.contains(node)) {
      strongConnect(node);
    }
  }
  return sccs;
}

static void replaceTerminatorTarget(ir::Instruction terminator,
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

static bool replaceTerminatorTarget(ir::Instruction terminator,
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

inline Edge createEdge(ir::Block from, ir::Block to) {
  for (int index = 0; auto &op : from.getLast().getOperands()) {

    if (op.getAsValue() == to) {
      return {from, index};
    }
    index++;
  }

  rx::die("attempt to create invalid edge");
}

struct CycleEdges {
  std::vector<Edge> entryEdges;
  std::vector<Edge> backEdges;
  std::vector<Edge> exitEdges;
};

static CycleEdges
calculateCycleEdges(const std::unordered_set<ir::Block> &cycles) {
  CycleEdges result;
  std::unordered_set<ir::Block> entryBlocks;

  for (auto block : cycles) {
    for (auto [pred, operandIndex] : getAllPredecessors(block)) {
      if (cycles.contains(pred)) {
        continue;
      }

      result.entryEdges.emplace_back(pred, operandIndex);
    }

    for (auto [succ, operandIndex] : getAllSuccessors(block)) {
      if (cycles.contains(succ))
        continue;

      entryBlocks.insert(succ);
      result.exitEdges.emplace_back(block, operandIndex);
    }
  }

  for (auto block : cycles) {
    for (auto [succ, operandIndex] : getAllSuccessors(block)) {
      if (entryBlocks.contains(succ))
        continue;

      result.backEdges.emplace_back(block, operandIndex);
    }
  }

  return result;
}

static ir::Block createMergeBlock(spv::Context &context,
                                  ir::InsertionPoint insertPoint,
                                  const std::unordered_set<ir::Block> &preds,
                                  ir::Block to) {
  rx::dieIf(preds.empty(), "createMergeBlock: unexpected edges count");

  auto loc = to.getLocation();

  auto mergeBlock = Builder::create(context, insertPoint).createBlock(loc);
  Builder::createAppend(context, mergeBlock).createSpvBranch(loc, to);

  if (preds.size() == getPredecessorCount(to)) {
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      phi.erase();
      mergeBlock.prependChild(phi);
    }
  } else if (preds.size() == 1) {
    auto pred = *preds.begin();
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
        if (phi.getOperand(i) == pred) {
          phi.replaceOperand(i, mergeBlock);
        }
      }
    }
  } else {
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      auto newPhi =
          Builder::createPrepend(context, mergeBlock)
              .createSpvPhi(phi.getLocation(), phi.getOperand(0).getAsValue());

      for (std::size_t i = 1; i < phi.getOperandCount();) {
        // auto value = phi.getOperand(i).getAsValue();
        auto label = phi.getOperand(i + 1).getAsValue().staticCast<ir::Block>();
        if (preds.contains(label)) {
          newPhi.addOperand(phi.eraseOperand(i));
          newPhi.addOperand(phi.eraseOperand(i));
        } else {
          i += 2;
        }
      }

      phi.addOperand(newPhi);
      phi.addOperand(mergeBlock);
    }
  }

  for (auto pred : preds) {
    replaceTerminatorTarget(getTerminator(pred), to, mergeBlock);
  }

  return mergeBlock;
}

static ir::Block createRouteBlock(spv::Context &context,
                                  ir::InsertionPoint insertPoint,
                                  const std::vector<Edge> &edges) {
  auto loc = context.getUnknownLocation();

  rx::dieIf(edges.empty(), "createRouteBlock: unexpected edges count");

  std::unordered_map<ir::Block, std::unordered_set<unsigned>> fromSucc;
  std::unordered_map<ir::Block, std::unordered_set<ir::Block>> toPreds;
  std::unordered_map<ir::Block, std::unordered_set<ir::Block>> toAllPreds;
  std::unordered_set<ir::Block> patchPredecessors;

  {
    std::unordered_set<ir::Block> routePredecessors;

    for (auto edge : edges) {
      if (!routePredecessors.insert(edge.from()).second) {
        patchPredecessors.insert(edge.from());
      }

      toPreds[edge.to()].emplace(edge.from());
      fromSucc[edge.from()].emplace(edge.operandIndex());
    }

    for (auto &[to, preds] : toPreds) {
      toAllPreds[to] = getPredecessors(to);
    }
  }

  if (toPreds.size() == 1) {
    auto &[to, preds] = *toPreds.begin();
    return createMergeBlock(context, insertPoint, preds, to);
  }

  auto route = Builder::create(context, insertPoint).createBlock(loc);
  ir::Value routePhi;

  if (toPreds.size() > 1) {
    routePhi =
        Builder::createPrepend(context, route)
            .createSpvPhi(loc, toPreds.size() == 2 ? context.getTypeBool()
                                                   : context.getTypeUInt32());
  }

  std::unordered_map<ir::Value, std::uint32_t> successorToId;

  if (toPreds.size() == 1) {
    // single successor, create unconditional branch
    Builder::createAppend(context, route)
        .createSpvBranch(loc, toPreds.begin()->first);
  } else if (toPreds.size() == 2) {
    // 2 successors, create conditional branch
    auto it = toPreds.begin();
    auto firstSuccessor = it->first;
    auto secondSuccessor = (++it)->first;

    Builder::createAppend(context, route)
        .createSpvBranchConditional(loc, routePhi, firstSuccessor,
                                    secondSuccessor);
  } else {
    // > 2 successors, create switch
    auto routeSwitch =
        Builder::createAppend(context, route)
            .createSpvSwitch(loc, routePhi, toPreds.begin()->first);

    successorToId.reserve(toPreds.size());

    for (std::uint32_t id = 0; auto &[succ, pred] : toPreds) {
      if (id) {
        routeSwitch.addOperand(id);
        routeSwitch.addOperand(succ);
      }

      successorToId[succ] = id++;
    }
  }

  auto getSuccessorId = [&](ir::Block successor) {
    if (toPreds.size() == 2) {
      return context.getBool(successor == toPreds.begin()->first);
    }

    return context.imm32(successorToId.at(successor));
  };

  for (auto patchBlock : patchPredecessors) {
    auto predSuccessors = getAllSuccessors(patchBlock);
    auto terminator = getTerminator(patchBlock);
    auto &routeSuccessors = fromSucc.at(patchBlock);

    int keepSuccessors = predSuccessors.size() - routeSuccessors.size();

    assert(keepSuccessors >= 0);
    assert(terminator == ir::spv::OpSwitch ||
           terminator == ir::spv::OpBranchConditional);

    auto cond = terminator.getOperand(0).getAsValue();
    auto condType = cond.getOperand(0).getAsValue();
    std::map<ir::Operand, ir::Block> condValueToSucc;
    ir::Block defaultSucc;

    if (keepSuccessors == 0) {
      // we are going to replace all successors of this block, create direct
      // jump to route block
      Builder::createInsertAfter(context, terminator)
          .createSpvBranch(terminator.getLocation(), route);

      if (terminator == ir::spv::OpBranchConditional) {
        condValueToSucc[context.getTrue()] =
            terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
        condValueToSucc[context.getFalse()] =
            terminator.getOperand(2).getAsValue().staticCast<ir::Block>();
      } else if (terminator == ir::spv::OpSwitch) {
        defaultSucc =
            terminator.getOperand(1).getAsValue().staticCast<ir::Block>();

        for (int i = 2, end = terminator.getOperandCount(); i < end; i += 2) {
          condValueToSucc[terminator.getOperand(i)] =
              terminator.getOperand(i + 1).getAsValue().staticCast<ir::Block>();
        }
      }
    } else if (terminator == ir::spv::OpSwitch) {
      if (routeSuccessors.contains(1)) {
        defaultSucc =
            terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
      }

      bool shouldReplaceDefault = defaultSucc != nullptr;

      for (int i = 2, id = 2, end = terminator.getOperandCount(); i < end;
           id += 2) {
        if (routeSuccessors.contains(id + 1)) {
          if (shouldReplaceDefault) {
            auto value = terminator.eraseOperand(i);
            auto successor = terminator.eraseOperand(i);

            condValueToSucc[value] =
                successor.getAsValue().staticCast<ir::Block>();

            continue;
          }

          condValueToSucc[terminator.getOperand(i)] =
              terminator.getOperand(i + 1).getAsValue().staticCast<ir::Block>();

          terminator.replaceOperand(i + 1, route);
        }

        i += 2;
      }

      if (shouldReplaceDefault) {
        terminator.replaceOperand(1, route);
      }
    } else {
      if (routeSuccessors.contains(1)) {
        condValueToSucc[context.getTrue()] =
            terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
        terminator.replaceOperand(1, route);
      } else {
        assert(routeSuccessors.contains(2));
        condValueToSucc[context.getFalse()] =
            terminator.getOperand(2).getAsValue().staticCast<ir::Block>();
        terminator.replaceOperand(2, route);
      }
    }

    if (routePhi) {
      auto boolType = context.getTypeBool();
      auto builder = Builder::createInsertBefore(context, terminator);

      ir::Value selector;

      if (defaultSucc) {
        selector = getSuccessorId(defaultSucc);
      }

      auto selectorType =
          toPreds.size() == 2 ? boolType : context.getTypeUInt32();
      for (auto &[value, to] : condValueToSucc) {
        if (!selector) {
          selector = getSuccessorId(to);
        } else {
          auto valueId = value.getAsValue();
          if (!valueId) {
            valueId = context.imm32(*value.getAsInt32());
          }

          ir::Value selectionCond;

          if (condType == boolType) {
            selectionCond = builder.createSpvLogicalEqual(
                terminator.getLocation(), boolType, cond, valueId);
          } else {
            selectionCond = builder.createSpvIEqual(terminator.getLocation(),
                                                    boolType, cond, valueId);
          }
          selector = builder.createSpvSelect(terminator.getLocation(),
                                             selectorType, selectionCond,
                                             getSuccessorId(to), selector);
        }
      }

      routePhi.addOperand(selector);
      routePhi.addOperand(patchBlock);
    }

    if (keepSuccessors == 0) {
      terminator.remove();
    }
  }

  for (auto &[to, preds] : toPreds) {
    if (toPreds.size() > 1) {
      auto successorId = getSuccessorId(to);

      for (auto from : preds) {
        // branches already resolved
        if (patchPredecessors.contains(from)) {
          continue;
        }

        routePhi.addOperand(successorId);
        routePhi.addOperand(from);
      }
    }

    for (auto from : preds) {
      if (patchPredecessors.contains(from)) {
        continue;
      }

      replaceTerminatorTarget(getTerminator(from), to, route);
    }

    if (toAllPreds.at(to).size() == preds.size()) {
      // all predecessors will be replaced, move phi nodes

      for (auto phi : ir::range(ir::Block(to).getFirst())) {
        if (phi != ir::spv::OpPhi) {
          break;
        }

        phi.erase();
        route.prependChild(phi);

        if (preds.size() != edges.size()) {
          // route block has additional edges. add dummy nodes to phi, this
          // block not reachable from new predecessors anyway

          auto undef = context.getUndef(phi.getOperand(0).getAsValue());

          for (auto edge : edges) {
            if (!preds.contains(edge.from())) {
              phi.addOperand(undef);
              phi.addOperand(edge.from());
            }
          }
        }
      }

      continue;
    }

    if (preds.size() == 1) {
      auto pred = *preds.begin();
      for (auto phi : ir::range(ir::Block(to).getFirst())) {
        if (phi != ir::spv::OpPhi) {
          break;
        }

        for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
          auto label = phi.getOperand(i).getAsValue();

          if (label == pred) {
            phi.replaceOperand(i, route);
          }
        }
      }

      continue;
    }

    // partial predecessors replacement, update PHIs

    for (auto phi : ir::range(ir::Block(to).getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      auto newPhi =
          Builder::createPrepend(context, route)
              .createSpvPhi(phi.getLocation(), phi.getOperand(0).getAsValue());

      for (std::size_t i = 1; i < phi.getOperandCount();) {
        // auto value = phi.getOperand(i).getAsValue();
        auto label = phi.getOperand(i + 1).getAsValue().cast<ir::Block>();

        if (preds.contains(label)) {
          newPhi.addOperand(phi.eraseOperand(i));
          newPhi.addOperand(phi.eraseOperand(i));
        } else {
          i += 2;
        }
      }

      phi.addOperand(newPhi);
      phi.addOperand(route);

      if (preds.size() != edges.size()) {
        // merge block has additional edges. add dummy nodes to phi, this
        // block not reachable from new blocks

        auto dummyValue = phi.getOperand(1).getAsValue();

        for (auto edge : edges) {
          if (!preds.contains(edge.from())) {
            phi.addOperand(dummyValue);
            phi.addOperand(edge.from());
          }
        }
      }
    }
  }

  return route;
}

static ir::Value transformToCanonicalRegion(spv::Context &context,
                                            ir::RegionLike region) {
  auto cfg = buildCFG(region.getFirst());
  std::vector<CFG::Node *> exitNodes;
  for (auto node : cfg.getPreorderNodes()) {
    if (!node->hasSuccessors()) {
      exitNodes.push_back(node);
    }
  }

  if (cfg.getEntryNode()->hasPredecessors()) {
    auto builder = Builder::createPrepend(context, region);
    auto prevEntry = cfg.getEntryLabel();
    auto newEntry = builder.createSpvLabel(prevEntry.getLocation());
    builder.createSpvBranch(prevEntry.getLocation(), prevEntry);

    for (auto it = prevEntry.getNext(); it && it == ir::spv::OpVariable;) {
      auto moveInst = it;
      it = it.getNext();

      moveInst.erase();
      region.insertAfter(newEntry, moveInst);
    }
  }

  if (exitNodes.empty()) {
    region.print(std::cerr, context.ns);
    rx::die("scfg: cfg without termination block");
  }

  if (exitNodes.size() == 1) {
    return exitNodes.back()->getLabel();
  }

  ir::Value returnType;
  ir::Instruction returnInst;

  for (auto exitNode : exitNodes) {
    auto terminator = exitNode->getTerminator();

    if (terminator && terminator == ir::spv::OpReturnValue) {
      auto terminatorReturnValue = terminator.getOperand(0).getAsValue();
      auto terminatorReturnType =
          terminatorReturnValue.getOperand(0).getAsValue();
      if (returnType && terminatorReturnType == returnType) {
        rx::die("scfg: unexpected terminator return type");
      } else {
        returnType = terminatorReturnType;
      }
    }

    if (terminator) {
      if (returnInst && returnInst.getInstId() != terminator.getInstId()) {
        returnInst.print(std::cerr, context.ns);
        std::cerr << '\n';
        terminator.print(std::cerr, context.ns);
        std::cerr << '\n';
        rx::die("scfg: unexpected return instruction kind change");
      } else {
        returnInst = terminator;
      }
    }
  }

  if (returnType) {
    auto variablePointerType =
        context.getTypePointer(ir::spv::StorageClass::Function, returnType);

    auto returnValueVariable =
        Builder::createInsertAfter(context, region.getFirst())
            .createSpvVariable(context.getUnknownLocation(),
                               variablePointerType,
                               ir::spv::StorageClass::Function);

    auto newExitBlock = [&] {
      auto loc = context.getUnknownLocation();
      auto builder = Builder::createAppend(context, region);
      auto newExitBlock = builder.createSpvLabel(loc);

      auto mergedReturnValue =
          builder.createSpvLoad(loc, returnType, returnValueVariable);
      builder.createSpvReturnValue(loc, mergedReturnValue);
      return newExitBlock;
    }();

    for (auto exitNode : exitNodes) {
      auto terminator = exitNode->getTerminator();

      if (terminator) {
        auto newTerminator = Builder::createInsertAfter(context, terminator);

        newTerminator.createSpvStore(terminator.getLocation(),
                                     returnValueVariable,
                                     terminator.getOperand(0).getAsValue());
        newTerminator.createSpvBranch(terminator.getLocation(), newExitBlock);
        terminator.erase();
      }
    }

    return newExitBlock;
  }

  if (!returnInst) {
    rx::die("scfg: unexpected cfg terminator");
  }

  auto newExitBlock = Builder::createAppend(context, region)
                          .createSpvLabel(context.getUnknownLocation());

  for (auto exitNode : exitNodes) {
    auto terminator = exitNode->getTerminator();

    if (terminator) {
      auto newTerminator = Builder::createInsertAfter(context, terminator);
      newTerminator.createSpvBranch(terminator.getLocation(), newExitBlock);
      terminator.erase();
    }
  }

  region.insertAfter(newExitBlock, returnInst);
  return newExitBlock;
}

static void transformToCf(spv::Context &context, ir::RegionLike region) {
  ir::Block currentBlock;

  for (auto inst : region.children()) {
    if (inst == ir::builtin::BLOCK) {
      continue;
    }

    if (inst == ir::spv::OpLabel) {
      currentBlock = Builder::createInsertBefore(context, inst)
                         .createBlock(inst.getLocation());

      if (auto name = context.ns.tryGetNameOf(inst); !name.empty()) {
        context.ns.setNameOf(currentBlock, std::string(name));
      }

      inst.staticCast<ir::Value>().replaceAllUsesWith(currentBlock);
      inst.remove();
      continue;
    }

    if (!currentBlock) {
      inst.print(std::cerr, context.ns);
      std::cerr << "\n";
      region.print(std::cerr, context.ns);
      std::cerr << "\n";
      rx::die("cfg: node without label");
    }

    inst.erase();
    currentBlock.addChild(inst);

    if (isTerminator(inst)) {
      currentBlock = nullptr;
    }
  }
}

static void transformToFlat(spv::Context &context, ir::RegionLike region) {
  std::vector<ir::Instruction> workList;

  workList.push_back(region.getFirst());

  auto insertPoint = Builder::createPrepend(context, region);

  while (!workList.empty()) {
    auto inst = workList.back();

    workList.pop_back();

    if (inst == nullptr) {
      continue;
    }

    auto unwrapBlock = [&](ir::Block block) {
      if (auto construct = block.cast<ir::LoopConstruct>()) {
        auto merge = construct.getMerge();
        auto cont = construct.getContinue().getHeader();
        auto body = construct.getHeader();

        auto blockLabel = insertPoint.createSpvLabel(block.getLocation());
        construct.replaceAllUsesWith(blockLabel);

        if (auto name = context.ns.tryGetNameOf(block); !name.empty()) {
          context.ns.setNameOf(blockLabel, std::string(name));
        }

        for (auto phi : ir::range(construct.getFirst())) {
          if (phi != ir::spv::OpPhi) {
            break;
          }

          insertPoint.eraseAndInsert(phi);
        }

        insertPoint.createSpvLoopMerge(construct.getLocation(), merge, cont,
                                       ir::spv::LoopControl::None());
        insertPoint.createSpvBranch(construct.getLocation(), body);

        workList.emplace_back(cont);
        workList.emplace_back(construct.getFirst());
        return;
      }

      if (auto construct = block.cast<ir::SelectionConstruct>()) {
        auto constructBody = construct.getHeader();

        auto header = ir::InsertionPoint::createPrepend(constructBody);
        auto merge = construct.getMerge();

        for (auto phi : ir::range(construct.getFirst())) {
          if (phi != ir::spv::OpPhi) {
            break;
          }

          for (std::size_t i = 1; i < phi.getOperandCount();) {
            if (phi.getOperand(i + 1) == construct) {
              phi.eraseOperand(i);
              phi.eraseOperand(i);
            } else {
              i += 2;
            }
          }

          header.eraseAndInsert(phi);
        }

        Builder::createInsertBefore(context, constructBody.getLast())
            .createSpvSelectionMerge(construct.getLocation(), merge,
                                     ir::spv::SelectionControl::None);

        construct.replaceAllUsesWith(constructBody);
        workList.emplace_back(constructBody);
        return;
      }

      auto blockLabel = insertPoint.createSpvLabel(block.getLocation());

      block.replaceAllUsesWith(blockLabel);

      workList.emplace_back(block.getFirst());

      if (auto name = context.ns.tryGetNameOf(block); !name.empty()) {
        context.ns.setNameOf(blockLabel, std::string(name));
      }
    };

    if (auto next = inst.getNext()) {
      workList.push_back(next);
    }

    if (auto block = inst.cast<ir::Block>()) {
      std::cout << "processing " << context.ns.getNameOf(block) << "\n";
      unwrapBlock(block);
      block.erase();
      continue;
    }

    insertPoint.eraseAndInsert(inst);
  }
}

bool isParentConstruct(ir::RegionLike parent, ir::RegionLike construct) {
  while (parent != construct && construct) {
    construct = construct.getParent();
  }

  return parent == construct;
}

static ir::LoopConstruct
createLoopConstruct(spv::Context &context, ir::RegionLike parentConstruct,
                    ir::Block header, ir::Block latch, ir::Block cont,
                    ir::Block merge,
                    const std::unordered_set<shader::ir::Block> &scc) {
  auto continueConstruct =
      Builder::createInsertAfter(context, header)
          .createContinueConstruct(header.getLocation(), cont, header);

  auto loopConstruct = Builder::createInsertBefore(context, header)
                           .createLoopConstruct(header.getLocation(), header,
                                                merge, continueConstruct);

  continueConstruct.erase();

  header.erase();
  loopConstruct.addChild(header);

  std::vector<ir::Block> workList;
  workList.emplace_back(header);

  while (!workList.empty()) {
    ir::Block block = workList.back();
    workList.pop_back();

    block.erase();
    loopConstruct.addChild(block);

    std::unordered_set<ir::Block> successors;
    if (isConstruct(block)) {
      successors = {getConstructMergeBlock(block)};
    } else {
      successors = getSuccessors(block);
    }

    for (auto succ : successors) {
      if (succ == merge || succ.getParent() != parentConstruct ||
          !scc.contains(succ)) {
        continue;
      }

      workList.push_back(succ);
    }
  }

  latch.erase();
  loopConstruct.addChild(latch);

  cont.erase();
  continueConstruct.addChild(cont);

  merge.erase();
  loopConstruct.getParent().insertAfter(loopConstruct, merge);

  return loopConstruct;
}

static ir::SelectionConstruct
createSelectionConstruct(spv::Context &context, ir::RegionLike parentConstruct,
                         const std::unordered_set<ir::Block> &components,
                         ir::Block header, ir::Block merge) {
  auto selectionConstruct =
      Builder::createInsertBefore(context, header)
          .createSelectionConstruct(header.getLocation(), header, merge);

  std::vector<ir::Block> workList;
  workList.emplace_back(header);

  while (!workList.empty()) {
    ir::Block block = workList.back();
    workList.pop_back();

    block.erase();
    selectionConstruct.addChild(block);

    std::unordered_set<ir::Block> successors;
    if (auto construct = block.cast<ir::Construct>()) {
      successors = {construct.getMerge()};
    } else {
      successors = getSuccessors(block);
    }

    for (auto succ : successors) {
      if (succ == merge || succ.getParent() != parentConstruct ||
          !components.contains(succ)) {
        continue;
      }

      workList.push_back(succ);
    }
  }

  merge.erase();
  selectionConstruct.getParent().insertAfter(selectionConstruct, merge);

  return selectionConstruct;
}

static void wrapLoopConstructs(spv::Context &context, ir::RegionLike root) {
  auto region = root.children<ir::Block>();
  auto sccs = findSCCs(region);

  for (auto scc : sccs) {
    auto edges = calculateCycleEdges(scc);

    ir::Block bodyLabel;
    ir::Block continueLabel;
    ir::Block mergeLabel;
    ir::Block latchLabel;

    if (!edges.entryEdges.empty()) {
      if (edges.entryEdges.size() == 1 && edges.backEdges.size() == 1 &&
          edges.entryEdges[0].to() == edges.backEdges[0].to()) {
        bodyLabel = edges.entryEdges[0].to();
        continueLabel = edges.backEdges[0].from();
      }

      if (!bodyLabel) {
        std::vector<Edge> entryEdges = edges.entryEdges;
        // back edges should jump to entry block
        entryEdges.insert(entryEdges.end(), edges.backEdges.begin(),
                          edges.backEdges.end());

        // for loop no need to split blocks, we can just rotate loop
        bodyLabel = createRouteBlock(
            context, ir::InsertionPoint::createInsertBefore(*scc.begin()),
            entryEdges);
        scc.insert(bodyLabel);
        edges = calculateCycleEdges(scc);
      }

      if (!continueLabel || bodyLabel == continueLabel ||
          getSuccessorCount(continueLabel) != 1) {

        std::unordered_set<ir::Block> preds;
        for (auto edge : edges.backEdges) {
          preds.insert(edge.from());
        }
        continueLabel = createMergeBlock(
            context, ir::InsertionPoint::createInsertAfter(bodyLabel), preds,
            bodyLabel);
        scc.insert(continueLabel);
        edges = calculateCycleEdges(scc);
      }
    }

    if (!edges.exitEdges.empty()) {
      mergeLabel = [&] -> ir::Block {
        auto exitEdges = std::span(edges.exitEdges);
        auto header = exitEdges[0].to();
        exitEdges = exitEdges.subspan(1);

        while (!exitEdges.empty()) {
          if (header != exitEdges[0].to()) {
            return {};
          }

          exitEdges = exitEdges.subspan(1);
        }

        return header;
      }();

      if (mergeLabel) {
        auto predecessors = getPredecessors(mergeLabel);

        for (auto pred : predecessors) {
          if (!scc.contains(pred)) {
            mergeLabel = {};
            break;
          }
        }

        if (mergeLabel && predecessors.size() == 1) {
          latchLabel = *predecessors.begin();

          auto latchSuccessors = getSuccessors(latchLabel);

          auto it = latchSuccessors.begin();
          auto firstSuccessor = *it;
          auto secondSuccessor = *++it;

          if ((firstSuccessor != continueLabel &&
               secondSuccessor != continueLabel)) {
            latchLabel = {};
            mergeLabel = {};
          }

          if (latchLabel && getPredecessorCount(continueLabel) != 1) {
            latchLabel = {};
          }
        }
      }

      if (!mergeLabel) {
        mergeLabel = createRouteBlock(
            context,
            ir::InsertionPoint::createInsertAfter(edges.exitEdges[0].from()),
            edges.exitEdges);

        edges = calculateCycleEdges(scc);
      }

      if (!latchLabel) {
        std::vector<Edge> exitEdges = edges.exitEdges;

        for (auto [pred, operandIndex] : getAllPredecessors(continueLabel)) {
          exitEdges.emplace_back(pred, operandIndex);
        }

        latchLabel = createRouteBlock(
            context,
            ir::InsertionPoint::createInsertAfter(edges.exitEdges[0].from()),
            exitEdges);
        scc.insert(latchLabel);
      }
    }

    if (bodyLabel && continueLabel && mergeLabel) {
      auto loopConstruct = createLoopConstruct(
          context, root, bodyLabel, latchLabel, continueLabel, mergeLabel, scc);

      // replace references to body outside this construct with header (i.e.
      // loop construct node)
      bodyLabel.replaceUsesIf(loopConstruct, [=](ir::ValueUse use) {
        return (isTerminator(use.user) ||
                (use.user != loopConstruct && isConstruct(use.user))) &&
               getConstructOf(use.user) != loopConstruct;
      });

      // move PHIs to construct
      for (auto phi : ir::range(bodyLabel.getFirst())) {
        if (phi != ir::spv::OpPhi) {
          break;
        }

        phi.erase();
        loopConstruct.prependChild(phi);
      }
    }
  }
}

static void wrapSelectionConstructs(spv::Context &context,
                                    ir::RegionLike root) {
  std::vector<ir::Range<ir::Block>> workList;
  workList.push_back(root.children<ir::Block>());
  std::unordered_set<ir::Block> usedMergeBlocks;

  while (!workList.empty()) {
    auto region = workList.back();
    workList.pop_back();

    for (auto entryBlock : region) {
      if (isConstruct(entryBlock)) {
        if (entryBlock == ir::builtin::SELECTION_CONSTRUCT) {
          if (auto body =
                  skipPhis(entryBlock.getFirst()).getNext().cast<ir::Block>()) {
            workList.emplace_back(ir::range(body));
          }
        } else if (auto body =
                       skipPhis(entryBlock.getFirst()).cast<ir::Block>()) {
          workList.emplace_back(ir::range(body));
        }
        continue;
      }

      auto terminator = entryBlock.getLast();
      if (!terminator || !isTerminator(terminator) ||
          (terminator != ir::spv::OpBranchConditional &&
           terminator != ir::spv::OpSwitch)) {
        continue;
      }

      ir::RegionLike parentConstruct = getConstructOf(entryBlock);

      if (auto parentSelection =
              parentConstruct.cast<ir::SelectionConstruct>()) {
        if (parentSelection.getHeader() == entryBlock) {
          continue;
        }
      }

      auto successors = getSuccessors(entryBlock);

      if (parentConstruct) {
        if (parentConstruct.getLast() == entryBlock) {
          // do not look at latch/continuation blocks
          continue;
        }
      }

      if (!parentConstruct) {
        parentConstruct = root;
      }

      std::unordered_set<ir::Block> components;
      components.insert(entryBlock);

      auto addConstructComponent = [&](ir::Construct construct) {
        components.insert(construct);

        // add whole body of construct
        for (auto child : construct.children<ir::Block>()) {
          components.insert(child);
        }

        if (auto loop = construct.cast<ir::LoopConstruct>()) {
          // it if is loop, add continue construct also
          for (auto child : loop.getContinue().children<ir::Block>()) {
            components.insert(child);
          }
        }

        auto constructMerge = construct.getMerge();
        if (parentConstruct != root &&
            getSuccessorCount(construct.getMerge()) == 0) {
          // we cannot take this merge block, it is exit from function block
          // create trampoline node and replace merge block of this node

          auto newMerge = createMergeBlock(
              context, ir::InsertionPoint::createInsertBefore(constructMerge),
              getPredecessors(constructMerge), constructMerge);

          construct.setMerge(newMerge);
          constructMerge = newMerge;
        }

        components.insert(constructMerge);

        return getSuccessors(constructMerge);
      };

      auto addComponent = [&](ir::Block block) {
        if (auto construct = block.cast<ir::Construct>()) {
          return addConstructComponent(construct);
        }

        if (hasAtLeastSuccessors(block, 1)) {
          components.insert(block);
          return getSuccessors(block);
        }

        auto trampoline = createMergeBlock(
            context, ir::InsertionPoint::createInsertBefore(block),
            getPredecessors(block), block);

        components.insert(trampoline);
        return getSuccessors(block);
      };

      {
        // try to find blocks that has no other predecessors

        auto parentEntry =
            skipPhis(parentConstruct.getFirst()).staticCast<ir::Block>();

        auto headerSuccessors = getSuccessors(entryBlock);

        std::vector<ir::Block> workList(headerSuccessors.begin(),
                                        headerSuccessors.end());
        while (!workList.empty()) {
          auto block = workList.back();
          workList.pop_back();

          if (components.contains(block)) {
            continue;
          }

          if (block.getParent() != parentConstruct) {
            continue;
          }

          if (block == parentEntry || block == parentConstruct.getLast()) {
            // do not take entry/latch/continuation of parent construct
            continue;
          }

          bool hasAllPreds = true;
          auto loop = block.cast<ir::LoopConstruct>();
          for (auto pred : getPredecessors(block)) {
            if (components.contains(pred)) {
              continue;
            }

            if (loop && pred == loop.getContinue().getLast()) {
              // ignore continue predecessor of loop
              continue;
            }

            hasAllPreds = false;
            break;
          }

          if (hasAllPreds) {
            addComponent(block);
          }
        }
      }

      if (components.size() == 1) {
        // all successors are used by nodes outside this header, it means it is
        // not structured loop node or case block of OpSwitch with fallthrough
        continue;
      }

      ir::Block entryLabel = entryBlock;
      ir::Block mergeLabel;
      bool mergeInserted = false;

      std::unordered_set<ir::Block> exitBlocks;
      std::vector<Edge> exitEdges;
      for (auto block : components) {
        for (auto [succ, operandIndex] : getAllSuccessors(block)) {
          if (!components.contains(succ)) {
            exitEdges.emplace_back(block, operandIndex);
            exitBlocks.insert(block);
          }
        }
      }

      if (!exitBlocks.empty()) {
        if (exitBlocks.size() == 1) {
          mergeLabel = *exitBlocks.begin();
        }

        if (!mergeLabel ||
            getAllPredecessors(mergeLabel).size() != exitEdges.size() ||
            isConstruct(mergeLabel)) {
          mergeLabel = createRouteBlock(
              context, ir::InsertionPoint::createInsertAfter(entryBlock),
              exitEdges);

          workList.emplace_back(ir::range(mergeLabel));
          mergeInserted = true;
        }
      } else {
        mergeLabel = parentConstruct.getLast().staticCast<ir::Block>();
      }

      if (!mergeInserted) {
        for (auto user : mergeLabel.getUserList()) {
          if (auto construct = user.cast<ir::Construct>()) {
            if (construct.getMerge() != mergeLabel) {
              continue;
            }
          }
          mergeLabel = createMergeBlock(
              context, ir::InsertionPoint::createInsertBefore(mergeLabel),
              getPredecessors(mergeLabel), mergeLabel);
          mergeInserted = true;
          break;
        }
      }

      if (!mergeInserted) {
        auto mergePreds = getPredecessors(mergeLabel);
        std::unordered_set<ir::Block> branchesInsideConstruct;

        for (auto pred : mergePreds) {
          if (components.contains(pred)) {
            branchesInsideConstruct.insert(pred);
          }
        }

        if (branchesInsideConstruct.size() != mergePreds.size()) {
          mergeLabel = createMergeBlock(
              context, ir::InsertionPoint::createInsertBefore(mergeLabel),
              branchesInsideConstruct, mergeLabel);
        }
      }

      auto construct = createSelectionConstruct(
          context, parentConstruct, components, entryLabel, mergeLabel);

      // update merge label
      construct.setMerge(mergeLabel);

      construct.getHeader().replaceUsesIf(construct, [=](ir::ValueUse use) {
        if (getConstructOf(use.user) != construct) {
          if (isTerminator(use.user)) {
            return true;
          }

          // allow update block merges
          if (isConstruct(use.user) && use.operandIndex == 1) {
            return true;
          }
        }

        if (use.user != construct && isConstruct(use.user)) {
          return true;
        }

        return false;
      });

      // move PHIs to construct
      for (auto phi : ir::range(construct.getHeader().getFirst())) {
        if (phi != ir::spv::OpPhi) {
          break;
        }

        phi.erase();
        construct.prependChild(phi);
      }

      // view child constructs
      if (auto child = construct.getHeader().getNext().cast<ir::Block>()) {
        workList.emplace_back(ir::range(child));
      }

      // view next constructs
      if (auto next = construct.getNext()) {
        workList.emplace_back(ir::range(next));
      }

      break;
    }
  }
}

void shader::structurizeCfg(spv::Context &context, ir::RegionLike region) {
  // std::cerr << "before transforms: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";

  transformToCanonicalRegion(context, region);
  transformToCf(context, region);

  wrapLoopConstructs(context, region);
  wrapSelectionConstructs(context, region);

  // std::cerr << "structured: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";
  transformToFlat(context, region);

  // std::cerr << "flat: ";
  // region.print(std::cerr, context.ns);
  // std::cerr << "\n";
}
