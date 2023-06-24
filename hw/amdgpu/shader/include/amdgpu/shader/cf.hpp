#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <vector>

namespace cf {
enum class TerminatorKind {
  None,
  Branch,
  BranchToUnknown,
  Return,
};

class BasicBlock {
  std::uint64_t address;
  std::uint64_t size = 0;

  std::set<BasicBlock *> predecessors;
  BasicBlock *successors[2]{};
  TerminatorKind terminator = TerminatorKind::None;

public:
  explicit BasicBlock(std::uint64_t address, std::uint64_t size = 0)
      : address(address), size(size) {}

  BasicBlock(const BasicBlock &) = delete;

  void setSize(std::uint64_t newSize) { size = newSize; }
  std::uint64_t getSize() const { return size; }
  std::uint64_t getAddress() const { return address; }
  TerminatorKind getTerminator() const { return terminator; }

  void createConditionalBranch(BasicBlock *ifTrue, BasicBlock *ifFalse);
  void createBranch(BasicBlock *target);
  void createBranchToUnknown();
  void createReturn();

  void replaceSuccessor(BasicBlock *origBB, BasicBlock *newBB);
  void replacePredecessor(BasicBlock *origBB, BasicBlock *newBB) {
    origBB->replaceSuccessor(this, newBB);
  }

  template <std::invocable<BasicBlock &> T> void walk(T &&cb) {
    std::vector<BasicBlock *> workStack;
    std::set<BasicBlock *> processed;

    workStack.push_back(this);
    processed.insert(this);

    while (!workStack.empty()) {
      auto block = workStack.back();
      workStack.pop_back();

      block->walkSuccessors([&](BasicBlock *successor) {
        if (processed.insert(successor).second) {
          workStack.push_back(successor);
        }
      });

      cb(*block);
    }
  }

  template <std::invocable<BasicBlock *> T> void walkSuccessors(T &&cb) const {
    if (successors[0]) {
      cb(successors[0]);

      if (successors[1]) {
        cb(successors[1]);
      }
    }
  }

  template <std::invocable<BasicBlock *> T> void walkPredecessors(T &&cb) const {
    for (auto pred : predecessors) {
      cb(pred);
    }
  }

  std::size_t getPredecessorsCount() const { return predecessors.size(); }

  bool hasDirectPredecessor(const BasicBlock &block) const;
  bool hasPredecessor(const BasicBlock &block) const;

  std::size_t getSuccessorsCount() const {
    if (successors[0] == nullptr) {
      return 0;
    }

    return successors[1] != nullptr ? 2 : 1;
  }

  BasicBlock *getSuccessor(std::size_t index) const { return successors[index]; }

  void split(BasicBlock *target);
};

class Context {
  std::map<std::uint64_t, BasicBlock, std::greater<>> basicBlocks;

public:
  BasicBlock *getBasicBlockAt(std::uint64_t address) {
    if (auto it = basicBlocks.find(address); it != basicBlocks.end()) {
      return &it->second;
    }

    return nullptr;
  }
  
  BasicBlock *getBasicBlock(std::uint64_t address) {
    if (auto it = basicBlocks.lower_bound(address); it != basicBlocks.end()) {
      auto bb = &it->second;

      if (bb->getAddress() <= address &&
          bb->getAddress() + bb->getSize() > address) {
        return bb;
      }
    }

    return nullptr;
  }

  BasicBlock *getOrCreateBasicBlock(std::uint64_t address, bool split = true) {
    auto it = basicBlocks.lower_bound(address);

    if (it != basicBlocks.end()) {
      auto bb = &it->second;

      if (bb->getAddress() <= address &&
          bb->getAddress() + bb->getSize() > address) {
        if (split && bb->getAddress() != address) {
          auto result = &basicBlocks.emplace_hint(it, address, address)->second;
          bb->split(result);
          return result;
        }

        return bb;
      }
    }

    return &basicBlocks.emplace_hint(it, address, address)->second;
  }
};
} // namespace cf
