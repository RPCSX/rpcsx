#include "cf.hpp"
#include <cassert>
#include <cstdlib>
#include <unordered_set>

void cf::BasicBlock::split(BasicBlock *target) {
  assert(target->address > address);
  target->size = size - (target->address - address);
  size = target->address - address;

  for (std::size_t i = 0, count = getSuccessorsCount(); i < count; ++i) {
    auto succ = getSuccessor(i);
    succ->predecessors.erase(this);
    succ->predecessors.insert(target);
    target->successors[i] = successors[i];
    successors[i] = nullptr;
  }

  target->terminator = terminator;
  terminator = TerminatorKind::None;

  createBranch(target);
}

void cf::BasicBlock::createConditionalBranch(BasicBlock *ifTrue,
                                             BasicBlock *ifFalse) {
  assert(terminator == TerminatorKind::None);
  assert(getSuccessorsCount() == 0);
  ifTrue->predecessors.insert(this);
  ifFalse->predecessors.insert(this);

  successors[0] = ifTrue;
  successors[1] = ifFalse;

  terminator = TerminatorKind::Branch;
}

void cf::BasicBlock::createBranch(BasicBlock *target) {
  assert(terminator == TerminatorKind::None);
  assert(getSuccessorsCount() == 0);

  target->predecessors.insert(this);
  successors[0] = target;

  terminator = TerminatorKind::Branch;
}

void cf::BasicBlock::createBranchToUnknown() {
  assert(terminator == TerminatorKind::None);
  assert(getSuccessorsCount() == 0);

  terminator = TerminatorKind::BranchToUnknown;
}

void cf::BasicBlock::createReturn() {
  assert(terminator == TerminatorKind::None);
  assert(getSuccessorsCount() == 0);

  terminator = TerminatorKind::Return;
}

void cf::BasicBlock::replaceSuccessor(BasicBlock *origBB, BasicBlock *newBB) {
  origBB->predecessors.erase(this);
  newBB->predecessors.insert(this);

  if (origBB == successors[0]) {
    successors[0] = newBB;
    return;
  }

  if (origBB == successors[1]) {
    successors[1] = newBB;
    return;
  }

  std::abort();
}

bool cf::BasicBlock::hasDirectPredecessor(const BasicBlock &block) const {
  for (auto pred : predecessors) {
    if (pred == &block) {
      return true;
    }
  }

  return false;
}

bool cf::BasicBlock::hasPredecessor(const BasicBlock &block) const {
  if (&block == this) {
    return hasDirectPredecessor(block);
  }

  std::vector<const BasicBlock *> workList;
  std::unordered_set<const BasicBlock *> visited;
  workList.push_back(this);
  visited.insert(this);

  while (!workList.empty()) {
    auto node = workList.back();

    if (node == &block) {
      return true;
    }

    workList.pop_back();
    workList.reserve(workList.size() + predecessors.size());

    for (auto pred : predecessors) {
      if (visited.insert(pred).second) {
        workList.push_back(pred);
      }
    }
  }

  return false;
}
