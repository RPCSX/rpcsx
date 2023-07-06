#pragma once

#include <cassert>
#include <cstdint>
#include <forward_list>
#include <functional>
#include <memory>

namespace cf {
class BasicBlock;
}

namespace scf {
class BasicBlock;
struct PrintOptions {
  unsigned char identCount = 2;
  char identChar = ' ';
  std::function<void(const PrintOptions &, unsigned depth, BasicBlock *)>
      blockPrinter;

  std::string makeIdent(unsigned depth) const {
    return std::string(depth * identCount, identChar);
  }
};

class Node {
  Node *mParent = nullptr;
  Node *mNext = nullptr;
  Node *mPrev = nullptr;

public:
  virtual ~Node() = default;
  virtual void print(const PrintOptions &options, unsigned depth) = 0;
  virtual bool isEqual(const Node &other) const { return this == &other; }

  void dump() { print({}, 0); }

  void setParent(Node *parent) { mParent = parent; }

  Node *getParent() const { return mParent; }

  template <typename T>
    requires(std::is_base_of_v<Node, T>)
  auto getParent() const -> decltype(dynCast<T>(mParent)) {
    return dynCast<T>(mParent);
  }

  Node *getNext() const { return mNext; }

  Node *getPrev() const { return mPrev; }

  friend class Block;
};

template <typename T, typename ST>
  requires(std::is_base_of_v<Node, T> && std::is_base_of_v<Node, ST>) &&
          requires(ST *s) { dynamic_cast<T *>(s); }
T *dynCast(ST *s) {
  return dynamic_cast<T *>(s);
}

template <typename T, typename ST>
  requires(std::is_base_of_v<Node, T> && std::is_base_of_v<Node, ST>) &&
          requires(const ST *s) { dynamic_cast<const T *>(s); }
const T *dynCast(const ST *s) {
  return dynamic_cast<const T *>(s);
}

inline bool isNodeEqual(const Node *lhs, const Node *rhs) {
  if (lhs == rhs) {
    return true;
  }

  return lhs != nullptr && rhs != nullptr && lhs->isEqual(*rhs);
}

struct UnknownBlock final : Node {
  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%sunknown\n", options.makeIdent(depth).c_str());
  }

  bool isEqual(const Node &other) const override {
    return this == &other || dynCast<UnknownBlock>(&other) != nullptr;
  }
};

struct Return final : Node {
  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%sreturn\n", options.makeIdent(depth).c_str());
  }

  bool isEqual(const Node &other) const override {
    return this == &other || dynCast<Return>(&other) != nullptr;
  }
};

class Context;

class Block final : public Node {
  Node *mBegin = nullptr;
  Node *mEnd = nullptr;

  void *mUserData = nullptr;

public:
  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%s{\n", options.makeIdent(depth).c_str());

    for (auto node = mBegin; node != nullptr; node = node->getNext()) {
      node->print(options, depth + 1);
    }
    std::printf("%s}\n", options.makeIdent(depth).c_str());
  }

  bool isEmpty() const { return mBegin == nullptr; }

  Node *getRootNode() const { return mBegin; }
  Node *getLastNode() const { return mEnd; }

  void setUserData(void *data) { mUserData = data; }
  void *getUserData() const { return mUserData; }
  template <typename T> T *getUserData() const {
    return static_cast<T *>(mUserData);
  }

  void eraseFrom(Node *endBefore);
  void splitInto(Block *target, Node *splitPoint);
  Block *split(Context &context, Node *splitPoint);

  void append(Node *node) {
    assert(node->mParent == nullptr);
    assert(node->mPrev == nullptr);
    assert(node->mNext == nullptr);

    node->mParent = this;
    node->mPrev = mEnd;

    if (mEnd != nullptr) {
      mEnd->mNext = node;
    }

    if (mBegin == nullptr) {
      mBegin = node;
    }

    mEnd = node;
  }

  void detachNode(Node *node) {
    if (node->mPrev != nullptr) {
      node->mPrev->mNext = node->mNext;
    }

    if (node->mNext != nullptr) {
      node->mNext->mPrev = node->mPrev;
    }

    if (mBegin == node) {
      mBegin = node->mNext;
    }

    if (mEnd == node) {
      mEnd = node->mPrev;
    }

    node->mNext = nullptr;
    node->mPrev = nullptr;
    node->mParent = nullptr;
  }

