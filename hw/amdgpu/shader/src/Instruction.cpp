#include "Instruction.hpp"
#include <cstdio>

namespace {
using namespace amdgpu::shader;

int printScalarOperand(int id, const std::uint32_t *inst) {
  switch (id) {
  case 0 ... 103:
    std::printf("sgpr[%d]", id);
    return 0;
  case 106:
    std::printf("VCC_LO");
    return 0;
  case 107:
    std::printf("VCC_HI");
    return 0;
  case 124:
    std::printf("M0");
    return 0;
  case 126:
    std::printf("EXEC_LO");
    return 0;
  case 127:
    std::printf("EXEC_HI");
    return 0;
  case 128 ... 192:
    std::printf("%d", id - 128);
    return 0;
  case 193 ... 208:
    std::printf("%d", -static_cast<std::int32_t>(id - 192));
    return 0;
  case 240:
    std::printf("0.5");
    return 0;
  case 241:
    std::printf("-0.5");
    return 0;
  case 242:
    std::printf("1.0");
    return 0;
  case 243:
    std::printf("-1.0");
    return 0;
  case 244:
    std::printf("2.0");
    return 0;
  case 245:
    std::printf("-2.0");
    return 0;
  case 246:
    std::printf("4.0");
    return 0;
  case 247:
    std::printf("-4.0");
    return 0;
  case 251:
    std::printf("VCCZ");
    return 0;
  case 252:
    std::printf("EXECZ");
    return 0;
  case 253:
    std::printf("SCC");
    return 0;
  case 254:
    std::printf("LDS_DIRECT");
    return 0;
  case 255:
    std::printf("%08x", *inst);
    return 1;
  case 256 ... 511:
    std::printf("vgpr[%u]", id - 256);
    return 0;
  }

  std::printf("<invalid %u>", id);
  return 0;
}

int printVectorOperand(int id, const std::uint32_t *inst) {
  std::printf("vgpr[%u]", id);
  return 0;
}

void printExpTarget(int target) {
  switch (target) {
  case 0 ... 7:
    std::printf("mrt%u", target);
    break;
  case 8:
    std::printf("mrtz");
    break;
  case 9:
    std::printf("null");
    break;
  case 12 ... 15:
    std::printf("pos%u", target - 12);
    break;
  case 32 ... 63:
    std::printf("param%u", target - 32);
    break;

  default:
    std::printf("<invalid %u>", target);
    break;
  }
}

void printSop1Opcode(Sop1::Op op) {
  if (auto string = sop1OpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printSop2Opcode(Sop2::Op op) {
  if (auto string = sop2OpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printSopkOpcode(Sopk::Op op) {
  if (auto string = sopkOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printSopcOpcode(Sopc::Op op) {
  if (auto string = sopcOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printSoppOpcode(Sopp::Op op) {
  if (auto string = soppOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printVop2Opcode(Vop2::Op op) {
  if (auto string = vop2OpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printVop1Opcode(Vop1::Op op) {
  if (auto string = vop1OpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printVopcOpcode(Vopc::Op op) {
  if (auto string = vopcOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printVop3Opcode(Vop3::Op op) {
  if (auto string = vop3OpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printSmrdOpcode(Smrd::Op op) {
  if (auto string = smrdOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printMubufOpcode(Mubuf::Op op) {
  if (auto string = mubufOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printMtbufOpcode(Mtbuf::Op op) {
  if (auto string = mtbufOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printMimgOpcode(Mimg::Op op) {
  if (auto string = mimgOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printDsOpcode(Ds::Op op) {
  if (auto string = dsOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}

void printVintrpOpcode(Vintrp::Op op) {
  if (auto string = vintrpOpcodeToString(op)) {
    std::printf("%s", string);
  } else {
    std::printf("<invalid %u>", static_cast<unsigned>(op));
  }
}
} // namespace

const char *amdgpu::shader::sop1OpcodeToString(Sop1::Op op) {
  switch (op) {
  case Sop1::Op::S_MOV_B32:
    return "s_mov_b32";
  case Sop1::Op::S_MOV_B64:
    return "s_mov_b64";
  case Sop1::Op::S_CMOV_B32:
    return "s_cmov_b32";
  case Sop1::Op::S_CMOV_B64:
    return "s_cmov_b64";
  case Sop1::Op::S_NOT_B32:
    return "s_not_b32";
  case Sop1::Op::S_NOT_B64:
    return "s_not_b64";
  case Sop1::Op::S_WQM_B32:
    return "s_wqm_b32";
  case Sop1::Op::S_WQM_B64:
    return "s_wqm_b64";
  case Sop1::Op::S_BREV_B32:
    return "s_brev_b32";
  case Sop1::Op::S_BREV_B64:
    return "s_brev_b64";
  case Sop1::Op::S_BCNT0_I32_B32:
    return "s_bcnt0_i32_b32";
  case Sop1::Op::S_BCNT0_I32_B64:
    return "s_bcnt0_i32_b64";
  case Sop1::Op::S_BCNT1_I32_B32:
    return "s_bcnt1_i32_b32";
  case Sop1::Op::S_BCNT1_I32_B64:
    return "s_bcnt1_i32_b64";
  case Sop1::Op::S_FF0_I32_B32:
    return "s_ff0_i32_b32";
  case Sop1::Op::S_FF0_I32_B64:
    return "s_ff0_i32_b64";
  case Sop1::Op::S_FF1_I32_B32:
    return "s_ff1_i32_b32";
  case Sop1::Op::S_FF1_I32_B64:
    return "s_ff1_i32_b64";
  case Sop1::Op::S_FLBIT_I32_B32:
    return "s_flbit_i32_b32";
  case Sop1::Op::S_FLBIT_I32_B64:
    return "s_flbit_i32_b64";
  case Sop1::Op::S_FLBIT_I32:
    return "s_flbit_i32";
  case Sop1::Op::S_FLBIT_I32_I64:
    return "s_flbit_i32_i64";
  case Sop1::Op::S_SEXT_I32_I8:
    return "s_sext_i32_i8";
  case Sop1::Op::S_SEXT_I32_I16:
    return "s_sext_i32_i16";
  case Sop1::Op::S_BITSET0_B32:
    return "s_bitset0_b32";
  case Sop1::Op::S_BITSET0_B64:
    return "s_bitset0_b64";
  case Sop1::Op::S_BITSET1_B32:
    return "s_bitset1_b32";
  case Sop1::Op::S_BITSET1_B64:
    return "s_bitset1_b64";
  case Sop1::Op::S_GETPC_B64:
    return "s_getpc_b64";
  case Sop1::Op::S_SETPC_B64:
    return "s_setpc_b64";
  case Sop1::Op::S_SWAPPC_B64:
    return "s_swappc_b64";
  case Sop1::Op::S_RFE_B64:
    return "s_rfe_b64";
  case Sop1::Op::S_AND_SAVEEXEC_B64:
    return "s_and_saveexec_b64";
  case Sop1::Op::S_OR_SAVEEXEC_B64:
    return "s_or_saveexec_b64";
  case Sop1::Op::S_XOR_SAVEEXEC_B64:
    return "s_xor_saveexec_b64";
  case Sop1::Op::S_ANDN2_SAVEEXEC_B64:
    return "s_andn2_saveexec_b64";
  case Sop1::Op::S_ORN2_SAVEEXEC_B64:
    return "s_orn2_saveexec_b64";
  case Sop1::Op::S_NAND_SAVEEXEC_B64:
    return "s_nand_saveexec_b64";
  case Sop1::Op::S_NOR_SAVEEXEC_B64:
    return "s_nor_saveexec_b64";
  case Sop1::Op::S_XNOR_SAVEEXEC_B64:
    return "s_xnor_saveexec_b64";
  case Sop1::Op::S_QUADMASK_B32:
    return "s_quadmask_b32";
  case Sop1::Op::S_QUADMASK_B64:
    return "s_quadmask_b64";
  case Sop1::Op::S_MOVRELS_B32:
    return "s_movrels_b32";
  case Sop1::Op::S_MOVRELS_B64:
    return "s_movrels_b64";
  case Sop1::Op::S_MOVRELD_B32:
    return "s_movreld_b32";
  case Sop1::Op::S_MOVRELD_B64:
    return "s_movreld_b64";
  case Sop1::Op::S_CBRANCH_JOIN:
    return "s_cbranch_join";
  case Sop1::Op::S_ABS_I32:
    return "s_abs_i32";
  case Sop1::Op::S_MOV_FED_B32:
    return "s_mov_fed_b32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::sop2OpcodeToString(Sop2::Op op) {
  switch (op) {
  case Sop2::Op::S_ADD_U32:
    return "s_add_u32";
  case Sop2::Op::S_SUB_U32:
    return "s_sub_u32";
  case Sop2::Op::S_ADD_I32:
    return "s_add_i32";
  case Sop2::Op::S_SUB_I32:
    return "s_sub_i32";
  case Sop2::Op::S_ADDC_U32:
    return "s_addc_u32";
  case Sop2::Op::S_SUBB_U32:
    return "s_subb_u32";
  case Sop2::Op::S_MIN_I32:
    return "s_min_i32";
  case Sop2::Op::S_MIN_U32:
    return "s_min_u32";
  case Sop2::Op::S_MAX_I32:
    return "s_max_i32";
  case Sop2::Op::S_MAX_U32:
    return "s_max_u32";
  case Sop2::Op::S_CSELECT_B32:
    return "s_cselect_b32";
  case Sop2::Op::S_CSELECT_B64:
    return "s_cselect_b64";
  case Sop2::Op::S_AND_B32:
    return "s_and_b32";
  case Sop2::Op::S_AND_B64:
    return "s_and_b64";
  case Sop2::Op::S_OR_B32:
    return "s_or_b32";
  case Sop2::Op::S_OR_B64:
    return "s_or_b64";
  case Sop2::Op::S_XOR_B32:
    return "s_xor_b32";
  case Sop2::Op::S_XOR_B64:
    return "s_xor_b64";
  case Sop2::Op::S_ANDN2_B32:
    return "s_andn2_b32";
  case Sop2::Op::S_ANDN2_B64:
    return "s_andn2_b64";
  case Sop2::Op::S_ORN2_B32:
    return "s_orn2_b32";
  case Sop2::Op::S_ORN2_B64:
    return "s_orn2_b64";
  case Sop2::Op::S_NAND_B32:
    return "s_nand_b32";
  case Sop2::Op::S_NAND_B64:
    return "s_nand_b64";
  case Sop2::Op::S_NOR_B32:
    return "s_nor_b32";
  case Sop2::Op::S_NOR_B64:
    return "s_nor_b64";
  case Sop2::Op::S_XNOR_B32:
    return "s_xnor_b32";
  case Sop2::Op::S_XNOR_B64:
    return "s_xnor_b64";
  case Sop2::Op::S_LSHL_B32:
    return "s_lshl_b32";
  case Sop2::Op::S_LSHL_B64:
    return "s_lshl_b64";
  case Sop2::Op::S_LSHR_B32:
    return "s_lshr_b32";
  case Sop2::Op::S_LSHR_B64:
    return "s_lshr_b64";
  case Sop2::Op::S_ASHR_I32:
    return "s_ashr_i32";
  case Sop2::Op::S_ASHR_I64:
    return "s_ashr_i64";
  case Sop2::Op::S_BFM_B32:
    return "s_bfm_b32";
  case Sop2::Op::S_BFM_B64:
    return "s_bfm_b64";
  case Sop2::Op::S_MUL_I32:
    return "s_mul_i32";
  case Sop2::Op::S_BFE_U32:
    return "s_bfe_u32";
  case Sop2::Op::S_BFE_I32:
    return "s_bfe_i32";
  case Sop2::Op::S_BFE_U64:
    return "s_bfe_u64";
  case Sop2::Op::S_BFE_I64:
    return "s_bfe_i64";
  case Sop2::Op::S_CBRANCH_G_FORK:
    return "s_cbranch_g_fork";
  case Sop2::Op::S_ABSDIFF_I32:
    return "s_absdiff_i32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::sopkOpcodeToString(Sopk::Op op) {
  switch (op) {
  case Sopk::Op::S_MOVK_I32:
    return "s_movk_i32";
  case Sopk::Op::S_CMOVK_I32:
    return "s_cmovk_i32";
  case Sopk::Op::S_CMPK_EQ_I32:
    return "s_cmpk_eq_i32";
  case Sopk::Op::S_CMPK_LG_I32:
    return "s_cmpk_lg_i32";
  case Sopk::Op::S_CMPK_GT_I32:
    return "s_cmpk_gt_i32";
  case Sopk::Op::S_CMPK_GE_I32:
    return "s_cmpk_ge_i32";
  case Sopk::Op::S_CMPK_LT_I32:
    return "s_cmpk_lt_i32";
  case Sopk::Op::S_CMPK_LE_I32:
    return "s_cmpk_le_i32";
  case Sopk::Op::S_CMPK_EQ_U32:
    return "s_cmpk_eq_u32";
  case Sopk::Op::S_CMPK_LG_U32:
    return "s_cmpk_lg_u32";
  case Sopk::Op::S_CMPK_GT_U32:
    return "s_cmpk_gt_u32";
  case Sopk::Op::S_CMPK_GE_U32:
    return "s_cmpk_ge_u32";
  case Sopk::Op::S_CMPK_LT_U32:
    return "s_cmpk_lt_u32";
  case Sopk::Op::S_CMPK_LE_U32:
    return "s_cmpk_le_u32";
  case Sopk::Op::S_ADDK_I32:
    return "s_addk_i32";
  case Sopk::Op::S_MULK_I32:
    return "s_mulk_i32";
  case Sopk::Op::S_CBRANCH_I_FORK:
    return "s_cbranch_i_fork";
  case Sopk::Op::S_GETREG_B32:
    return "s_getreg_b32";
  case Sopk::Op::S_SETREG_B32:
    return "s_setreg_b32";
  case Sopk::Op::S_SETREG_IMM:
    return "s_setreg_imm";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::sopcOpcodeToString(Sopc::Op op) {
  switch (op) {
  case Sopc::Op::S_CMP_EQ_I32:
    return "s_cmp_eq_i32";
  case Sopc::Op::S_CMP_LG_I32:
    return "s_cmp_lg_i32";
  case Sopc::Op::S_CMP_GT_I32:
    return "s_cmp_gt_i32";
  case Sopc::Op::S_CMP_GE_I32:
    return "s_cmp_ge_i32";
  case Sopc::Op::S_CMP_LT_I32:
    return "s_cmp_lt_i32";
  case Sopc::Op::S_CMP_LE_I32:
    return "s_cmp_le_i32";
  case Sopc::Op::S_CMP_EQ_U32:
    return "s_cmp_eq_u32";
  case Sopc::Op::S_CMP_LG_U32:
    return "s_cmp_lg_u32";
  case Sopc::Op::S_CMP_GT_U32:
    return "s_cmp_gt_u32";
  case Sopc::Op::S_CMP_GE_U32:
    return "s_cmp_ge_u32";
  case Sopc::Op::S_CMP_LT_U32:
    return "s_cmp_lt_u32";
  case Sopc::Op::S_CMP_LE_U32:
    return "s_cmp_le_u32";
  case Sopc::Op::S_BITCMP0_B32:
    return "s_bitcmp0_b32";
  case Sopc::Op::S_BITCMP1_B32:
    return "s_bitcmp1_b32";
  case Sopc::Op::S_BITCMP0_B64:
    return "s_bitcmp0_b64";
  case Sopc::Op::S_BITCMP1_B64:
    return "s_bitcmp1_b64";
  case Sopc::Op::S_SETVSKIP:
    return "s_setvskip";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::soppOpcodeToString(Sopp::Op op) {
  switch (op) {
  case Sopp::Op::S_NOP:
    return "s_nop";
  case Sopp::Op::S_ENDPGM:
    return "s_endpgm";
  case Sopp::Op::S_BRANCH:
    return "s_branch";
  case Sopp::Op::S_CBRANCH_SCC0:
    return "s_cbranch_scc0";
  case Sopp::Op::S_CBRANCH_SCC1:
    return "s_cbranch_scc1";
  case Sopp::Op::S_CBRANCH_VCCZ:
    return "s_cbranch_vccz";
  case Sopp::Op::S_CBRANCH_VCCNZ:
    return "s_cbranch_vccnz";
  case Sopp::Op::S_CBRANCH_EXECZ:
    return "s_cbranch_execz";
  case Sopp::Op::S_CBRANCH_EXECNZ:
    return "s_cbranch_execnz";
  case Sopp::Op::S_BARRIER:
    return "s_barrier";
  case Sopp::Op::S_WAITCNT:
    return "s_waitcnt";
  case Sopp::Op::S_SETHALT:
    return "s_sethalt";
  case Sopp::Op::S_SLEEP:
    return "s_sleep";
  case Sopp::Op::S_SETPRIO:
    return "s_setprio";
  case Sopp::Op::S_SENDMSG:
    return "s_sendmsg";
  case Sopp::Op::S_SENDMSGHALT:
    return "s_sendmsghalt";
  case Sopp::Op::S_TRAP:
    return "s_trap";
  case Sopp::Op::S_ICACHE_INV:
    return "s_icache_inv";
  case Sopp::Op::S_INCPERFLEVEL:
    return "s_incperflevel";
  case Sopp::Op::S_DECPERFLEVEL:
    return "s_decperflevel";
  case Sopp::Op::S_TTRACEDATA:
    return "s_ttracedata";
  case Sopp::Op::S_CBRANCH_CDBGSYS:
    return "s_cbranch_cdbgsys";
  case Sopp::Op::S_CBRANCH_CDBGUSER:
    return "s_cbranch_cdbguser";
  case Sopp::Op::S_CBRANCH_CDBGSYS_OR_USER:
    return "s_cbranch_cdbgsys_or_user";
  case Sopp::Op::S_CBRANCH_CDBGSYS_AND_USER:
    return "s_cbranch_cdbgsys_and_user";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::vop2OpcodeToString(Vop2::Op op) {
  switch (op) {
  case Vop2::Op::V_CNDMASK_B32:
    return "v_cndmask_b32";
  case Vop2::Op::V_READLANE_B32:
    return "v_readlane_b32";
  case Vop2::Op::V_WRITELANE_B32:
    return "v_writelane_b32";
  case Vop2::Op::V_ADD_F32:
    return "v_add_f32";
  case Vop2::Op::V_SUB_F32:
    return "v_sub_f32";
  case Vop2::Op::V_SUBREV_F32:
    return "v_subrev_f32";
  case Vop2::Op::V_MAC_LEGACY_F32:
    return "v_mac_legacy_f32";
  case Vop2::Op::V_MUL_LEGACY_F32:
    return "v_mul_legacy_f32";
  case Vop2::Op::V_MUL_F32:
    return "v_mul_f32";
  case Vop2::Op::V_MUL_I32_I24:
    return "v_mul_i32_i24";
  case Vop2::Op::V_MUL_HI_I32_I24:
    return "v_mul_hi_i32_i24";
  case Vop2::Op::V_MUL_U32_U24:
    return "v_mul_u32_u24";
  case Vop2::Op::V_MUL_HI_U32_U24:
    return "v_mul_hi_u32_u24";
  case Vop2::Op::V_MIN_LEGACY_F32:
    return "v_min_legacy_f32";
  case Vop2::Op::V_MAX_LEGACY_F32:
    return "v_max_legacy_f32";
  case Vop2::Op::V_MIN_F32:
    return "v_min_f32";
  case Vop2::Op::V_MAX_F32:
    return "v_max_f32";
  case Vop2::Op::V_MIN_I32:
    return "v_min_i32";
  case Vop2::Op::V_MAX_I32:
    return "v_max_i32";
  case Vop2::Op::V_MIN_U32:
    return "v_min_u32";
  case Vop2::Op::V_MAX_U32:
    return "v_max_u32";
  case Vop2::Op::V_LSHR_B32:
    return "v_lshr_b32";
  case Vop2::Op::V_LSHRREV_B32:
    return "v_lshrrev_b32";
  case Vop2::Op::V_ASHR_I32:
    return "v_ashr_i32";
  case Vop2::Op::V_ASHRREV_I32:
    return "v_ashrrev_i32";
  case Vop2::Op::V_LSHL_B32:
    return "v_lshl_b32";
  case Vop2::Op::V_LSHLREV_B32:
    return "v_lshlrev_b32";
  case Vop2::Op::V_AND_B32:
    return "v_and_b32";
  case Vop2::Op::V_OR_B32:
    return "v_or_b32";
  case Vop2::Op::V_XOR_B32:
    return "v_xor_b32";
  case Vop2::Op::V_BFM_B32:
    return "v_bfm_b32";
  case Vop2::Op::V_MAC_F32:
    return "v_mac_f32";
  case Vop2::Op::V_MADMK_F32:
    return "v_madmk_f32";
  case Vop2::Op::V_MADAK_F32:
    return "v_madak_f32";
  case Vop2::Op::V_BCNT_U32_B32:
    return "v_bcnt_u32_b32";
  case Vop2::Op::V_MBCNT_LO_U32_B32:
    return "v_mbcnt_lo_u32_b32";
  case Vop2::Op::V_MBCNT_HI_U32_B32:
    return "v_mbcnt_hi_u32_b32";
  case Vop2::Op::V_ADD_I32:
    return "v_add_i32";
  case Vop2::Op::V_SUB_I32:
    return "v_sub_i32";
  case Vop2::Op::V_SUBREV_I32:
    return "v_subrev_i32";
  case Vop2::Op::V_ADDC_U32:
    return "v_addc_u32";
  case Vop2::Op::V_SUBB_U32:
    return "v_subb_u32";
  case Vop2::Op::V_SUBBREV_U32:
    return "v_subbrev_u32";
  case Vop2::Op::V_LDEXP_F32:
    return "v_ldexp_f32";
  case Vop2::Op::V_CVT_PKACCUM_U8_F32:
    return "v_cvt_pkaccum_u8_f32";
  case Vop2::Op::V_CVT_PKNORM_I16_F32:
    return "v_cvt_pknorm_i16_f32";
  case Vop2::Op::V_CVT_PKNORM_U16_F32:
    return "v_cvt_pknorm_u16_f32";
  case Vop2::Op::V_CVT_PKRTZ_F16_F32:
    return "v_cvt_pkrtz_f16_f32";
  case Vop2::Op::V_CVT_PK_U16_U32:
    return "v_cvt_pk_u16_u32";
  case Vop2::Op::V_CVT_PK_I16_I32:
    return "v_cvt_pk_i16_i32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::vop1OpcodeToString(Vop1::Op op) {
  switch (op) {
  case Vop1::Op::V_NOP:
    return "v_nop";
  case Vop1::Op::V_MOV_B32:
    return "v_mov_b32";
  case Vop1::Op::V_READFIRSTLANE_B32:
    return "v_readfirstlane_b32";
  case Vop1::Op::V_CVT_I32_F64:
    return "v_cvt_i32_f64";
  case Vop1::Op::V_CVT_F64_I32:
    return "v_cvt_f64_i32";
  case Vop1::Op::V_CVT_F32_I32:
    return "v_cvt_f32_i32";
  case Vop1::Op::V_CVT_F32_U32:
    return "v_cvt_f32_u32";
  case Vop1::Op::V_CVT_U32_F32:
    return "v_cvt_u32_f32";
  case Vop1::Op::V_CVT_I32_F32:
    return "v_cvt_i32_f32";
  case Vop1::Op::V_MOV_FED_B32:
    return "v_mov_fed_b32";
  case Vop1::Op::V_CVT_F16_F32:
    return "v_cvt_f16_f32";
  case Vop1::Op::V_CVT_F32_F16:
    return "v_cvt_f32_f16";
  case Vop1::Op::V_CVT_RPI_I32_F32:
    return "v_cvt_rpi_i32_f32";
  case Vop1::Op::V_CVT_FLR_I32_F32:
    return "v_cvt_flr_i32_f32";
  case Vop1::Op::V_CVT_OFF_F32_I4:
    return "v_cvt_off_f32_i4";
  case Vop1::Op::V_CVT_F32_F64:
    return "v_cvt_f32_f64";
  case Vop1::Op::V_CVT_F64_F32:
    return "v_cvt_f64_f32";
  case Vop1::Op::V_CVT_F32_UBYTE0:
    return "v_cvt_f32_ubyte0";
  case Vop1::Op::V_CVT_F32_UBYTE1:
    return "v_cvt_f32_ubyte1";
  case Vop1::Op::V_CVT_F32_UBYTE2:
    return "v_cvt_f32_ubyte2";
  case Vop1::Op::V_CVT_F32_UBYTE3:
    return "v_cvt_f32_ubyte3";
  case Vop1::Op::V_CVT_U32_F64:
    return "v_cvt_u32_f64";
  case Vop1::Op::V_CVT_F64_U32:
    return "v_cvt_f64_u32";
  case Vop1::Op::V_FRACT_F32:
    return "v_fract_f32";
  case Vop1::Op::V_TRUNC_F32:
    return "v_trunc_f32";
  case Vop1::Op::V_CEIL_F32:
    return "v_ceil_f32";
  case Vop1::Op::V_RNDNE_F32:
    return "v_rndne_f32";
  case Vop1::Op::V_FLOOR_F32:
    return "v_floor_f32";
  case Vop1::Op::V_EXP_F32:
    return "v_exp_f32";
  case Vop1::Op::V_LOG_CLAMP_F32:
    return "v_log_clamp_f32";
  case Vop1::Op::V_LOG_F32:
    return "v_log_f32";
  case Vop1::Op::V_RCP_CLAMP_F32:
    return "v_rcp_clamp_f32";
  case Vop1::Op::V_RCP_LEGACY_F32:
    return "v_rcp_legacy_f32";
  case Vop1::Op::V_RCP_F32:
    return "v_rcp_f32";
  case Vop1::Op::V_RCP_IFLAG_F32:
    return "v_rcp_iflag_f32";
  case Vop1::Op::V_RSQ_CLAMP_F32:
    return "v_rsq_clamp_f32";
  case Vop1::Op::V_RSQ_LEGACY_F32:
    return "v_rsq_legacy_f32";
  case Vop1::Op::V_RSQ_F32:
    return "v_rsq_f32";
  case Vop1::Op::V_RCP_F64:
    return "v_rcp_f64";
  case Vop1::Op::V_RCP_CLAMP_F64:
    return "v_rcp_clamp_f64";
  case Vop1::Op::V_RSQ_F64:
    return "v_rsq_f64";
  case Vop1::Op::V_RSQ_CLAMP_F64:
    return "v_rsq_clamp_f64";
  case Vop1::Op::V_SQRT_F32:
    return "v_sqrt_f32";
  case Vop1::Op::V_SQRT_F64:
    return "v_sqrt_f64";
  case Vop1::Op::V_SIN_F32:
    return "v_sin_f32";
  case Vop1::Op::V_COS_F32:
    return "v_cos_f32";
  case Vop1::Op::V_NOT_B32:
    return "v_not_b32";
  case Vop1::Op::V_BFREV_B32:
    return "v_bfrev_b32";
  case Vop1::Op::V_FFBH_U32:
    return "v_ffbh_u32";
  case Vop1::Op::V_FFBL_B32:
    return "v_ffbl_b32";
  case Vop1::Op::V_FFBH_I32:
    return "v_ffbh_i32";
  case Vop1::Op::V_FREXP_EXP_I32_F64:
    return "v_frexp_exp_i32_f64";
  case Vop1::Op::V_FREXP_MANT_F64:
    return "v_frexp_mant_f64";
  case Vop1::Op::V_FRACT_F64:
    return "v_fract_f64";
  case Vop1::Op::V_FREXP_EXP_I32_F32:
    return "v_frexp_exp_i32_f32";
  case Vop1::Op::V_FREXP_MANT_F32:
    return "v_frexp_mant_f32";
  case Vop1::Op::V_CLREXCP:
    return "v_clrexcp";
  case Vop1::Op::V_MOVRELD_B32:
    return "v_movreld_b32";
  case Vop1::Op::V_MOVRELS_B32:
    return "v_movrels_b32";
  case Vop1::Op::V_MOVRELSD_B32:
    return "v_movrelsd_b32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::vopcOpcodeToString(Vopc::Op op) {
  switch (op) {
  case Vopc::Op::V_CMP_F_F32:
    return "v_cmp_f_f32";
  case Vopc::Op::V_CMP_LT_F32:
    return "v_cmp_lt_f32";
  case Vopc::Op::V_CMP_EQ_F32:
    return "v_cmp_eq_f32";
  case Vopc::Op::V_CMP_LE_F32:
    return "v_cmp_le_f32";
  case Vopc::Op::V_CMP_GT_F32:
    return "v_cmp_gt_f32";
  case Vopc::Op::V_CMP_LG_F32:
    return "v_cmp_lg_f32";
  case Vopc::Op::V_CMP_GE_F32:
    return "v_cmp_ge_f32";
  case Vopc::Op::V_CMP_O_F32:
    return "v_cmp_o_f32";
  case Vopc::Op::V_CMP_U_F32:
    return "v_cmp_u_f32";
  case Vopc::Op::V_CMP_NGE_F32:
    return "v_cmp_nge_f32";
  case Vopc::Op::V_CMP_NLG_F32:
    return "v_cmp_nlg_f32";
  case Vopc::Op::V_CMP_NGT_F32:
    return "v_cmp_ngt_f32";
  case Vopc::Op::V_CMP_NLE_F32:
    return "v_cmp_nle_f32";
  case Vopc::Op::V_CMP_NEQ_F32:
    return "v_cmp_neq_f32";
  case Vopc::Op::V_CMP_NLT_F32:
    return "v_cmp_nlt_f32";
  case Vopc::Op::V_CMP_TRU_F32:
    return "v_cmp_tru_f32";
  case Vopc::Op::V_CMPX_F_F32:
    return "v_cmpx_f_f32";
  case Vopc::Op::V_CMPX_LT_F32:
    return "v_cmpx_lt_f32";
  case Vopc::Op::V_CMPX_EQ_F32:
    return "v_cmpx_eq_f32";
  case Vopc::Op::V_CMPX_LE_F32:
    return "v_cmpx_le_f32";
  case Vopc::Op::V_CMPX_GT_F32:
    return "v_cmpx_gt_f32";
  case Vopc::Op::V_CMPX_LG_F32:
    return "v_cmpx_lg_f32";
  case Vopc::Op::V_CMPX_GE_F32:
    return "v_cmpx_ge_f32";
  case Vopc::Op::V_CMPX_O_F32:
    return "v_cmpx_o_f32";
  case Vopc::Op::V_CMPX_U_F32:
    return "v_cmpx_u_f32";
  case Vopc::Op::V_CMPX_NGE_F32:
    return "v_cmpx_nge_f32";
  case Vopc::Op::V_CMPX_NLG_F32:
    return "v_cmpx_nlg_f32";
  case Vopc::Op::V_CMPX_NGT_F32:
    return "v_cmpx_ngt_f32";
  case Vopc::Op::V_CMPX_NLE_F32:
    return "v_cmpx_nle_f32";
  case Vopc::Op::V_CMPX_NEQ_F32:
    return "v_cmpx_neq_f32";
  case Vopc::Op::V_CMPX_NLT_F32:
    return "v_cmpx_nlt_f32";
  case Vopc::Op::V_CMPX_TRU_F32:
    return "v_cmpx_tru_f32";
  case Vopc::Op::V_CMP_F_F64:
    return "v_cmp_f_f64";
  case Vopc::Op::V_CMP_LT_F64:
    return "v_cmp_lt_f64";
  case Vopc::Op::V_CMP_EQ_F64:
    return "v_cmp_eq_f64";
  case Vopc::Op::V_CMP_LE_F64:
    return "v_cmp_le_f64";
  case Vopc::Op::V_CMP_GT_F64:
    return "v_cmp_gt_f64";
  case Vopc::Op::V_CMP_LG_F64:
    return "v_cmp_lg_f64";
  case Vopc::Op::V_CMP_GE_F64:
    return "v_cmp_ge_f64";
  case Vopc::Op::V_CMP_O_F64:
    return "v_cmp_o_f64";
  case Vopc::Op::V_CMP_U_F64:
    return "v_cmp_u_f64";
  case Vopc::Op::V_CMP_NGE_F64:
    return "v_cmp_nge_f64";
  case Vopc::Op::V_CMP_NLG_F64:
    return "v_cmp_nlg_f64";
  case Vopc::Op::V_CMP_NGT_F64:
    return "v_cmp_ngt_f64";
  case Vopc::Op::V_CMP_NLE_F64:
    return "v_cmp_nle_f64";
  case Vopc::Op::V_CMP_NEQ_F64:
    return "v_cmp_neq_f64";
  case Vopc::Op::V_CMP_NLT_F64:
    return "v_cmp_nlt_f64";
  case Vopc::Op::V_CMP_TRU_F64:
    return "v_cmp_tru_f64";
  case Vopc::Op::V_CMPX_F_F64:
    return "v_cmpx_f_f64";
  case Vopc::Op::V_CMPX_LT_F64:
    return "v_cmpx_lt_f64";
  case Vopc::Op::V_CMPX_EQ_F64:
    return "v_cmpx_eq_f64";
  case Vopc::Op::V_CMPX_LE_F64:
    return "v_cmpx_le_f64";
  case Vopc::Op::V_CMPX_GT_F64:
    return "v_cmpx_gt_f64";
  case Vopc::Op::V_CMPX_LG_F64:
    return "v_cmpx_lg_f64";
  case Vopc::Op::V_CMPX_GE_F64:
    return "v_cmpx_ge_f64";
  case Vopc::Op::V_CMPX_O_F64:
    return "v_cmpx_o_f64";
  case Vopc::Op::V_CMPX_U_F64:
    return "v_cmpx_u_f64";
  case Vopc::Op::V_CMPX_NGE_F64:
    return "v_cmpx_nge_f64";
  case Vopc::Op::V_CMPX_NLG_F64:
    return "v_cmpx_nlg_f64";
  case Vopc::Op::V_CMPX_NGT_F64:
    return "v_cmpx_ngt_f64";
  case Vopc::Op::V_CMPX_NLE_F64:
    return "v_cmpx_nle_f64";
  case Vopc::Op::V_CMPX_NEQ_F64:
    return "v_cmpx_neq_f64";
  case Vopc::Op::V_CMPX_NLT_F64:
    return "v_cmpx_nlt_f64";
  case Vopc::Op::V_CMPX_TRU_F64:
    return "v_cmpx_tru_f64";
  case Vopc::Op::V_CMPS_F_F32:
    return "v_cmps_f_f32";
  case Vopc::Op::V_CMPS_LT_F32:
    return "v_cmps_lt_f32";
  case Vopc::Op::V_CMPS_EQ_F32:
    return "v_cmps_eq_f32";
  case Vopc::Op::V_CMPS_LE_F32:
    return "v_cmps_le_f32";
  case Vopc::Op::V_CMPS_GT_F32:
    return "v_cmps_gt_f32";
  case Vopc::Op::V_CMPS_LG_F32:
    return "v_cmps_lg_f32";
  case Vopc::Op::V_CMPS_GE_F32:
    return "v_cmps_ge_f32";
  case Vopc::Op::V_CMPS_O_F32:
    return "v_cmps_o_f32";
  case Vopc::Op::V_CMPS_U_F32:
    return "v_cmps_u_f32";
  case Vopc::Op::V_CMPS_NGE_F32:
    return "v_cmps_nge_f32";
  case Vopc::Op::V_CMPS_NLG_F32:
    return "v_cmps_nlg_f32";
  case Vopc::Op::V_CMPS_NGT_F32:
    return "v_cmps_ngt_f32";
  case Vopc::Op::V_CMPS_NLE_F32:
    return "v_cmps_nle_f32";
  case Vopc::Op::V_CMPS_NEQ_F32:
    return "v_cmps_neq_f32";
  case Vopc::Op::V_CMPS_NLT_F32:
    return "v_cmps_nlt_f32";
  case Vopc::Op::V_CMPS_TRU_F32:
    return "v_cmps_tru_f32";
  case Vopc::Op::V_CMPSX_F_F32:
    return "v_cmpsx_f_f32";
  case Vopc::Op::V_CMPSX_LT_F32:
    return "v_cmpsx_lt_f32";
  case Vopc::Op::V_CMPSX_EQ_F32:
    return "v_cmpsx_eq_f32";
  case Vopc::Op::V_CMPSX_LE_F32:
    return "v_cmpsx_le_f32";
  case Vopc::Op::V_CMPSX_GT_F32:
    return "v_cmpsx_gt_f32";
  case Vopc::Op::V_CMPSX_LG_F32:
    return "v_cmpsx_lg_f32";
  case Vopc::Op::V_CMPSX_GE_F32:
    return "v_cmpsx_ge_f32";
  case Vopc::Op::V_CMPSX_O_F32:
    return "v_cmpsx_o_f32";
  case Vopc::Op::V_CMPSX_U_F32:
    return "v_cmpsx_u_f32";
  case Vopc::Op::V_CMPSX_NGE_F32:
    return "v_cmpsx_nge_f32";
  case Vopc::Op::V_CMPSX_NLG_F32:
    return "v_cmpsx_nlg_f32";
  case Vopc::Op::V_CMPSX_NGT_F32:
    return "v_cmpsx_ngt_f32";
  case Vopc::Op::V_CMPSX_NLE_F32:
    return "v_cmpsx_nle_f32";
  case Vopc::Op::V_CMPSX_NEQ_F32:
    return "v_cmpsx_neq_f32";
  case Vopc::Op::V_CMPSX_NLT_F32:
    return "v_cmpsx_nlt_f32";
  case Vopc::Op::V_CMPSX_TRU_F32:
    return "v_cmpsx_tru_f32";
  case Vopc::Op::V_CMPS_F_F64:
    return "v_cmps_f_f64";
  case Vopc::Op::V_CMPS_LT_F64:
    return "v_cmps_lt_f64";
  case Vopc::Op::V_CMPS_EQ_F64:
    return "v_cmps_eq_f64";
  case Vopc::Op::V_CMPS_LE_F64:
    return "v_cmps_le_f64";
  case Vopc::Op::V_CMPS_GT_F64:
    return "v_cmps_gt_f64";
  case Vopc::Op::V_CMPS_LG_F64:
    return "v_cmps_lg_f64";
  case Vopc::Op::V_CMPS_GE_F64:
    return "v_cmps_ge_f64";
  case Vopc::Op::V_CMPS_O_F64:
    return "v_cmps_o_f64";
  case Vopc::Op::V_CMPS_U_F64:
    return "v_cmps_u_f64";
  case Vopc::Op::V_CMPS_NGE_F64:
    return "v_cmps_nge_f64";
  case Vopc::Op::V_CMPS_NLG_F64:
    return "v_cmps_nlg_f64";
  case Vopc::Op::V_CMPS_NGT_F64:
    return "v_cmps_ngt_f64";
  case Vopc::Op::V_CMPS_NLE_F64:
    return "v_cmps_nle_f64";
  case Vopc::Op::V_CMPS_NEQ_F64:
    return "v_cmps_neq_f64";
  case Vopc::Op::V_CMPS_NLT_F64:
    return "v_cmps_nlt_f64";
  case Vopc::Op::V_CMPS_TRU_F64:
    return "v_cmps_tru_f64";
  case Vopc::Op::V_CMPSX_F_F64:
    return "v_cmpsx_f_f64";
  case Vopc::Op::V_CMPSX_LT_F64:
    return "v_cmpsx_lt_f64";
  case Vopc::Op::V_CMPSX_EQ_F64:
    return "v_cmpsx_eq_f64";
  case Vopc::Op::V_CMPSX_LE_F64:
    return "v_cmpsx_le_f64";
  case Vopc::Op::V_CMPSX_GT_F64:
    return "v_cmpsx_gt_f64";
  case Vopc::Op::V_CMPSX_LG_F64:
    return "v_cmpsx_lg_f64";
  case Vopc::Op::V_CMPSX_GE_F64:
    return "v_cmpsx_ge_f64";
  case Vopc::Op::V_CMPSX_O_F64:
    return "v_cmpsx_o_f64";
  case Vopc::Op::V_CMPSX_U_F64:
    return "v_cmpsx_u_f64";
  case Vopc::Op::V_CMPSX_NGE_F64:
    return "v_cmpsx_nge_f64";
  case Vopc::Op::V_CMPSX_NLG_F64:
    return "v_cmpsx_nlg_f64";
  case Vopc::Op::V_CMPSX_NGT_F64:
    return "v_cmpsx_ngt_f64";
  case Vopc::Op::V_CMPSX_NLE_F64:
    return "v_cmpsx_nle_f64";
  case Vopc::Op::V_CMPSX_NEQ_F64:
    return "v_cmpsx_neq_f64";
  case Vopc::Op::V_CMPSX_NLT_F64:
    return "v_cmpsx_nlt_f64";
  case Vopc::Op::V_CMPSX_TRU_F64:
    return "v_cmpsx_tru_f64";
  case Vopc::Op::V_CMP_F_I32:
    return "v_cmp_f_i32";
  case Vopc::Op::V_CMP_LT_I32:
    return "v_cmp_lt_i32";
  case Vopc::Op::V_CMP_EQ_I32:
    return "v_cmp_eq_i32";
  case Vopc::Op::V_CMP_LE_I32:
    return "v_cmp_le_i32";
  case Vopc::Op::V_CMP_GT_I32:
    return "v_cmp_gt_i32";
  case Vopc::Op::V_CMP_NE_I32:
    return "v_cmp_ne_i32";
  case Vopc::Op::V_CMP_GE_I32:
    return "v_cmp_ge_i32";
  case Vopc::Op::V_CMP_T_I32:
    return "v_cmp_t_i32";
  case Vopc::Op::V_CMP_CLASS_F32:
    return "v_cmp_class_f32";
  case Vopc::Op::V_CMP_LT_I16:
    return "v_cmp_lt_i16";
  case Vopc::Op::V_CMP_EQ_I16:
    return "v_cmp_eq_i16";
  case Vopc::Op::V_CMP_LE_I16:
    return "v_cmp_le_i16";
  case Vopc::Op::V_CMP_GT_I16:
    return "v_cmp_gt_i16";
  case Vopc::Op::V_CMP_NE_I16:
    return "v_cmp_ne_i16";
  case Vopc::Op::V_CMP_GE_I16:
    return "v_cmp_ge_i16";
  case Vopc::Op::V_CMP_CLASS_F16:
    return "v_cmp_class_f16";
  case Vopc::Op::V_CMPX_F_I32:
    return "v_cmpx_f_i32";
  case Vopc::Op::V_CMPX_LT_I32:
    return "v_cmpx_lt_i32";
  case Vopc::Op::V_CMPX_EQ_I32:
    return "v_cmpx_eq_i32";
  case Vopc::Op::V_CMPX_LE_I32:
    return "v_cmpx_le_i32";
  case Vopc::Op::V_CMPX_GT_I32:
    return "v_cmpx_gt_i32";
  case Vopc::Op::V_CMPX_NE_I32:
    return "v_cmpx_ne_i32";
  case Vopc::Op::V_CMPX_GE_I32:
    return "v_cmpx_ge_i32";
  case Vopc::Op::V_CMPX_T_I32:
    return "v_cmpx_t_i32";
  case Vopc::Op::V_CMPX_CLASS_F32:
    return "v_cmpx_class_f32";
  case Vopc::Op::V_CMPX_LT_I16:
    return "v_cmpx_lt_i16";
  case Vopc::Op::V_CMPX_EQ_I16:
    return "v_cmpx_eq_i16";
  case Vopc::Op::V_CMPX_LE_I16:
    return "v_cmpx_le_i16";
  case Vopc::Op::V_CMPX_GT_I16:
    return "v_cmpx_gt_i16";
  case Vopc::Op::V_CMPX_NE_I16:
    return "v_cmpx_ne_i16";
  case Vopc::Op::V_CMPX_GE_I16:
    return "v_cmpx_ge_i16";
  case Vopc::Op::V_CMPX_CLASS_F16:
    return "v_cmpx_class_f16";
  case Vopc::Op::V_CMP_F_I64:
    return "v_cmp_f_i64";
  case Vopc::Op::V_CMP_LT_I64:
    return "v_cmp_lt_i64";
  case Vopc::Op::V_CMP_EQ_I64:
    return "v_cmp_eq_i64";
  case Vopc::Op::V_CMP_LE_I64:
    return "v_cmp_le_i64";
  case Vopc::Op::V_CMP_GT_I64:
    return "v_cmp_gt_i64";
  case Vopc::Op::V_CMP_NE_I64:
    return "v_cmp_ne_i64";
  case Vopc::Op::V_CMP_GE_I64:
    return "v_cmp_ge_i64";
  case Vopc::Op::V_CMP_T_I64:
    return "v_cmp_t_i64";
  case Vopc::Op::V_CMP_CLASS_F64:
    return "v_cmp_class_f64";
  case Vopc::Op::V_CMP_LT_U16:
    return "v_cmp_lt_u16";
  case Vopc::Op::V_CMP_EQ_U16:
    return "v_cmp_eq_u16";
  case Vopc::Op::V_CMP_LE_U16:
    return "v_cmp_le_u16";
  case Vopc::Op::V_CMP_GT_U16:
    return "v_cmp_gt_u16";
  case Vopc::Op::V_CMP_NE_U16:
    return "v_cmp_ne_u16";
  case Vopc::Op::V_CMP_GE_U16:
    return "v_cmp_ge_u16";
  case Vopc::Op::V_CMPX_F_I64:
    return "v_cmpx_f_i64";
  case Vopc::Op::V_CMPX_LT_I64:
    return "v_cmpx_lt_i64";
  case Vopc::Op::V_CMPX_EQ_I64:
    return "v_cmpx_eq_i64";
  case Vopc::Op::V_CMPX_LE_I64:
    return "v_cmpx_le_i64";
  case Vopc::Op::V_CMPX_GT_I64:
    return "v_cmpx_gt_i64";
  case Vopc::Op::V_CMPX_NE_I64:
    return "v_cmpx_ne_i64";
  case Vopc::Op::V_CMPX_GE_I64:
    return "v_cmpx_ge_i64";
  case Vopc::Op::V_CMPX_T_I64:
    return "v_cmpx_t_i64";
  case Vopc::Op::V_CMPX_CLASS_F64:
    return "v_cmpx_class_f64";
  case Vopc::Op::V_CMPX_LT_U16:
    return "v_cmpx_lt_u16";
  case Vopc::Op::V_CMPX_EQ_U16:
    return "v_cmpx_eq_u16";
  case Vopc::Op::V_CMPX_LE_U16:
    return "v_cmpx_le_u16";
  case Vopc::Op::V_CMPX_GT_U16:
    return "v_cmpx_gt_u16";
  case Vopc::Op::V_CMPX_NE_U16:
    return "v_cmpx_ne_u16";
  case Vopc::Op::V_CMPX_GE_U16:
    return "v_cmpx_ge_u16";
  case Vopc::Op::V_CMP_F_U32:
    return "v_cmp_f_u32";
  case Vopc::Op::V_CMP_LT_U32:
    return "v_cmp_lt_u32";
  case Vopc::Op::V_CMP_EQ_U32:
    return "v_cmp_eq_u32";
  case Vopc::Op::V_CMP_LE_U32:
    return "v_cmp_le_u32";
  case Vopc::Op::V_CMP_GT_U32:
    return "v_cmp_gt_u32";
  case Vopc::Op::V_CMP_NE_U32:
    return "v_cmp_ne_u32";
  case Vopc::Op::V_CMP_GE_U32:
    return "v_cmp_ge_u32";
  case Vopc::Op::V_CMP_T_U32:
    return "v_cmp_t_u32";
  case Vopc::Op::V_CMP_F_F16:
    return "v_cmp_f_f16";
  case Vopc::Op::V_CMP_LT_F16:
    return "v_cmp_lt_f16";
  case Vopc::Op::V_CMP_EQ_F16:
    return "v_cmp_eq_f16";
  case Vopc::Op::V_CMP_LE_F16:
    return "v_cmp_le_f16";
  case Vopc::Op::V_CMP_GT_F16:
    return "v_cmp_gt_f16";
  case Vopc::Op::V_CMP_LG_F16:
    return "v_cmp_lg_f16";
  case Vopc::Op::V_CMP_GE_F16:
    return "v_cmp_ge_f16";
  case Vopc::Op::V_CMP_O_F16:
    return "v_cmp_o_f16";
  case Vopc::Op::V_CMPX_F_U32:
    return "v_cmpx_f_u32";
  case Vopc::Op::V_CMPX_LT_U32:
    return "v_cmpx_lt_u32";
  case Vopc::Op::V_CMPX_EQ_U32:
    return "v_cmpx_eq_u32";
  case Vopc::Op::V_CMPX_LE_U32:
    return "v_cmpx_le_u32";
  case Vopc::Op::V_CMPX_GT_U32:
    return "v_cmpx_gt_u32";
  case Vopc::Op::V_CMPX_NE_U32:
    return "v_cmpx_ne_u32";
  case Vopc::Op::V_CMPX_GE_U32:
    return "v_cmpx_ge_u32";
  case Vopc::Op::V_CMPX_T_U32:
    return "v_cmpx_t_u32";
  case Vopc::Op::V_CMPX_F_F16:
    return "v_cmpx_f_f16";
  case Vopc::Op::V_CMPX_LT_F16:
    return "v_cmpx_lt_f16";
  case Vopc::Op::V_CMPX_EQ_F16:
    return "v_cmpx_eq_f16";
  case Vopc::Op::V_CMPX_LE_F16:
    return "v_cmpx_le_f16";
  case Vopc::Op::V_CMPX_GT_F16:
    return "v_cmpx_gt_f16";
  case Vopc::Op::V_CMPX_LG_F16:
    return "v_cmpx_lg_f16";
  case Vopc::Op::V_CMPX_GE_F16:
    return "v_cmpx_ge_f16";
  case Vopc::Op::V_CMPX_O_F16:
    return "v_cmpx_o_f16";
  case Vopc::Op::V_CMP_F_U64:
    return "v_cmp_f_u64";
  case Vopc::Op::V_CMP_LT_U64:
    return "v_cmp_lt_u64";
  case Vopc::Op::V_CMP_EQ_U64:
    return "v_cmp_eq_u64";
  case Vopc::Op::V_CMP_LE_U64:
    return "v_cmp_le_u64";
  case Vopc::Op::V_CMP_GT_U64:
    return "v_cmp_gt_u64";
  case Vopc::Op::V_CMP_NE_U64:
    return "v_cmp_ne_u64";
  case Vopc::Op::V_CMP_GE_U64:
    return "v_cmp_ge_u64";
  case Vopc::Op::V_CMP_T_U64:
    return "v_cmp_t_u64";
  case Vopc::Op::V_CMP_U_F16:
    return "v_cmp_u_f16";
  case Vopc::Op::V_CMP_NGE_F16:
    return "v_cmp_nge_f16";
  case Vopc::Op::V_CMP_NLG_F16:
    return "v_cmp_nlg_f16";
  case Vopc::Op::V_CMP_NGT_F16:
    return "v_cmp_ngt_f16";
  case Vopc::Op::V_CMP_NLE_F16:
    return "v_cmp_nle_f16";
  case Vopc::Op::V_CMP_NEQ_F16:
    return "v_cmp_neq_f16";
  case Vopc::Op::V_CMP_NLT_F16:
    return "v_cmp_nlt_f16";
  case Vopc::Op::V_CMP_TRU_F16:
    return "v_cmp_tru_f16";
  case Vopc::Op::V_CMPX_F_U64:
    return "v_cmpx_f_u64";
  case Vopc::Op::V_CMPX_LT_U64:
    return "v_cmpx_lt_u64";
  case Vopc::Op::V_CMPX_EQ_U64:
    return "v_cmpx_eq_u64";
  case Vopc::Op::V_CMPX_LE_U64:
    return "v_cmpx_le_u64";
  case Vopc::Op::V_CMPX_GT_U64:
    return "v_cmpx_gt_u64";
  case Vopc::Op::V_CMPX_NE_U64:
    return "v_cmpx_ne_u64";
  case Vopc::Op::V_CMPX_GE_U64:
    return "v_cmpx_ge_u64";
  case Vopc::Op::V_CMPX_T_U64:
    return "v_cmpx_t_u64";
  case Vopc::Op::V_CMPX_U_F16:
    return "v_cmpx_u_f16";
  case Vopc::Op::V_CMPX_NGE_F16:
    return "v_cmpx_nge_f16";
  case Vopc::Op::V_CMPX_NLG_F16:
    return "v_cmpx_nlg_f16";
  case Vopc::Op::V_CMPX_NGT_F16:
    return "v_cmpx_ngt_f16";
  case Vopc::Op::V_CMPX_NLE_F16:
    return "v_cmpx_nle_f16";
  case Vopc::Op::V_CMPX_NEQ_F16:
    return "v_cmpx_neq_f16";
  case Vopc::Op::V_CMPX_NLT_F16:
    return "v_cmpx_nlt_f16";
  case Vopc::Op::V_CMPX_TRU_F16:
    return "v_cmpx_tru_f16";

  default:
    return nullptr;
  }
}

const char *amdgpu::shader::vop3OpcodeToString(Vop3::Op op) {
  switch (op) {
  case Vop3::Op::V3_CMP_F_F32:
    return "v3_cmp_f_f32";
  case Vop3::Op::V3_CMP_LT_F32:
    return "v3_cmp_lt_f32";
  case Vop3::Op::V3_CMP_EQ_F32:
    return "v3_cmp_eq_f32";
  case Vop3::Op::V3_CMP_LE_F32:
    return "v3_cmp_le_f32";
  case Vop3::Op::V3_CMP_GT_F32:
    return "v3_cmp_gt_f32";
  case Vop3::Op::V3_CMP_LG_F32:
    return "v3_cmp_lg_f32";
  case Vop3::Op::V3_CMP_GE_F32:
    return "v3_cmp_ge_f32";
  case Vop3::Op::V3_CMP_O_F32:
    return "v3_cmp_o_f32";
  case Vop3::Op::V3_CMP_U_F32:
    return "v3_cmp_u_f32";
  case Vop3::Op::V3_CMP_NGE_F32:
    return "v3_cmp_nge_f32";
  case Vop3::Op::V3_CMP_NLG_F32:
    return "v3_cmp_nlg_f32";
  case Vop3::Op::V3_CMP_NGT_F32:
    return "v3_cmp_ngt_f32";
  case Vop3::Op::V3_CMP_NLE_F32:
    return "v3_cmp_nle_f32";
  case Vop3::Op::V3_CMP_NEQ_F32:
    return "v3_cmp_neq_f32";
  case Vop3::Op::V3_CMP_NLT_F32:
    return "v3_cmp_nlt_f32";
  case Vop3::Op::V3_CMP_TRU_F32:
    return "v3_cmp_tru_f32";
  case Vop3::Op::V3_CMPX_F_F32:
    return "v3_cmpx_f_f32";
  case Vop3::Op::V3_CMPX_LT_F32:
    return "v3_cmpx_lt_f32";
  case Vop3::Op::V3_CMPX_EQ_F32:
    return "v3_cmpx_eq_f32";
  case Vop3::Op::V3_CMPX_LE_F32:
    return "v3_cmpx_le_f32";
  case Vop3::Op::V3_CMPX_GT_F32:
    return "v3_cmpx_gt_f32";
  case Vop3::Op::V3_CMPX_LG_F32:
    return "v3_cmpx_lg_f32";
  case Vop3::Op::V3_CMPX_GE_F32:
    return "v3_cmpx_ge_f32";
  case Vop3::Op::V3_CMPX_O_F32:
    return "v3_cmpx_o_f32";
  case Vop3::Op::V3_CMPX_U_F32:
    return "v3_cmpx_u_f32";
  case Vop3::Op::V3_CMPX_NGE_F32:
    return "v3_cmpx_nge_f32";
  case Vop3::Op::V3_CMPX_NLG_F32:
    return "v3_cmpx_nlg_f32";
  case Vop3::Op::V3_CMPX_NGT_F32:
    return "v3_cmpx_ngt_f32";
  case Vop3::Op::V3_CMPX_NLE_F32:
    return "v3_cmpx_nle_f32";
  case Vop3::Op::V3_CMPX_NEQ_F32:
    return "v3_cmpx_neq_f32";
  case Vop3::Op::V3_CMPX_NLT_F32:
    return "v3_cmpx_nlt_f32";
  case Vop3::Op::V3_CMPX_TRU_F32:
    return "v3_cmpx_tru_f32";
  case Vop3::Op::V3_CMP_F_F64:
    return "v3_cmp_f_f64";
  case Vop3::Op::V3_CMP_LT_F64:
    return "v3_cmp_lt_f64";
  case Vop3::Op::V3_CMP_EQ_F64:
    return "v3_cmp_eq_f64";
  case Vop3::Op::V3_CMP_LE_F64:
    return "v3_cmp_le_f64";
  case Vop3::Op::V3_CMP_GT_F64:
    return "v3_cmp_gt_f64";
  case Vop3::Op::V3_CMP_LG_F64:
    return "v3_cmp_lg_f64";
  case Vop3::Op::V3_CMP_GE_F64:
    return "v3_cmp_ge_f64";
  case Vop3::Op::V3_CMP_O_F64:
    return "v3_cmp_o_f64";
  case Vop3::Op::V3_CMP_U_F64:
    return "v3_cmp_u_f64";
  case Vop3::Op::V3_CMP_NGE_F64:
    return "v3_cmp_nge_f64";
  case Vop3::Op::V3_CMP_NLG_F64:
    return "v3_cmp_nlg_f64";
  case Vop3::Op::V3_CMP_NGT_F64:
    return "v3_cmp_ngt_f64";
  case Vop3::Op::V3_CMP_NLE_F64:
    return "v3_cmp_nle_f64";
  case Vop3::Op::V3_CMP_NEQ_F64:
    return "v3_cmp_neq_f64";
  case Vop3::Op::V3_CMP_NLT_F64:
    return "v3_cmp_nlt_f64";
  case Vop3::Op::V3_CMP_TRU_F64:
    return "v3_cmp_tru_f64";
  case Vop3::Op::V3_CMPX_F_F64:
    return "v3_cmpx_f_f64";
  case Vop3::Op::V3_CMPX_LT_F64:
    return "v3_cmpx_lt_f64";
  case Vop3::Op::V3_CMPX_EQ_F64:
    return "v3_cmpx_eq_f64";
  case Vop3::Op::V3_CMPX_LE_F64:
    return "v3_cmpx_le_f64";
  case Vop3::Op::V3_CMPX_GT_F64:
    return "v3_cmpx_gt_f64";
  case Vop3::Op::V3_CMPX_LG_F64:
    return "v3_cmpx_lg_f64";
  case Vop3::Op::V3_CMPX_GE_F64:
    return "v3_cmpx_ge_f64";
  case Vop3::Op::V3_CMPX_O_F64:
    return "v3_cmpx_o_f64";
  case Vop3::Op::V3_CMPX_U_F64:
    return "v3_cmpx_u_f64";
  case Vop3::Op::V3_CMPX_NGE_F64:
    return "v3_cmpx_nge_f64";
  case Vop3::Op::V3_CMPX_NLG_F64:
    return "v3_cmpx_nlg_f64";
  case Vop3::Op::V3_CMPX_NGT_F64:
    return "v3_cmpx_ngt_f64";
  case Vop3::Op::V3_CMPX_NLE_F64:
    return "v3_cmpx_nle_f64";
  case Vop3::Op::V3_CMPX_NEQ_F64:
    return "v3_cmpx_neq_f64";
  case Vop3::Op::V3_CMPX_NLT_F64:
    return "v3_cmpx_nlt_f64";
  case Vop3::Op::V3_CMPX_TRU_F64:
    return "v3_cmpx_tru_f64";
  case Vop3::Op::V3_CMPS_F_F32:
    return "v3_cmps_f_f32";
  case Vop3::Op::V3_CMPS_LT_F32:
    return "v3_cmps_lt_f32";
  case Vop3::Op::V3_CMPS_EQ_F32:
    return "v3_cmps_eq_f32";
  case Vop3::Op::V3_CMPS_LE_F32:
    return "v3_cmps_le_f32";
  case Vop3::Op::V3_CMPS_GT_F32:
    return "v3_cmps_gt_f32";
  case Vop3::Op::V3_CMPS_LG_F32:
    return "v3_cmps_lg_f32";
  case Vop3::Op::V3_CMPS_GE_F32:
    return "v3_cmps_ge_f32";
  case Vop3::Op::V3_CMPS_O_F32:
    return "v3_cmps_o_f32";
  case Vop3::Op::V3_CMPS_U_F32:
    return "v3_cmps_u_f32";
  case Vop3::Op::V3_CMPS_NGE_F32:
    return "v3_cmps_nge_f32";
  case Vop3::Op::V3_CMPS_NLG_F32:
    return "v3_cmps_nlg_f32";
  case Vop3::Op::V3_CMPS_NGT_F32:
    return "v3_cmps_ngt_f32";
  case Vop3::Op::V3_CMPS_NLE_F32:
    return "v3_cmps_nle_f32";
  case Vop3::Op::V3_CMPS_NEQ_F32:
    return "v3_cmps_neq_f32";
  case Vop3::Op::V3_CMPS_NLT_F32:
    return "v3_cmps_nlt_f32";
  case Vop3::Op::V3_CMPS_TRU_F32:
    return "v3_cmps_tru_f32";
  case Vop3::Op::V3_CMPSX_F_F32:
    return "v3_cmpsx_f_f32";
  case Vop3::Op::V3_CMPSX_LT_F32:
    return "v3_cmpsx_lt_f32";
  case Vop3::Op::V3_CMPSX_EQ_F32:
    return "v3_cmpsx_eq_f32";
  case Vop3::Op::V3_CMPSX_LE_F32:
    return "v3_cmpsx_le_f32";
  case Vop3::Op::V3_CMPSX_GT_F32:
    return "v3_cmpsx_gt_f32";
  case Vop3::Op::V3_CMPSX_LG_F32:
    return "v3_cmpsx_lg_f32";
  case Vop3::Op::V3_CMPSX_GE_F32:
    return "v3_cmpsx_ge_f32";
  case Vop3::Op::V3_CMPSX_O_F32:
    return "v3_cmpsx_o_f32";
  case Vop3::Op::V3_CMPSX_U_F32:
    return "v3_cmpsx_u_f32";
  case Vop3::Op::V3_CMPSX_NGE_F32:
    return "v3_cmpsx_nge_f32";
  case Vop3::Op::V3_CMPSX_NLG_F32:
    return "v3_cmpsx_nlg_f32";
  case Vop3::Op::V3_CMPSX_NGT_F32:
    return "v3_cmpsx_ngt_f32";
  case Vop3::Op::V3_CMPSX_NLE_F32:
    return "v3_cmpsx_nle_f32";
  case Vop3::Op::V3_CMPSX_NEQ_F32:
    return "v3_cmpsx_neq_f32";
  case Vop3::Op::V3_CMPSX_NLT_F32:
    return "v3_cmpsx_nlt_f32";
  case Vop3::Op::V3_CMPSX_TRU_F32:
    return "v3_cmpsx_tru_f32";
  case Vop3::Op::V3_CMPS_F_F64:
    return "v3_cmps_f_f64";
  case Vop3::Op::V3_CMPS_LT_F64:
    return "v3_cmps_lt_f64";
  case Vop3::Op::V3_CMPS_EQ_F64:
    return "v3_cmps_eq_f64";
  case Vop3::Op::V3_CMPS_LE_F64:
    return "v3_cmps_le_f64";
  case Vop3::Op::V3_CMPS_GT_F64:
    return "v3_cmps_gt_f64";
  case Vop3::Op::V3_CMPS_LG_F64:
    return "v3_cmps_lg_f64";
  case Vop3::Op::V3_CMPS_GE_F64:
    return "v3_cmps_ge_f64";
  case Vop3::Op::V3_CMPS_O_F64:
    return "v3_cmps_o_f64";
  case Vop3::Op::V3_CMPS_U_F64:
    return "v3_cmps_u_f64";
  case Vop3::Op::V3_CMPS_NGE_F64:
    return "v3_cmps_nge_f64";
  case Vop3::Op::V3_CMPS_NLG_F64:
    return "v3_cmps_nlg_f64";
  case Vop3::Op::V3_CMPS_NGT_F64:
    return "v3_cmps_ngt_f64";
  case Vop3::Op::V3_CMPS_NLE_F64:
    return "v3_cmps_nle_f64";
  case Vop3::Op::V3_CMPS_NEQ_F64:
    return "v3_cmps_neq_f64";
  case Vop3::Op::V3_CMPS_NLT_F64:
    return "v3_cmps_nlt_f64";
  case Vop3::Op::V3_CMPS_TRU_F64:
    return "v3_cmps_tru_f64";
  case Vop3::Op::V3_CMPSX_F_F64:
    return "v3_cmpsx_f_f64";
  case Vop3::Op::V3_CMPSX_LT_F64:
    return "v3_cmpsx_lt_f64";
  case Vop3::Op::V3_CMPSX_EQ_F64:
    return "v3_cmpsx_eq_f64";
  case Vop3::Op::V3_CMPSX_LE_F64:
    return "v3_cmpsx_le_f64";
  case Vop3::Op::V3_CMPSX_GT_F64:
    return "v3_cmpsx_gt_f64";
  case Vop3::Op::V3_CMPSX_LG_F64:
    return "v3_cmpsx_lg_f64";
  case Vop3::Op::V3_CMPSX_GE_F64:
    return "v3_cmpsx_ge_f64";
  case Vop3::Op::V3_CMPSX_O_F64:
    return "v3_cmpsx_o_f64";
  case Vop3::Op::V3_CMPSX_U_F64:
    return "v3_cmpsx_u_f64";
  case Vop3::Op::V3_CMPSX_NGE_F64:
    return "v3_cmpsx_nge_f64";
  case Vop3::Op::V3_CMPSX_NLG_F64:
    return "v3_cmpsx_nlg_f64";
  case Vop3::Op::V3_CMPSX_NGT_F64:
    return "v3_cmpsx_ngt_f64";
  case Vop3::Op::V3_CMPSX_NLE_F64:
    return "v3_cmpsx_nle_f64";
  case Vop3::Op::V3_CMPSX_NEQ_F64:
    return "v3_cmpsx_neq_f64";
  case Vop3::Op::V3_CMPSX_NLT_F64:
    return "v3_cmpsx_nlt_f64";
  case Vop3::Op::V3_CMPSX_TRU_F64:
    return "v3_cmpsx_tru_f64";
  case Vop3::Op::V3_CMP_F_I32:
    return "v3_cmp_f_i32";
  case Vop3::Op::V3_CMP_LT_I32:
    return "v3_cmp_lt_i32";
  case Vop3::Op::V3_CMP_EQ_I32:
    return "v3_cmp_eq_i32";
  case Vop3::Op::V3_CMP_LE_I32:
    return "v3_cmp_le_i32";
  case Vop3::Op::V3_CMP_GT_I32:
    return "v3_cmp_gt_i32";
  case Vop3::Op::V3_CMP_NE_I32:
    return "v3_cmp_ne_i32";
  case Vop3::Op::V3_CMP_GE_I32:
    return "v3_cmp_ge_i32";
  case Vop3::Op::V3_CMP_T_I32:
    return "v3_cmp_t_i32";
  case Vop3::Op::V3_CMP_CLASS_F32:
    return "v3_cmp_class_f32";
  case Vop3::Op::V3_CMP_LT_I16:
    return "v3_cmp_lt_i16";
  case Vop3::Op::V3_CMP_EQ_I16:
    return "v3_cmp_eq_i16";
  case Vop3::Op::V3_CMP_LE_I16:
    return "v3_cmp_le_i16";
  case Vop3::Op::V3_CMP_GT_I16:
    return "v3_cmp_gt_i16";
  case Vop3::Op::V3_CMP_NE_I16:
    return "v3_cmp_ne_i16";
  case Vop3::Op::V3_CMP_GE_I16:
    return "v3_cmp_ge_i16";
  case Vop3::Op::V3_CMP_CLASS_F16:
    return "v3_cmp_class_f16";
  case Vop3::Op::V3_CMPX_F_I32:
    return "v3_cmpx_f_i32";
  case Vop3::Op::V3_CMPX_LT_I32:
    return "v3_cmpx_lt_i32";
  case Vop3::Op::V3_CMPX_EQ_I32:
    return "v3_cmpx_eq_i32";
  case Vop3::Op::V3_CMPX_LE_I32:
    return "v3_cmpx_le_i32";
  case Vop3::Op::V3_CMPX_GT_I32:
    return "v3_cmpx_gt_i32";
  case Vop3::Op::V3_CMPX_NE_I32:
    return "v3_cmpx_ne_i32";
  case Vop3::Op::V3_CMPX_GE_I32:
    return "v3_cmpx_ge_i32";
  case Vop3::Op::V3_CMPX_T_I32:
    return "v3_cmpx_t_i32";
  case Vop3::Op::V3_CMPX_CLASS_F32:
    return "v3_cmpx_class_f32";
  case Vop3::Op::V3_CMPX_LT_I16:
    return "v3_cmpx_lt_i16";
  case Vop3::Op::V3_CMPX_EQ_I16:
    return "v3_cmpx_eq_i16";
  case Vop3::Op::V3_CMPX_LE_I16:
    return "v3_cmpx_le_i16";
  case Vop3::Op::V3_CMPX_GT_I16:
    return "v3_cmpx_gt_i16";
  case Vop3::Op::V3_CMPX_NE_I16:
    return "v3_cmpx_ne_i16";
  case Vop3::Op::V3_CMPX_GE_I16:
    return "v3_cmpx_ge_i16";
  case Vop3::Op::V3_CMPX_CLASS_F16:
    return "v3_cmpx_class_f16";
  case Vop3::Op::V3_CMP_F_I64:
    return "v3_cmp_f_i64";
  case Vop3::Op::V3_CMP_LT_I64:
    return "v3_cmp_lt_i64";
  case Vop3::Op::V3_CMP_EQ_I64:
    return "v3_cmp_eq_i64";
  case Vop3::Op::V3_CMP_LE_I64:
    return "v3_cmp_le_i64";
  case Vop3::Op::V3_CMP_GT_I64:
    return "v3_cmp_gt_i64";
  case Vop3::Op::V3_CMP_NE_I64:
    return "v3_cmp_ne_i64";
  case Vop3::Op::V3_CMP_GE_I64:
    return "v3_cmp_ge_i64";
  case Vop3::Op::V3_CMP_T_I64:
    return "v3_cmp_t_i64";
  case Vop3::Op::V3_CMP_CLASS_F64:
    return "v3_cmp_class_f64";
  case Vop3::Op::V3_CMP_LT_U16:
    return "v3_cmp_lt_u16";
  case Vop3::Op::V3_CMP_EQ_U16:
    return "v3_cmp_eq_u16";
  case Vop3::Op::V3_CMP_LE_U16:
    return "v3_cmp_le_u16";
  case Vop3::Op::V3_CMP_GT_U16:
    return "v3_cmp_gt_u16";
  case Vop3::Op::V3_CMP_NE_U16:
    return "v3_cmp_ne_u16";
  case Vop3::Op::V3_CMP_GE_U16:
    return "v3_cmp_ge_u16";
  case Vop3::Op::V3_CMPX_F_I64:
    return "v3_cmpx_f_i64";
  case Vop3::Op::V3_CMPX_LT_I64:
    return "v3_cmpx_lt_i64";
  case Vop3::Op::V3_CMPX_EQ_I64:
    return "v3_cmpx_eq_i64";
  case Vop3::Op::V3_CMPX_LE_I64:
    return "v3_cmpx_le_i64";
  case Vop3::Op::V3_CMPX_GT_I64:
    return "v3_cmpx_gt_i64";
  case Vop3::Op::V3_CMPX_NE_I64:
    return "v3_cmpx_ne_i64";
  case Vop3::Op::V3_CMPX_GE_I64:
    return "v3_cmpx_ge_i64";
  case Vop3::Op::V3_CMPX_T_I64:
    return "v3_cmpx_t_i64";
  case Vop3::Op::V3_CMPX_CLASS_F64:
    return "v3_cmpx_class_f64";
  case Vop3::Op::V3_CMPX_LT_U16:
    return "v3_cmpx_lt_u16";
  case Vop3::Op::V3_CMPX_EQ_U16:
    return "v3_cmpx_eq_u16";
  case Vop3::Op::V3_CMPX_LE_U16:
    return "v3_cmpx_le_u16";
  case Vop3::Op::V3_CMPX_GT_U16:
    return "v3_cmpx_gt_u16";
  case Vop3::Op::V3_CMPX_NE_U16:
    return "v3_cmpx_ne_u16";
  case Vop3::Op::V3_CMPX_GE_U16:
    return "v3_cmpx_ge_u16";
  case Vop3::Op::V3_CMP_F_U32:
    return "v3_cmp_f_u32";
  case Vop3::Op::V3_CMP_LT_U32:
    return "v3_cmp_lt_u32";
  case Vop3::Op::V3_CMP_EQ_U32:
    return "v3_cmp_eq_u32";
  case Vop3::Op::V3_CMP_LE_U32:
    return "v3_cmp_le_u32";
  case Vop3::Op::V3_CMP_GT_U32:
    return "v3_cmp_gt_u32";
  case Vop3::Op::V3_CMP_NE_U32:
    return "v3_cmp_ne_u32";
  case Vop3::Op::V3_CMP_GE_U32:
    return "v3_cmp_ge_u32";
  case Vop3::Op::V3_CMP_T_U32:
    return "v3_cmp_t_u32";
  case Vop3::Op::V3_CMP_F_F16:
    return "v3_cmp_f_f16";
  case Vop3::Op::V3_CMP_LT_F16:
    return "v3_cmp_lt_f16";
  case Vop3::Op::V3_CMP_EQ_F16:
    return "v3_cmp_eq_f16";
  case Vop3::Op::V3_CMP_LE_F16:
    return "v3_cmp_le_f16";
  case Vop3::Op::V3_CMP_GT_F16:
    return "v3_cmp_gt_f16";
  case Vop3::Op::V3_CMP_LG_F16:
    return "v3_cmp_lg_f16";
  case Vop3::Op::V3_CMP_GE_F16:
    return "v3_cmp_ge_f16";
  case Vop3::Op::V3_CMP_O_F16:
    return "v3_cmp_o_f16";
  case Vop3::Op::V3_CMPX_F_U32:
    return "v3_cmpx_f_u32";
  case Vop3::Op::V3_CMPX_LT_U32:
    return "v3_cmpx_lt_u32";
  case Vop3::Op::V3_CMPX_EQ_U32:
    return "v3_cmpx_eq_u32";
  case Vop3::Op::V3_CMPX_LE_U32:
    return "v3_cmpx_le_u32";
  case Vop3::Op::V3_CMPX_GT_U32:
    return "v3_cmpx_gt_u32";
  case Vop3::Op::V3_CMPX_NE_U32:
    return "v3_cmpx_ne_u32";
  case Vop3::Op::V3_CMPX_GE_U32:
    return "v3_cmpx_ge_u32";
  case Vop3::Op::V3_CMPX_T_U32:
    return "v3_cmpx_t_u32";
  case Vop3::Op::V3_CMPX_F_F16:
    return "v3_cmpx_f_f16";
  case Vop3::Op::V3_CMPX_LT_F16:
    return "v3_cmpx_lt_f16";
  case Vop3::Op::V3_CMPX_EQ_F16:
    return "v3_cmpx_eq_f16";
  case Vop3::Op::V3_CMPX_LE_F16:
    return "v3_cmpx_le_f16";
  case Vop3::Op::V3_CMPX_GT_F16:
    return "v3_cmpx_gt_f16";
  case Vop3::Op::V3_CMPX_LG_F16:
    return "v3_cmpx_lg_f16";
  case Vop3::Op::V3_CMPX_GE_F16:
    return "v3_cmpx_ge_f16";
  case Vop3::Op::V3_CMPX_O_F16:
    return "v3_cmpx_o_f16";
  case Vop3::Op::V3_CMP_F_U64:
    return "v3_cmp_f_u64";
  case Vop3::Op::V3_CMP_LT_U64:
    return "v3_cmp_lt_u64";
  case Vop3::Op::V3_CMP_EQ_U64:
    return "v3_cmp_eq_u64";
  case Vop3::Op::V3_CMP_LE_U64:
    return "v3_cmp_le_u64";
  case Vop3::Op::V3_CMP_GT_U64:
    return "v3_cmp_gt_u64";
  case Vop3::Op::V3_CMP_NE_U64:
    return "v3_cmp_ne_u64";
  case Vop3::Op::V3_CMP_GE_U64:
    return "v3_cmp_ge_u64";
  case Vop3::Op::V3_CMP_T_U64:
    return "v3_cmp_t_u64";
  case Vop3::Op::V3_CMP_U_F16:
    return "v3_cmp_u_f16";
  case Vop3::Op::V3_CMP_NGE_F16:
    return "v3_cmp_nge_f16";
  case Vop3::Op::V3_CMP_NLG_F16:
    return "v3_cmp_nlg_f16";
  case Vop3::Op::V3_CMP_NGT_F16:
    return "v3_cmp_ngt_f16";
  case Vop3::Op::V3_CMP_NLE_F16:
    return "v3_cmp_nle_f16";
  case Vop3::Op::V3_CMP_NEQ_F16:
    return "v3_cmp_neq_f16";
  case Vop3::Op::V3_CMP_NLT_F16:
    return "v3_cmp_nlt_f16";
  case Vop3::Op::V3_CMP_TRU_F16:
    return "v3_cmp_tru_f16";
  case Vop3::Op::V3_CMPX_F_U64:
    return "v3_cmpx_f_u64";
  case Vop3::Op::V3_CMPX_LT_U64:
    return "v3_cmpx_lt_u64";
  case Vop3::Op::V3_CMPX_EQ_U64:
    return "v3_cmpx_eq_u64";
  case Vop3::Op::V3_CMPX_LE_U64:
    return "v3_cmpx_le_u64";
  case Vop3::Op::V3_CMPX_GT_U64:
    return "v3_cmpx_gt_u64";
  case Vop3::Op::V3_CMPX_NE_U64:
    return "v3_cmpx_ne_u64";
  case Vop3::Op::V3_CMPX_GE_U64:
    return "v3_cmpx_ge_u64";
  case Vop3::Op::V3_CMPX_T_U64:
    return "v3_cmpx_t_u64";
  case Vop3::Op::V3_CNDMASK_B32:
    return "v3_cndmask_b32";
  case Vop3::Op::V3_READLANE_B32:
    return "v3_readlane_b32";
  case Vop3::Op::V3_WRITELANE_B32:
    return "v3_writelane_b32";
  case Vop3::Op::V3_ADD_F32:
    return "v3_add_f32";
  case Vop3::Op::V3_SUB_F32:
    return "v3_sub_f32";
  case Vop3::Op::V3_SUBREV_F32:
    return "v3_subrev_f32";
  case Vop3::Op::V3_MAC_LEGACY_F32:
    return "v3_mac_legacy_f32";
  case Vop3::Op::V3_MUL_LEGACY_F32:
    return "v3_mul_legacy_f32";
  case Vop3::Op::V3_MUL_F32:
    return "v3_mul_f32";
  case Vop3::Op::V3_MUL_I32_I24:
    return "v3_mul_i32_i24";
  case Vop3::Op::V3_MUL_HI_I32_I24:
    return "v3_mul_hi_i32_i24";
  case Vop3::Op::V3_MUL_U32_U24:
    return "v3_mul_u32_u24";
  case Vop3::Op::V3_MUL_HI_U32_U24:
    return "v3_mul_hi_u32_u24";
  case Vop3::Op::V3_MIN_LEGACY_F32:
    return "v3_min_legacy_f32";
  case Vop3::Op::V3_MAX_LEGACY_F32:
    return "v3_max_legacy_f32";
  case Vop3::Op::V3_MIN_F32:
    return "v3_min_f32";
  case Vop3::Op::V3_MAX_F32:
    return "v3_max_f32";
  case Vop3::Op::V3_MIN_I32:
    return "v3_min_i32";
  case Vop3::Op::V3_MAX_I32:
    return "v3_max_i32";
  case Vop3::Op::V3_MIN_U32:
    return "v3_min_u32";
  case Vop3::Op::V3_MAX_U32:
    return "v3_max_u32";
  case Vop3::Op::V3_LSHR_B32:
    return "v3_lshr_b32";
  case Vop3::Op::V3_LSHRREV_B32:
    return "v3_lshrrev_b32";
  case Vop3::Op::V3_ASHR_I32:
    return "v3_ashr_i32";
  case Vop3::Op::V3_ASHRREV_I32:
    return "v3_ashrrev_i32";
  case Vop3::Op::V3_LSHL_B32:
    return "v3_lshl_b32";
  case Vop3::Op::V3_LSHLREV_B32:
    return "v3_lshlrev_b32";
  case Vop3::Op::V3_AND_B32:
    return "v3_and_b32";
  case Vop3::Op::V3_OR_B32:
    return "v3_or_b32";
  case Vop3::Op::V3_XOR_B32:
    return "v3_xor_b32";
  case Vop3::Op::V3_BFM_B32:
    return "v3_bfm_b32";
  case Vop3::Op::V3_MAC_F32:
    return "v3_mac_f32";
  case Vop3::Op::V3_MADMK_F32:
    return "v3_madmk_f32";
  case Vop3::Op::V3_MADAK_F32:
    return "v3_madak_f32";
  case Vop3::Op::V3_BCNT_U32_B32:
    return "v3_bcnt_u32_b32";
  case Vop3::Op::V3_MBCNT_LO_U32_B32:
    return "v3_mbcnt_lo_u32_b32";
  case Vop3::Op::V3_MBCNT_HI_U32_B32:
    return "v3_mbcnt_hi_u32_b32";
  case Vop3::Op::V3_ADD_I32:
    return "v3_add_i32";
  case Vop3::Op::V3_SUB_I32:
    return "v3_sub_i32";
  case Vop3::Op::V3_SUBREV_I32:
    return "v3_subrev_i32";
  case Vop3::Op::V3_ADDC_U32:
    return "v3_addc_u32";
  case Vop3::Op::V3_SUBB_U32:
    return "v3_subb_u32";
  case Vop3::Op::V3_SUBBREV_U32:
    return "v3_subbrev_u32";
  case Vop3::Op::V3_LDEXP_F32:
    return "v3_ldexp_f32";
  case Vop3::Op::V3_CVT_PKACCUM_U8_F32:
    return "v3_cvt_pkaccum_u8_f32";
  case Vop3::Op::V3_CVT_PKNORM_I16_F32:
    return "v3_cvt_pknorm_i16_f32";
  case Vop3::Op::V3_CVT_PKNORM_U16_F32:
    return "v3_cvt_pknorm_u16_f32";
  case Vop3::Op::V3_CVT_PKRTZ_F16_F32:
    return "v3_cvt_pkrtz_f16_f32";
  case Vop3::Op::V3_CVT_PK_U16_U32:
    return "v3_cvt_pk_u16_u32";
  case Vop3::Op::V3_CVT_PK_I16_I32:
    return "v3_cvt_pk_i16_i32";
  case Vop3::Op::V3_MAD_LEGACY_F32:
    return "v3_mad_legacy_f32";
  case Vop3::Op::V3_MAD_F32:
    return "v3_mad_f32";
  case Vop3::Op::V3_MAD_I32_I24:
    return "v3_mad_i32_i24";
  case Vop3::Op::V3_MAD_U32_U24:
    return "v3_mad_u32_u24";
  case Vop3::Op::V3_CUBEID_F32:
    return "v3_cubeid_f32";
  case Vop3::Op::V3_CUBESC_F32:
    return "v3_cubesc_f32";
  case Vop3::Op::V3_CUBETC_F32:
    return "v3_cubetc_f32";
  case Vop3::Op::V3_CUBEMA_F32:
    return "v3_cubema_f32";
  case Vop3::Op::V3_BFE_U32:
    return "v3_bfe_u32";
  case Vop3::Op::V3_BFE_I32:
    return "v3_bfe_i32";
  case Vop3::Op::V3_BFI_B32:
    return "v3_bfi_b32";
  case Vop3::Op::V3_FMA_F32:
    return "v3_fma_f32";
  case Vop3::Op::V3_FMA_F64:
    return "v3_fma_f64";
  case Vop3::Op::V3_LERP_U8:
    return "v3_lerp_u8";
  case Vop3::Op::V3_ALIGNBIT_B32:
    return "v3_alignbit_b32";
  case Vop3::Op::V3_ALIGNBYTE_B32:
    return "v3_alignbyte_b32";
  case Vop3::Op::V3_MULLIT_F32:
    return "v3_mullit_f32";
  case Vop3::Op::V3_MIN3_F32:
    return "v3_min3_f32";
  case Vop3::Op::V3_MIN3_I32:
    return "v3_min3_i32";
  case Vop3::Op::V3_MIN3_U32:
    return "v3_min3_u32";
  case Vop3::Op::V3_MAX3_F32:
    return "v3_max3_f32";
  case Vop3::Op::V3_MAX3_I32:
    return "v3_max3_i32";
  case Vop3::Op::V3_MAX3_U32:
    return "v3_max3_u32";
  case Vop3::Op::V3_MED3_F32:
    return "v3_med3_f32";
  case Vop3::Op::V3_MED3_I32:
    return "v3_med3_i32";
  case Vop3::Op::V3_MED3_U32:
    return "v3_med3_u32";
  case Vop3::Op::V3_SAD_U8:
    return "v3_sad_u8";
  case Vop3::Op::V3_SAD_HI_U8:
    return "v3_sad_hi_u8";
  case Vop3::Op::V3_SAD_U16:
    return "v3_sad_u16";
  case Vop3::Op::V3_SAD_U32:
    return "v3_sad_u32";
  case Vop3::Op::V3_CVT_PK_U8_F32:
    return "v3_cvt_pk_u8_f32";
  case Vop3::Op::V3_DIV_FIXUP_F32:
    return "v3_div_fixup_f32";
  case Vop3::Op::V3_DIV_FIXUP_F64:
    return "v3_div_fixup_f64";
  case Vop3::Op::V3_LSHL_B64:
    return "v3_lshl_b64";
  case Vop3::Op::V3_LSHR_B64:
    return "v3_lshr_b64";
  case Vop3::Op::V3_ASHR_I64:
    return "v3_ashr_i64";
  case Vop3::Op::V3_ADD_F64:
    return "v3_add_f64";
  case Vop3::Op::V3_MUL_F64:
    return "v3_mul_f64";
  case Vop3::Op::V3_MIN_F64:
    return "v3_min_f64";
  case Vop3::Op::V3_MAX_F64:
    return "v3_max_f64";
  case Vop3::Op::V3_LDEXP_F64:
    return "v3_ldexp_f64";
  case Vop3::Op::V3_MUL_LO_U32:
    return "v3_mul_lo_u32";
  case Vop3::Op::V3_MUL_HI_U32:
    return "v3_mul_hi_u32";
  case Vop3::Op::V3_MUL_LO_I32:
    return "v3_mul_lo_i32";
  case Vop3::Op::V3_MUL_HI_I32:
    return "v3_mul_hi_i32";
  case Vop3::Op::V3_DIV_SCALE_F32:
    return "v3_div_scale_f32";
  case Vop3::Op::V3_DIV_SCALE_F64:
    return "v3_div_scale_f64";
  case Vop3::Op::V3_DIV_FMAS_F32:
    return "v3_div_fmas_f32";
  case Vop3::Op::V3_DIV_FMAS_F64:
    return "v3_div_fmas_f64";
  case Vop3::Op::V3_MSAD_U8:
    return "v3_msad_u8";
  case Vop3::Op::V3_QSAD_U8:
    return "v3_qsad_u8";
  case Vop3::Op::V3_MQSAD_U8:
    return "v3_mqsad_u8";
  case Vop3::Op::V3_TRIG_PREOP_F64:
    return "v3_trig_preop_f64";
  case Vop3::Op::V3_NOP:
    return "v3_nop";
  case Vop3::Op::V3_MOV_B32:
    return "v3_mov_b32";
  case Vop3::Op::V3_READFIRSTLANE_B32:
    return "v3_readfirstlane_b32";
  case Vop3::Op::V3_CVT_I32_F64:
    return "v3_cvt_i32_f64";
  case Vop3::Op::V3_CVT_F64_I32:
    return "v3_cvt_f64_i32";
  case Vop3::Op::V3_CVT_F32_I32:
    return "v3_cvt_f32_i32";
  case Vop3::Op::V3_CVT_F32_U32:
    return "v3_cvt_f32_u32";
  case Vop3::Op::V3_CVT_U32_F32:
    return "v3_cvt_u32_f32";
  case Vop3::Op::V3_CVT_I32_F32:
    return "v3_cvt_i32_f32";
  case Vop3::Op::V3_MOV_FED_B32:
    return "v3_mov_fed_b32";
  case Vop3::Op::V3_CVT_F16_F32:
    return "v3_cvt_f16_f32";
  case Vop3::Op::V3_CVT_F32_F16:
    return "v3_cvt_f32_f16";
  case Vop3::Op::V3_CVT_RPI_I32_F32:
    return "v3_cvt_rpi_i32_f32";
  case Vop3::Op::V3_CVT_FLR_I32_F32:
    return "v3_cvt_flr_i32_f32";
  case Vop3::Op::V3_CVT_OFF_F32_I4:
    return "v3_cvt_off_f32_i4";
  case Vop3::Op::V3_CVT_F32_F64:
    return "v3_cvt_f32_f64";
  case Vop3::Op::V3_CVT_F64_F32:
    return "v3_cvt_f64_f32";
  case Vop3::Op::V3_CVT_F32_UBYTE0:
    return "v3_cvt_f32_ubyte0";
  case Vop3::Op::V3_CVT_F32_UBYTE1:
    return "v3_cvt_f32_ubyte1";
  case Vop3::Op::V3_CVT_F32_UBYTE2:
    return "v3_cvt_f32_ubyte2";
  case Vop3::Op::V3_CVT_F32_UBYTE3:
    return "v3_cvt_f32_ubyte3";
  case Vop3::Op::V3_CVT_U32_F64:
    return "v3_cvt_u32_f64";
  case Vop3::Op::V3_CVT_F64_U32:
    return "v3_cvt_f64_u32";
  case Vop3::Op::V3_FRACT_F32:
    return "v3_fract_f32";
  case Vop3::Op::V3_TRUNC_F32:
    return "v3_trunc_f32";
  case Vop3::Op::V3_CEIL_F32:
    return "v3_ceil_f32";
  case Vop3::Op::V3_RNDNE_F32:
    return "v3_rndne_f32";
  case Vop3::Op::V3_FLOOR_F32:
    return "v3_floor_f32";
  case Vop3::Op::V3_EXP_F32:
    return "v3_exp_f32";
  case Vop3::Op::V3_LOG_CLAMP_F32:
    return "v3_log_clamp_f32";
  case Vop3::Op::V3_LOG_F32:
    return "v3_log_f32";
  case Vop3::Op::V3_RCP_CLAMP_F32:
    return "v3_rcp_clamp_f32";
  case Vop3::Op::V3_RCP_LEGACY_F32:
    return "v3_rcp_legacy_f32";
  case Vop3::Op::V3_RCP_F32:
    return "v3_rcp_f32";
  case Vop3::Op::V3_RCP_IFLAG_F32:
    return "v3_rcp_iflag_f32";
  case Vop3::Op::V3_RSQ_CLAMP_F32:
    return "v3_rsq_clamp_f32";
  case Vop3::Op::V3_RSQ_LEGACY_F32:
    return "v3_rsq_legacy_f32";
  case Vop3::Op::V3_RSQ_F32:
    return "v3_rsq_f32";
  case Vop3::Op::V3_RCP_F64:
    return "v3_rcp_f64";
  case Vop3::Op::V3_RCP_CLAMP_F64:
    return "v3_rcp_clamp_f64";
  case Vop3::Op::V3_RSQ_F64:
    return "v3_rsq_f64";
  case Vop3::Op::V3_RSQ_CLAMP_F64:
    return "v3_rsq_clamp_f64";
  case Vop3::Op::V3_SQRT_F32:
    return "v3_sqrt_f32";
  case Vop3::Op::V3_SQRT_F64:
    return "v3_sqrt_f64";
  case Vop3::Op::V3_SIN_F32:
    return "v3_sin_f32";
  case Vop3::Op::V3_COS_F32:
    return "v3_cos_f32";
  case Vop3::Op::V3_NOT_B32:
    return "v3_not_b32";
  case Vop3::Op::V3_BFREV_B32:
    return "v3_bfrev_b32";
  case Vop3::Op::V3_FFBH_U32:
    return "v3_ffbh_u32";
  case Vop3::Op::V3_FFBL_B32:
    return "v3_ffbl_b32";
  case Vop3::Op::V3_FFBH_I32:
    return "v3_ffbh_i32";
  case Vop3::Op::V3_FREXP_EXP_I32_F64:
    return "v3_frexp_exp_i32_f64";
  case Vop3::Op::V3_FREXP_MANT_F64:
    return "v3_frexp_mant_f64";
  case Vop3::Op::V3_FRACT_F64:
    return "v3_fract_f64";
  case Vop3::Op::V3_FREXP_EXP_I32_F32:
    return "v3_frexp_exp_i32_f32";
  case Vop3::Op::V3_FREXP_MANT_F32:
    return "v3_frexp_mant_f32";
  case Vop3::Op::V3_CLREXCP:
    return "v3_clrexcp";
  case Vop3::Op::V3_MOVRELD_B32:
    return "v3_movreld_b32";
  case Vop3::Op::V3_MOVRELS_B32:
    return "v3_movrels_b32";
  case Vop3::Op::V3_MOVRELSD_B32:
    return "v3_movrelsd_b32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::smrdOpcodeToString(Smrd::Op op) {
  switch (op) {
  case Smrd::Op::S_LOAD_DWORD:
    return "s_load_dword";
  case Smrd::Op::S_LOAD_DWORDX2:
    return "s_load_dwordx2";
  case Smrd::Op::S_LOAD_DWORDX4:
    return "s_load_dwordx4";
  case Smrd::Op::S_LOAD_DWORDX8:
    return "s_load_dwordx8";
  case Smrd::Op::S_LOAD_DWORDX16:
    return "s_load_dwordx16";
  case Smrd::Op::S_BUFFER_LOAD_DWORD:
    return "s_buffer_load_dword";
  case Smrd::Op::S_BUFFER_LOAD_DWORDX2:
    return "s_buffer_load_dwordx2";
  case Smrd::Op::S_BUFFER_LOAD_DWORDX4:
    return "s_buffer_load_dwordx4";
  case Smrd::Op::S_BUFFER_LOAD_DWORDX8:
    return "s_buffer_load_dwordx8";
  case Smrd::Op::S_BUFFER_LOAD_DWORDX16:
    return "s_buffer_load_dwordx16";
  case Smrd::Op::S_DCACHE_INV_VOL:
    return "s_dcache_inv_vol";
  case Smrd::Op::S_MEMTIME:
    return "s_memtime";
  case Smrd::Op::S_DCACHE_INV:
    return "s_dcache_inv";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::mubufOpcodeToString(Mubuf::Op op) {
  switch (op) {
  case Mubuf::Op::BUFFER_LOAD_FORMAT_X:
    return "buffer_load_format_x";
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XY:
    return "buffer_load_format_xy";
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XYZ:
    return "buffer_load_format_xyz";
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XYZW:
    return "buffer_load_format_xyzw";
  case Mubuf::Op::BUFFER_STORE_FORMAT_X:
    return "buffer_store_format_x";
  case Mubuf::Op::BUFFER_STORE_FORMAT_XY:
    return "buffer_store_format_xy";
  case Mubuf::Op::BUFFER_STORE_FORMAT_XYZ:
    return "buffer_store_format_xyz";
  case Mubuf::Op::BUFFER_STORE_FORMAT_XYZW:
    return "buffer_store_format_xyzw";
  case Mubuf::Op::BUFFER_LOAD_UBYTE:
    return "buffer_load_ubyte";
  case Mubuf::Op::BUFFER_LOAD_SBYTE:
    return "buffer_load_sbyte";
  case Mubuf::Op::BUFFER_LOAD_USHORT:
    return "buffer_load_ushort";
  case Mubuf::Op::BUFFER_LOAD_SSHORT:
    return "buffer_load_sshort";
  case Mubuf::Op::BUFFER_LOAD_DWORD:
    return "buffer_load_dword";
  case Mubuf::Op::BUFFER_LOAD_DWORDX2:
    return "buffer_load_dwordx2";
  case Mubuf::Op::BUFFER_LOAD_DWORDX4:
    return "buffer_load_dwordx4";
  case Mubuf::Op::BUFFER_LOAD_DWORDX3:
    return "buffer_load_dwordx3";
  case Mubuf::Op::BUFFER_STORE_BYTE:
    return "buffer_store_byte";
  case Mubuf::Op::BUFFER_STORE_SHORT:
    return "buffer_store_short";
  case Mubuf::Op::BUFFER_STORE_DWORD:
    return "buffer_store_dword";
  case Mubuf::Op::BUFFER_STORE_DWORDX2:
    return "buffer_store_dwordx2";
  case Mubuf::Op::BUFFER_STORE_DWORDX4:
    return "buffer_store_dwordx4";
  case Mubuf::Op::BUFFER_STORE_DWORDX3:
    return "buffer_store_dwordx3";
  case Mubuf::Op::BUFFER_ATOMIC_SWAP:
    return "buffer_atomic_swap";
  case Mubuf::Op::BUFFER_ATOMIC_CMPSWAP:
    return "buffer_atomic_cmpswap";
  case Mubuf::Op::BUFFER_ATOMIC_ADD:
    return "buffer_atomic_add";
  case Mubuf::Op::BUFFER_ATOMIC_SUB:
    return "buffer_atomic_sub";
  case Mubuf::Op::BUFFER_ATOMIC_RSUB:
    return "buffer_atomic_rsub";
  case Mubuf::Op::BUFFER_ATOMIC_SMIN:
    return "buffer_atomic_smin";
  case Mubuf::Op::BUFFER_ATOMIC_UMIN:
    return "buffer_atomic_umin";
  case Mubuf::Op::BUFFER_ATOMIC_SMAX:
    return "buffer_atomic_smax";
  case Mubuf::Op::BUFFER_ATOMIC_UMAX:
    return "buffer_atomic_umax";
  case Mubuf::Op::BUFFER_ATOMIC_AND:
    return "buffer_atomic_and";
  case Mubuf::Op::BUFFER_ATOMIC_OR:
    return "buffer_atomic_or";
  case Mubuf::Op::BUFFER_ATOMIC_XOR:
    return "buffer_atomic_xor";
  case Mubuf::Op::BUFFER_ATOMIC_INC:
    return "buffer_atomic_inc";
  case Mubuf::Op::BUFFER_ATOMIC_DEC:
    return "buffer_atomic_dec";
  case Mubuf::Op::BUFFER_ATOMIC_FCMPSWAP:
    return "buffer_atomic_fcmpswap";
  case Mubuf::Op::BUFFER_ATOMIC_FMIN:
    return "buffer_atomic_fmin";
  case Mubuf::Op::BUFFER_ATOMIC_FMAX:
    return "buffer_atomic_fmax";
  case Mubuf::Op::BUFFER_ATOMIC_SWAP_X2:
    return "buffer_atomic_swap_x2";
  case Mubuf::Op::BUFFER_ATOMIC_CMPSWAP_X2:
    return "buffer_atomic_cmpswap_x2";
  case Mubuf::Op::BUFFER_ATOMIC_ADD_X2:
    return "buffer_atomic_add_x2";
  case Mubuf::Op::BUFFER_ATOMIC_SUB_X2:
    return "buffer_atomic_sub_x2";
  case Mubuf::Op::BUFFER_ATOMIC_RSUB_X2:
    return "buffer_atomic_rsub_x2";
  case Mubuf::Op::BUFFER_ATOMIC_SMIN_X2:
    return "buffer_atomic_smin_x2";
  case Mubuf::Op::BUFFER_ATOMIC_UMIN_X2:
    return "buffer_atomic_umin_x2";
  case Mubuf::Op::BUFFER_ATOMIC_SMAX_X2:
    return "buffer_atomic_smax_x2";
  case Mubuf::Op::BUFFER_ATOMIC_UMAX_X2:
    return "buffer_atomic_umax_x2";
  case Mubuf::Op::BUFFER_ATOMIC_AND_X2:
    return "buffer_atomic_and_x2";
  case Mubuf::Op::BUFFER_ATOMIC_OR_X2:
    return "buffer_atomic_or_x2";
  case Mubuf::Op::BUFFER_ATOMIC_XOR_X2:
    return "buffer_atomic_xor_x2";
  case Mubuf::Op::BUFFER_ATOMIC_INC_X2:
    return "buffer_atomic_inc_x2";
  case Mubuf::Op::BUFFER_ATOMIC_DEC_X2:
    return "buffer_atomic_dec_x2";
  case Mubuf::Op::BUFFER_ATOMIC_FCMPSWAP_X2:
    return "buffer_atomic_fcmpswap_x2";
  case Mubuf::Op::BUFFER_ATOMIC_FMIN_X2:
    return "buffer_atomic_fmin_x2";
  case Mubuf::Op::BUFFER_ATOMIC_FMAX_X2:
    return "buffer_atomic_fmax_x2";
  case Mubuf::Op::BUFFER_WBINVL1_SC_VOL:
    return "buffer_wbinvl1_sc/vol";
  case Mubuf::Op::BUFFER_WBINVL1:
    return "buffer_wbinvl1";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::mtbufOpcodeToString(Mtbuf::Op op) {
  switch (op) {
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_X:
    return "tbuffer_load_format_x";
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XY:
    return "tbuffer_load_format_xy";
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XYZ:
    return "tbuffer_load_format_xyz";
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XYZW:
    return "tbuffer_load_format_xyzw";
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_X:
    return "tbuffer_store_format_x";
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XY:
    return "tbuffer_store_format_xy";
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XYZ:
    return "tbuffer_store_format_xyz";
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XYZW:
    return "tbuffer_store_format_xyzw";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::mimgOpcodeToString(Mimg::Op op) {
  switch (op) {
  case Mimg::Op::IMAGE_LOAD:
    return "image_load";
  case Mimg::Op::IMAGE_LOAD_MIP:
    return "image_load_mip";
  case Mimg::Op::IMAGE_LOAD_PCK:
    return "image_load_pck";
  case Mimg::Op::IMAGE_LOAD_PCK_SGN:
    return "image_load_pck_sgn";
  case Mimg::Op::IMAGE_LOAD_MIP_PCK:
    return "image_load_mip_pck";
  case Mimg::Op::IMAGE_LOAD_MIP_PCK_SGN:
    return "image_load_mip_pck_sgn";
  case Mimg::Op::IMAGE_STORE:
    return "image_store";
  case Mimg::Op::IMAGE_STORE_MIP:
    return "image_store_mip";
  case Mimg::Op::IMAGE_STORE_PCK:
    return "image_store_pck";
  case Mimg::Op::IMAGE_STORE_MIP_PCK:
    return "image_store_mip_pck";
  case Mimg::Op::IMAGE_GET_RESINFO:
    return "image_get_resinfo";
  case Mimg::Op::IMAGE_ATOMIC_SWAP:
    return "image_atomic_swap";
  case Mimg::Op::IMAGE_ATOMIC_CMPSWAP:
    return "image_atomic_cmpswap";
  case Mimg::Op::IMAGE_ATOMIC_ADD:
    return "image_atomic_add";
  case Mimg::Op::IMAGE_ATOMIC_SUB:
    return "image_atomic_sub";
  case Mimg::Op::IMAGE_ATOMIC_RSUB:
    return "image_atomic_rsub";
  case Mimg::Op::IMAGE_ATOMIC_SMIN:
    return "image_atomic_smin";
  case Mimg::Op::IMAGE_ATOMIC_UMIN:
    return "image_atomic_umin";
  case Mimg::Op::IMAGE_ATOMIC_SMAX:
    return "image_atomic_smax";
  case Mimg::Op::IMAGE_ATOMIC_UMAX:
    return "image_atomic_umax";
  case Mimg::Op::IMAGE_ATOMIC_AND:
    return "image_atomic_and";
  case Mimg::Op::IMAGE_ATOMIC_OR:
    return "image_atomic_or";
  case Mimg::Op::IMAGE_ATOMIC_XOR:
    return "image_atomic_xor";
  case Mimg::Op::IMAGE_ATOMIC_INC:
    return "image_atomic_inc";
  case Mimg::Op::IMAGE_ATOMIC_DEC:
    return "image_atomic_dec";
  case Mimg::Op::IMAGE_ATOMIC_FCMPSWAP:
    return "image_atomic_fcmpswap";
  case Mimg::Op::IMAGE_ATOMIC_FMIN:
    return "image_atomic_fmin";
  case Mimg::Op::IMAGE_ATOMIC_FMAX:
    return "image_atomic_fmax";
  case Mimg::Op::IMAGE_SAMPLE:
    return "image_sample";
  case Mimg::Op::IMAGE_SAMPLE_CL:
    return "image_sample_cl";
  case Mimg::Op::IMAGE_SAMPLE_D:
    return "image_sample_d";
  case Mimg::Op::IMAGE_SAMPLE_D_CL:
    return "image_sample_d_cl";
  case Mimg::Op::IMAGE_SAMPLE_L:
    return "image_sample_l";
  case Mimg::Op::IMAGE_SAMPLE_B:
    return "image_sample_b";
  case Mimg::Op::IMAGE_SAMPLE_B_CL:
    return "image_sample_b_cl";
  case Mimg::Op::IMAGE_SAMPLE_LZ:
    return "image_sample_lz";
  case Mimg::Op::IMAGE_SAMPLE_C:
    return "image_sample_c";
  case Mimg::Op::IMAGE_SAMPLE_C_CL:
    return "image_sample_c_cl";
  case Mimg::Op::IMAGE_SAMPLE_C_D:
    return "image_sample_c_d";
  case Mimg::Op::IMAGE_SAMPLE_C_D_CL:
    return "image_sample_c_d_cl";
  case Mimg::Op::IMAGE_SAMPLE_C_L:
    return "image_sample_c_l";
  case Mimg::Op::IMAGE_SAMPLE_C_B:
    return "image_sample_c_b";
  case Mimg::Op::IMAGE_SAMPLE_C_B_CL:
    return "image_sample_c_b_cl";
  case Mimg::Op::IMAGE_SAMPLE_C_LZ:
    return "image_sample_c_lz";
  case Mimg::Op::IMAGE_SAMPLE_O:
    return "image_sample_o";
  case Mimg::Op::IMAGE_SAMPLE_CL_O:
    return "image_sample_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_D_O:
    return "image_sample_d_o";
  case Mimg::Op::IMAGE_SAMPLE_D_CL_O:
    return "image_sample_d_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_L_O:
    return "image_sample_l_o";
  case Mimg::Op::IMAGE_SAMPLE_B_O:
    return "image_sample_b_o";
  case Mimg::Op::IMAGE_SAMPLE_B_CL_O:
    return "image_sample_b_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_LZ_O:
    return "image_sample_lz_o";
  case Mimg::Op::IMAGE_SAMPLE_C_O:
    return "image_sample_c_o";
  case Mimg::Op::IMAGE_SAMPLE_C_CL_O:
    return "image_sample_c_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_C_D_O:
    return "image_sample_c_d_o";
  case Mimg::Op::IMAGE_SAMPLE_C_D_CL_O:
    return "image_sample_c_d_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_C_L_O:
    return "image_sample_c_l_o";
  case Mimg::Op::IMAGE_SAMPLE_C_B_O:
    return "image_sample_c_b_o";
  case Mimg::Op::IMAGE_SAMPLE_C_B_CL_O:
    return "image_sample_c_b_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_C_LZ_O:
    return "image_sample_c_lz_o";
  case Mimg::Op::IMAGE_GATHER4:
    return "image_gather4";
  case Mimg::Op::IMAGE_GATHER4_CL:
    return "image_gather4_cl";
  case Mimg::Op::IMAGE_GATHER4_L:
    return "image_gather4_l";
  case Mimg::Op::IMAGE_GATHER4_B:
    return "image_gather4_b";
  case Mimg::Op::IMAGE_GATHER4_B_CL:
    return "image_gather4_b_cl";
  case Mimg::Op::IMAGE_GATHER4_LZ:
    return "image_gather4_lz";
  case Mimg::Op::IMAGE_GATHER4_C:
    return "image_gather4_c";
  case Mimg::Op::IMAGE_GATHER4_C_CL:
    return "image_gather4_c_cl";
  case Mimg::Op::IMAGE_GATHER4_C_L:
    return "image_gather4_c_l";
  case Mimg::Op::IMAGE_GATHER4_C_B:
    return "image_gather4_c_b";
  case Mimg::Op::IMAGE_GATHER4_C_B_CL:
    return "image_gather4_c_b_cl";
  case Mimg::Op::IMAGE_GATHER4_C_LZ:
    return "image_gather4_c_lz";
  case Mimg::Op::IMAGE_GATHER4_O:
    return "image_gather4_o";
  case Mimg::Op::IMAGE_GATHER4_CL_O:
    return "image_gather4_cl_o";
  case Mimg::Op::IMAGE_GATHER4_L_O:
    return "image_gather4_l_o";
  case Mimg::Op::IMAGE_GATHER4_B_O:
    return "image_gather4_b_o";
  case Mimg::Op::IMAGE_GATHER4_B_CL_O:
    return "image_gather4_b_cl_o";
  case Mimg::Op::IMAGE_GATHER4_LZ_O:
    return "image_gather4_lz_o";
  case Mimg::Op::IMAGE_GATHER4_C_O:
    return "image_gather4_c_o";
  case Mimg::Op::IMAGE_GATHER4_C_CL_O:
    return "image_gather4_c_cl_o";
  case Mimg::Op::IMAGE_GATHER4_C_L_O:
    return "image_gather4_c_l_o";
  case Mimg::Op::IMAGE_GATHER4_C_B_O:
    return "image_gather4_c_b_o";
  case Mimg::Op::IMAGE_GATHER4_C_B_CL_O:
    return "image_gather4_c_b_cl_o";
  case Mimg::Op::IMAGE_GATHER4_C_LZ_O:
    return "image_gather4_c_lz_o";
  case Mimg::Op::IMAGE_GET_LOD:
    return "image_get_lod";
  case Mimg::Op::IMAGE_SAMPLE_CD:
    return "image_sample_cd";
  case Mimg::Op::IMAGE_SAMPLE_CD_CL:
    return "image_sample_cd_cl";
  case Mimg::Op::IMAGE_SAMPLE_C_CD:
    return "image_sample_c_cd";
  case Mimg::Op::IMAGE_SAMPLE_C_CD_CL:
    return "image_sample_c_cd_cl";
  case Mimg::Op::IMAGE_SAMPLE_CD_O:
    return "image_sample_cd_o";
  case Mimg::Op::IMAGE_SAMPLE_CD_CL_O:
    return "image_sample_cd_cl_o";
  case Mimg::Op::IMAGE_SAMPLE_C_CD_O:
    return "image_sample_c_cd_o";
  case Mimg::Op::IMAGE_SAMPLE_C_CD_CL_O:
    return "image_sample_c_cd_cl_o";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::dsOpcodeToString(Ds::Op op) {
  switch (op) {
  case Ds::Op::DS_ADD_U32:
    return "ds_add_u32";
  case Ds::Op::DS_SUB_U32:
    return "ds_sub_u32";
  case Ds::Op::DS_RSUB_U32:
    return "ds_rsub_u32";
  case Ds::Op::DS_INC_U32:
    return "ds_inc_u32";
  case Ds::Op::DS_DEC_U32:
    return "ds_dec_u32";
  case Ds::Op::DS_MIN_I32:
    return "ds_min_i32";
  case Ds::Op::DS_MAX_I32:
    return "ds_max_i32";
  case Ds::Op::DS_MIN_U32:
    return "ds_min_u32";
  case Ds::Op::DS_MAX_U32:
    return "ds_max_u32";
  case Ds::Op::DS_AND_B32:
    return "ds_and_b32";
  case Ds::Op::DS_OR_B32:
    return "ds_or_b32";
  case Ds::Op::DS_XOR_B32:
    return "ds_xor_b32";
  case Ds::Op::DS_MSKOR_B32:
    return "ds_mskor_b32";
  case Ds::Op::DS_WRITE_B32:
    return "ds_write_b32";
  case Ds::Op::DS_WRITE2_B32:
    return "ds_write2_b32";
  case Ds::Op::DS_WRITE2ST64_B32:
    return "ds_write2st64_b32";
  case Ds::Op::DS_CMPST_B32:
    return "ds_cmpst_b32";
  case Ds::Op::DS_CMPST_F32:
    return "ds_cmpst_f32";
  case Ds::Op::DS_MIN_F32:
    return "ds_min_f32";
  case Ds::Op::DS_MAX_F32:
    return "ds_max_f32";
  case Ds::Op::DS_NOP:
    return "ds_nop";
  case Ds::Op::DS_GWS_SEMA_RELEASE_ALL:
    return "ds_gws_sema_release_all";
  case Ds::Op::DS_GWS_INIT:
    return "ds_gws_init";
  case Ds::Op::DS_GWS_SEMA_V:
    return "ds_gws_sema_v";
  case Ds::Op::DS_GWS_SEMA_BR:
    return "ds_gws_sema_br";
  case Ds::Op::DS_GWS_SEMA_P:
    return "ds_gws_sema_p";
  case Ds::Op::DS_GWS_BARRIER:
    return "ds_gws_barrier";
  case Ds::Op::DS_WRITE_B8:
    return "ds_write_b8";
  case Ds::Op::DS_WRITE_B16:
    return "ds_write_b16";
  case Ds::Op::DS_ADD_RTN_U32:
    return "ds_add_rtn_u32";
  case Ds::Op::DS_SUB_RTN_U32:
    return "ds_sub_rtn_u32";
  case Ds::Op::DS_RSUB_RTN_U32:
    return "ds_rsub_rtn_u32";
  case Ds::Op::DS_INC_RTN_U32:
    return "ds_inc_rtn_u32";
  case Ds::Op::DS_DEC_RTN_U32:
    return "ds_dec_rtn_u32";
  case Ds::Op::DS_MIN_RTN_I32:
    return "ds_min_rtn_i32";
  case Ds::Op::DS_MAX_RTN_I32:
    return "ds_max_rtn_i32";
  case Ds::Op::DS_MIN_RTN_U32:
    return "ds_min_rtn_u32";
  case Ds::Op::DS_MAX_RTN_U32:
    return "ds_max_rtn_u32";
  case Ds::Op::DS_AND_RTN_B32:
    return "ds_and_rtn_b32";
  case Ds::Op::DS_OR_RTN_B32:
    return "ds_or_rtn_b32";
  case Ds::Op::DS_XOR_RTN_B32:
    return "ds_xor_rtn_b32";
  case Ds::Op::DS_MSKOR_RTN_B32:
    return "ds_mskor_rtn_b32";
  case Ds::Op::DS_WRXCHG_RTN_B32:
    return "ds_wrxchg_rtn_b32";
  case Ds::Op::DS_WRXCHG2_RTN_B32:
    return "ds_wrxchg2_rtn_b32";
  case Ds::Op::DS_WRXCHG2ST64_RTN_B32:
    return "ds_wrxchg2st64_rtn_b32";
  case Ds::Op::DS_CMPST_RTN_B32:
    return "ds_cmpst_rtn_b32";
  case Ds::Op::DS_CMPST_RTN_F32:
    return "ds_cmpst_rtn_f32";
  case Ds::Op::DS_MIN_RTN_F32:
    return "ds_min_rtn_f32";
  case Ds::Op::DS_MAX_RTN_F32:
    return "ds_max_rtn_f32";
  case Ds::Op::DS_WRAP_RTN_B32:
    return "ds_wrap_rtn_b32";
  case Ds::Op::DS_SWIZZLE_B32:
    return "ds_swizzle_b32";
  case Ds::Op::DS_READ_B32:
    return "ds_read_b32";
  case Ds::Op::DS_READ2_B32:
    return "ds_read2_b32";
  case Ds::Op::DS_READ2ST64_B32:
    return "ds_read2st64_b32";
  case Ds::Op::DS_READ_I8:
    return "ds_read_i8";
  case Ds::Op::DS_READ_U8:
    return "ds_read_u8";
  case Ds::Op::DS_READ_I16:
    return "ds_read_i16";
  case Ds::Op::DS_READ_U16:
    return "ds_read_u16";
  case Ds::Op::DS_CONSUME:
    return "ds_consume";
  case Ds::Op::DS_APPEND:
    return "ds_append";
  case Ds::Op::DS_ORDERED_COUNT:
    return "ds_ordered_count";
  case Ds::Op::DS_ADD_U64:
    return "ds_add_u64";
  case Ds::Op::DS_SUB_U64:
    return "ds_sub_u64";
  case Ds::Op::DS_RSUB_U64:
    return "ds_rsub_u64";
  case Ds::Op::DS_INC_U64:
    return "ds_inc_u64";
  case Ds::Op::DS_DEC_U64:
    return "ds_dec_u64";
  case Ds::Op::DS_MIN_I64:
    return "ds_min_i64";
  case Ds::Op::DS_MAX_I64:
    return "ds_max_i64";
  case Ds::Op::DS_MIN_U64:
    return "ds_min_u64";
  case Ds::Op::DS_MAX_U64:
    return "ds_max_u64";
  case Ds::Op::DS_AND_B64:
    return "ds_and_b64";
  case Ds::Op::DS_OR_B64:
    return "ds_or_b64";
  case Ds::Op::DS_XOR_B64:
    return "ds_xor_b64";
  case Ds::Op::DS_MSKOR_B64:
    return "ds_mskor_b64";
  case Ds::Op::DS_WRITE_B64:
    return "ds_write_b64";
  case Ds::Op::DS_WRITE2_B64:
    return "ds_write2_b64";
  case Ds::Op::DS_WRITE2ST64_B64:
    return "ds_write2st64_b64";
  case Ds::Op::DS_CMPST_B64:
    return "ds_cmpst_b64";
  case Ds::Op::DS_CMPST_F64:
    return "ds_cmpst_f64";
  case Ds::Op::DS_MIN_F64:
    return "ds_min_f64";
  case Ds::Op::DS_MAX_F64:
    return "ds_max_f64";
  case Ds::Op::DS_ADD_RTN_U64:
    return "ds_add_rtn_u64";
  case Ds::Op::DS_SUB_RTN_U64:
    return "ds_sub_rtn_u64";
  case Ds::Op::DS_RSUB_RTN_U64:
    return "ds_rsub_rtn_u64";
  case Ds::Op::DS_INC_RTN_U64:
    return "ds_inc_rtn_u64";
  case Ds::Op::DS_DEC_RTN_U64:
    return "ds_dec_rtn_u64";
  case Ds::Op::DS_MIN_RTN_I64:
    return "ds_min_rtn_i64";
  case Ds::Op::DS_MAX_RTN_I64:
    return "ds_max_rtn_i64";
  case Ds::Op::DS_MIN_RTN_U64:
    return "ds_min_rtn_u64";
  case Ds::Op::DS_MAX_RTN_U64:
    return "ds_max_rtn_u64";
  case Ds::Op::DS_AND_RTN_B64:
    return "ds_and_rtn_b64";
  case Ds::Op::DS_OR_RTN_B64:
    return "ds_or_rtn_b64";
  case Ds::Op::DS_XOR_RTN_B64:
    return "ds_xor_rtn_b64";
  case Ds::Op::DS_MSKOR_RTN_B64:
    return "ds_mskor_rtn_b64";
  case Ds::Op::DS_WRXCHG_RTN_B64:
    return "ds_wrxchg_rtn_b64";
  case Ds::Op::DS_WRXCHG2_RTN_B64:
    return "ds_wrxchg2_rtn_b64";
  case Ds::Op::DS_WRXCHG2ST64_RTN_B64:
    return "ds_wrxchg2st64_rtn_b64";
  case Ds::Op::DS_CMPST_RTN_B64:
    return "ds_cmpst_rtn_b64";
  case Ds::Op::DS_CMPST_RTN_F64:
    return "ds_cmpst_rtn_f64";
  case Ds::Op::DS_MIN_RTN_F64:
    return "ds_min_rtn_f64";
  case Ds::Op::DS_MAX_RTN_F64:
    return "ds_max_rtn_f64";
  case Ds::Op::DS_READ_B64:
    return "ds_read_b64";
  case Ds::Op::DS_READ2_B64:
    return "ds_read2_b64";
  case Ds::Op::DS_READ2ST64_B64:
    return "ds_read2st64_b64";
  case Ds::Op::DS_CONDXCHG32_RTN_B64:
    return "ds_condxchg32_rtn_b64";
  case Ds::Op::DS_ADD_SRC2_U32:
    return "ds_add_src2_u32";
  case Ds::Op::DS_SUB_SRC2_U32:
    return "ds_sub_src2_u32";
  case Ds::Op::DS_RSUB_SRC2_U32:
    return "ds_rsub_src2_u32";
  case Ds::Op::DS_INC_SRC2_U32:
    return "ds_inc_src2_u32";
  case Ds::Op::DS_DEC_SRC2_U32:
    return "ds_dec_src2_u32";
  case Ds::Op::DS_MIN_SRC2_I32:
    return "ds_min_src2_i32";
  case Ds::Op::DS_MAX_SRC2_I32:
    return "ds_max_src2_i32";
  case Ds::Op::DS_MIN_SRC2_U32:
    return "ds_min_src2_u32";
  case Ds::Op::DS_MAX_SRC2_U32:
    return "ds_max_src2_u32";
  case Ds::Op::DS_AND_SRC2_B32:
    return "ds_and_src2_b32";
  case Ds::Op::DS_OR_SRC2_B32:
    return "ds_or_src2_b32";
  case Ds::Op::DS_XOR_SRC2_B32:
    return "ds_xor_src2_b32";
  case Ds::Op::DS_WRITE_SRC2_B32:
    return "ds_write_src2_b32";
  case Ds::Op::DS_MIN_SRC2_F32:
    return "ds_min_src2_f32";
  case Ds::Op::DS_MAX_SRC2_F32:
    return "ds_max_src2_f32";
  case Ds::Op::DS_ADD_SRC2_U64:
    return "ds_add_src2_u64";
  case Ds::Op::DS_SUB_SRC2_U64:
    return "ds_sub_src2_u64";
  case Ds::Op::DS_RSUB_SRC2_U64:
    return "ds_rsub_src2_u64";
  case Ds::Op::DS_INC_SRC2_U64:
    return "ds_inc_src2_u64";
  case Ds::Op::DS_DEC_SRC2_U64:
    return "ds_dec_src2_u64";
  case Ds::Op::DS_MIN_SRC2_I64:
    return "ds_min_src2_i64";
  case Ds::Op::DS_MAX_SRC2_I64:
    return "ds_max_src2_i64";
  case Ds::Op::DS_MIN_SRC2_U64:
    return "ds_min_src2_u64";
  case Ds::Op::DS_MAX_SRC2_U64:
    return "ds_max_src2_u64";
  case Ds::Op::DS_AND_SRC2_B64:
    return "ds_and_src2_b64";
  case Ds::Op::DS_OR_SRC2_B64:
    return "ds_or_src2_b64";
  case Ds::Op::DS_XOR_SRC2_B64:
    return "ds_xor_src2_b64";
  case Ds::Op::DS_WRITE_SRC2_B64:
    return "ds_write_src2_b64";
  case Ds::Op::DS_MIN_SRC2_F64:
    return "ds_min_src2_f64";
  case Ds::Op::DS_MAX_SRC2_F64:
    return "ds_max_src2_f64";
  case Ds::Op::DS_WRITE_B96:
    return "ds_write_b96";
  case Ds::Op::DS_WRITE_B128:
    return "ds_write_b128";
  case Ds::Op::DS_CONDXCHG32_RTN_B128:
    return "ds_condxchg32_rtn_b128";
  case Ds::Op::DS_READ_B96:
    return "ds_read_b96";
  case Ds::Op::DS_READ_B128:
    return "ds_read_b128";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::vintrpOpcodeToString(Vintrp::Op op) {
  switch (op) {
  case Vintrp::Op::V_INTERP_P1_F32:
    return "v_interp_p1_f32";
  case Vintrp::Op::V_INTERP_P2_F32:
    return "v_interp_p2_f32";
  case Vintrp::Op::V_INTERP_MOV_F32:
    return "v_interp_mov_f32";
  default:
    return nullptr;
  }
}

const char *amdgpu::shader::opcodeToString(InstructionClass instClass, int op) {
  switch (instClass) {
  case InstructionClass::Vop2:
    return vop2OpcodeToString(static_cast<Vop2::Op>(op));
  case InstructionClass::Sop2:
    return sop2OpcodeToString(static_cast<Sop2::Op>(op));
  case InstructionClass::Sopk:
    return sopkOpcodeToString(static_cast<Sopk::Op>(op));
  case InstructionClass::Smrd:
    return smrdOpcodeToString(static_cast<Smrd::Op>(op));
  case InstructionClass::Vop3:
    return vop3OpcodeToString(static_cast<Vop3::Op>(op));
  case InstructionClass::Mubuf:
    return mubufOpcodeToString(static_cast<Mubuf::Op>(op));
  case InstructionClass::Mtbuf:
    return mtbufOpcodeToString(static_cast<Mtbuf::Op>(op));
  case InstructionClass::Mimg:
    return mimgOpcodeToString(static_cast<Mimg::Op>(op));
  case InstructionClass::Ds:
    return dsOpcodeToString(static_cast<Ds::Op>(op));
  case InstructionClass::Vintrp:
    return vintrpOpcodeToString(static_cast<Vintrp::Op>(op));
  case InstructionClass::Exp:
    return nullptr;
  case InstructionClass::Vop1:
    return vop1OpcodeToString(static_cast<Vop1::Op>(op));
  case InstructionClass::Vopc:
    return vopcOpcodeToString(static_cast<Vopc::Op>(op));
  case InstructionClass::Sop1:
    return sop1OpcodeToString(static_cast<Sop1::Op>(op));
  case InstructionClass::Sopc:
    return sopcOpcodeToString(static_cast<Sopc::Op>(op));
  case InstructionClass::Sopp:
    return soppOpcodeToString(static_cast<Sopp::Op>(op));

  default:
    return nullptr;
  }
}

void amdgpu::shader::Sop1::dump() const {
  int instSize = kMinInstSize;
  printSop1Opcode(op);
  std::printf(" ");
  instSize += printScalarOperand(sdst, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(ssrc0, inst + instSize);
}

void amdgpu::shader::Sopk::dump() const {
  int instSize = kMinInstSize;
  printSopkOpcode(op);
  std::printf(" ");
  instSize += printScalarOperand(sdst, inst + instSize);
  std::printf(", %d", simm);
}

void amdgpu::shader::Sopc::dump() const {
  int instSize = kMinInstSize;
  printSopcOpcode(op);
  std::printf(" ");
  instSize += printScalarOperand(ssrc0, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(ssrc1, inst + instSize);
}

void amdgpu::shader::Sop2::dump() const {
  int instSize = kMinInstSize;
  printSop2Opcode(op);
  std::printf(" ");
  instSize += printScalarOperand(sdst, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(ssrc0, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(ssrc1, inst + instSize);
}

void amdgpu::shader::Sopp::dump() const {
  int instSize = kMinInstSize;
  printSoppOpcode(op);
  std::printf(" ");
  instSize += printScalarOperand(simm, inst + instSize);
}

void amdgpu::shader::Vop1::dump() const {
  int instSize = kMinInstSize;
  printVop1Opcode(op);
  std::printf(" ");
  instSize += printVectorOperand(vdst, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(src0, inst + instSize);
}

void amdgpu::shader::Vop2::dump() const {
  int instSize = kMinInstSize;
  printVop2Opcode(op);
  std::printf(" ");
  instSize += printVectorOperand(vdst, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(src0, inst + instSize);
  std::printf(", ");
  instSize += printVectorOperand(vsrc1, inst + instSize);

  if (op == Vop2::Op::V_MADMK_F32 || op == Vop2::Op::V_MADAK_F32) {
    std::printf(", ");
    instSize += printScalarOperand(255, inst + instSize);
  }
}

void amdgpu::shader::Vop3::dump() const {

  /*
  v_add_i32
  v_addc_u32
  v_sub_i32
  v_subb_u32,
  v_subbrev_u32
  v_subrev_i32
  v_div_scale_f32
  v_div_scale_f64
  */

  int instSize = kMinInstSize;
  printVop3Opcode(op);
  std::printf(" ");
  instSize += printVectorOperand(vdst, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(src0, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(src1, inst + instSize);
  std::printf(", ");
  instSize += printScalarOperand(src2, inst + instSize);

  std::printf(" #abs=%x, clmp=%x, neg=%x, omod=%x, ", abs, clmp, neg, omod);
  instSize += printScalarOperand(sdst, inst + instSize);
}

void amdgpu::shader::Vopc::dump() const {
  int instSize = kMinInstSize;

  printVopcOpcode(op);
  std::printf(" ");
  instSize += printScalarOperand(src0, inst + instSize);
  std::printf(", ");
  instSize += printVectorOperand(vsrc1, inst + instSize);
}

void amdgpu::shader::Smrd::dump() const {
  int instSize = kMinInstSize;

  printSmrdOpcode(op);
  printf(" ");
  printScalarOperand(sdst, inst + instSize);
  printf(", ");
  printScalarOperand(sbase << 1, inst + instSize);
  printf(", ");

  if (imm) {
    printf("%u", offset << 2);
  } else {
    printScalarOperand(offset, inst + instSize);
  }

  std::printf(" #sdst=%x,sbase=%x,imm=%x,offset=%x", sdst, sbase, imm, offset);
}
void amdgpu::shader::Mubuf::dump() const {
  int instSize = kMinInstSize;

  printMubufOpcode(op);
  printf(" ");
  printVectorOperand(vdata, inst + instSize);
  printf(", ");
  printVectorOperand(vaddr, inst + instSize);
  printf(", ");
  printScalarOperand(srsrc << 2, inst + instSize);
  printf(", ");
  printScalarOperand(soffset, inst + instSize);
  printf(" #offset=%x, "
         "offen=%x,idxen=%x,glc=%x,lds=%x,vaddr=%x,vdata=%x,srsrc=%x,slc=%x,"
         "tfe=%x,soffset=%d",
         offset, offen, idxen, glc, lds, vaddr, vdata, srsrc, slc, tfe,
         soffset);
}
void amdgpu::shader::Mtbuf::dump() const {
  int instSize = kMinInstSize;

  printMtbufOpcode(op);
  printf(" ");
  printVectorOperand(vdata, inst + instSize);
  printf(", ");
  printScalarOperand(srsrc << 2, inst + instSize);
  printf(", ");
  printScalarOperand(soffset, inst + instSize);
  printf(" #offset=%x,offen=%x,idxen=%x,glc=%x,op=%x,dfmt=%x,nfmt=%x,vaddr=%x,"
         "vdata=%x,srsrc=%x,slc=%x,tfe=%x,soffset=%x",
         offset, offen, idxen, glc, (unsigned)op, dfmt, nfmt, vaddr, vdata, srsrc, slc,
         tfe, soffset);
}
void amdgpu::shader::Mimg::dump() const {
  int instSize = kMinInstSize;

  printMimgOpcode(op);

  printf(" #dmask=%x,unrm=%x,glc=%x,da=%x,r128=%x,tfe=%x,lwe=%x,slc=%x,"
         "vaddr=%x,vdata=%x,srsrc=%x,ssamp=%x",
         dmask, unrm, glc, da, r128, tfe, lwe, slc, vaddr, vdata, srsrc, ssamp);
}

void amdgpu::shader::Ds::dump() const {
  int instSize = kMinInstSize;

  printDsOpcode(op);
}

void amdgpu::shader::Vintrp::dump() const {
  int instSize = kMinInstSize;

  printVintrpOpcode(op);
  printf(" ");
  instSize += printVectorOperand(vdst, inst + instSize);
  printf(", ");
  instSize += printVectorOperand(vsrc, inst + instSize);
  const char channels[] = {'x', 'y', 'z', 'w'};

  printf(", attr%d.%c", attr, channels[attrChan]);
}
void amdgpu::shader::Exp::dump() const {
  int instSize = kMinInstSize;

  printExpTarget(target);
  printf(" ");
  instSize += printVectorOperand(vsrc0, inst + instSize);
  printf(", ");
  instSize += printVectorOperand(vsrc1, inst + instSize);
  printf(", ");
  instSize += printVectorOperand(vsrc2, inst + instSize);
  printf(", ");
  instSize += printVectorOperand(vsrc3, inst + instSize);
  printf(" #en=%x, compr=%x, done=%x, vm=%x", en, compr, done, vm);
}

void amdgpu::shader::Instruction::dump() const {
  printf("%-6s ", instructionClassToString(instClass));

  switch (instClass) {
  case InstructionClass::Invalid:
    break;
  case InstructionClass::Vop2:
    Vop2(inst).dump();
    return;
  case InstructionClass::Sop2:
    Sop2(inst).dump();
    return;
  case InstructionClass::Sopk:
    Sopk(inst).dump();
    return;
  case InstructionClass::Smrd:
    Smrd(inst).dump();
    return;
  case InstructionClass::Vop3:
    Vop3(inst).dump();
    return;
  case InstructionClass::Mubuf:
    Mubuf(inst).dump();
    return;
  case InstructionClass::Mtbuf:
    Mtbuf(inst).dump();
    return;
  case InstructionClass::Mimg:
    Mimg(inst).dump();
    return;
  case InstructionClass::Ds:
    Ds(inst).dump();
    return;
  case InstructionClass::Vintrp:
    Vintrp(inst).dump();
    return;
  case InstructionClass::Exp:
    Exp(inst).dump();
    return;
  case InstructionClass::Vop1:
    Vop1(inst).dump();
    return;
  case InstructionClass::Vopc:
    Vopc(inst).dump();
    return;
  case InstructionClass::Sop1:
    Sop1(inst).dump();
    return;
  case InstructionClass::Sopc:
    Sopc(inst).dump();
    return;
  case InstructionClass::Sopp:
    Sopp(inst).dump();
    return;
  }

  printf("<invalid>");
}

const char *
amdgpu::shader::instructionClassToString(InstructionClass instrClass) {
  switch (instrClass) {
  case InstructionClass::Invalid:
    return "INVALID";
  case InstructionClass::Vop2:
    return "VOP2";
  case InstructionClass::Sop2:
    return "SOP2";
  case InstructionClass::Sopk:
    return "SOPK";
  case InstructionClass::Smrd:
    return "SMRD";
  case InstructionClass::Vop3:
    return "VOP3";
  case InstructionClass::Mubuf:
    return "MUBUF";
  case InstructionClass::Mtbuf:
    return "MTBUF";
  case InstructionClass::Mimg:
    return "MIMG";
  case InstructionClass::Ds:
    return "DS";
  case InstructionClass::Vintrp:
    return "VINTRP";
  case InstructionClass::Exp:
    return "EXP";
  case InstructionClass::Vop1:
    return "VOP1";
  case InstructionClass::Vopc:
    return "VOPC";
  case InstructionClass::Sop1:
    return "SOP1";
  case InstructionClass::Sopc:
    return "SOPC";
  case InstructionClass::Sopp:
    return "SOPP";
  }

  __builtin_trap();
}
