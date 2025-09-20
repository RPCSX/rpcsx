#pragma once
#include "SpvConverter.hpp"
#include "ir.hpp"

namespace shader {
void structurizeCfg(spv::Context &context, ir::RegionLike region);
}
