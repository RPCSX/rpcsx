#pragma once

namespace shader::ir::exp {
enum Op {
  EXP = 0,

  OpCount
};

inline const char *getInstructionName(unsigned) { return "exp"; }
} // namespace shader::ir::exp
