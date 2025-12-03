#include "transform/wrap.hpp"
#include "SpvConverter.hpp"
#include "dialect.hpp"
#include "rx/print.hpp"
#include "transform/Edge.hpp"
#include "transform/construct.hpp"
#include "transform/merge.hpp"
#include "transform/route.hpp"
#include <iostream>
#include <rx/die.hpp>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

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

static ir::Instruction skipPhis(ir::Instruction inst) {
  while (inst && inst == ir::spv::OpPhi) {
    inst = inst.getNext();
  }

  return inst;
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

void shader::transform::wrapLoopConstructs(spv::Context &context,
                                           ir::RegionLike root) {
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
    } else {
      rx::print(stderr, "infinity loop in the shader:");
      for (auto block : scc) {
        rx::print(stderr, " {}", context.ns.getNameOf(block));
      }
      rx::println("");
      root.print(std::cerr, context.ns);
      rx::die("infinity loop");
    }
  }
}

void shader::transform::wrapSelectionConstructs(spv::Context &context,
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
