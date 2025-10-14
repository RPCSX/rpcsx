#include "SpvConverter.hpp"
#include "transform/merge.hpp"
#include "analyze.hpp"
#include "transform/replace.hpp"
#include "dialect.hpp"
#include <rx/die.hpp>

using namespace shader;
using namespace shader::transform;

using Builder = ir::Builder<ir::builtin::Builder, ir::spv::Builder>;

ir::Block shader::transform::createMergeBlock(spv::Context &context,
                                  ir::InsertionPoint insertPoint,
                                  const std::unordered_set<ir::Block> &preds,
                                  ir::Block to) {
  rx::dieIf(preds.empty(), "createMergeBlock: unexpected edges count");

  auto loc = to.getLocation();

  auto mergeBlock = Builder::create(context, insertPoint).createBlock(loc);
  Builder::createAppend(context, mergeBlock).createSpvBranch(loc, to);

  if (preds.size() == getPredecessorCount(to)) {
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      phi.erase();
      mergeBlock.prependChild(phi);
    }
  } else if (preds.size() == 1) {
    auto pred = *preds.begin();
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      for (std::size_t i = 2; i < phi.getOperandCount(); i += 2) {
        if (phi.getOperand(i) == pred) {
          phi.replaceOperand(i, mergeBlock);
        }
      }
    }
  } else {
    for (auto phi : ir::range(to.getFirst())) {
      if (phi != ir::spv::OpPhi) {
        break;
      }

      auto newPhi =
          Builder::createPrepend(context, mergeBlock)
              .createSpvPhi(phi.getLocation(), phi.getOperand(0).getAsValue());

      for (std::size_t i = 1; i < phi.getOperandCount();) {
        // auto value = phi.getOperand(i).getAsValue();
        auto label = phi.getOperand(i + 1).getAsValue().staticCast<ir::Block>();
        if (preds.contains(label)) {
          newPhi.addOperand(phi.eraseOperand(i));
          newPhi.addOperand(phi.eraseOperand(i));
        } else {
          i += 2;
        }
      }

      phi.addOperand(newPhi);
      phi.addOperand(mergeBlock);
    }
  }

  for (auto pred : preds) {
    replaceTerminatorTarget(getTerminator(pred), to, mergeBlock);
  }

  return mergeBlock;
}                    