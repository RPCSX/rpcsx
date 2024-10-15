#include "transform.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include <rx/die.hpp>
#include <unordered_set>

using namespace shader;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

struct InstCloner : ir::CloneMap {
  ir::Node getOrClone(ir::Context &context, ir::Node node,
                      bool isOperand) override {
    if (isOperand) {
      return node;
    }

    return ir::CloneMap::getOrClone(context, node, isOperand);
  }

  template <typename T> T get(T object) {
    if (auto result = getOverride(object)) {
      return result.template staticCast<T>();
    }

    return object;
  }
};

static bool replaceTerminatorTarget(ir::Instruction terminator,
                                    ir::Value oldTarget, ir::Value newTarget) {
  bool changes = false;
  for (std::size_t i = 0, end = terminator.getOperandCount(); i < end; ++i) {
    if (terminator.getOperand(i) == oldTarget) {
      terminator.replaceOperand(i, newTarget);
      changes = true;
    }
  }

  if (!changes) {
    return false;
  }

  auto selection = terminator.getPrev();

  if (selection == ir::spv::OpSelectionMerge ||
      selection == ir::spv::OpLoopMerge) {
    for (std::size_t i = 0, end = selection.getOperandCount(); i < end; ++i) {
      if (selection.getOperand(i) == oldTarget) {
        selection.replaceOperand(i, newTarget);
      }
    }
  }

  return true;
}

static void
cloneBlockRange(spv::Context &context, Construct &construct,
                CFG::Node *startNode, std::unordered_set<ir::Value> stopLabels,
                const std::unordered_set<CFG::Node *> &keepPredecessors) {
  std::unordered_set<CFG::Node *> visited;
  std::vector<CFG::Node *> workList;
  workList.push_back(startNode);
  visited.insert(startNode);

  InstCloner cloner;
  std::vector<ir::Value> clonedLabels;

  while (!workList.empty()) {
    auto bb = workList.back();
    workList.pop_back();

    if (!bb->hasTerminator()) {
      continue;
    }

    auto region = bb->getLabel().getParent();

    for (auto inst : bb->rangeWithoutTerminator()) {
      auto clonedInst = ir::clone(inst, context, cloner);
      region.addChild(clonedInst);

      if (inst == ir::spv::OpLabel) {
        clonedLabels.push_back(inst.staticCast<ir::Value>());
        context.ns.setNameOf(clonedInst, "clone_" + context.ns.getNameOf(inst));
      }
    }

    auto terminator = ir::clone(bb->getTerminator(), context, cloner);

    if (terminator != nullptr) {
      region.addChild(terminator);

      for (std::size_t i = 0, end = terminator.getOperandCount(); i < end;
           ++i) {
        auto target = terminator.getOperand(i).getAsValue();
        if (target != ir::spv::OpLabel || stopLabels.contains(target)) {
          continue;
        }

        terminator.replaceOperand(i, ir::clone(target, context, cloner));
      }

      auto selection = terminator.getPrev();

      if (selection == ir::spv::OpSelectionMerge ||
          selection == ir::spv::OpLoopMerge) {
        for (std::size_t i = 0, end = selection.getOperandCount(); i < end;
             ++i) {
          auto target = selection.getOperand(i).getAsValue();
          if (target != ir::spv::OpLabel || stopLabels.contains(target)) {
            continue;
          }

          selection.replaceOperand(i, ir::clone(target, context, cloner));
        }
      }
    }

    for (auto succ : bb->getSuccessors()) {
      if (stopLabels.contains(succ->getLabel())) {
        continue;
      }

      if (visited.insert(succ).second) {
        workList.push_back(succ);
      }
    }
  }

  for (auto label : clonedLabels) {
    for (auto inst : ir::range(label.getNext())) {
      if (inst != ir::spv::OpPhi) {
        break;
      }

      if (label == startNode->getLabel()) {
        auto clonedInst = ir::clone(inst, context, cloner);
        auto newClonedPhi = Builder::createInsertBefore(context, clonedInst)
                                .createSpvPhi(inst.getLocation(),
                                              inst.getOperand(0).getAsValue());
        clonedInst.staticCast<ir::Value>().replaceAllUsesWith(newClonedPhi);
        clonedInst.remove();

        for (std::size_t i = 1, end = inst.getOperandCount(); i < end; i += 2) {
          auto target = inst.getOperand(i).getAsValue();
          if (target != ir::spv::OpLabel) {
            continue;
          }

          if (cloner.getOverride(target) == nullptr) {
            continue;
          }

          bool hasPred = false;

          for (auto pred : keepPredecessors) {
            if (pred->getLabel() == target) {
              hasPred = true;
              break;
            }
          }

          if (hasPred) {
            newClonedPhi.addOperand(inst.eraseOperand(i));
            newClonedPhi.addOperand(inst.eraseOperand(i));
          } else {
            inst.replaceOperand(i, ir::clone(target, context, cloner));
          }
        }
      } else {
        for (std::size_t i = 2, end = inst.getOperandCount(); i < end; i += 2) {
          auto target = inst.getOperand(i).getAsValue();
          if (target != ir::spv::OpLabel || stopLabels.contains(target)) {
            continue;
          }

          inst.replaceOperand(i, ir::clone(target, context, cloner));
        }
      }

      break;
    }
  }

  auto clonedStartLabel = cloner.get(startNode->getLabel());
  auto backEdges = construct.getBackEdges(startNode->getLabel());
  for (auto pred : keepPredecessors) {
    if (backEdges && backEdges->contains(pred->getLabel())) {
      continue;
    }

    replaceTerminatorTarget(pred->getTerminator(), startNode->getLabel(),
                            clonedStartLabel);
  }
}

