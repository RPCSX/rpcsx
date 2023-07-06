#include "scf.hpp"
#include "cf.hpp"
#include <cassert>
#include <fstream>
#include <unordered_set>
#include <utility>

void scf::Block::eraseFrom(Node *endBefore) {
  mEnd = endBefore->getPrev();
  if (mEnd != nullptr) {
    mEnd->mNext = nullptr;
  } else {
    mBegin = nullptr;
  }
}

void scf::Block::splitInto(Block *target, Node *splitPoint) {
  auto targetEnd = std::exchange(mEnd, splitPoint->mPrev);

  if (mEnd != nullptr) {
    mEnd->mNext = nullptr;
  } else {
    mBegin = nullptr;
  }

  for (auto node = splitPoint; node != nullptr; node = node->getNext()) {
    node->mParent = target;
  }

  if (target->mEnd != nullptr) {
    target->mEnd->mNext = splitPoint;
  }

  splitPoint->mPrev = target->mEnd;
  target->mEnd = targetEnd;

  if (target->mBegin == nullptr) {
    target->mBegin = splitPoint;
  }
}

scf::Block *scf::Block::split(Context &context, Node *splitPoint) {
  auto result = context.create<Block>();
  splitInto(result, splitPoint);
  return result;
}

static scf::BasicBlock *findJumpTargetIn(scf::Block *parentBlock,
                                         scf::Block *testBlock) {
  auto jumpNode = dynCast<scf::Jump>(testBlock->getLastNode());

  if (jumpNode == nullptr || jumpNode->target->getParent() != parentBlock) {
    return nullptr;
  }

  return jumpNode->target;
}

static bool transformJumpToLoop(scf::Context &ctxt, scf::Block *block) {
  // bb0
  // bb1
  // if true {
  //   bb2
  //   jump bb1
  // } else {
  //   bb3
  // }
  //
  // -->
  //
  // bb0
  // loop {
  //   bb1
  //   if false {
  //     break
  //   }
  //   bb2
  // }
  // bb3

  if (block->isEmpty()) {
    return false;
  }

  auto ifElse = dynCast<scf::IfElse>(block->getLastNode());

  if (ifElse == nullptr) {
    return false;
  }

  auto loopTarget = findJumpTargetIn(block, ifElse->ifTrue);
  auto loopBlock = ifElse->ifTrue;
  auto invariantBlock = ifElse->ifFalse;

  if (loopTarget == nullptr) {
    loopTarget = findJumpTargetIn(block, ifElse->ifFalse);
    loopBlock = ifElse->ifFalse;
    invariantBlock = ifElse->ifTrue;

    if (loopTarget == nullptr) {
      return false;
    }
  }

  auto loopBody = block->split(ctxt, loopTarget);
  auto loop = ctxt.create<scf::Loop>(loopBody);
  block->append(loop);

  for (auto node = invariantBlock->getRootNode(); node != nullptr;) {
    auto nextNode = node->getNext();
    invariantBlock->detachNode(node);
    block->append(node);
    node = nextNode;
  }

  loopBlock->detachNode(loopBlock->getLastNode());

  for (auto node = loopBlock->getRootNode(); node != nullptr;) {
    auto nextNode = node->getNext();
    loopBlock->detachNode(node);
    loopBody->append(node);
    node = nextNode;
  }

  invariantBlock->append(ctxt.create<scf::Break>());

  return true;
}

static bool moveSameLastBlocksTo(scf::IfElse *ifElse, scf::Block *block) {
  if (ifElse->ifTrue->isEmpty() || ifElse->ifFalse->isEmpty()) {
    return false;
  }

  auto ifTrueIt = ifElse->ifTrue->getLastNode();
  auto ifFalseIt = ifElse->ifFalse->getLastNode();

  while (ifTrueIt != nullptr && ifFalseIt != nullptr) {
    if (!ifTrueIt->isEqual(*ifFalseIt)) {
      break;
    }

    ifTrueIt = ifTrueIt->getPrev();
    ifFalseIt = ifFalseIt->getPrev();
  }

  if (ifTrueIt == ifElse->ifTrue->getLastNode()) {
    return false;
  }

  if (ifTrueIt == nullptr) {
    ifTrueIt = ifElse->ifTrue->getRootNode();
  } else {
    ifTrueIt = ifTrueIt->getNext();
  }

  if (ifFalseIt == nullptr) {
    ifFalseIt = ifElse->ifFalse->getRootNode();
  } else {
    ifFalseIt = ifFalseIt->getNext();
  }

  ifElse->ifTrue->splitInto(block, ifTrueIt);
  ifElse->ifFalse->eraseFrom(ifFalseIt);
  return true;
}

class Structurizer {
  scf::Context &context;

public:
  Structurizer(scf::Context &context) : context(context) {}

  scf::Block *structurize(cf::BasicBlock *bb) {
    return structurizeBlock(bb, {});
  }

public:
  scf::IfElse *structurizeIfElse(
      cf::BasicBlock *ifTrue, cf::BasicBlock *ifFalse,
      std::unordered_map<cf::BasicBlock *, scf::BasicBlock *> &visited) {
    auto ifTrueBlock = structurizeBlock(ifTrue, visited);
    auto ifFalseBlock = structurizeBlock(ifFalse, visited);

    return context.create<scf::IfElse>(ifTrueBlock, ifFalseBlock);
  }

  scf::Block *structurizeBlock(
      cf::BasicBlock *bb,
      std::unordered_map<cf::BasicBlock *, scf::BasicBlock *> visited) {
    auto result = context.create<scf::Block>();
    std::vector<cf::BasicBlock *> workList;
    workList.push_back(bb);

    while (!workList.empty()) {
      auto block = workList.back();
      workList.pop_back();

      auto [it, inserted] = visited.try_emplace(block, nullptr);
      if (!inserted) {
        result->append(context.create<scf::Jump>(it->second));
        continue;
      }

      auto scfBlock = context.create<scf::BasicBlock>(block->getAddress(),
                                                      block->getSize());
      it->second = scfBlock;
      result->append(scfBlock);

      switch (block->getTerminator()) {
      case cf::TerminatorKind::None:
        std::abort();
        break;

      case cf::TerminatorKind::Branch:
        switch (block->getSuccessorsCount()) {
        case 1:
          workList.push_back(block->getSuccessor(0));
          break;

        case 2: {
          auto ifElse = structurizeIfElse(block->getSuccessor(0),
                                          block->getSuccessor(1), visited);
          result->append(ifElse);

          while (moveSameLastBlocksTo(ifElse, result) ||
                 transformJumpToLoop(context, result)) {
            ;
          }

          break;
        }
        }
        break;

      case cf::TerminatorKind::BranchToUnknown:
        result->append(context.create<scf::UnknownBlock>());
        break;

      case cf::TerminatorKind::Return:
        result->append(context.create<scf::Return>());
        break;
      }
    }

    return result;
  }
};

scf::Block *scf::structurize(Context &ctxt, cf::BasicBlock *bb) {
  return Structurizer{ctxt}.structurize(bb);
}
