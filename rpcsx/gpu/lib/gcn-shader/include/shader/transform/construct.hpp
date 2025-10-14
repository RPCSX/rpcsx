#pragma once

#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "ir.hpp"

namespace shader::transform {

bool isConstruct(ir::Instruction block);
bool isParentConstruct(ir::RegionLike parent, 
                       ir::RegionLike construct);

ir::Block getConstructOf(ir::Instruction inst);
ir::Block getConstructMergeBlock(ir::Block block);

ir::SelectionConstruct createSelectionConstruct(spv::Context &context, 
                                                ir::RegionLike parentConstruct,
                                                const std::unordered_set<ir::Block> &components,
                                                ir::Block header, ir::Block merge);

ir::LoopConstruct createLoopConstruct(spv::Context &context,
                                      ir::RegionLike parentConstruct,
                                      ir::Block header,
                                      ir::Block latch,
                                      ir::Block cont,
                                      ir::Block merge,
                                      const std::unordered_set<shader::ir::Block> &scc);                        
}