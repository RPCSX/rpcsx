#pragma once

namespace shader::ir::vop1 {
enum Op {
  NOP,
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
  FRACT_F32 = 32,
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
  CVT_F16_U16 = 80,
  CVT_F16_I16,
  CVT_U16_F16,
  CVT_I16_F16,
  RCP_F16,
  SQRT_F16,
  RSQ_F16,
  LOG_F16,
  EXP_F16,
  FREXP_MANT_F16,
  FREXP_EXP_I16_F16,
  FLOOR_F16,
  CEIL_F16,
  TRUNC_F16,
  RNDNE_F16,
  FRACT_F16,
  SIN_F16,
  COS_F16,
  SAT_PK_U8_I16,
  CVT_NORM_I16_F16,
  CVT_NORM_U16_F16,
  SWAP_B32,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
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
  case CVT_F16_U16:
    return "v_cvt_f16_u16";
  case CVT_F16_I16:
    return "v_cvt_f16_i16";
  case CVT_U16_F16:
    return "v_cvt_u16_f16";
  case CVT_I16_F16:
    return "v_cvt_i16_f16";
  case RCP_F16:
    return "v_rcp_f16";
  case SQRT_F16:
    return "v_sqrt_f16";
  case RSQ_F16:
    return "v_rsq_f16";
  case LOG_F16:
    return "v_log_f16";
  case EXP_F16:
    return "v_exp_f16";
  case FREXP_MANT_F16:
    return "v_frexp_mant_f16";
  case FREXP_EXP_I16_F16:
    return "v_frexp_exp_i16_f16";
  case FLOOR_F16:
    return "v_floor_f16";
  case CEIL_F16:
    return "v_ceil_f16";
  case TRUNC_F16:
    return "v_trunc_f16";
  case RNDNE_F16:
    return "v_rndne_f16";
  case FRACT_F16:
    return "v_fract_f16";
  case SIN_F16:
    return "v_sin_f16";
  case COS_F16:
    return "v_cos_f16";
  case SAT_PK_U8_I16:
    return "v_sat_pk_u8_i16";
  case CVT_NORM_I16_F16:
    return "v_cvt_norm_i16_f16";
  case CVT_NORM_U16_F16:
    return "v_cvt_norm_u16_f16";
  case SWAP_B32:
    return "v_swap_b32";
  }
  return nullptr;
}
} // namespace shader::ir::vop1
