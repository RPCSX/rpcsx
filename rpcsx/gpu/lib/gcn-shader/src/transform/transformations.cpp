#include "transform/transformations.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include <rx/die.hpp>

#include <iostream>
#include <rx/die.hpp>
#include <vector>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

ir::Value shader::transform::transformToCanonicalRegion(spv::Context &context,
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

void shader::transform::transformToCf(spv::Context &context,
                                      ir::RegionLike region) {
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

void shader::transform::transformToFlat(spv::Context &context,
                                        ir::RegionLike region) {
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
      // std::cout << "processing " << context.ns.getNameOf(block) << "\n";
      unwrapBlock(block);
      block.erase();
      continue;
    }

    insertPoint.eraseAndInsert(inst);
  }
}