static ir::Instruction findTerminator(ir::Instruction label) {
  while (!isTerminator(label)) {
    label = label.getNext();
  }

  return label;
}

static ir::Value createMergeBlock(
    spv::Context &context, CFG::Node *originalNode,
    const std::unordered_multimap<CFG::Node *, CFG::Node *> &edges) {
  auto loc = originalNode->getLabel().getLocation();
  auto mergeBlockBuilder =
      Builder::createInsertAfter(context, originalNode->getTerminator());
  auto mergeLabel = mergeBlockBuilder.createSpvLabel(loc);
  auto region = mergeLabel.getParent();

  rx::dieIf(edges.empty(), "createMergeBlock: unexpected edges count");
  if (edges.size() == 1) {
    auto [from, to] = *edges.begin();
    mergeBlockBuilder.createSpvBranch(loc, to->getLabel());

    replaceTerminatorTarget(from->getTerminator(), to->getLabel(), mergeLabel);
  } else if (edges.size() == 2) {
    auto blockMergePhi =
        mergeBlockBuilder.createSpvPhi(loc, context.getTypeBool());

    auto firstEdgeIt = edges.begin();
    auto secondEdgeIt = std::next(firstEdgeIt);

    mergeBlockBuilder.createSpvBranchConditional(
        loc, blockMergePhi, secondEdgeIt->second->getLabel(),
        firstEdgeIt->second->getLabel());

    for (std::uint32_t index = 0; auto [from, to] : edges) {
      auto terminator = from->getTerminator();

      auto terminateBlockBuilder = Builder::createAppend(context, region);
      auto terminateBlock = terminateBlockBuilder.createSpvLabel(loc);
      terminateBlockBuilder.createSpvBranch(loc, mergeLabel);
      blockMergePhi.addOperand(context.getBool(index++ > 0));
      blockMergePhi.addOperand(terminateBlock);

      replaceTerminatorTarget(terminator, to->getLabel(), terminateBlock);
    }
  } else {
    auto blockMergePhi =
        mergeBlockBuilder.createSpvPhi(loc, context.getTypeUInt32());

    auto blockMergeSwitch = mergeBlockBuilder.createSpvSwitch(
        loc, blockMergePhi, edges.begin()->second->getLabel());

    for (std::uint32_t index = 0; auto [from, to] : edges) {
      auto terminator = from->getTerminator();

      auto terminateBlockBuilder = Builder::createAppend(context, region);
      auto terminateBlock = terminateBlockBuilder.createSpvLabel(loc);
      terminateBlockBuilder.createSpvBranch(loc, mergeLabel);

      auto blockId = context.imm32(index);
      if (index != 0) {
        blockMergeSwitch.addOperand(blockId);
        blockMergeSwitch.addOperand(to->getLabel());
      }

      ++index;

      blockMergePhi.addOperand(blockId);
      blockMergePhi.addOperand(terminateBlock);

      replaceTerminatorTarget(terminator, to->getLabel(), terminateBlock);
    }
  }

  return mergeLabel;
}

