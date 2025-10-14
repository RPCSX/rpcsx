#pragma once

#include "analyze.hpp"
#include "dialect.hpp"
#include "ir.hpp"

namespace shader::transform {
void replaceTerminatorTarget(ir::Instruction terminator,
                             int operandIndex,
                             ir::Value newTarget);

bool replaceTerminatorTarget(ir::Instruction terminator, 
                             ir::Value oldTarget, 
                             ir::Value newTarget); 
}