#pragma once

#include "ModuleInfo.hpp"
#include "SemanticInfo.hpp"
#include "dialect/memssa.hpp"
#include "graph.hpp"
#include "ir/Instruction.hpp"
#include "ir/Value.hpp"
#include "rx/FunctionRef.hpp"
#include "rx/TypeId.hpp"
#include <map>
#include <ostream>
#include <utility>
#include <vector>

namespace shader {
struct DomTree;
struct PostDomTree;
class CFG {
public:
  class Node {
    ir::Value mLabel;
    ir::Instruction mTerminator;
    std::unordered_set<Node *> mPredecessors;
    std::unordered_set<Node *> mSuccessors;

  public:
    using Iterator = std::unordered_set<Node *>::iterator;

    Node() = default;
    Node(ir::Value label) : mLabel(label) {}

    ir::Value getLabel() { return mLabel; }

    void setTerminator(ir::Instruction inst) { mTerminator = inst; }
    bool hasTerminator() { return mTerminator != nullptr; }
    ir::Instruction getTerminator() { return mTerminator; }

    void addEdge(Node *to) {
      to->mPredecessors.insert(this);
      mSuccessors.insert(to);
    }

    bool hasPredecessor(Node *node) { return mPredecessors.contains(node); }
    bool hasSuccessor(Node *node) { return mSuccessors.contains(node); }
    auto &getPredecessors() { return mPredecessors; }
    auto &getSuccessors() { return mSuccessors; }
    std::size_t getPredecessorCount() { return mPredecessors.size(); }
    std::size_t getSuccessorCount() { return mSuccessors.size(); }
    bool hasPredecessors() { return !mPredecessors.empty(); }
    bool hasSuccessors() { return !mSuccessors.empty(); }

    template <typename T = ir::Instruction> auto range() {
      return ir::range<T>(mLabel, mTerminator.getNext());
    }

    template <typename T = ir::Instruction> auto rangeWithoutLabel() {
      return ir::range<T>(mLabel.getNext(),
                          mTerminator ? mTerminator.getNext() : nullptr);
    }

    template <typename T = ir::Instruction> auto rangeWithoutTerminator() {
      return ir::range<T>(mLabel, mTerminator);
    }

    template <typename T = ir::Instruction>
    auto rangeWithoutLabelAndTerminator() {
      return ir::range<T>(mLabel.getNext(), mTerminator);
    }
  };

private:
  std::map<ir::Value, Node> mNodes;
  std::vector<Node *> mPreorderNodes;
  std::vector<Node *> mPostorderNodes;
  Node *mEntryNode = nullptr;

public:
  bool empty() { return mNodes.empty(); }
  void clear() {
    mNodes.clear();
    mPreorderNodes.clear();
    mPostorderNodes.clear();
    mEntryNode = nullptr;
  }

  void addPreorderNode(Node *node) { mPreorderNodes.push_back(node); }
  void addPostorderNode(Node *node) { mPostorderNodes.push_back(node); }

  Node *getEntryNode() { return mEntryNode; }
  ir::Value getEntryLabel() { return getEntryNode()->getLabel(); }
  void setEntryNode(Node *node) { mEntryNode = node; }

  std::span<Node *> getPreorderNodes() { return mPreorderNodes; }
  std::span<Node *> getPostorderNodes() { return mPostorderNodes; }

  Node *getOrCreateNode(ir::Value label) {
    return &mNodes.emplace(label, label).first->second;
  }

  Node *getNode(ir::Value label) {
    if (auto it = mNodes.find(label); it != mNodes.end()) {
      return &it->second;
    }

    return nullptr;
  }

  auto &getSuccessors(ir::Value label) {
    return getNode(label)->getSuccessors();
  }

  auto &getPredecessors(ir::Value label) {
    return getNode(label)->getPredecessors();
  }

  void print(std::ostream &os, ir::NameStorage &ns, bool subgraph = false,
             std::string_view nameSuffix = "");
  std::string genTest();

  CFG buildView(CFG::Node *from, PostDomTree *domTree = nullptr,
                const std::unordered_set<ir::Value> &stopLabels = {},
                ir::Value continueLabel = nullptr);

  CFG buildView(ir::Value from, PostDomTree *domTree = nullptr,
                const std::unordered_set<ir::Value> &stopLabels = {},
                ir::Value continueLabel = nullptr) {
    return buildView(getNode(from), domTree, stopLabels, continueLabel);
  }
};

class MemorySSA {
public:
  ir::Context context;
  ir::Region region;
  std::map<ir::Value, ir::memssa::Var> variableToVar;
  std::map<ir::Instruction, std::map<ir::memssa::Var, ir::memssa::Def>>
      userDefs;