static std::pair<ir::Value, ir::Instruction>
createTrampolineBlock(spv::Context &context,
                      const std::unordered_set<CFG::Node *> &preds,
                      CFG::Node *to) {

  rx::dieIf(preds.empty(), "createTrampolineBlock: unexpected edges count");

  auto loc = to->getLabel().getLocation();

  auto trampolineBuilder = Builder::createInsertBefore(context, to->getLabel());
  auto trampolineLabel = trampolineBuilder.createSpvLabel(loc);
  auto terminator = trampolineBuilder.createSpvBranch(loc, to->getLabel());

  if (preds.size() == to->getPredecessorCount()) {
    for (auto phi : ir::range(to->getLabel().getNext())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      phi.erase();
      trampolineLabel.getParent().insertAfter(trampolineLabel, phi);
    }
  } else if (preds.size() == 1) {
    for (auto phi : ir::range(to->getLabel().getNext())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
        if (phi.getOperand(i) == to->getLabel()) {
          phi.replaceOperand(i, trampolineLabel);
        }
      }
    }
  } else {
    for (auto phi : ir::range(to->getLabel().getNext())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      auto newPhi =
          Builder::createInsertAfter(context, trampolineLabel)
              .createSpvPhi(phi.getLocation(), phi.getOperand(0).getAsValue());

      for (std::size_t i = 1; i < phi.getOperandCount();) {
        auto value = phi.getOperand(i).getAsValue();
        auto label = phi.getOperand(i + 1).getAsValue();

        bool hasPred = false;
        for (auto pred : preds) {
          if (pred->getLabel() == label) {
            hasPred = true;
            break;
          }
        }

        if (hasPred) {
          newPhi.addOperand(phi.eraseOperand(i));
          newPhi.addOperand(phi.eraseOperand(i));
        } else {
          i += 2;
        }
      }

      phi.addOperand(newPhi);
      phi.addOperand(trampolineLabel);
    }
  }

  for (auto pred : preds) {
    replaceTerminatorTarget(pred->getTerminator(), to->getLabel(),
                            trampolineLabel);
  }

  return {trampolineLabel, terminator};
}

