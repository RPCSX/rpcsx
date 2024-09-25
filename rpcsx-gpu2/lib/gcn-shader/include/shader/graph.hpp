#pragma once

#include <map>
#include <vector>

namespace graph {
template <typename BasicBlockPtrT> class DomTree {
public:
  struct Node {
    BasicBlockPtrT block = nullptr;
    Node *immDom = nullptr;
    unsigned dfsNumIn = ~0;
    unsigned dfsNumOut = ~0;
    unsigned level = 0;
    std::vector<Node *> children;

    bool isLeaf() const { return children.empty(); }

    bool dominatedBy(const Node *other) const {
      return this->dfsNumIn >= other->dfsNumIn &&
             this->dfsNumOut <= other->dfsNumOut;
    }
  };

private:
  std::map<BasicBlockPtrT, Node> bbToNodes;
  Node *rootNode = nullptr;

public:
  Node *getNode(BasicBlockPtrT bb) {
    auto it = bbToNodes.find(bb);
    if (it != bbToNodes.end()) {
      return &it->second;
    }

    return nullptr;
  }

  Node *createChild(BasicBlockPtrT bb, Node *parent) {
    auto &child = bbToNodes[bb];
    child.block = bb;
    child.immDom = parent;
    child.level = parent->level + 1;
    parent->children.push_back(&child);
    return &child;
  }

  Node *createRoot(BasicBlockPtrT bb) {
    auto &root = bbToNodes[bb];
    rootNode = &root;
    root.block = bb;
    return rootNode;
  }

  Node *getRootNode() { return rootNode; }

  void updateDFSNumbers() {
    std::vector<std::pair<Node *, typename std::vector<Node *>::iterator>>
        workStack;

    auto root = getRootNode();
    if (!root)
      return;

    workStack.push_back({root, root->children.begin()});

    unsigned dfsNum = 0;
    root->dfsNumIn = dfsNum++;

    while (!workStack.empty()) {
      auto node = workStack.back().first;
      const auto childIt = workStack.back().second;

      if (childIt == node->children.end()) {
        node->dfsNumOut = dfsNum++;
        workStack.pop_back();
      } else {
        auto child = *childIt;
        ++workStack.back().second;

        workStack.push_back({child, child->children.begin()});
        child->dfsNumIn = dfsNum++;
      }
    }
  }

  bool dominates(Node *a, Node *b) {
    if (a == b || b->immDom == a) {
      return true;
    }

    if (a->immDom == b || a->level >= b->level) {
      return false;
    }

    return b->dominatedBy(a);
  }

  bool dominates(BasicBlockPtrT a, BasicBlockPtrT b) {
    return dominates(getNode(a), getNode(b));
  }

  BasicBlockPtrT getImmediateDominator(BasicBlockPtrT a) {
    auto immDom = getNode(a)->immDom;
    if (immDom) {
      return immDom->block;
    }
    return{};
  }

  bool isImmediateDominator(BasicBlockPtrT block, BasicBlockPtrT immDomBlock) {
    if (immDomBlock == nullptr) {
      return false;
    }

    return getImmediateDominator(immDomBlock) == block;
  }

  BasicBlockPtrT findNearestCommonDominator(BasicBlockPtrT a,
                                            BasicBlockPtrT b) {
    auto aNode = getNode(a);
    auto bNode = getNode(b);

    if (aNode == rootNode || bNode == rootNode) {
      return rootNode->block;
    }

    while (aNode != bNode) {
      if (aNode->level < bNode->level) {
        std::swap(aNode, bNode);
      }

      aNode = aNode->immDom;
    }

    return aNode->block;
  }
};

template <typename BasicBlockPtrT> class DomTreeBuilder {
  using DomTreeNode = typename DomTree<BasicBlockPtrT>::Node;

  struct NodeInfo {
    unsigned dfsNum = 0;
    unsigned parent = 0;
    unsigned semi = 0;
    BasicBlockPtrT label = nullptr;
    BasicBlockPtrT immDom = nullptr;
    std::vector<BasicBlockPtrT> revChildren;
  };

  std::vector<BasicBlockPtrT> indexToNode = {nullptr};
  std::map<BasicBlockPtrT, NodeInfo> nodeToInfo;

  template <typename WalkFn>
  void runDFS(BasicBlockPtrT root, const WalkFn &walk) {
    std::vector<BasicBlockPtrT> workList;
    workList.reserve(10);
    workList.push_back(root);
    unsigned index = 0;

    while (!workList.empty()) {
      auto bb = workList.back();
      workList.pop_back();

      auto &bbInfo = nodeToInfo[bb];

      if (bbInfo.dfsNum != 0) {
        continue;
      }

      bbInfo.dfsNum = bbInfo.semi = ++index;
      bbInfo.label = bb;
      indexToNode.push_back(bb);

      walk(bb, [&](BasicBlockPtrT successor) {
        auto it = nodeToInfo.find(successor);
        if (it != nodeToInfo.end() && it->second.dfsNum != 0) {
          if (successor != bb) {
            it->second.revChildren.push_back(bb);
          }

          return;
        }

        auto &succInfo = nodeToInfo[successor];
        workList.push_back(successor);
        succInfo.parent = index;
        succInfo.revChildren.push_back(bb);
      });
    }
  }

  void runSemiNCA() {
    const unsigned nextDFS = indexToNode.size();

    for (unsigned i = 1; i < nextDFS; ++i) {
      const BasicBlockPtrT node = indexToNode[i];
      auto &NodeInfo = nodeToInfo[node];
      NodeInfo.immDom = indexToNode[NodeInfo.parent];
    }

    std::vector<NodeInfo *> evalStack;
    evalStack.reserve(10);

    for (unsigned i = nextDFS - 1; i >= 2; --i) {
      BasicBlockPtrT node = indexToNode[i];
      auto &nodeInfo = nodeToInfo[node];

      nodeInfo.semi = nodeInfo.parent;
      for (const auto &child : nodeInfo.revChildren) {
        if (!nodeToInfo.contains(child)) {
          continue;
        }

        unsigned childSemi = nodeToInfo[eval(child, i + 1, evalStack)].semi;
        if (childSemi < nodeInfo.semi) {
          nodeInfo.semi = childSemi;
        }
      }
    }

    for (unsigned i = 2; i < nextDFS; ++i) {
      const BasicBlockPtrT node = indexToNode[i];
      auto &nodeInfo = nodeToInfo[node];
      const unsigned sDomNum = nodeToInfo[indexToNode[nodeInfo.semi]].dfsNum;
      BasicBlockPtrT immDom = nodeInfo.immDom;

      while (nodeToInfo[immDom].dfsNum > sDomNum) {
        immDom = nodeToInfo[immDom].immDom;
      }

      nodeInfo.immDom = immDom;
    }
  }

  BasicBlockPtrT eval(BasicBlockPtrT block, unsigned LastLinked,
                      std::vector<NodeInfo *> &stack) {
    NodeInfo *blockInfo = &nodeToInfo[block];
    if (blockInfo->parent < LastLinked)
      return blockInfo->label;

    do {
      stack.push_back(blockInfo);
      blockInfo = &nodeToInfo[indexToNode[blockInfo->parent]];
    } while (blockInfo->parent >= LastLinked);

    const NodeInfo *pInfo = blockInfo;
    const NodeInfo *pLabelInfo = &nodeToInfo[pInfo->label];
    do {
      blockInfo = stack.back();
      stack.pop_back();

      blockInfo->parent = pInfo->parent;
      const NodeInfo *labelInfo = &nodeToInfo[blockInfo->label];
      if (pLabelInfo->semi < labelInfo->semi) {
        blockInfo->label = pInfo->label;
      } else {
        pLabelInfo = labelInfo;
      }

      pInfo = blockInfo;
    } while (!stack.empty());
    return blockInfo->label;
  }

  DomTreeNode *getNodeForBlock(BasicBlockPtrT BB, DomTree<BasicBlockPtrT> &DT) {
    if (auto Node = DT.getNode(BB))
      return Node;

    BasicBlockPtrT IDom = getIDom(BB);
    auto IDomNode = getNodeForBlock(IDom, DT);

    return DT.createChild(BB, IDomNode);
  }

  BasicBlockPtrT getIDom(BasicBlockPtrT BB) const {
    auto InfoIt = nodeToInfo.find(BB);
    if (InfoIt == nodeToInfo.end())
      return nullptr;

    return InfoIt->second.immDom;
  }

public:
  template <typename WalkFn>
  DomTree<BasicBlockPtrT> build(BasicBlockPtrT root,
                                const WalkFn &walkSuccessors) {
    runDFS(root, walkSuccessors);
    runSemiNCA();

    DomTree<BasicBlockPtrT> domTree;
    domTree.createRoot(root);

    nodeToInfo[indexToNode[1]].immDom = root;

    for (size_t i = 1, e = indexToNode.size(); i != e; ++i) {
      BasicBlockPtrT node = indexToNode[i];

      if (domTree.getNode(node))
        continue;

      BasicBlockPtrT immDom = getIDom(node);

      auto immDomNode = getNodeForBlock(immDom, domTree);
      domTree.createChild(node, immDomNode);
    }

    domTree.updateDFSNumbers();
    return domTree;
  }
};

template <typename BasicBlockPtrT>
DomTree<BasicBlockPtrT> buildDomTree(BasicBlockPtrT root, auto &&walkSuccessors)
  requires requires(void (*cb)(BasicBlockPtrT)) { walkSuccessors(root, cb); }
{
  return DomTreeBuilder<BasicBlockPtrT>().build(root, walkSuccessors);
}
} // namespace graph
