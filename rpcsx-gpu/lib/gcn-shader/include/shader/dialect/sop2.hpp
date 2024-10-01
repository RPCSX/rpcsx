#pragma once
#include "../ir.hpp"

namespace shader::ir::sop2 {
enum Op {
  ADD_U32,
  SUB_U32,
  ADD_I32,
  SUB_I32,
  ADDC_U32,
  SUBB_U32,
  MIN_I32,
  MIN_U32,
  MAX_I32,
  MAX_U32,
  CSELECT_B32,
  CSELECT_B64,
  AND_B32 = 14,
  AND_B64,
  OR_B32,
  OR_B64,
  XOR_B32,
  XOR_B64,
  ANDN2_B32,
  ANDN2_B64,
  ORN2_B32,
  ORN2_B64,
  NAND_B32,
  NAND_B64,
  NOR_B32,
  NOR_B64,
  XNOR_B32,
  XNOR_B64,
  LSHL_B32,
  LSHL_B64,
  LSHR_B32,
  LSHR_B64,
  ASHR_I32,
  ASHR_I64,
  BFM_B32,
  BFM_B64,
  MUL_I32,
  BFE_U32,
  BFE_I32,
  BFE_U64,
  BFE_I64,
  CBRANCH_G_FORK,
  ABSDIFF_I32,
  LSHL1_ADD_U32,
  LSHL2_ADD_U32,
  LSHL3_ADD_U32,
  LSHL4_ADD_U32,
  PACK_LL_B32_B16,
  PACK_LH_B32_B16,
  PACK_HH_B32_B16,
  MUL_HI_U32,
  MUL_HI_I32,

  OpCount
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case ADD_U32:
    return "s_add_u32";
  case SUB_U32:
    return "s_sub_u32";
  case ADD_I32:
    return "s_add_i32";
  case SUB_I32:
    return "s_sub_i32";
  case ADDC_U32:
    return "s_addc_u32";
  case SUBB_U32:
    return "s_subb_u32";
  case MIN_I32:
    return "s_min_i32";
  case MIN_U32:
    return "s_min_u32";
  case MAX_I32:
    return "s_max_i32";
  case MAX_U32:
    return "s_max_u32";
  case CSELECT_B32:
    return "s_cselect_b32";
  case CSELECT_B64:
    return "s_cselect_b64";
  case AND_B32:
    return "s_and_b32";
  case AND_B64:
    return "s_and_b64";
  case OR_B32:
    return "s_or_b32";
  case OR_B64:
    return "s_or_b64";
  case XOR_B32:
    return "s_xor_b32";
  case XOR_B64:
    return "s_xor_b64";
  case ANDN2_B32:
    return "s_andn2_b32";
  case ANDN2_B64:
    return "s_andn2_b64";
  case ORN2_B32:
    return "s_orn2_b32";
  case ORN2_B64:
    return "s_orn2_b64";
  case NAND_B32:
    return "s_nand_b32";
  case NAND_B64:
    return "s_nand_b64";
  case NOR_B32:
    return "s_nor_b32";
  case NOR_B64:
    return "s_nor_b64";
  case XNOR_B32:
    return "s_xnor_b32";
  case XNOR_B64:
    return "s_xnor_b64";
  case LSHL_B32:
    return "s_lshl_b32";
  case LSHL_B64:
    return "s_lshl_b64";
  case LSHR_B32:
    return "s_lshr_b32";
  case LSHR_B64:
    return "s_lshr_b64";
  case ASHR_I32:
    return "s_ashr_i32";
  case ASHR_I64:
    return "s_ashr_i64";
  case BFM_B32:
    return "s_bfm_b32";
  case BFM_B64:
    return "s_bfm_b64";
  case MUL_I32:
    return "s_mul_i32";
  case BFE_U32:
    return "s_bfe_u32";
  case BFE_I32:
    return "s_bfe_i32";
  case BFE_U64:
    return "s_bfe_u64";
  case BFE_I64:
    return "s_bfe_i64";
  case CBRANCH_G_FORK:
    return "s_cbranch_g_fork";
  case ABSDIFF_I32:
    return "s_absdiff_i32";
  case LSHL1_ADD_U32:
    return "s_lshl1_add_u32";
  case LSHL2_ADD_U32:
    return "s_lshl2_add_u32";
  case LSHL3_ADD_U32:
    return "s_lshl3_add_u32";
  case LSHL4_ADD_U32:
    return "s_lshl4_add_u32";
  case PACK_LL_B32_B16:
    return "s_pack_ll_b32_b16";
  case PACK_LH_B32_B16:
    return "s_pack_lh_b32_b16";
  case PACK_HH_B32_B16:
    return "s_pack_hh_b32_b16";
  case MUL_HI_U32:
    return "s_mul_hi_u32";
  case MUL_HI_I32:
    return "s_mul_hi_i32";
  }
  return nullptr;
}
} // namespace shader::ir::sop2