static ir::Value createEntryBlock(
    spv::Context &context, CFG::Node *originalHeadNode,
    const std::unordered_map<CFG::Node *, std::unordered_set<CFG::Node *>>
        &edges) {

  auto loc = originalHeadNode->getLabel().getLocation();
  auto entryBuilder =
      Builder::createInsertBefore(context, originalHeadNode->getLabel());
  auto entryLabel = entryBuilder.createSpvLabel(loc);
  context.ns.setUniqueNameOf(entryLabel, "head");
  auto region = originalHeadNode->getLabel().getParent();

  rx::dieIf(edges.empty(), "createEntryBlock: unexpected edges count");

  ir::Value selectorPhi;
  ir::Value defaultPhiValue;

  if (edges.size() == 1) {
    selectorPhi = entryBuilder.createSpvPhi(loc, context.getTypeBool());
    defaultPhiValue = context.getFalse();
    auto &[to, fromList] = *edges.begin();

    auto [trampoline, terminator] =
        createTrampolineBlock(context, fromList, to);

    entryBuilder.createSpvBranchConditional(loc, selectorPhi, to->getLabel(),
                                            originalHeadNode->getLabel());
    replaceTerminatorTarget(terminator, to->getLabel(), entryLabel);
    selectorPhi.addOperand(context.getTrue());
    selectorPhi.addOperand(trampoline);
  } else {
    selectorPhi = entryBuilder.createSpvPhi(loc, context.getTypeUInt32());
    defaultPhiValue = context.imm32(0);
    auto selectorSwitch = entryBuilder.createSpvSwitch(
        loc, selectorPhi, originalHeadNode->getLabel());

    for (std::uint32_t index = 1; auto [to, fromList] : edges) {
      selectorSwitch.addOperand(index);
      selectorSwitch.addOperand(to->getLabel());
      auto [trampoline, terminator] =
          createTrampolineBlock(context, fromList, to);
      replaceTerminatorTarget(terminator, to->getLabel(), entryLabel);
      selectorPhi.addOperand(context.imm32(index));
      selectorPhi.addOperand(trampoline);

      ++index;
    }
  }

  for (auto originalPred : originalHeadNode->getPredecessors()) {
    if (replaceTerminatorTarget(originalPred->getTerminator(),
                                originalHeadNode->getLabel(), entryLabel)) {
      selectorPhi.addOperand(defaultPhiValue);
      selectorPhi.addOperand(originalPred->getLabel());
    }
  }

  return entryLabel;
}

