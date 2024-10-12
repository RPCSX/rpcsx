#pragma once

namespace shader::ir::sopc {
enum Op {
  CMP_EQ_I32,
  CMP_LG_I32,
  CMP_GT_I32,
  CMP_GE_I32,
  CMP_LT_I32,
  CMP_LE_I32,
  CMP_EQ_U32,
  CMP_LG_U32,
  CMP_GT_U32,
  CMP_GE_U32,
  CMP_LT_U32,
  CMP_LE_U32,
  BITCMP0_B32,
  BITCMP1_B32,
  BITCMP0_B64,
  BITCMP1_B64,
  SETVSKIP,
  ILLEGALD,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case CMP_EQ_I32:
    return "s_cmp_eq_i32";
  case CMP_LG_I32:
    return "s_cmp_lg_i32";
  case CMP_GT_I32:
    return "s_cmp_gt_i32";
  case CMP_GE_I32:
    return "s_cmp_ge_i32";
  case CMP_LT_I32:
    return "s_cmp_lt_i32";
  case CMP_LE_I32:
    return "s_cmp_le_i32";
  case CMP_EQ_U32:
    return "s_cmp_eq_u32";
  case CMP_LG_U32:
    return "s_cmp_lg_u32";
  case CMP_GT_U32:
    return "s_cmp_gt_u32";
  case CMP_GE_U32:
    return "s_cmp_ge_u32";
  case CMP_LT_U32:
    return "s_cmp_lt_u32";
  case CMP_LE_U32:
    return "s_cmp_le_u32";
  case BITCMP0_B32:
    return "bitcmp0_b32";
  case BITCMP1_B32:
    return "bitcmp1_b32";
  case BITCMP0_B64:
    return "bitcmp0_b64";
  case BITCMP1_B64:
    return "bitcmp1_b64";
  case SETVSKIP:
    return "setvskip";
  case ILLEGALD:
    return "illegald";
  }
  return nullptr;
}
} // namespace shader::ir::sopc
