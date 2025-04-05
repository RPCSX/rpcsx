#pragma once

namespace shader::ir::ds {
enum Op {
  ADD_U32,
  SUB_U32,
  RSUB_U32,
  INC_U32,
  DEC_U32,
  MIN_I32,
  MAX_I32,
  MIN_U32,
  MAX_U32,
  AND_B32,
  OR_B32,
  XOR_B32,
  MSKOR_B32,
  WRITE_B32,
  WRITE2_B32,
  WRITE2ST64_B32,
  CMPST_B32,
  CMPST_F32,
  MIN_F32,
  MAX_F32,
  NOP,
  GWS_SEMA_RELEASE_ALL = 24,
  GWS_INIT,
  GWS_SEMA_V,
  GWS_SEMA_BR,
  GWS_SEMA_P,
  GWS_BARRIER,
  WRITE_B8,
  WRITE_B16,
  ADD_RTN_U32,
  SUB_RTN_U32,
  RSUB_RTN_U32,
  INC_RTN_U32,
  DEC_RTN_U32,
  MIN_RTN_I32,
  MAX_RTN_I32,
  MIN_RTN_U32,
  MAX_RTN_U32,
  AND_RTN_B32,
  OR_RTN_B32,
  XOR_RTN_B32,
  MSKOR_RTN_B32,
  WRXCHG_RTN_B32,
  WRXCHG2_RTN_B32,
  WRXCHG2ST64_RTN_B32,
  CMPST_RTN_B32,
  CMPST_RTN_F32,
  MIN_RTN_F32,
  MAX_RTN_F32,
  WRAP_RTN_B32,
  SWIZZLE_B32,
  READ_B32,
  READ2_B32,
  READ2ST64_B32,
  READ_I8,
  READ_U8,
  READ_I16,
  READ_U16,
  CONSUME,
  APPEND,
  ORDERED_COUNT,
  ADD_U64,
  SUB_U64,
  RSUB_U64,
  INC_U64,
  DEC_U64,
  MIN_I64,
  MAX_I64,
  MIN_U64,
  MAX_U64,
  AND_B64,
  OR_B64,
  XOR_B64,
  MSKOR_B64,
  WRITE_B64,
  WRITE2_B64,
  WRITE2ST64_B64,
  CMPST_B64,
  CMPST_F64,
  MIN_F64,
  MAX_F64,
  ADD_RTN_U64 = 96,
  SUB_RTN_U64,
  RSUB_RTN_U64,
  INC_RTN_U64,
  DEC_RTN_U64,
  MIN_RTN_I64,
  MAX_RTN_I64,
  MIN_RTN_U64,
  MAX_RTN_U64,
  AND_RTN_B64,
  OR_RTN_B64,
  XOR_RTN_B64,
  MSKOR_RTN_B64,
  WRXCHG_RTN_B64,
  WRXCHG2_RTN_B64,
  WRXCHG2ST64_RTN_B64,
  CMPST_RTN_B64,
  CMPST_RTN_F64,
  MIN_RTN_F64,
  MAX_RTN_F64,
  READ_B64 = 118,
  READ2_B64,
  READ2ST64_B64,
  CONDXCHG32_RTN_B64 = 126,
  ADD_SRC2_U32 = 128,
  SUB_SRC2_U32,
  RSUB_SRC2_U32,
  INC_SRC2_U32,
  DEC_SRC2_U32,
  MIN_SRC2_I32,
  MAX_SRC2_I32,
  MIN_SRC2_U32,
  MAX_SRC2_U32,
  AND_SRC2_B32,
  OR_SRC2_B32,
  XOR_SRC2_B32,
  WRITE_SRC2_B32,
  MIN_SRC2_F32 = 146,
  MAX_SRC2_F32,
  ADD_SRC2_U64 = 192,
  SUB_SRC2_U64,
  RSUB_SRC2_U64,
  INC_SRC2_U64,
  DEC_SRC2_U64,
  MIN_SRC2_I64,
  MAX_SRC2_I64,
  MIN_SRC2_U64,
  MAX_SRC2_U64,
  AND_SRC2_B64,
  OR_SRC2_B64,
  XOR_SRC2_B64,
  WRITE_SRC2_B64,
  MIN_SRC2_F64 = 210,
  MAX_SRC2_F64,
  WRITE_B96 = 222,
  WRITE_B128,
  CONDXCHG32_RTN_B128 = 253,
  READ_B96,
  READ_B128,

