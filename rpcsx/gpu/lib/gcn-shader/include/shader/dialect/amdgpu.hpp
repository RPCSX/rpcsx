#pragma once

namespace shader::ir::amdgpu {

enum Op {
  EXEC_TEST,
  BRANCH,
  IMM,
  USER_SGPR,
  VBUFFER,
  SAMPLER,
  TBUFFER,
  POINTER,
  OMOD,
  NEG_ABS,
  PS_INPUT_VGPR,
  PS_COMP_SWAP,
  VS_GET_INDEX,
  CS_INPUT_SGPR,
  CS_SET_INITIAL_EXEC,
  CS_SET_THREAD_ID,
  RESOURCE_PHI,

  OpCount,
};

inline const char *getInstructionName(unsigned op) {
  switch (op) {
  case EXEC_TEST:
    return "exec_test";
  case BRANCH:
    return "branch";
  case IMM:
    return "imm";
  case USER_SGPR:
    return "user_sgpr";
  case VBUFFER:
    return "vbuffer";
  case SAMPLER:
    return "sampler";
  case TBUFFER:
    return "tbuffer";
  case POINTER:
    return "pointer";
  case OMOD:
    return "omod";
  case NEG_ABS:
    return "neg_abs";
  case PS_INPUT_VGPR:
    return "ps_input_vgpr";
  case PS_COMP_SWAP:
    return "ps_comp_swap";
  case VS_GET_INDEX:
    return "vs_get_index";
  case CS_INPUT_SGPR:
    return "cs_input_sgpr";
  case CS_SET_INITIAL_EXEC:
    return "cs_set_initial_exec";
  case CS_SET_THREAD_ID:
    return "cs_set_thread_id";
  case RESOURCE_PHI:
    return "resource_phi";
  }
  return nullptr;
}
} // namespace shader::ir::amdgpu
