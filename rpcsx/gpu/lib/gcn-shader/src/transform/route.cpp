
#include "transform/route.hpp"
#include "ir/Block.hpp"
#include "transform/merge.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include <functional>
#include <iostream>
#include <sstream>
#include <rx/die.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

// Data structures for route block creation
struct RouteBlockData {
  std::unordered_map<ir::Block, std::unordered_set<unsigned>> fromSuccessors;
  std::unordered_map<ir::Block, std::unordered_set<ir::Block>> toPredecessors;
  std::unordered_map<ir::Block, std::unordered_set<ir::Block>> toAllPredecessors;
  std::unordered_set<ir::Block> patchPredecessors;
};

// Helper function to get block name or generate one
static std::string getBlockName(spv::Context& context, ir::Block block) {
  auto label = block.getFirst();
  auto name = context.ns.tryGetNameOf(label);
  return name.empty() 
    ? "unnamed_" + std::to_string((std::uint32_t)label.getInstId()) 
    : std::string(name);
}

// Log detailed information about phi nodes and their predecessors
static void logPhiPredecessorsMismatch(spv::Context& context, ir::Block to, ir::Instruction firstInst) {
  // Get block label and name
  auto blockName = getBlockName(context, to);
  auto predsCount = getPredecessors(to).size();

  for (auto phi = firstInst; phi && (phi == ir::spv::OpPhi); phi = phi.getNext()) {
    std::cerr << "[DEBUG] Block '" << blockName << "' (ID: " << (std::uint32_t)to.getInstId() << ") has " << predsCount << " predecessors";
    
    // Get number of incoming blocks from phi node
    auto incomingCount = phi.getOperandCount() / 2;

    // Log mismatch if counts differ
    if (incomingCount != predsCount) {
      std::cerr << "\n  Phi ID: " << (std::uint32_t)phi.getInstId() << ", incoming blocks: " << incomingCount;
      std::cerr << " *** MISMATCH! Expected: " << predsCount << " ***\n\n";

      // Detailed phi node information
      std::cerr << "  Phi: ";
      phi.print(std::cerr, context.ns);
      std::cerr << "\n";

      // Print detailed incoming blocks
      std::stringstream phiOperands;
      auto opts = PrintOptions().nextLevel();
      phiOperands << "  Value-Blocks: [\n";

      for (std::size_t i = 1; i < phi.getOperandCount(); i += 2) {
        auto value = phi.getOperand(i + 0).getAsValue();
        auto block = phi.getOperand(i + 1).getAsValue().staticCast<ir::Block>();

        phiOperands << "    ";
        value.print(phiOperands, context.ns, opts.nextLevel());
        phiOperands << "\n    ";
        block.print(phiOperands, context.ns, opts.nextLevel());
        phiOperands << ",\n\n";
      }

      auto str = phiOperands.str();
      if (str.size() >= 3) {
        str.pop_back();
        str.pop_back();
        str.pop_back();
      }

      std::cerr << str << "]\n";
    }
    else {
      std::cerr << " and matching incoming blocks\n";
    }
  }
}

// Analyze edges and build routing data structures
static RouteBlockData analyzeEdges(spv::Context &context, const std::vector<Edge> &edges) {
  RouteBlockData data;
  std::unordered_set<ir::Block> routePredecessors;

  for (auto edge : edges) {
    if (!routePredecessors.insert(edge.from()).second) {
      data.patchPredecessors.insert(edge.from());
    }

    data.toPredecessors[edge.to()].emplace(edge.from());
    data.fromSuccessors[edge.from()].emplace(edge.operandIndex());
  }

  for (auto &[to, preds] : data.toPredecessors) {
    data.toAllPredecessors[to] = getPredecessors(to);
  }

  // Debug logging for mismatches
  for (auto& [to, _] : data.toPredecessors) {
    logPhiPredecessorsMismatch(context, to, ir::Block(to).getFirst());
  }

  return data;
}

