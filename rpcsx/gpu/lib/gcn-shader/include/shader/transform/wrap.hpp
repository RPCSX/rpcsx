#pragma once

#include "SpvConverter.hpp"

namespace shader::transform {
void wrapLoopConstructs(spv::Context &context, ir::RegionLike root);
void wrapSelectionConstructs(spv::Context &context, ir::RegionLike root);
} // namespace shader::transform
