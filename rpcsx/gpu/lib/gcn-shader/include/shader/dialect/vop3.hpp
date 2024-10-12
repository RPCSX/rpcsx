#pragma once

namespace shader::ir::vop3 {
enum Op {
  CMP_F_F32,
  CMP_LT_F32,
  CMP_EQ_F32,
  CMP_LE_F32,
  CMP_GT_F32,
  CMP_LG_F32,
  CMP_GE_F32,
  CMP_O_F32,
  CMP_U_F32,
  CMP_NGE_F32,
  CMP_NLG_F32,
  CMP_NGT_F32,
  CMP_NLE_F32,
  CMP_NEQ_F32,
  CMP_NLT_F32,
  CMP_TRU_F32,
  CMPX_F_F32,
  CMPX_LT_F32,
  CMPX_EQ_F32,
  CMPX_LE_F32,
  CMPX_GT_F32,
  CMPX_LG_F32,
  CMPX_GE_F32,
  CMPX_O_F32,
  CMPX_U_F32,
  CMPX_NGE_F32,
  CMPX_NLG_F32,
  CMPX_NGT_F32,
  CMPX_NLE_F32,
  CMPX_NEQ_F32,
  CMPX_NLT_F32,
  CMPX_TRU_F32,
  CMP_F_F64,
  CMP_LT_F64,
  CMP_EQ_F64,
  CMP_LE_F64,
  CMP_GT_F64,
  CMP_LG_F64,
  CMP_GE_F64,
  CMP_O_F64,
  CMP_U_F64,
  CMP_NGE_F64,
  CMP_NLG_F64,
  CMP_NGT_F64,
  CMP_NLE_F64,
  CMP_NEQ_F64,
  CMP_NLT_F64,
  CMP_TRU_F64,
  CMPX_F_F64,
  CMPX_LT_F64,
  CMPX_EQ_F64,
  CMPX_LE_F64,
  CMPX_GT_F64,
  CMPX_LG_F64,
  CMPX_GE_F64,
  CMPX_O_F64,
  CMPX_U_F64,
  CMPX_NGE_F64,
  CMPX_NLG_F64,
  CMPX_NGT_F64,
  CMPX_NLE_F64,
  CMPX_NEQ_F64,
  CMPX_NLT_F64,
  CMPX_TRU_F64,
  CMPS_F_F32,
  CMPS_LT_F32,
  CMPS_EQ_F32,
  CMPS_LE_F32,
  CMPS_GT_F32,
  CMPS_LG_F32,
  CMPS_GE_F32,
  CMPS_O_F32,
  CMPS_U_F32,
  CMPS_NGE_F32,
  CMPS_NLG_F32,
  CMPS_NGT_F32,
  CMPS_NLE_F32,
  CMPS_NEQ_F32,
  CMPS_NLT_F32,
  CMPS_TRU_F32,
  CMPSX_F_F32,
  CMPSX_LT_F32,
  CMPSX_EQ_F32,
  CMPSX_LE_F32,
  CMPSX_GT_F32,
  CMPSX_LG_F32,
  CMPSX_GE_F32,
  CMPSX_O_F32,
  CMPSX_U_F32,
  CMPSX_NGE_F32,
  CMPSX_NLG_F32,
  CMPSX_NGT_F32,
  CMPSX_NLE_F32,
  CMPSX_NEQ_F32,
  CMPSX_NLT_F32,
  CMPSX_TRU_F32,
  CMPS_F_F64,
  CMPS_LT_F64,
  CMPS_EQ_F64,
  CMPS_LE_F64,
  CMPS_GT_F64,
  CMPS_LG_F64,
  CMPS_GE_F64,
  CMPS_O_F64,
  CMPS_U_F64,
  CMPS_NGE_F64,
  CMPS_NLG_F64,
  CMPS_NGT_F64,
  CMPS_NLE_F64,
  CMPS_NEQ_F64,
  CMPS_NLT_F64,
  CMPS_TRU_F64,
  CMPSX_F_F64,
  CMPSX_LT_F64,
  CMPSX_EQ_F64,
  CMPSX_LE_F64,
  CMPSX_GT_F64,
  CMPSX_LG_F64,
  CMPSX_GE_F64,
  CMPSX_O_F64,
  CMPSX_U_F64,
  CMPSX_NGE_F64,
  CMPSX_NLG_F64,
  CMPSX_NGT_F64,
  CMPSX_NLE_F64,
  CMPSX_NEQ_F64,
  CMPSX_NLT_F64,
  CMPSX_TRU_F64,
  CMP_F_I32,
  CMP_LT_I32,
  CMP_EQ_I32,
  CMP_LE_I32,
  CMP_GT_I32,
  CMP_NE_I32,
  CMP_GE_I32,
  CMP_T_I32,
  CMP_CLASS_F32,
  CMP_LT_I16,
  CMP_EQ_I16,
  CMP_LE_I16,
  CMP_GT_I16,
  CMP_NE_I16,
  CMP_GE_I16,
  CMP_CLASS_F16,
  CMPX_F_I32,
  CMPX_LT_I32,
  CMPX_EQ_I32,
  CMPX_LE_I32,
  CMPX_GT_I32,
  CMPX_NE_I32,
  CMPX_GE_I32,
  CMPX_T_I32,
  CMPX_CLASS_F32,
  CMPX_LT_I16,
  CMPX_EQ_I16,
  CMPX_LE_I16,
  CMPX_GT_I16,
  CMPX_NE_I16,
  CMPX_GE_I16,
  CMPX_CLASS_F16,
  CMP_F_I64,
  CMP_LT_I64,
  CMP_EQ_I64,
  CMP_LE_I64,
  CMP_GT_I64,
  CMP_NE_I64,
  CMP_GE_I64,
  CMP_T_I64,
  CMP_CLASS_F64,
  CMP_LT_U16,
  CMP_EQ_U16,
  CMP_LE_U16,
  CMP_GT_U16,
  CMP_NE_U16,
  CMP_GE_U16,
  CMPX_F_I64 = 176,
  CMPX_LT_I64,
  CMPX_EQ_I64,
  CMPX_LE_I64,
  CMPX_GT_I64,
  CMPX_NE_I64,
  CMPX_GE_I64,
  CMPX_T_I64,
  CMPX_CLASS_F64,
  CMPX_LT_U16,
  CMPX_EQ_U16,
  CMPX_LE_U16,
  CMPX_GT_U16,
  CMPX_NE_U16,
  CMPX_GE_U16,
  CMP_F_U32 = 192,
  CMP_LT_U32,
  CMP_EQ_U32,
  CMP_LE_U32,
  CMP_GT_U32,
  CMP_NE_U32,
  CMP_GE_U32,
  CMP_T_U32,
  CMP_F_F16,
  CMP_LT_F16,
  CMP_EQ_F16,
  CMP_LE_F16,
  CMP_GT_F16,
  CMP_LG_F16,
  CMP_GE_F16,
  CMP_O_F16,
  CMPX_F_U32,
  CMPX_LT_U32,
  CMPX_EQ_U32,
  CMPX_LE_U32,
  CMPX_GT_U32,
  CMPX_NE_U32,
  CMPX_GE_U32,
  CMPX_T_U32,
  CMPX_F_F16,
  CMPX_LT_F16,
  CMPX_EQ_F16,
  CMPX_LE_F16,
  CMPX_GT_F16,
  CMPX_LG_F16,
  CMPX_GE_F16,
  CMPX_O_F16,
  CMP_F_U64,
  CMP_LT_U64,
  CMP_EQ_U64,
  CMP_LE_U64,
  CMP_GT_U64,
  CMP_NE_U64,
  CMP_GE_U64,
  CMP_T_U64,
  CMP_U_F16,
  CMP_NGE_F16,
  CMP_NLG_F16,
  CMP_NGT_F16,
  CMP_NLE_F16,
  CMP_NEQ_F16,
  CMP_NLT_F16,
  CMP_TRU_F16,
  CMPX_F_U64,
  CMPX_LT_U64,
  CMPX_EQ_U64,
  CMPX_LE_U64,
  CMPX_GT_U64,
  CMPX_NE_U64,
  CMPX_GE_U64,
  CMPX_T_U64,
  CNDMASK_B32 = 256,
  READLANE_B32,
  WRITELANE_B32,
  ADD_F32,
  SUB_F32,
  SUBREV_F32,
  MAC_LEGACY_F32,
  MUL_LEGACY_F32,
  MUL_F32,
  MUL_I32_I24,
  MUL_HI_I32_I24,
  MUL_U32_U24,
  MUL_HI_U32_U24,
  MIN_LEGACY_F32,
  MAX_LEGACY_F32,
  MIN_F32,
  MAX_F32,
  MIN_I32,
  MAX_I32,
  MIN_U32,
  MAX_U32,
  LSHR_B32,
  LSHRREV_B32,
  ASHR_I32,
  ASHRREV_I32,
  LSHL_B32,
  LSHLREV_B32,
  AND_B32,
  OR_B32,
  XOR_B32,
  BFM_B32,
  MAC_F32,
  MADMK_F32,
  MADAK_F32,
  BCNT_U32_B32,
  MBCNT_LO_U32_B32,
  MBCNT_HI_U32_B32,
  ADD_I32,
  SUB_I32,
  SUBREV_I32,
  ADDC_U32,
  SUBB_U32,
  SUBBREV_U32,
  LDEXP_F32,
  CVT_PKACCUM_U8_F32,
  CVT_PKNORM_I16_F32,
  CVT_PKNORM_U16_F32,
  CVT_PKRTZ_F16_F32,
  CVT_PK_U16_U32,
  CVT_PK_I16_I32,
  MAD_LEGACY_F32 = 320,
  MAD_F32,
  MAD_I32_I24,
  MAD_U32_U24,
  CUBEID_F32,
  CUBESC_F32,
  CUBETC_F32,
  CUBEMA_F32,
  BFE_U32,
  BFE_I32,
  BFI_B32,
  FMA_F32,
  FMA_F64,
  LERP_U8,
  ALIGNBIT_B32,
  ALIGNBYTE_B32,
  MULLIT_F32,
  MIN3_F32,
  MIN3_I32,
  MIN3_U32,
  MAX3_F32,
  MAX3_I32,
  MAX3_U32,
  MED3_F32,
  MED3_I32,
  MED3_U32,
  SAD_U8,
  SAD_HI_U8,
  SAD_U16,
  SAD_U32,
  CVT_PK_U8_F32,
  DIV_FIXUP_F32,
  DIV_FIXUP_F64,
  LSHL_B64,
  LSHR_B64,
  ASHR_I64,
  ADD_F64,
  MUL_F64,
  MIN_F64,
  MAX_F64,
  LDEXP_F64,
  MUL_LO_U32,
  MUL_HI_U32,
  MUL_LO_I32,
  MUL_HI_I32,
  DIV_SCALE_F32,
  DIV_SCALE_F64,
  DIV_FMAS_F32,
  DIV_FMAS_F64,
  MSAD_U8,
  QSAD_U8,
  MQSAD_U8,
  TRIG_PREOP_F64,
  MQSAD_U32_U8,
  MAD_U64_U32,
  MAD_I64_I32,
  NOP = 384,
  MOV_B32,
  READFIRSTLANE_B32,
  CVT_I32_F64,
  CVT_F64_I32,
  CVT_F32_I32,
  CVT_F32_U32,
  CVT_U32_F32,
  CVT_I32_F32,
  MOV_FED_B32,
  CVT_F16_F32,
  CVT_F32_F16,
  CVT_RPI_I32_F32,
  CVT_FLR_I32_F32,
  CVT_OFF_F32_I4,
  CVT_F32_F64,
  CVT_F64_F32,
  CVT_F32_UBYTE0,
  CVT_F32_UBYTE1,
  CVT_F32_UBYTE2,
  CVT_F32_UBYTE3,
  CVT_U32_F64,
  CVT_F64_U32,
  FRACT_F32 = 416,
  TRUNC_F32,
  CEIL_F32,
  RNDNE_F32,
  FLOOR_F32,
  EXP_F32,
  LOG_CLAMP_F32,
  LOG_F32,
  RCP_CLAMP_F32,
  RCP_LEGACY_F32,
  RCP_F32,
  RCP_IFLAG_F32,
  RSQ_CLAMP_F32,
  RSQ_LEGACY_F32,
  RSQ_F32,
  RCP_F64,
  RCP_CLAMP_F64,
  RSQ_F64,
  RSQ_CLAMP_F64,
  SQRT_F32,
  SQRT_F64,
  SIN_F32,
  COS_F32,
  NOT_B32,
  BFREV_B32,
  FFBH_U32,
  FFBL_B32,
  FFBH_I32,
  FREXP_EXP_I32_F64,
  FREXP_MANT_F64,
  FRACT_F64,
  FREXP_EXP_I32_F32,
  FREXP_MANT_F32,
  CLREXCP,
  MOVRELD_B32,
  MOVRELS_B32,
  MOVRELSD_B32,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case CMP_F_F32:
    return "v_cmp_f_f32";
  case CMP_LT_F32:
    return "v_cmp_lt_f32";
  case CMP_EQ_F32:
    return "v_cmp_eq_f32";
  case CMP_LE_F32:
    return "v_cmp_le_f32";
  case CMP_GT_F32:
    return "v_cmp_gt_f32";
  case CMP_LG_F32:
    return "v_cmp_lg_f32";
  case CMP_GE_F32:
    return "v_cmp_ge_f32";
  case CMP_O_F32:
    return "v_cmp_o_f32";
  case CMP_U_F32:
    return "v_cmp_u_f32";
  case CMP_NGE_F32:
    return "v_cmp_nge_f32";
  case CMP_NLG_F32:
    return "v_cmp_nlg_f32";
  case CMP_NGT_F32:
    return "v_cmp_ngt_f32";
  case CMP_NLE_F32:
    return "v_cmp_nle_f32";
  case CMP_NEQ_F32:
    return "v_cmp_neq_f32";
  case CMP_NLT_F32:
    return "v_cmp_nlt_f32";
  case CMP_TRU_F32:
    return "v_cmp_tru_f32";
  case CMPX_F_F32:
    return "v_cmpx_f_f32";
  case CMPX_LT_F32:
    return "v_cmpx_lt_f32";
  case CMPX_EQ_F32:
    return "v_cmpx_eq_f32";
  case CMPX_LE_F32:
    return "v_cmpx_le_f32";
  case CMPX_GT_F32:
    return "v_cmpx_gt_f32";
  case CMPX_LG_F32:
    return "v_cmpx_lg_f32";
  case CMPX_GE_F32:
    return "v_cmpx_ge_f32";
  case CMPX_O_F32:
    return "v_cmpx_o_f32";
  case CMPX_U_F32:
    return "v_cmpx_u_f32";
  case CMPX_NGE_F32:
    return "v_cmpx_nge_f32";
  case CMPX_NLG_F32:
    return "v_cmpx_nlg_f32";
  case CMPX_NGT_F32:
    return "v_cmpx_ngt_f32";
  case CMPX_NLE_F32:
    return "v_cmpx_nle_f32";
  case CMPX_NEQ_F32:
    return "v_cmpx_neq_f32";
  case CMPX_NLT_F32:
    return "v_cmpx_nlt_f32";
  case CMPX_TRU_F32:
    return "v_cmpx_tru_f32";
  case CMP_F_F64:
    return "v_cmp_f_f64";
  case CMP_LT_F64:
    return "v_cmp_lt_f64";
  case CMP_EQ_F64:
    return "v_cmp_eq_f64";
  case CMP_LE_F64:
    return "v_cmp_le_f64";
  case CMP_GT_F64:
    return "v_cmp_gt_f64";
  case CMP_LG_F64:
    return "v_cmp_lg_f64";
  case CMP_GE_F64:
    return "v_cmp_ge_f64";
  case CMP_O_F64:
    return "v_cmp_o_f64";
  case CMP_U_F64:
    return "v_cmp_u_f64";
  case CMP_NGE_F64:
    return "v_cmp_nge_f64";
  case CMP_NLG_F64:
    return "v_cmp_nlg_f64";
  case CMP_NGT_F64:
    return "v_cmp_ngt_f64";
  case CMP_NLE_F64:
    return "v_cmp_nle_f64";
  case CMP_NEQ_F64:
    return "v_cmp_neq_f64";
  case CMP_NLT_F64:
    return "v_cmp_nlt_f64";
  case CMP_TRU_F64:
    return "v_cmp_tru_f64";
  case CMPX_F_F64:
    return "v_cmpx_f_f64";
  case CMPX_LT_F64:
    return "v_cmpx_lt_f64";
  case CMPX_EQ_F64:
    return "v_cmpx_eq_f64";
  case CMPX_LE_F64:
    return "v_cmpx_le_f64";
  case CMPX_GT_F64:
    return "v_cmpx_gt_f64";
  case CMPX_LG_F64:
    return "v_cmpx_lg_f64";
  case CMPX_GE_F64:
    return "v_cmpx_ge_f64";
  case CMPX_O_F64:
    return "v_cmpx_o_f64";
  case CMPX_U_F64:
    return "v_cmpx_u_f64";
  case CMPX_NGE_F64:
    return "v_cmpx_nge_f64";
  case CMPX_NLG_F64:
    return "v_cmpx_nlg_f64";
  case CMPX_NGT_F64:
    return "v_cmpx_ngt_f64";
  case CMPX_NLE_F64:
    return "v_cmpx_nle_f64";
  case CMPX_NEQ_F64:
    return "v_cmpx_neq_f64";
  case CMPX_NLT_F64:
    return "v_cmpx_nlt_f64";
  case CMPX_TRU_F64:
    return "v_cmpx_tru_f64";
  case CMPS_F_F32:
    return "v_cmps_f_f32";
  case CMPS_LT_F32:
    return "v_cmps_lt_f32";
  case CMPS_EQ_F32:
    return "v_cmps_eq_f32";
  case CMPS_LE_F32:
    return "v_cmps_le_f32";
  case CMPS_GT_F32:
    return "v_cmps_gt_f32";
  case CMPS_LG_F32:
    return "v_cmps_lg_f32";
  case CMPS_GE_F32:
    return "v_cmps_ge_f32";
  case CMPS_O_F32:
    return "v_cmps_o_f32";
  case CMPS_U_F32:
    return "v_cmps_u_f32";
  case CMPS_NGE_F32:
    return "v_cmps_nge_f32";
  case CMPS_NLG_F32:
    return "v_cmps_nlg_f32";
  case CMPS_NGT_F32:
    return "v_cmps_ngt_f32";
  case CMPS_NLE_F32:
    return "v_cmps_nle_f32";
  case CMPS_NEQ_F32:
    return "v_cmps_neq_f32";
  case CMPS_NLT_F32:
    return "v_cmps_nlt_f32";
  case CMPS_TRU_F32:
    return "v_cmps_tru_f32";
  case CMPSX_F_F32:
    return "v_cmpsx_f_f32";
  case CMPSX_LT_F32:
    return "v_cmpsx_lt_f32";
  case CMPSX_EQ_F32:
    return "v_cmpsx_eq_f32";
  case CMPSX_LE_F32:
    return "v_cmpsx_le_f32";
  case CMPSX_GT_F32:
    return "v_cmpsx_gt_f32";
  case CMPSX_LG_F32:
    return "v_cmpsx_lg_f32";
  case CMPSX_GE_F32:
    return "v_cmpsx_ge_f32";
  case CMPSX_O_F32:
    return "v_cmpsx_o_f32";
  case CMPSX_U_F32:
    return "v_cmpsx_u_f32";
  case CMPSX_NGE_F32:
    return "v_cmpsx_nge_f32";
  case CMPSX_NLG_F32:
    return "v_cmpsx_nlg_f32";
  case CMPSX_NGT_F32:
    return "v_cmpsx_ngt_f32";
  case CMPSX_NLE_F32:
    return "v_cmpsx_nle_f32";
  case CMPSX_NEQ_F32:
    return "v_cmpsx_neq_f32";
  case CMPSX_NLT_F32:
    return "v_cmpsx_nlt_f32";
  case CMPSX_TRU_F32:
    return "v_cmpsx_tru_f32";
  case CMPS_F_F64:
    return "v_cmps_f_f64";
  case CMPS_LT_F64:
    return "v_cmps_lt_f64";
  case CMPS_EQ_F64:
    return "v_cmps_eq_f64";
  case CMPS_LE_F64:
    return "v_cmps_le_f64";
  case CMPS_GT_F64:
    return "v_cmps_gt_f64";
  case CMPS_LG_F64:
    return "v_cmps_lg_f64";
  case CMPS_GE_F64:
    return "v_cmps_ge_f64";
  case CMPS_O_F64:
    return "v_cmps_o_f64";
  case CMPS_U_F64:
    return "v_cmps_u_f64";
  case CMPS_NGE_F64:
    return "v_cmps_nge_f64";
  case CMPS_NLG_F64:
    return "v_cmps_nlg_f64";
  case CMPS_NGT_F64:
    return "v_cmps_ngt_f64";
  case CMPS_NLE_F64:
    return "v_cmps_nle_f64";
  case CMPS_NEQ_F64:
    return "v_cmps_neq_f64";
  case CMPS_NLT_F64:
    return "v_cmps_nlt_f64";
  case CMPS_TRU_F64:
    return "v_cmps_tru_f64";
  case CMPSX_F_F64:
    return "v_cmpsx_f_f64";
  case CMPSX_LT_F64:
    return "v_cmpsx_lt_f64";
  case CMPSX_EQ_F64:
    return "v_cmpsx_eq_f64";
  case CMPSX_LE_F64:
    return "v_cmpsx_le_f64";
  case CMPSX_GT_F64:
    return "v_cmpsx_gt_f64";
  case CMPSX_LG_F64:
    return "v_cmpsx_lg_f64";
  case CMPSX_GE_F64:
    return "v_cmpsx_ge_f64";
  case CMPSX_O_F64:
    return "v_cmpsx_o_f64";
  case CMPSX_U_F64:
    return "v_cmpsx_u_f64";
  case CMPSX_NGE_F64:
    return "v_cmpsx_nge_f64";
  case CMPSX_NLG_F64:
    return "v_cmpsx_nlg_f64";
  case CMPSX_NGT_F64:
    return "v_cmpsx_ngt_f64";
  case CMPSX_NLE_F64:
    return "v_cmpsx_nle_f64";
  case CMPSX_NEQ_F64:
    return "v_cmpsx_neq_f64";
  case CMPSX_NLT_F64:
    return "v_cmpsx_nlt_f64";
  case CMPSX_TRU_F64:
    return "v_cmpsx_tru_f64";
  case CMP_F_I32:
    return "v_cmp_f_i32";
  case CMP_LT_I32:
    return "v_cmp_lt_i32";
  case CMP_EQ_I32:
    return "v_cmp_eq_i32";
  case CMP_LE_I32:
    return "v_cmp_le_i32";
  case CMP_GT_I32:
    return "v_cmp_gt_i32";
  case CMP_NE_I32:
    return "v_cmp_ne_i32";
  case CMP_GE_I32:
    return "v_cmp_ge_i32";
  case CMP_T_I32:
    return "v_cmp_t_i32";
  case CMP_CLASS_F32:
    return "v_cmp_class_f32";
  case CMP_LT_I16:
    return "v_cmp_lt_i16";
  case CMP_EQ_I16:
    return "v_cmp_eq_i16";
  case CMP_LE_I16:
    return "v_cmp_le_i16";
  case CMP_GT_I16:
    return "v_cmp_gt_i16";
  case CMP_NE_I16:
    return "v_cmp_ne_i16";
  case CMP_GE_I16:
    return "v_cmp_ge_i16";
  case CMP_CLASS_F16:
    return "v_cmp_class_f16";
  case CMPX_F_I32:
    return "v_cmpx_f_i32";
  case CMPX_LT_I32:
    return "v_cmpx_lt_i32";
  case CMPX_EQ_I32:
    return "v_cmpx_eq_i32";
  case CMPX_LE_I32:
    return "v_cmpx_le_i32";
  case CMPX_GT_I32:
    return "v_cmpx_gt_i32";
  case CMPX_NE_I32:
    return "v_cmpx_ne_i32";
  case CMPX_GE_I32:
    return "v_cmpx_ge_i32";
  case CMPX_T_I32:
    return "v_cmpx_t_i32";
  case CMPX_CLASS_F32:
    return "v_cmpx_class_f32";
  case CMPX_LT_I16:
    return "v_cmpx_lt_i16";
  case CMPX_EQ_I16:
    return "v_cmpx_eq_i16";
  case CMPX_LE_I16:
    return "v_cmpx_le_i16";
  case CMPX_GT_I16:
    return "v_cmpx_gt_i16";
  case CMPX_NE_I16:
    return "v_cmpx_ne_i16";
  case CMPX_GE_I16:
    return "v_cmpx_ge_i16";
  case CMPX_CLASS_F16:
    return "v_cmpx_class_f16";
  case CMP_F_I64:
    return "v_cmp_f_i64";
  case CMP_LT_I64:
    return "v_cmp_lt_i64";
  case CMP_EQ_I64:
    return "v_cmp_eq_i64";
  case CMP_LE_I64:
    return "v_cmp_le_i64";
  case CMP_GT_I64:
    return "v_cmp_gt_i64";
  case CMP_NE_I64:
    return "v_cmp_ne_i64";
  case CMP_GE_I64:
    return "v_cmp_ge_i64";
  case CMP_T_I64:
    return "v_cmp_t_i64";
  case CMP_CLASS_F64:
    return "v_cmp_class_f64";
  case CMP_LT_U16:
    return "v_cmp_lt_u16";
  case CMP_EQ_U16:
    return "v_cmp_eq_u16";
  case CMP_LE_U16:
    return "v_cmp_le_u16";
  case CMP_GT_U16:
    return "v_cmp_gt_u16";
  case CMP_NE_U16:
    return "v_cmp_ne_u16";
  case CMP_GE_U16:
    return "v_cmp_ge_u16";
  case CMPX_F_I64:
    return "v_cmpx_f_i64";
  case CMPX_LT_I64:
    return "v_cmpx_lt_i64";
  case CMPX_EQ_I64:
    return "v_cmpx_eq_i64";
  case CMPX_LE_I64:
    return "v_cmpx_le_i64";
  case CMPX_GT_I64:
    return "v_cmpx_gt_i64";
  case CMPX_NE_I64:
    return "v_cmpx_ne_i64";
  case CMPX_GE_I64:
    return "v_cmpx_ge_i64";
  case CMPX_T_I64:
    return "v_cmpx_t_i64";
  case CMPX_CLASS_F64:
    return "v_cmpx_class_f64";
  case CMPX_LT_U16:
    return "v_cmpx_lt_u16";
  case CMPX_EQ_U16:
    return "v_cmpx_eq_u16";
  case CMPX_LE_U16:
    return "v_cmpx_le_u16";
  case CMPX_GT_U16:
    return "v_cmpx_gt_u16";
  case CMPX_NE_U16:
    return "v_cmpx_ne_u16";
  case CMPX_GE_U16:
    return "v_cmpx_ge_u16";
  case CMP_F_U32:
    return "v_cmp_f_u32";
  case CMP_LT_U32:
    return "v_cmp_lt_u32";
  case CMP_EQ_U32:
    return "v_cmp_eq_u32";
  case CMP_LE_U32:
    return "v_cmp_le_u32";
  case CMP_GT_U32:
    return "v_cmp_gt_u32";
  case CMP_NE_U32:
    return "v_cmp_ne_u32";
  case CMP_GE_U32:
    return "v_cmp_ge_u32";
  case CMP_T_U32:
    return "v_cmp_t_u32";
  case CMP_F_F16:
    return "v_cmp_f_f16";
  case CMP_LT_F16:
    return "v_cmp_lt_f16";
  case CMP_EQ_F16:
    return "v_cmp_eq_f16";
  case CMP_LE_F16:
    return "v_cmp_le_f16";
  case CMP_GT_F16:
    return "v_cmp_gt_f16";
  case CMP_LG_F16:
    return "v_cmp_lg_f16";
  case CMP_GE_F16:
    return "v_cmp_ge_f16";
  case CMP_O_F16:
    return "v_cmp_o_f16";
  case CMPX_F_U32:
    return "v_cmpx_f_u32";
  case CMPX_LT_U32:
    return "v_cmpx_lt_u32";
  case CMPX_EQ_U32:
    return "v_cmpx_eq_u32";
  case CMPX_LE_U32:
    return "v_cmpx_le_u32";
  case CMPX_GT_U32:
    return "v_cmpx_gt_u32";
  case CMPX_NE_U32:
    return "v_cmpx_ne_u32";
  case CMPX_GE_U32:
    return "v_cmpx_ge_u32";
  case CMPX_T_U32:
    return "v_cmpx_t_u32";
  case CMPX_F_F16:
    return "v_cmpx_f_f16";
  case CMPX_LT_F16:
    return "v_cmpx_lt_f16";
  case CMPX_EQ_F16:
    return "v_cmpx_eq_f16";
  case CMPX_LE_F16:
    return "v_cmpx_le_f16";
  case CMPX_GT_F16:
    return "v_cmpx_gt_f16";
  case CMPX_LG_F16:
    return "v_cmpx_lg_f16";
  case CMPX_GE_F16:
    return "v_cmpx_ge_f16";
  case CMPX_O_F16:
    return "v_cmpx_o_f16";
  case CMP_F_U64:
    return "v_cmp_f_u64";
  case CMP_LT_U64:
    return "v_cmp_lt_u64";
  case CMP_EQ_U64:
    return "v_cmp_eq_u64";
  case CMP_LE_U64:
    return "v_cmp_le_u64";
  case CMP_GT_U64:
    return "v_cmp_gt_u64";
  case CMP_NE_U64:
    return "v_cmp_ne_u64";
  case CMP_GE_U64:
    return "v_cmp_ge_u64";
  case CMP_T_U64:
    return "v_cmp_t_u64";
  case CMP_U_F16:
    return "v_cmp_u_f16";
  case CMP_NGE_F16:
    return "v_cmp_nge_f16";
  case CMP_NLG_F16:
    return "v_cmp_nlg_f16";
  case CMP_NGT_F16:
    return "v_cmp_ngt_f16";
  case CMP_NLE_F16:
    return "v_cmp_nle_f16";
  case CMP_NEQ_F16:
    return "v_cmp_neq_f16";
  case CMP_NLT_F16:
    return "v_cmp_nlt_f16";
  case CMP_TRU_F16:
    return "v_cmp_tru_f16";
  case CMPX_F_U64:
    return "v_cmpx_f_u64";
  case CMPX_LT_U64:
    return "v_cmpx_lt_u64";
  case CMPX_EQ_U64:
    return "v_cmpx_eq_u64";
  case CMPX_LE_U64:
    return "v_cmpx_le_u64";
  case CMPX_GT_U64:
    return "v_cmpx_gt_u64";
  case CMPX_NE_U64:
    return "v_cmpx_ne_u64";
  case CMPX_GE_U64:
    return "v_cmpx_ge_u64";
  case CMPX_T_U64:
    return "v_cmpx_t_u64";
  case CNDMASK_B32:
    return "v_cndmask_b32";
  case READLANE_B32:
    return "v_readlane_b32";
  case WRITELANE_B32:
    return "v_writelane_b32";
  case ADD_F32:
    return "v_add_f32";
  case SUB_F32:
    return "v_sub_f32";
  case SUBREV_F32:
    return "v_subrev_f32";
  case MAC_LEGACY_F32:
    return "v_mac_legacy_f32";
  case MUL_LEGACY_F32:
    return "v_mul_legacy_f32";
  case MUL_F32:
    return "v_mul_f32";
  case MUL_I32_I24:
    return "v_mul_i32_i24";
  case MUL_HI_I32_I24:
    return "v_mul_hi_i32_i24";
  case MUL_U32_U24:
    return "v_mul_u32_u24";
  case MUL_HI_U32_U24:
    return "v_mul_hi_u32_u24";
  case MIN_LEGACY_F32:
    return "v_min_legacy_f32";
  case MAX_LEGACY_F32:
    return "v_max_legacy_f32";
  case MIN_F32:
    return "v_min_f32";
  case MAX_F32:
    return "v_max_f32";
  case MIN_I32:
    return "v_min_i32";
  case MAX_I32:
    return "v_max_i32";
  case MIN_U32:
    return "v_min_u32";
  case MAX_U32:
    return "v_max_u32";
  case LSHR_B32:
    return "v_lshr_b32";
  case LSHRREV_B32:
    return "v_lshrrev_b32";
  case ASHR_I32:
    return "v_ashr_i32";
  case ASHRREV_I32:
    return "v_ashrrev_i32";
  case LSHL_B32:
    return "v_lshl_b32";
  case LSHLREV_B32:
    return "v_lshlrev_b32";
  case AND_B32:
    return "v_and_b32";
  case OR_B32:
    return "v_or_b32";
  case XOR_B32:
    return "v_xor_b32";
  case BFM_B32:
    return "v_bfm_b32";
  case MAC_F32:
    return "v_mac_f32";
  case MADMK_F32:
    return "v_madmk_f32";
  case MADAK_F32:
    return "v_madak_f32";
  case BCNT_U32_B32:
    return "v_bcnt_u32_b32";
  case MBCNT_LO_U32_B32:
    return "v_mbcnt_lo_u32_b32";
  case MBCNT_HI_U32_B32:
    return "v_mbcnt_hi_u32_b32";
  case ADD_I32:
    return "v_add_i32";
  case SUB_I32:
    return "v_sub_i32";
  case SUBREV_I32:
    return "v_subrev_i32";
  case ADDC_U32:
    return "v_addc_u32";
  case SUBB_U32:
    return "v_subb_u32";
  case SUBBREV_U32:
    return "v_subbrev_u32";
  case LDEXP_F32:
    return "v_ldexp_f32";
  case CVT_PKACCUM_U8_F32:
    return "v_cvt_pkaccum_u8_f32";
  case CVT_PKNORM_I16_F32:
    return "v_cvt_pknorm_i16_f32";
  case CVT_PKNORM_U16_F32:
    return "v_cvt_pknorm_u16_f32";
  case CVT_PKRTZ_F16_F32:
    return "v_cvt_pkrtz_f16_f32";
  case CVT_PK_U16_U32:
    return "v_cvt_pk_u16_u32";
  case CVT_PK_I16_I32:
    return "v_cvt_pk_i16_i32";
  case MAD_LEGACY_F32:
    return "v_mad_legacy_f32";
  case MAD_F32:
    return "v_mad_f32";
  case MAD_I32_I24:
    return "v_mad_i32_i24";
  case MAD_U32_U24:
    return "v_mad_u32_u24";
  case CUBEID_F32:
    return "v_cubeid_f32";
  case CUBESC_F32:
    return "v_cubesc_f32";
  case CUBETC_F32:
    return "v_cubetc_f32";
  case CUBEMA_F32:
    return "v_cubema_f32";
  case BFE_U32:
    return "v_bfe_u32";
  case BFE_I32:
    return "v_bfe_i32";
  case BFI_B32:
    return "v_bfi_b32";
  case FMA_F32:
    return "v_fma_f32";
  case FMA_F64:
    return "v_fma_f64";
  case LERP_U8:
    return "v_lerp_u8";
  case ALIGNBIT_B32:
    return "v_alignbit_b32";
  case ALIGNBYTE_B32:
    return "v_alignbyte_b32";
  case MULLIT_F32:
    return "v_mullit_f32";
  case MIN3_F32:
    return "v_min3_f32";
  case MIN3_I32:
    return "v_min3_i32";
  case MIN3_U32:
    return "v_min3_u32";
  case MAX3_F32:
    return "v_max3_f32";
  case MAX3_I32:
    return "v_max3_i32";
  case MAX3_U32:
    return "v_max3_u32";
  case MED3_F32:
    return "v_med3_f32";
  case MED3_I32:
    return "v_med3_i32";
  case MED3_U32:
    return "v_med3_u32";
  case SAD_U8:
    return "v_sad_u8";
  case SAD_HI_U8:
    return "v_sad_hi_u8";
  case SAD_U16:
    return "v_sad_u16";
  case SAD_U32:
    return "v_sad_u32";
  case CVT_PK_U8_F32:
    return "v_cvt_pk_u8_f32";
  case DIV_FIXUP_F32:
    return "v_div_fixup_f32";
  case DIV_FIXUP_F64:
    return "v_div_fixup_f64";
  case LSHL_B64:
    return "v_lshl_b64";
  case LSHR_B64:
    return "v_lshr_b64";
  case ASHR_I64:
    return "v_ashr_i64";
  case ADD_F64:
    return "v_add_f64";
  case MUL_F64:
    return "v_mul_f64";
  case MIN_F64:
    return "v_min_f64";
  case MAX_F64:
    return "v_max_f64";
  case LDEXP_F64:
    return "v_ldexp_f64";
  case MUL_LO_U32:
    return "v_mul_lo_u32";
  case MUL_HI_U32:
    return "v_mul_hi_u32";
  case MUL_LO_I32:
    return "v_mul_lo_i32";
  case MUL_HI_I32:
    return "v_mul_hi_i32";
  case DIV_SCALE_F32:
    return "v_div_scale_f32";
  case DIV_SCALE_F64:
    return "v_div_scale_f64";
  case DIV_FMAS_F32:
    return "v_div_fmas_f32";
  case DIV_FMAS_F64:
    return "v_div_fmas_f64";
  case MSAD_U8:
    return "v_msad_u8";
  case QSAD_U8:
    return "v_qsad_u8";
  case MQSAD_U8:
    return "v_mqsad_u8";
  case TRIG_PREOP_F64:
    return "v_trig_preop_f64";
  case MQSAD_U32_U8:
    return "v_mqsad_u32_u8";
  case MAD_U64_U32:
    return "v_mad_u64_u32";
  case MAD_I64_I32:
    return "v_mad_i64_i32";
  case NOP:
    return "v_nop";
  case MOV_B32:
    return "v_mov_b32";
  case READFIRSTLANE_B32:
    return "v_readfirstlane_b32";
  case CVT_I32_F64:
    return "v_cvt_i32_f64";
  case CVT_F64_I32:
    return "v_cvt_f64_i32";
  case CVT_F32_I32:
    return "v_cvt_f32_i32";
  case CVT_F32_U32:
    return "v_cvt_f32_u32";
  case CVT_U32_F32:
    return "v_cvt_u32_f32";
  case CVT_I32_F32:
    return "v_cvt_i32_f32";
  case MOV_FED_B32:
    return "v_mov_fed_b32";
  case CVT_F16_F32:
    return "v_cvt_f16_f32";
  case CVT_F32_F16:
    return "v_cvt_f32_f16";
  case CVT_RPI_I32_F32:
    return "v_cvt_rpi_i32_f32";
  case CVT_FLR_I32_F32:
    return "v_cvt_flr_i32_f32";
  case CVT_OFF_F32_I4:
    return "v_cvt_off_f32_i4";
  case CVT_F32_F64:
    return "v_cvt_f32_f64";
  case CVT_F64_F32:
    return "v_cvt_f64_f32";
  case CVT_F32_UBYTE0:
    return "v_cvt_f32_ubyte0";
  case CVT_F32_UBYTE1:
    return "v_cvt_f32_ubyte1";
  case CVT_F32_UBYTE2:
    return "v_cvt_f32_ubyte2";
  case CVT_F32_UBYTE3:
    return "v_cvt_f32_ubyte3";
  case CVT_U32_F64:
    return "v_cvt_u32_f64";
  case CVT_F64_U32:
    return "v_cvt_f64_u32";
  case FRACT_F32:
    return "v_fract_f32";
  case TRUNC_F32:
    return "v_trunc_f32";
  case CEIL_F32:
    return "v_ceil_f32";
  case RNDNE_F32:
    return "v_rndne_f32";
  case FLOOR_F32:
    return "v_floor_f32";
  case EXP_F32:
    return "v_exp_f32";
  case LOG_CLAMP_F32:
    return "v_log_clamp_f32";
  case LOG_F32:
    return "v_log_f32";
  case RCP_CLAMP_F32:
    return "v_rcp_clamp_f32";
  case RCP_LEGACY_F32:
    return "v_rcp_legacy_f32";
  case RCP_F32:
    return "v_rcp_f32";
  case RCP_IFLAG_F32:
    return "v_rcp_iflag_f32";
  case RSQ_CLAMP_F32:
    return "v_rsq_clamp_f32";
  case RSQ_LEGACY_F32:
    return "v_rsq_legacy_f32";
  case RSQ_F32:
    return "v_rsq_f32";
  case RCP_F64:
    return "v_rcp_f64";
  case RCP_CLAMP_F64:
    return "v_rcp_clamp_f64";
  case RSQ_F64:
    return "v_rsq_f64";
  case RSQ_CLAMP_F64:
    return "v_rsq_clamp_f64";
  case SQRT_F32:
    return "v_sqrt_f32";
  case SQRT_F64:
    return "v_sqrt_f64";
  case SIN_F32:
    return "v_sin_f32";
  case COS_F32:
    return "v_cos_f32";
  case NOT_B32:
    return "v_not_b32";
  case BFREV_B32:
    return "v_bfrev_b32";
  case FFBH_U32:
    return "v_ffbh_u32";
  case FFBL_B32:
    return "v_ffbl_b32";
  case FFBH_I32:
    return "v_ffbh_i32";
  case FREXP_EXP_I32_F64:
    return "v_frexp_exp_i32_f64";
  case FREXP_MANT_F64:
    return "v_frexp_mant_f64";
  case FRACT_F64:
    return "v_fract_f64";
  case FREXP_EXP_I32_F32:
    return "v_frexp_exp_i32_f32";
  case FREXP_MANT_F32:
    return "v_frexp_mant_f32";
  case CLREXCP:
    return "v_clrexcp";
  case MOVRELD_B32:
    return "v_movreld_b32";
  case MOVRELS_B32:
    return "v_movrels_b32";
  case MOVRELSD_B32:
    return "v_movrelsd_b32";
  }
  return nullptr;
}
}