// Create route block with appropriate phi node
static std::pair<ir::Block, ir::Value> createRouteBlockWithPhi(
    spv::Context &context, ir::InsertionPoint insertPoint, 
    ir::Location loc, size_t predsCount) {
  auto route = Builder::create(context, insertPoint).createBlock(loc);
  ir::Value routePhi;

  if (predsCount > 1) {
    routePhi = Builder::createPrepend(context, route)
                   .createSpvPhi(loc, predsCount == 2
                                          ? context.getTypeBool()
                                          : context.getTypeUInt32());
  }

  return {route, routePhi};
}

// Create terminator based on number of successors
static std::unordered_map<ir::Value, std::uint32_t> createRouteTerminator(
    spv::Context &context, ir::Block route, ir::Value routePhi,
    ir::Location loc,
    const std::unordered_map<ir::Block, std::unordered_set<ir::Block>>
        &toPreds) {
  std::unordered_map<ir::Value, std::uint32_t> successorToId;

  if (toPreds.size() == 1) {
    // Single successor: unconditional branch
    Builder::createAppend(context, route)
        .createSpvBranch(loc, toPreds.begin()->first);
  } else if (toPreds.size() == 2) {
    // Two successors: conditional branch
    auto it = toPreds.begin();
    auto firstSuccessor = it->first;
    auto secondSuccessor = (++it)->first;

    Builder::createAppend(context, route)
        .createSpvBranchConditional(loc, routePhi, firstSuccessor,
                                    secondSuccessor);
  } else {
    // Multiple successors: switch statement
    auto routeSwitch = Builder::createAppend(context, route)
                           .createSpvSwitch(loc, routePhi, toPreds.begin()->first);

    successorToId.reserve(toPreds.size());

    for (std::uint32_t id = 0; auto &[succ, pred] : toPreds) {
      if (id) {
        routeSwitch.addOperand(id);
        routeSwitch.addOperand(succ);
      }
      successorToId[succ] = id++;
    }
  }

  return successorToId;
}

// Get successor ID based on routing strategy
static ir::Value getSuccessorIdValue(
    spv::Context &context, ir::Block successor,
    const std::unordered_map<ir::Block, std::unordered_set<ir::Block>>
        &toPreds,
    const std::unordered_map<ir::Value, std::uint32_t> &successorToId) {
  if (toPreds.size() == 2) {
    return context.getBool(successor == toPreds.begin()->first);
  }
  return context.imm32(successorToId.at(successor));
}

