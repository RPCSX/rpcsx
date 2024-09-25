#pragma once
#include "ir/Context.hpp"
#include "ir/Region.hpp"

namespace shader {
bool optimize(ir::Context &context, ir::Region region);
}
