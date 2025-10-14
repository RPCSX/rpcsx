#pragma once

#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include "ir.hpp"


namespace shader::transform {
ir::Block createMergeBlock(spv::Context &context,
                           ir::InsertionPoint insertPoint,
                           const std::unordered_set<ir::Block> &preds,
                           ir::Block to);
}