  ir::memssa::Var getVar(ir::Value variable, std::span<const ir::Operand> path);
  ir::memssa::Var getVar(ir::Value pointer);

  ir::memssa::Def getDef(ir::Instruction user, ir::memssa::Var var) {
    auto userIt = userDefs.find(user);
    if (userIt == userDefs.end()) {
      return {};
    }

    if (auto it = userIt->second.find(var); it != userIt->second.end()) {
      return it->second;
    }

    return {};
  }

  ir::memssa::Def getDef(ir::Instruction user, ir::Value pointer) {
    if (auto var = getVar(pointer)) {
      return getDef(user, var);
    }

    return {};
  }

  ir::Instruction getDefInst(ir::Instruction user, ir::Value pointer) {
    if (auto def = getDef(user, pointer)) {
      return def.getLinkedInst();
    }

    return {};
  }

  void print(std::ostream &os, ir::Region irRegion, ir::NameStorage &ns);
  void print(std::ostream &os, ir::NameStorage &ns);
  void dump();

private:
  ir::memssa::Var getVarImpl(ir::Value variable);
};

bool isWithoutSideEffects(ir::InstructionId id);
bool isTerminator(ir::Instruction inst);
bool isBranch(ir::Instruction inst);
ir::Value unwrapPointer(ir::Value pointer);
graph::DomTree<ir::Value> buildDomTree(CFG &cfg, ir::Value root = nullptr);
graph::DomTree<ir::Value> buildPostDomTree(CFG &cfg, ir::Value root);

CFG buildCFG(ir::Instruction firstInstruction,
             const std::unordered_set<ir::Value> &exitLabels = {},
             ir::Value continueLabel = nullptr);
MemorySSA buildMemorySSA(CFG &cfg, ModuleInfo *moduleInfo = nullptr);

MemorySSA buildMemorySSA(CFG &cfg, const SemanticInfo &instructionSemantic,
                         std::function<ir::Value(int)> getRegisterVarCb);

bool dominates(ir::Instruction a, ir::Instruction b, bool isPostDom,
               graph::DomTree<ir::Value> &domTree);

ir::Value findNearestCommonDominator(ir::Instruction a, ir::Instruction b,
                                     graph::DomTree<ir::Value> &domTree);

class BackEdgeStorage {
  std::unordered_map<ir::Value, std::unordered_set<ir::Value>> backEdges;

public:
  BackEdgeStorage() = default;
  BackEdgeStorage(CFG &cfg);

  const std::unordered_set<ir::Value> *get(ir::Value value) {
    if (auto it = backEdges.find(value); it != backEdges.end()) {
      return &it->second;
    }
    return nullptr;
  }

  auto &all() { return backEdges; }
};

struct AnalysisStorage {
  template <typename... T>
    requires(sizeof...(T) > 0)
  bool invalidate() {
    bool invalidated = false;
    ((invalidated = invalidate(rx::TypeId::get<T>()) || invalidated), ...);
    return invalidated;
  }

  bool invalidate(rx::TypeId id) {
    if (auto it = mStorage.find(id); it != mStorage.end()) {
      return std::exchange(it->second.invalid, true) == false;
    }

    return false;
  }
  void invalidateAll() {
    for (auto &entry : mStorage) {
      entry.second.invalid = true;
    }
  }

  template <typename T, typename... ArgsT>
  T &get(ArgsT &&...args)
    requires requires { T(std::forward<ArgsT>(args)...); }
  {
    void *result = getImpl(
        rx::TypeId::get<T>(), getDeleter<T>(),
        [&] {
          return std::make_unique<T>(std::forward<ArgsT>(args)...).release();
        },
        [&](void *object) {
          *reinterpret_cast<T *>(object) = T(std::forward<ArgsT>(args)...);
        });

    return *static_cast<T *>(result);
  }

  template <typename T, typename BuilderFn>
  T &get(BuilderFn &&builder)
    requires requires { T(std::forward<BuilderFn>(builder)()); }
  {
    void *result = getImpl(
        rx::TypeId::get<T>(), getDeleter<T>(),
        [&] {
          return std::make_unique<T>(std::forward<BuilderFn>(builder)())
              .release();
        },
        [&](void *object) {
          *reinterpret_cast<T *>(object) = std::forward<BuilderFn>(builder)();
        });

    return *static_cast<T *>(result);
  }

private:
  template <typename T> static void (*getDeleter())(void *) {
    return +[](void *data) { delete static_cast<T *>(data); };
  }

