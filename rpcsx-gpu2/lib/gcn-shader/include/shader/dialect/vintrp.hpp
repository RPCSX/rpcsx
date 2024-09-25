#pragma once

namespace shader::ir::vintrp {
enum Op {
  P1_F32,
  P2_F32,
  MOV_F32,

  OpCount
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case P1_F32:
    return "v_interp_p1_f32";
  case P2_F32:
    return "v_interp_p2_f32";
  case MOV_F32:
    return "v_interp_mov_f32";
  }
  return nullptr;
}
} // namespace shader::ir::vintrp
