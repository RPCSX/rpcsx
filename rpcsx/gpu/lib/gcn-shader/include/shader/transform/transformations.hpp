#pragma once

#include "SpvConverter.hpp"
#include "ir.hpp"

namespace shader::transform {
ir::Value toCanonicalRegion(spv::Context &context, ir::RegionLike region);
void toCf(spv::Context &context, ir::RegionLike region);
void toFlat(spv::Context &context, ir::RegionLike region);
void canonicalizeSwitchSelectionConstructs(spv::Context &context,
                                           ir::RegionLike root);
} // namespace shader::transform