// Process single predecessor block that needs patching
static void patchPredecessorBlock(
    spv::Context &context, ir::Block patchBlock, ir::Block route,
    ir::Value routePhi, const RouteBlockData &data,
    const std::unordered_map<ir::Block, std::unordered_set<ir::Block>> &toPreds,
    const std::function<ir::Value(ir::Block)> &getSuccessorId) {
  
  auto predSuccessors = getAllSuccessors(patchBlock);
  auto terminator = getTerminator(patchBlock);
  auto &routeSuccessors = data.fromSuccessors.at(patchBlock);

  int keepSuccessors = predSuccessors.size() - routeSuccessors.size();

  assert(keepSuccessors >= 0);
  assert(terminator == ir::spv::OpSwitch ||
         terminator == ir::spv::OpBranchConditional);

  auto cond = terminator.getOperand(0).getAsValue();
  auto condType = cond.getOperand(0).getAsValue();
  std::map<ir::Operand, ir::Block> condValueToSucc;
  ir::Block defaultSucc;

  if (keepSuccessors == 0) {
    // we are going to replace all successors of this block, create direct
    // jump to route block
    Builder::createInsertAfter(context, terminator)
        .createSpvBranch(terminator.getLocation(), route);

    if (terminator == ir::spv::OpBranchConditional) {
      condValueToSucc[context.getTrue()] =
          terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
      condValueToSucc[context.getFalse()] =
          terminator.getOperand(2).getAsValue().staticCast<ir::Block>();
    } else if (terminator == ir::spv::OpSwitch) {
      defaultSucc =
          terminator.getOperand(1).getAsValue().staticCast<ir::Block>();

      for (int i = 2, end = terminator.getOperandCount(); i < end; i += 2) {
        condValueToSucc[terminator.getOperand(i)] =
            terminator.getOperand(i + 1).getAsValue().staticCast<ir::Block>();
      }
    }
  } else if (terminator == ir::spv::OpSwitch) {
    if (routeSuccessors.contains(1)) {
      defaultSucc =
          terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
    }

    bool shouldReplaceDefault = defaultSucc != nullptr;

    for (int i = 2, id = 2, end = terminator.getOperandCount(); i < end;
         id += 2) {
      if (routeSuccessors.contains(id + 1)) {
        if (shouldReplaceDefault) {
          auto value = terminator.eraseOperand(i);
          auto successor = terminator.eraseOperand(i);

          condValueToSucc[value] =
              successor.getAsValue().staticCast<ir::Block>();

          continue;
        }

        condValueToSucc[terminator.getOperand(i)] =
            terminator.getOperand(i + 1).getAsValue().staticCast<ir::Block>();

        terminator.replaceOperand(i + 1, route);
      }

      i += 2;
    }

    if (shouldReplaceDefault) {
      terminator.replaceOperand(1, route);
    }
  } else {
    if (routeSuccessors.contains(1)) {
      condValueToSucc[context.getTrue()] =
          terminator.getOperand(1).getAsValue().staticCast<ir::Block>();
      terminator.replaceOperand(1, route);
    } else {
      assert(routeSuccessors.contains(2));
      condValueToSucc[context.getFalse()] =
          terminator.getOperand(2).getAsValue().staticCast<ir::Block>();
      terminator.replaceOperand(2, route);
    }
  }

  if (routePhi) {
    auto boolType = context.getTypeBool();
    auto builder = Builder::createInsertBefore(context, terminator);

    ir::Value selector;

    if (defaultSucc) {
      selector = getSuccessorId(defaultSucc);
    }

    auto selectorType =
        toPreds.size() == 2 ? boolType : context.getTypeUInt32();
    for (auto &[value, to] : condValueToSucc) {
      if (!selector) {
        selector = getSuccessorId(to);
      } else {
        auto valueId = value.getAsValue();
        if (!valueId) {
          valueId = context.imm32(*value.getAsInt32());
        }

        ir::Value selectionCond;

        if (condType == boolType) {
          selectionCond = builder.createSpvLogicalEqual(
              terminator.getLocation(), boolType, cond, valueId);
        } else {
          selectionCond = builder.createSpvIEqual(terminator.getLocation(),
                                                  boolType, cond, valueId);
        }
        selector = builder.createSpvSelect(terminator.getLocation(),
                                           selectorType, selectionCond,
                                           getSuccessorId(to), selector);
      }
    }

    routePhi.addOperand(selector);
    routePhi.addOperand(patchBlock);
  }

  if (keepSuccessors == 0) {
    terminator.remove();
  }
}

// Move all phi nodes from target to route block
static void moveAllPhiNodes(spv::Context &context, ir::Block to, ir::Block route,
                            const std::unordered_set<ir::Block> &preds,
                            const std::vector<Edge> &edges) {
  for (auto phi : ir::range(ir::Block(to).getFirst())) {
    if (phi != ir::spv::OpPhi) {
      break;
    }

    phi.erase();
    route.prependChild(phi);

    if (preds.size() != edges.size()) {
      // route block has additional edges. add dummy nodes to phi, this
      // block not reachable from new predecessors anyway

      auto undef = context.getUndef(phi.getOperand(0).getAsValue());

      for (auto edge : edges) {
        if (!preds.contains(edge.from())) {
          phi.addOperand(undef);
          phi.addOperand(edge.from());
        }
      }
    }
  }
}

// Update phi nodes for single predecessor
static void updatePhiNodesForSinglePred(ir::Block to, ir::Block pred,
                                        ir::Block route) {
  for (auto phi : ir::range(ir::Block(to).getFirst())) {
    if (phi != ir::spv::OpPhi) {
      break;
    }

    for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
      auto label = phi.getOperand(i).getAsValue();

      if (label == pred) {
        phi.replaceOperand(i, route);
      }
    }
  }
}

