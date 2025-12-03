#pragma once

#include "SpvConverter.hpp"
#include "ir.hpp"

namespace shader::transform {
ir::Value transformToCanonicalRegion(spv::Context &context,
                                     ir::RegionLike region);
void transformToCf(spv::Context &context, ir::RegionLike region);
void transformToFlat(spv::Context &context, ir::RegionLike region);
} // namespace shader::transform