static std::pair<Construct *, bool>
structurizeConstruct(spv::Context &context, Construct &parentConstruct,
                     ir::Value entry) {
  ir::Value mergeLabel;
  auto &parentCfg = parentConstruct.getCfg();

  bool isLoop = false;
  ir::Instruction entryTerminator;
  {

    auto entryNode = parentCfg.getNode(entry);
    entryTerminator = entryNode->getTerminator();

    auto queryConstruct =
        parentConstruct.createTemporaryChild(entry, parentConstruct.merge);
    queryConstruct.loopContinue = parentConstruct.loopContinue;
    auto &postDomTree = queryConstruct.getPostDomTree();

    for (auto succ : entryNode->getSuccessors()) {
      if (mergeLabel == nullptr) {
        mergeLabel = succ->getLabel();
      } else {
        if (mergeLabel == succ->getLabel()) {
          continue;
        }
        mergeLabel = postDomTree.findNearestCommonDominator(mergeLabel,
                                                            succ->getLabel());
      }
    }

    if (auto backEdges = queryConstruct.getBackEdgesWithoutContinue(entry)) {
      isLoop = entry != parentConstruct.loopContinue;

      auto &domTree = queryConstruct.getDomTree();

      for (auto backEdge : *backEdges) {
        mergeLabel =
            postDomTree.findNearestCommonDominator(mergeLabel, backEdge);

        if (mergeLabel == parentConstruct.merge) {
          break;
        }

        for (auto pred :
             queryConstruct.getCfg().getNode(backEdge)->getPredecessors()) {
          mergeLabel = postDomTree.findNearestCommonDominator(mergeLabel,
                                                              pred->getLabel());
        }

        if (mergeLabel == parentConstruct.merge) {
          break;
        }
      }
    }

    if (queryConstruct.merge != mergeLabel) {
      queryConstruct.merge = mergeLabel;
      queryConstruct.analysis.invalidateAll();
    }

    if (isLoop) {
      isLoop = queryConstruct.getCfg().getNode(entry)->hasPredecessors();
    }

    while (mergeLabel != parentConstruct.merge) {
      // if selected merge block has branches to construct nodes, it is invalid
      // merge block, need to find another one
      auto &cfg = queryConstruct.getCfg();
      if (!cfg.getNode(mergeLabel)->hasSuccessors()) {
        break;
      }

      auto &postDomTree = parentConstruct.getPostDomTree();
      for (auto succ : parentCfg.getNode(mergeLabel)->getSuccessors()) {
        mergeLabel = postDomTree.findNearestCommonDominator(succ->getLabel(),
                                                            mergeLabel);
        if (queryConstruct.merge != mergeLabel) {
          queryConstruct.merge = mergeLabel;
          queryConstruct.analysis.invalidateAll();
        }

        if (mergeLabel == parentConstruct.merge) {
          break;
        }
      }
    }

    // pick latest available merge block
    while (mergeLabel != parentConstruct.merge) {
      auto mergeNode = parentConstruct.getCfg().getNode(mergeLabel);
      if (mergeNode->getSuccessorCount() != 1) {
        break;
      }

      auto nextMergeNode = *mergeNode->getSuccessors().begin();

      if (nextMergeNode->getPredecessorCount() != 1) {
        break;
      }

      auto nextLabel = nextMergeNode->getLabel();

      if (nextLabel == mergeLabel || nextLabel == parentConstruct.merge) {
        break;
      }

      mergeLabel = nextLabel;
    }
  }

  auto result = parentConstruct.createChild(entry, mergeLabel);
  result->loopContinue = parentConstruct.loopContinue;

  std::unordered_multimap<CFG::Node *, CFG::Node *> invalidExitEdges;
  std::unordered_map<CFG::Node *, std::unordered_set<CFG::Node *>>
      invalidEnterEdges;
  std::unordered_map<ir::Value, std::unordered_set<ir::Value>> invalidEdges;
  bool invalidMerge = result->merge == parentConstruct.merge;

  auto &cfg = result->getCfg();
  bool changes = false;

  for (auto block : cfg.getPreorderNodes()) {
    if (block == cfg.getEntryNode()) {
      continue;
    }

    auto parentBlock = parentCfg.getNode(block->getLabel());

    for (auto blockPred : parentBlock->getPredecessors()) {
      if (cfg.getNode(blockPred->getLabel()) != nullptr) {
        continue;
      }

      // it is branch to construct node from external block, need to fix it

      if (block->getLabel() == mergeLabel) {
        // only this construct can have branches to merge block
        invalidMerge = true;
        continue;
      }

      invalidEdges[block->getLabel()].emplace(blockPred->getLabel());
      invalidEnterEdges[block].emplace(blockPred);
      continue;
    }

    if (block->getLabel() == mergeLabel) {
      continue;
    }

    for (auto succ : parentBlock->getSuccessors()) {
      if (cfg.getNode(succ->getLabel()) == nullptr) {
        // branch to block outside this construct, it should be done from
        // merge block
        invalidExitEdges.emplace(block, succ);
      }
    }
  }

  for (auto &[edge, fromList] : invalidEnterEdges) {
    for (auto pred : edge->getPredecessors()) {
      fromList.insert(pred);
    }
  }

  bool isInvalidLoopHeader =
      isLoop && cfg.getEntryNode()->getTerminator() != ir::spv::OpBranch;
  bool isInvalidLoopContinue = false;

  if (isLoop) {
    auto entryNode = cfg.getEntryNode();
    if (entryNode->getPredecessorCount() > 1) {
      isInvalidLoopContinue = true;
    }
    if (!isInvalidLoopContinue) {
      auto predLabel = (*entryNode->getPredecessors().begin())->getLabel();
      auto continueNode = parentCfg.getNode(predLabel);

      // continue block is not part of construct, it should contain only
      // branch to header
      isInvalidLoopContinue = continueNode->getSuccessorCount() > 1;
    }
  } else {
    if (entryTerminator == ir::spv::OpBranch) {
      return {};
    }
  }

  if (isLoop) {
    if (!isInvalidLoopContinue) {
      result->loopContinue =
          (*cfg.getEntryNode()->getPredecessors().begin())->getLabel();
    }
    if (!isInvalidLoopHeader) {
      result->loopBody =
          (*cfg.getEntryNode()->getSuccessors().begin())->getLabel();
    }
  }

  if (isLoop) {
    if (isInvalidLoopContinue) {
      result->loopContinue =
          createTrampolineBlock(context, cfg.getEntryNode()->getPredecessors(),
                                parentCfg.getNode(result->header))
              .first;
      context.ns.setUniqueNameOf(result->loopContinue, "continue");
      return {nullptr, true};
    }

    if (isInvalidLoopHeader) {
      auto prevHeader = parentCfg.getNode(result->header);
      result->header = createTrampolineBlock(
                           context, prevHeader->getPredecessors(), prevHeader)
                           .first;
      return {nullptr, true};
    }
  }

  if (!invalidEdges.empty()) {
    auto &domTree = parentConstruct.getDomTree();
    for (auto &[to, fromList] : invalidEdges) {
      cloneBlockRange(context, *result, parentCfg.getNode(to),
                      {result->merge, isLoop ? result->header : nullptr},
                      cfg.getNode(to)->getPredecessors());
    }

    return {nullptr, true};
  }

  if (!invalidExitEdges.empty()) {
    auto mergeNode = parentCfg.getNode(result->merge);
    result->merge = createMergeBlock(context, mergeNode, invalidExitEdges);
    return {nullptr, true};
  }

  if (invalidMerge) {
    auto mergeNode = parentCfg.getNode(result->merge);
    result->merge =
        createTrampolineBlock(
            context, cfg.getNode(result->merge)->getPredecessors(), mergeNode)
            .first;
    return {nullptr, true};
  }

  if (!isInvalidLoopHeader && !invalidEnterEdges.empty()) {
    result->header = createEntryBlock(
        context, parentCfg.getNode(result->header), invalidEnterEdges);
    return {nullptr, true};
  }

  return {result, changes};
}

