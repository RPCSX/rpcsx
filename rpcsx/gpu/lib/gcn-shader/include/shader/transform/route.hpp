#pragma once

#include "Edge.hpp"
#include "SpvConverter.hpp"
#include "ir.hpp"

namespace shader::transform {
ir::Block createRouteBlock(spv::Context &context,
                           ir::InsertionPoint insertPoint,
                           const std::vector<Edge> &edges);
}