  void *getImpl(rx::TypeId typeId, void (*deleter)(void *),
                rx::FunctionRef<void *()> constructor,
                rx::FunctionRef<void(void *)> placementConstructor) {
    auto [it, inserted] = mStorage.emplace(typeId, getNullPointer());

    if (inserted) {
      it->second.object =
          std::unique_ptr<void, void (*)(void *)>(constructor(), deleter);
    } else if (it->second.invalid) {
      placementConstructor(it->second.object.get());
      it->second.invalid = false;
    }

    return it->second.object.get();
  }
  static constexpr std::unique_ptr<void, void (*)(void *)> getNullPointer() {
    return {nullptr, [](void *) {}};
  }

  struct Entry {
    std::unique_ptr<void, void (*)(void *)> object;
    bool invalid = false;
  };

  std::map<rx::TypeId, Entry> mStorage;
};

struct PostDomTree : graph::DomTree<ir::Value> {
  PostDomTree() = default;
  PostDomTree(graph::DomTree<ir::Value> &&other)
      : graph::DomTree<ir::Value>::DomTree(std::move(other)) {}
  PostDomTree(CFG &cfg, ir::Value root)
      : PostDomTree(buildPostDomTree(cfg, root)) {}
};

struct DomTree : graph::DomTree<ir::Value> {
  DomTree() = default;
  DomTree(graph::DomTree<ir::Value> &&other)
      : graph::DomTree<ir::Value>::DomTree(std::move(other)) {}
  DomTree(CFG &cfg, ir::Value root = nullptr)
      : DomTree(buildDomTree(cfg, root)) {}
};

template <typename T, std::size_t> struct Tag : T {
  using T::T;
  using T::operator=;

  Tag(T &&other) : T(std::move(other)) {}
  Tag(const T &other) : T(other) {}

  Tag &operator=(T &&other) {
    T::operator=(std::move(other));
    return *this;
  }
  Tag &operator=(const T &other) {
    T::operator=(other);
    return *this;
  }
};

struct Construct {
  Construct *parent;
  std::forward_list<Construct> children;
  ir::Value header;
  ir::Value merge;
  ir::Value loopBody;
  ir::Value loopContinue;
  AnalysisStorage analysis;

  static std::unique_ptr<Construct> createRoot(ir::RegionLike region,
                                               ir::Value merge) {
    auto result = std::make_unique<Construct>();
    auto &cfg =
        result->analysis.get<CFG>([&] { return buildCFG(region.getFirst()); });
    result->header = cfg.getEntryLabel();
    result->merge = merge;
    return result;
  }

  Construct *createChild(ir::Value header, ir::Value merge) {
    auto &result = children.emplace_front();
    result.parent = this;
    result.header = header;
    result.merge = merge;
    return &result;
  }

  Construct *createChild(ir::Value header, ir::Value merge,
                         ir::Value loopContinue, ir::Value loopBody) {
    auto &result = children.emplace_front();
    result.parent = this;
    result.header = header;
    result.merge = merge;
    result.loopContinue = loopContinue;
    result.loopBody = loopBody;
    return &result;
  }

  Construct createTemporaryChild(ir::Value header, ir::Value merge) {
    Construct result;
    result.parent = this;
    result.header = header;
    result.merge = merge;
    return result;
  }

  CFG &getCfg() {
    return analysis.get<CFG>([this] {
      if (parent != nullptr) {
        return parent->getCfg().buildView(
            header,
            &parent->getPostDomTree(),
            {header, merge});
      }

      return buildCFG(header);
    });
  }

  CFG &getCfgWithoutContinue() {
    if (loopContinue == nullptr) {
      return getCfg();
    }

    return analysis.get<Tag<CFG, kWithoutContinue>>([this] {
      if (parent != nullptr) {
        return parent->getCfg().buildView(
            header,
            &parent->getPostDomTree(),
            {header, merge}, loopContinue);
      }

      return buildCFG(header, {}, loopContinue);
    });
  }

  DomTree &getDomTree() { return analysis.get<DomTree>(getCfg(), header); }
  PostDomTree &getPostDomTree() {
    return analysis.get<PostDomTree>(getCfg(), merge);
  }
  BackEdgeStorage &getBackEdgeStorage() {
    return analysis.get<BackEdgeStorage>(getCfg());
  }
  BackEdgeStorage &getBackEdgeWithoutContinueStorage() {
    if (loopContinue == nullptr) {
      return getBackEdgeStorage();
    }
    return analysis.get<Tag<BackEdgeStorage, kWithoutContinue>>(
        getCfgWithoutContinue());
  }
  auto getBackEdges(ir::Value node) { return getBackEdgeStorage().get(node); }
  auto getBackEdgesWithoutContinue(ir::Value node) {
    return getBackEdgeWithoutContinueStorage().get(node);
  }
  auto getBackEdges() { return getBackEdges(header); }
  void invalidate();
  void invalidateAll();

  bool isNull() const { return header == nullptr; }

  void removeLastChild() { children.pop_front(); }

private:
  enum {
    kWithoutContinue,
  };
};
} // namespace shader