static bool structurizeCfgImpl(spv::Context &context, ir::RegionLike region,
                               ir::Value exitLabel) {
  bool changes = false;
  std::unordered_map<ir::Value, Construct *> resultConstructs;
  auto rootConstruct = Construct::createRoot(region, exitLabel);

  struct Entry {
    ir::Value header;
    std::vector<ir::Value> successors;
  };
  std::vector<Entry> workList;

  auto pushWorkList = [&](CFG::Node *node, ir::Value continueLabel = nullptr) {
    auto &entry = workList.emplace_back(Entry{node->getLabel()});

    for (auto succ : node->getSuccessors()) {
      if (continueLabel != succ->getLabel()) {
        entry.successors.push_back(succ->getLabel());
      }
    }
  };

  std::unordered_set<ir::Value> visited;
  std::unordered_set<ir::Value> seen;
  auto entryNode = rootConstruct->getCfg().getEntryNode();

  pushWorkList(entryNode);
  resultConstructs[entryNode->getLabel()] = rootConstruct.get();
  auto currentConstruct = rootConstruct.get();

  while (!workList.empty()) {
    auto &entry = workList.back();
    if (entry.successors.empty()) {
      if (currentConstruct->header == entry.header) {
        currentConstruct = currentConstruct->parent;
      }

      workList.pop_back();
      continue;
    }

    auto label = entry.successors.back();
    entry.successors.pop_back();

    if (label == currentConstruct->merge) {
      continue;
    }

    if (!visited.insert(label).second) {
      continue;
    }

    CFG::Node *bb = currentConstruct->getCfg().getNode(label);
    ir::Value currentHeader = currentConstruct->header;

    if (bb == nullptr) {
      continue;
    }

    auto terminator = bb->getTerminator();
    if (terminator == nullptr) {
      continue;
    }

    auto selection = terminator.getPrev();

    if (selection == ir::spv::OpLoopMerge ||
        selection == ir::spv::OpSelectionMerge) {
      auto parentContinue = currentConstruct->loopContinue;
      currentConstruct = currentConstruct->createChild(
          bb->getLabel(), selection.getOperand(0).getAsValue());
      currentConstruct->loopContinue = parentContinue;

      seen.insert(bb->getLabel());
      seen.insert(selection.getOperand(0).getAsValue());

      if (selection == ir::spv::OpLoopMerge) {
        currentConstruct->loopContinue = selection.getOperand(1).getAsValue();

        seen.insert(selection.getOperand(1).getAsValue());
      }
    } else {
      selection = nullptr;
    }

    bool requiresSelection = false;

    if (selection == nullptr && isBranch(terminator)) {
      requiresSelection = true;

      if (terminator == ir::spv::OpBranchConditional) {
        if (seen.contains(terminator.getOperand(1).getAsValue()) &&
            seen.contains(terminator.getOperand(2).getAsValue())) {
          requiresSelection = false;
        }
      }
    }

    if (requiresSelection) {
      auto [newConstruct, cfgChanges] =
          structurizeConstruct(context, *currentConstruct, label);
      if (cfgChanges) {
        return true;
      }

      if (newConstruct != nullptr) {
        seen.insert(newConstruct->header);
        seen.insert(newConstruct->merge);

        if (newConstruct->loopContinue) {
          seen.insert(newConstruct->loopContinue);
        }

        auto structuralBlock =
            newConstruct->getCfg().getNode(newConstruct->header);
        auto mergeNode =
            currentConstruct->getCfg().getNode(newConstruct->merge);

        if (newConstruct->loopContinue == nullptr) {
          for (auto pred : mergeNode->getPredecessors()) {
            pushWorkList(pred, newConstruct->loopContinue);
          }
        } else {
          pushWorkList(mergeNode, newConstruct->loopContinue);
        }

        pushWorkList(structuralBlock);

        if (auto [it, inserted] =
                resultConstructs.emplace(newConstruct->header, nullptr);
            inserted) {
          it->second = newConstruct;
        }

        currentConstruct = newConstruct;
        continue;
      }
    }

    pushWorkList(bb);
  }

  auto &cfg = rootConstruct->getCfg();
  auto &domTree = rootConstruct->getDomTree();

  if (currentConstruct != nullptr) {
    rx::die("currentConstruct: %s-%s\n",
            context.ns.getNameOf(currentConstruct->header).c_str(),
            context.ns.getNameOf(currentConstruct->merge).c_str());
  }

  std::unordered_set<ir::Value> insertedLoops;
  std::unordered_set<ir::Value> insertedMerges;

  for (auto &[header, construct] : resultConstructs) {
    if (construct->loopBody != nullptr) {
      auto headerNode = cfg.getNode(construct->header);
      auto terminator = headerNode->getTerminator();

      Builder::createInsertBefore(context, terminator)
          .createSpvLoopMerge(terminator.getLocation(), construct->merge,
                              construct->loopContinue,
                              ir::spv::LoopControl::None());
      changes = true;
    } else {
      auto headerNode = cfg.getNode(construct->header);
      auto terminator = headerNode->getTerminator();

      if (terminator == ir::spv::OpBranch ||
          terminator.getPrev() == ir::spv::OpSelectionMerge) {
        continue;
      }

      if (!domTree.dominates(construct->header, construct->merge)) {
        continue;
      }

      Builder::createInsertBefore(context, terminator)
          .createSpvSelectionMerge(terminator.getLocation(), construct->merge,
                                   ir::spv::SelectionControl::None);
      changes = true;
    }
  }
  return changes;
}

void shader::structurizeCfg(spv::Context &context, ir::RegionLike region,
                            ir::Value exitLabel) {
  while (structurizeCfgImpl(context, region, exitLabel)) {
  }
}