  bool isEqual(const Node &other) const override {
    if (this == &other) {
      return true;
    }

    auto otherBlock = dynCast<Block>(&other);

    if (otherBlock == nullptr) {
      return false;
    }

    auto thisIt = mBegin;
    auto otherIt = otherBlock->mBegin;

    while (thisIt != nullptr && otherIt != nullptr) {
      if (!thisIt->isEqual(*otherIt)) {
        return false;
      }

      thisIt = thisIt->mNext;
      otherIt = otherIt->mNext;
    }

    return thisIt == otherIt;
  }
};

class BasicBlock final : public Node {
  std::uint64_t address;
  std::uint64_t size = 0;

public:
  explicit BasicBlock(std::uint64_t address, std::uint64_t size = 0)
      : address(address), size(size) {}

  std::uint64_t getSize() const { return size; }
  std::uint64_t getAddress() const { return address; }

  void print(const PrintOptions &options, unsigned depth) override {
    std::printf(
        "%sbb%lx\n",
        std::string(depth * options.identCount, options.identChar).c_str(),
        getAddress());
    if (depth != 0 && options.blockPrinter) {
      options.blockPrinter(options, depth + 1, this);
    }
  }

  Block *getBlock() const { return dynCast<Block>(getParent()); }

  bool isEqual(const Node &other) const override {
    if (this == &other) {
      return true;
    }

    if (auto otherBlock = dynCast<BasicBlock>(&other)) {
      return address == otherBlock->address;
    }

    return false;
  }
};

struct IfElse final : Node {
  Block *ifTrue;
  Block *ifFalse;

  IfElse(Block *ifTrue, Block *ifFalse) : ifTrue(ifTrue), ifFalse(ifFalse) {
    ifTrue->setParent(this);
    ifFalse->setParent(this);
  }

  void print(const PrintOptions &options, unsigned depth) override {
    if (ifTrue->isEmpty()) {
      std::printf("%sif false\n", options.makeIdent(depth).c_str());
      ifFalse->print(options, depth);
      return;
    }

    std::printf("%sif true\n", options.makeIdent(depth).c_str());
    ifTrue->print(options, depth);
    if (!ifFalse->isEmpty()) {
      std::printf("%selse\n", options.makeIdent(depth).c_str());
      ifFalse->print(options, depth);
    }
  }

  bool isEqual(const Node &other) const override {
    if (this == &other) {
      return true;
    }

    if (auto otherBlock = dynCast<IfElse>(&other)) {
      return ifTrue->isEqual(*otherBlock->ifTrue) &&
             ifFalse->isEqual(*otherBlock->ifFalse);
    }

    return false;
  }
};

struct Jump final : Node {
  BasicBlock *target;

  Jump(BasicBlock *target) : target(target) {}

  bool isEqual(const Node &other) const override {
    if (this == &other) {
      return true;
    }

    if (auto otherJump = dynCast<Jump>(&other)) {
      return target == otherJump->target;
    }

    return false;
  }

  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%sjump ", options.makeIdent(depth).c_str());
    target->print(options, 0);
  }
};

struct Loop final : Node {
  Block *body;

  Loop(Block *body) : body(body) { body->setParent(this); }

  bool isEqual(const Node &other) const override {
    if (this == &other) {
      return true;
    }

    if (auto otherLoop = dynCast<Loop>(&other)) {
      return body->isEqual(*otherLoop->body);
    }

    return false;
  }

  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%sloop {\n", options.makeIdent(depth).c_str());
    body->print(options, depth + 1);
    std::printf("%s}\n", options.makeIdent(depth).c_str());
  }
};

struct Break final : Node {
  bool isEqual(const Node &other) const override {
    return this == &other || dynCast<Break>(&other) != nullptr;
  }

  void print(const PrintOptions &options, unsigned depth) override {
    std::printf("%sbreak\n", options.makeIdent(depth).c_str());
  }
};

class Context {
  std::forward_list<std::unique_ptr<Node>> mNodes;

public:
  template <typename T, typename... ArgsT>
    requires(std::is_constructible_v<T, ArgsT...>)
  T *create(ArgsT &&...args) {
    auto result = new T(std::forward<ArgsT>(args)...);
    mNodes.push_front(std::unique_ptr<Node>{result});
    return result;
  }
};

scf::Block *structurize(Context &ctxt, cf::BasicBlock *bb);
void makeUniqueBasicBlocks(Context &ctxt, Block *block);
} // namespace scf