// Update phi nodes for partial predecessor replacement
static void updatePhiNodesPartial(spv::Context &context, ir::Block to,
                                  ir::Block route,
                                  const std::unordered_set<ir::Block> &preds,
                                  const std::vector<Edge> &edges) {
  for (auto phi : ir::range(ir::Block(to).getFirst())) {
    if (phi != ir::spv::OpPhi) {
      break;
    }

    auto newPhi =
        Builder::createPrepend(context, route)
            .createSpvPhi(phi.getLocation(), phi.getOperand(0).getAsValue());

    for (std::size_t i = 1; i < phi.getOperandCount();) {
      auto label = phi.getOperand(i + 1).getAsValue().cast<ir::Block>();

      if (preds.contains(label)) {
        newPhi.addOperand(phi.eraseOperand(i));
        newPhi.addOperand(phi.eraseOperand(i));
      } else {
        i += 2;
      }
    }

    phi.addOperand(newPhi);
    phi.addOperand(route);

    if (preds.size() != edges.size()) {
      // merge block has additional edges. add dummy nodes to phi, this
      // block not reachable from new blocks

      auto dummyValue = phi.getOperand(1).getAsValue();

      for (auto edge : edges) {
        if (!preds.contains(edge.from())) {
          phi.addOperand(dummyValue);
          phi.addOperand(edge.from());
        }
      }
    }
  }
}

// Process all target blocks and update their phi nodes
static void processTargetBlocks(
    spv::Context &context, ir::Block route, ir::Value routePhi,
    const RouteBlockData &data,
    const std::unordered_map<ir::Block, std::unordered_set<ir::Block>> &toPreds,
    const std::vector<Edge> &edges,
    const std::function<ir::Value(ir::Block)> &getSuccessorId) {
  
  for (auto &[to, preds] : toPreds) {
    if (toPreds.size() > 1) {
      auto successorId = getSuccessorId(to);

      for (auto from : preds) {
        // branches already resolved
        if (data.patchPredecessors.contains(from)) {
          continue;
        }

        routePhi.addOperand(successorId);
        routePhi.addOperand(from);
      }
    }

    for (auto from : preds) {
      if (data.patchPredecessors.contains(from)) {
        continue;
      }

      replaceTerminatorTarget(getTerminator(from), to, route);
    }

    if (data.toAllPredecessors.at(to).size() == preds.size()) {
      // all predecessors will be replaced, move phi nodes
      moveAllPhiNodes(context, to, route, preds, edges);
      continue;
    }

    if (preds.size() == 1) {
      auto pred = *preds.begin();
      updatePhiNodesForSinglePred(to, pred, route);
      continue;
    }

    // partial predecessors replacement, update PHIs
    updatePhiNodesPartial(context, to, route, preds, edges);
  }
}

// Main function
ir::Block shader::transform::createRouteBlock(spv::Context &context,
                                  ir::InsertionPoint insertPoint,
                                  const std::vector<Edge> &edges) {
  auto loc = context.getUnknownLocation();

  rx::dieIf(edges.empty(), "createRouteBlock: unexpected edges count");

  // Step 1: Analyze edges and build data structures
  auto data = analyzeEdges(context, edges);

  // Step 2: Handle simple case - single target block
  if (data.toPredecessors.size() == 1) {
    auto &[to, preds] = *data.toPredecessors.begin();
    return createMergeBlock(context, insertPoint, preds, to);
  }

  // Step 3: Create route block and phi node
  auto [route, routePhi] = createRouteBlockWithPhi(context, insertPoint,
                                                    loc, data.toPredecessors.size());

  // Step 4: Create appropriate terminator (branch/conditional/switch)
  auto successorToId = createRouteTerminator(context, route, routePhi,
                                             loc, data.toPredecessors);

  // Step 5: Create lambda for getting successor IDs
  auto getSuccessorId = [&](ir::Block successor) {
    return getSuccessorIdValue(context, successor, data.toPredecessors, successorToId);
  };

  // Step 6: Patch predecessor blocks that have multiple routes
  for (auto patchBlock : data.patchPredecessors) {
    patchPredecessorBlock(context, patchBlock, route, routePhi, data,
                         data.toPredecessors, getSuccessorId);
  }

  // Step 7: Process target blocks and update phi nodes
  processTargetBlocks(context, route, routePhi, data, data.toPredecessors, edges,
                     getSuccessorId);

  return route;
}