  OpCount
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case ADD_U32:
    return "ds_add_u32";
  case SUB_U32:
    return "ds_sub_u32";
  case RSUB_U32:
    return "ds_rsub_u32";
  case INC_U32:
    return "ds_inc_u32";
  case DEC_U32:
    return "ds_dec_u32";
  case MIN_I32:
    return "ds_min_i32";
  case MAX_I32:
    return "ds_max_i32";
  case MIN_U32:
    return "ds_min_u32";
  case MAX_U32:
    return "ds_max_u32";
  case AND_B32:
    return "ds_and_b32";
  case OR_B32:
    return "ds_or_b32";
  case XOR_B32:
    return "ds_xor_b32";
  case MSKOR_B32:
    return "ds_mskor_b32";
  case WRITE_B32:
    return "ds_write_b32";
  case WRITE2_B32:
    return "ds_write2_b32";
  case WRITE2ST64_B32:
    return "ds_write2st64_b32";
  case CMPST_B32:
    return "ds_cmpst_b32";
  case CMPST_F32:
    return "ds_cmpst_f32";
  case MIN_F32:
    return "ds_min_f32";
  case MAX_F32:
    return "ds_max_f32";
  case NOP:
    return "ds_nop";
  case GWS_SEMA_RELEASE_ALL:
    return "ds_gws_sema_release_all";
  case GWS_INIT:
    return "ds_gws_init";
  case GWS_SEMA_V:
    return "ds_gws_sema_v";
  case GWS_SEMA_BR:
    return "ds_gws_sema_br";
  case GWS_SEMA_P:
    return "ds_gws_sema_p";
  case GWS_BARRIER:
    return "ds_gws_barrier";
  case WRITE_B8:
    return "ds_write_b8";
  case WRITE_B16:
    return "ds_write_b16";
  case ADD_RTN_U32:
    return "ds_add_rtn_u32";
  case SUB_RTN_U32:
    return "ds_sub_rtn_u32";
  case RSUB_RTN_U32:
    return "ds_rsub_rtn_u32";
  case INC_RTN_U32:
    return "ds_inc_rtn_u32";
  case DEC_RTN_U32:
    return "ds_dec_rtn_u32";
  case MIN_RTN_I32:
    return "ds_min_rtn_i32";
  case MAX_RTN_I32:
    return "ds_max_rtn_i32";
  case MIN_RTN_U32:
    return "ds_min_rtn_u32";
  case MAX_RTN_U32:
    return "ds_max_rtn_u32";
  case AND_RTN_B32:
    return "ds_and_rtn_b32";
  case OR_RTN_B32:
    return "ds_or_rtn_b32";
  case XOR_RTN_B32:
    return "ds_xor_rtn_b32";
  case MSKOR_RTN_B32:
    return "ds_mskor_rtn_b32";
  case WRXCHG_RTN_B32:
    return "ds_wrxchg_rtn_b32";
  case WRXCHG2_RTN_B32:
    return "ds_wrxchg2_rtn_b32";
  case WRXCHG2ST64_RTN_B32:
    return "ds_wrxchg2st64_rtn_b32";
  case CMPST_RTN_B32:
    return "ds_cmpst_rtn_b32";
  case CMPST_RTN_F32:
    return "ds_cmpst_rtn_f32";
  case MIN_RTN_F32:
    return "ds_min_rtn_f32";
  case MAX_RTN_F32:
    return "ds_max_rtn_f32";
  case WRAP_RTN_B32:
    return "ds_wrap_rtn_b32";
  case SWIZZLE_B32:
    return "ds_swizzle_b32";
  case READ_B32:
    return "ds_read_b32";
  case READ2_B32:
    return "ds_read2_b32";
  case READ2ST64_B32:
    return "ds_read2st64_b32";
  case READ_I8:
    return "ds_read_i8";
  case READ_U8:
    return "ds_read_u8";
  case READ_I16:
    return "ds_read_i16";
  case READ_U16:
    return "ds_read_u16";
  case CONSUME:
    return "ds_consume";
  case APPEND:
    return "ds_append";
  case ORDERED_COUNT:
    return "ds_ordered_count";
  case ADD_U64:
    return "ds_add_u64";
  case SUB_U64:
    return "ds_sub_u64";
  case RSUB_U64:
    return "ds_rsub_u64";
  case INC_U64:
    return "ds_inc_u64";
  case DEC_U64:
    return "ds_dec_u64";
  case MIN_I64:
    return "ds_min_i64";
  case MAX_I64:
    return "ds_max_i64";
  case MIN_U64:
    return "ds_min_u64";
  case MAX_U64:
    return "ds_max_u64";
  case AND_B64:
    return "ds_and_b64";
  case OR_B64:
    return "ds_or_b64";
  case XOR_B64:
    return "ds_xor_b64";
  case MSKOR_B64:
    return "ds_mskor_b64";
  case WRITE_B64:
    return "ds_write_b64";
  case WRITE2_B64:
    return "ds_write2_b64";
  case WRITE2ST64_B64:
    return "ds_write2st64_b64";
  case CMPST_B64:
    return "ds_cmpst_b64";
  case CMPST_F64:
    return "ds_cmpst_f64";
  case MIN_F64:
    return "ds_min_f64";
  case MAX_F64:
    return "ds_max_f64";
  case ADD_RTN_U64:
    return "ds_add_rtn_u64";
  case SUB_RTN_U64:
    return "ds_sub_rtn_u64";
  case RSUB_RTN_U64:
    return "ds_rsub_rtn_u64";
  case INC_RTN_U64:
    return "ds_inc_rtn_u64";
  case DEC_RTN_U64:
    return "ds_dec_rtn_u64";
  case MIN_RTN_I64:
    return "ds_min_rtn_i64";
  case MAX_RTN_I64:
    return "ds_max_rtn_i64";
  case MIN_RTN_U64:
    return "ds_min_rtn_u64";
  case MAX_RTN_U64:
    return "ds_max_rtn_u64";
  case AND_RTN_B64:
    return "ds_and_rtn_b64";
  case OR_RTN_B64:
    return "ds_or_rtn_b64";
  case XOR_RTN_B64:
    return "ds_xor_rtn_b64";
  case MSKOR_RTN_B64:
    return "ds_mskor_rtn_b64";
  case WRXCHG_RTN_B64:
    return "ds_wrxchg_rtn_b64";
  case WRXCHG2_RTN_B64:
    return "ds_wrxchg2_rtn_b64";
  case WRXCHG2ST64_RTN_B64:
    return "ds_wrxchg2st64_rtn_b64";
  case CMPST_RTN_B64:
    return "ds_cmpst_rtn_b64";
  case CMPST_RTN_F64:
    return "ds_cmpst_rtn_f64";
  case MIN_RTN_F64:
    return "ds_min_rtn_f64";
  case MAX_RTN_F64:
    return "ds_max_rtn_f64";
  case READ_B64:
    return "ds_read_b64";
  case READ2_B64:
    return "ds_read2_b64";
  case READ2ST64_B64:
    return "ds_read2st64_b64";
  case CONDXCHG32_RTN_B64:
    return "ds_condxchg32_rtn_b64";
  case ADD_SRC2_U32:
    return "ds_add_src2_u32";
  case SUB_SRC2_U32:
    return "ds_sub_src2_u32";
  case RSUB_SRC2_U32:
    return "ds_rsub_src2_u32";
  case INC_SRC2_U32:
    return "ds_inc_src2_u32";
  case DEC_SRC2_U32:
    return "ds_dec_src2_u32";
  case MIN_SRC2_I32:
    return "ds_min_src2_i32";
  case MAX_SRC2_I32:
    return "ds_max_src2_i32";
  case MIN_SRC2_U32:
    return "ds_min_src2_u32";
  case MAX_SRC2_U32:
    return "ds_max_src2_u32";
  case AND_SRC2_B32:
    return "ds_and_src2_b32";
  case OR_SRC2_B32:
    return "ds_or_src2_b32";
  case XOR_SRC2_B32:
    return "ds_xor_src2_b32";
  case WRITE_SRC2_B32:
    return "ds_write_src2_b32";
  case MIN_SRC2_F32:
    return "ds_min_src2_f32";
  case MAX_SRC2_F32:
    return "ds_max_src2_f32";
  case ADD_SRC2_U64:
    return "ds_add_src2_u64";
  case SUB_SRC2_U64:
    return "ds_sub_src2_u64";
  case RSUB_SRC2_U64:
    return "ds_rsub_src2_u64";
  case INC_SRC2_U64:
    return "ds_inc_src2_u64";
  case DEC_SRC2_U64:
    return "ds_dec_src2_u64";
  case MIN_SRC2_I64:
    return "ds_min_src2_i64";
  case MAX_SRC2_I64:
    return "ds_max_src2_i64";
  case MIN_SRC2_U64:
    return "ds_min_src2_u64";
  case MAX_SRC2_U64:
    return "ds_max_src2_u64";
  case AND_SRC2_B64:
    return "ds_and_src2_b64";
  case OR_SRC2_B64:
    return "ds_or_src2_b64";
  case XOR_SRC2_B64:
    return "ds_xor_src2_b64";
  case WRITE_SRC2_B64:
    return "ds_write_src2_b64";
  case MIN_SRC2_F64:
    return "ds_min_src2_f64";
  case MAX_SRC2_F64:
    return "ds_max_src2_f64";
  case WRITE_B96:
    return "ds_write_b96";
  case WRITE_B128:
    return "ds_write_b128";
  case CONDXCHG32_RTN_B128:
    return "ds_condxchg32_rtn_b128";
  case READ_B96:
    return "ds_read_b96";
  case READ_B128:
    return "ds_read_b128";
  }
  return nullptr;
}
} // namespace shader::ir::ds
