#pragma once

namespace shader::ir::sop1 {
enum Op {
  MOV_B32 = 3,
  MOV_B64,
  CMOV_B32,
  CMOV_B64,
  NOT_B32,
  NOT_B64,
  WQM_B32,
  WQM_B64,
  BREV_B32,
  BREV_B64,
  BCNT0_I32_B32,
  BCNT0_I32_B64,
  BCNT1_I32_B32,
  BCNT1_I32_B64,
  FF0_I32_B32,
  FF0_I32_B64,
  FF1_I32_B32,
  FF1_I32_B64,
  FLBIT_I32_B32,
  FLBIT_I32_B64,
  FLBIT_I32,
  FLBIT_I32_I64,
  SEXT_I32_I8,
  SEXT_I32_I16,
  BITSET0_B32,
  BITSET0_B64,
  BITSET1_B32,
  BITSET1_B64,
  GETPC_B64,
  SETPC_B64,
  SWAPPC_B64,
  AND_SAVEEXEC_B64 = 36,
  OR_SAVEEXEC_B64,
  XOR_SAVEEXEC_B64,
  ANDN2_SAVEEXEC_B64,
  ORN2_SAVEEXEC_B64,
  NAND_SAVEEXEC_B64,
  NOR_SAVEEXEC_B64,
  XNOR_SAVEEXEC_B64,
  QUADMASK_B32,
  QUADMASK_B64,
  MOVRELS_B32,
  MOVRELS_B64,
  MOVRELD_B32,
  MOVRELD_B64,
  CBRANCH_JOIN,
  ABS_I32 = 52,
  MOV_FED_B32,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case MOV_B32:
    return "s_mov_b32";
  case MOV_B64:
    return "s_mov_b64";
  case CMOV_B32:
    return "s_cmov_b32";
  case CMOV_B64:
    return "s_cmov_b64";
  case NOT_B32:
    return "s_not_b32";
  case NOT_B64:
    return "s_not_b64";
  case WQM_B32:
    return "s_wqm_b32";
  case WQM_B64:
    return "s_wqm_b64";
  case BREV_B32:
    return "s_brev_b32";
  case BREV_B64:
    return "s_brev_b64";
  case BCNT0_I32_B32:
    return "s_bcnt0_i32_b32";
  case BCNT0_I32_B64:
    return "s_bcnt0_i32_b64";
  case BCNT1_I32_B32:
    return "s_bcnt1_i32_b32";
  case BCNT1_I32_B64:
    return "s_bcnt1_i32_b64";
  case FF0_I32_B32:
    return "s_ff0_i32_b32";
  case FF0_I32_B64:
    return "s_ff0_i32_b64";
  case FF1_I32_B32:
    return "s_ff1_i32_b32";
  case FF1_I32_B64:
    return "s_ff1_i32_b64";
  case FLBIT_I32_B32:
    return "s_flbit_i32_b32";
  case FLBIT_I32_B64:
    return "s_flbit_i32_b64";
  case FLBIT_I32:
    return "s_flbit_i32";
  case FLBIT_I32_I64:
    return "s_flbit_i32_i64";
  case SEXT_I32_I8:
    return "s_sext_i32_i8";
  case SEXT_I32_I16:
    return "s_sext_i32_i16";
  case BITSET0_B32:
    return "s_bitset0_b32";
  case BITSET0_B64:
    return "s_bitset0_b64";
  case BITSET1_B32:
    return "s_bitset1_b32";
  case BITSET1_B64:
    return "s_bitset1_b64";
  case GETPC_B64:
    return "s_getpc_b64";
  case SETPC_B64:
    return "s_setpc_b64";
  case SWAPPC_B64:
    return "s_swappc_b64";
  case AND_SAVEEXEC_B64:
    return "s_and_saveexec_b64";
  case OR_SAVEEXEC_B64:
    return "s_or_saveexec_b64";
  case XOR_SAVEEXEC_B64:
    return "s_xor_saveexec_b64";
  case ANDN2_SAVEEXEC_B64:
    return "s_andn2_saveexec_b64";
  case ORN2_SAVEEXEC_B64:
    return "s_orn2_saveexec_b64";
  case NAND_SAVEEXEC_B64:
    return "s_nand_saveexec_b64";
  case NOR_SAVEEXEC_B64:
    return "s_nor_saveexec_b64";
  case XNOR_SAVEEXEC_B64:
    return "s_xnor_saveexec_b64";
  case QUADMASK_B32:
    return "s_quadmask_b32";
  case QUADMASK_B64:
    return "s_quadmask_b64";
  case MOVRELS_B32:
    return "s_movrels_b32";
  case MOVRELS_B64:
    return "s_movrels_b64";
  case MOVRELD_B32:
    return "s_movreld_b32";
  case MOVRELD_B64:
    return "s_movreld_b64";
  case CBRANCH_JOIN:
    return "s_cbranch_join";
  case ABS_I32:
    return "s_abs_i32";
  case MOV_FED_B32:
    return "s_mov_fed_b32";
  }
  return nullptr;
}
} // namespace shader::ir::sop1
