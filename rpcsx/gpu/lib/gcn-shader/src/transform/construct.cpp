#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "transform/construct.hpp"
#include "dialect.hpp"
#include <rx/die.hpp>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

bool shader::transform::isConstruct(ir::Instruction block) {
  return block == ir::builtin::LOOP_CONSTRUCT ||
         block == ir::builtin::SELECTION_CONSTRUCT ||
         block == ir::builtin::CONTINUE_CONSTRUCT;
}

ir::Block shader::transform::getConstructOf(ir::Instruction inst) {
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

ir::Block shader::transform::getConstructMergeBlock(ir::Block block) {
  if (auto construct = block.cast<ir::Construct>()) {
    return construct.getMerge();
  }

  return {};
}

bool shader::transform::isParentConstruct(ir::RegionLike parent, 
                                          ir::RegionLike construct) {
  while (parent != construct && construct) {
    construct = construct.getParent();
  }

  return parent == construct;
}


ir::SelectionConstruct
shader::transform::createSelectionConstruct(spv::Context &context, 
                                            ir::RegionLike parentConstruct,
                                            const std::unordered_set<ir::Block> &components,
                                            ir::Block header, 
                                            ir::Block merge) {
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
      auto merge = construct.getMerge();
      merge.erase();
      selectionConstruct.addChild(merge);
      successors = getSuccessors(merge);
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

ir::LoopConstruct
shader::transform::createLoopConstruct(spv::Context &context, 
                                       ir::RegionLike parentConstruct,
                                       ir::Block header, 
                                       ir::Block latch, 
                                       ir::Block cont,
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
