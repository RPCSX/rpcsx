#pragma once

namespace shader::ir::sopk {
enum Op {
  MOVK_I32,
  CMOVK_I32 = 2,
  CMPK_EQ_I32,
  CMPK_LG_I32,
  CMPK_GT_I32,
  CMPK_GE_I32,
  CMPK_LT_I32,
  CMPK_LE_I32,
  CMPK_EQ_U32,
  CMPK_LG_U32,
  CMPK_GT_U32,
  CMPK_GE_U32,
  CMPK_LT_U32,
  CMPK_LE_U32,
  ADDK_I32,
  MULK_I32,
  CBRANCH_I_FORK,
  GETREG_B32,
  SETREG_B32,
  SETREG_IMM,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case MOVK_I32:
    return "s_movk_i32";
  case CMOVK_I32:
    return "s_cmovk_i32";
  case CMPK_EQ_I32:
    return "s_cmpk_eq_i32";
  case CMPK_LG_I32:
    return "s_cmpk_lg_i32";
  case CMPK_GT_I32:
    return "s_cmpk_gt_i32";
  case CMPK_GE_I32:
    return "s_cmpk_ge_i32";
  case CMPK_LT_I32:
    return "s_cmpk_lt_i32";
  case CMPK_LE_I32:
    return "s_cmpk_le_i32";
  case CMPK_EQ_U32:
    return "s_cmpk_eq_u32";
  case CMPK_LG_U32:
    return "s_cmpk_lg_u32";
  case CMPK_GT_U32:
    return "s_cmpk_gt_u32";
  case CMPK_GE_U32:
    return "s_cmpk_ge_u32";
  case CMPK_LT_U32:
    return "s_cmpk_lt_u32";
  case CMPK_LE_U32:
    return "s_cmpk_le_u32";
  case ADDK_I32:
    return "s_addk_i32";
  case MULK_I32:
    return "s_mulk_i32";
  case CBRANCH_I_FORK:
    return "s_cbranch_i_fork";
  case GETREG_B32:
    return "s_getreg_b32";
  case SETREG_B32:
    return "s_setreg_b32";
  case SETREG_IMM:
    return "s_setreg_imm";
  }
  return nullptr;
}
} // namespace shader::ir::sopk
