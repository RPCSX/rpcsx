#pragma once

namespace shader::ir::vop2 {
enum Op {
  CNDMASK_B32,
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

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
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
  }
  return nullptr;
}

} // namespace shader::ir::vop2
