#pragma once

namespace shader::ir::vopc {
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
  CMPX_U_F16,
  CMPX_NGE_F16,
  CMPX_NLG_F16,
  CMPX_NGT_F16,
  CMPX_NLE_F16,
  CMPX_NEQ_F16,
  CMPX_NLT_F16,
  CMPX_TRU_F16,

  OpCount
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case CMP_F_F32: return "v_cmp_f_f32";
  case CMP_LT_F32: return "v_cmp_lt_f32";
  case CMP_EQ_F32: return "v_cmp_eq_f32";
  case CMP_LE_F32: return "v_cmp_le_f32";
  case CMP_GT_F32: return "v_cmp_gt_f32";
  case CMP_LG_F32: return "v_cmp_lg_f32";
  case CMP_GE_F32: return "v_cmp_ge_f32";
  case CMP_O_F32: return "v_cmp_o_f32";
  case CMP_U_F32: return "v_cmp_u_f32";
  case CMP_NGE_F32: return "v_cmp_nge_f32";
  case CMP_NLG_F32: return "v_cmp_nlg_f32";
  case CMP_NGT_F32: return "v_cmp_ngt_f32";
  case CMP_NLE_F32: return "v_cmp_nle_f32";
  case CMP_NEQ_F32: return "v_cmp_neq_f32";
  case CMP_NLT_F32: return "v_cmp_nlt_f32";
  case CMP_TRU_F32: return "v_cmp_tru_f32";
  case CMPX_F_F32: return "v_cmpx_f_f32";
  case CMPX_LT_F32: return "v_cmpx_lt_f32";
  case CMPX_EQ_F32: return "v_cmpx_eq_f32";
  case CMPX_LE_F32: return "v_cmpx_le_f32";
  case CMPX_GT_F32: return "v_cmpx_gt_f32";
  case CMPX_LG_F32: return "v_cmpx_lg_f32";
  case CMPX_GE_F32: return "v_cmpx_ge_f32";
  case CMPX_O_F32: return "v_cmpx_o_f32";
  case CMPX_U_F32: return "v_cmpx_u_f32";
  case CMPX_NGE_F32: return "v_cmpx_nge_f32";
  case CMPX_NLG_F32: return "v_cmpx_nlg_f32";
  case CMPX_NGT_F32: return "v_cmpx_ngt_f32";
  case CMPX_NLE_F32: return "v_cmpx_nle_f32";
  case CMPX_NEQ_F32: return "v_cmpx_neq_f32";
  case CMPX_NLT_F32: return "v_cmpx_nlt_f32";
  case CMPX_TRU_F32: return "v_cmpx_tru_f32";
  case CMP_F_F64: return "v_cmp_f_f64";
  case CMP_LT_F64: return "v_cmp_lt_f64";
  case CMP_EQ_F64: return "v_cmp_eq_f64";
  case CMP_LE_F64: return "v_cmp_le_f64";
  case CMP_GT_F64: return "v_cmp_gt_f64";
  case CMP_LG_F64: return "v_cmp_lg_f64";
  case CMP_GE_F64: return "v_cmp_ge_f64";
  case CMP_O_F64: return "v_cmp_o_f64";
  case CMP_U_F64: return "v_cmp_u_f64";
  case CMP_NGE_F64: return "v_cmp_nge_f64";
  case CMP_NLG_F64: return "v_cmp_nlg_f64";
  case CMP_NGT_F64: return "v_cmp_ngt_f64";
  case CMP_NLE_F64: return "v_cmp_nle_f64";
  case CMP_NEQ_F64: return "v_cmp_neq_f64";
  case CMP_NLT_F64: return "v_cmp_nlt_f64";
  case CMP_TRU_F64: return "v_cmp_tru_f64";
  case CMPX_F_F64: return "v_cmpx_f_f64";
  case CMPX_LT_F64: return "v_cmpx_lt_f64";
  case CMPX_EQ_F64: return "v_cmpx_eq_f64";
  case CMPX_LE_F64: return "v_cmpx_le_f64";
  case CMPX_GT_F64: return "v_cmpx_gt_f64";
  case CMPX_LG_F64: return "v_cmpx_lg_f64";
  case CMPX_GE_F64: return "v_cmpx_ge_f64";
  case CMPX_O_F64: return "v_cmpx_o_f64";
  case CMPX_U_F64: return "v_cmpx_u_f64";
  case CMPX_NGE_F64: return "v_cmpx_nge_f64";
  case CMPX_NLG_F64: return "v_cmpx_nlg_f64";
  case CMPX_NGT_F64: return "v_cmpx_ngt_f64";
  case CMPX_NLE_F64: return "v_cmpx_nle_f64";
  case CMPX_NEQ_F64: return "v_cmpx_neq_f64";
  case CMPX_NLT_F64: return "v_cmpx_nlt_f64";
  case CMPX_TRU_F64: return "v_cmpx_tru_f64";
  case CMPS_F_F32: return "v_cmps_f_f32";
  case CMPS_LT_F32: return "v_cmps_lt_f32";
  case CMPS_EQ_F32: return "v_cmps_eq_f32";
  case CMPS_LE_F32: return "v_cmps_le_f32";
  case CMPS_GT_F32: return "v_cmps_gt_f32";
  case CMPS_LG_F32: return "v_cmps_lg_f32";
  case CMPS_GE_F32: return "v_cmps_ge_f32";
  case CMPS_O_F32: return "v_cmps_o_f32";
  case CMPS_U_F32: return "v_cmps_u_f32";
  case CMPS_NGE_F32: return "v_cmps_nge_f32";
  case CMPS_NLG_F32: return "v_cmps_nlg_f32";
  case CMPS_NGT_F32: return "v_cmps_ngt_f32";
  case CMPS_NLE_F32: return "v_cmps_nle_f32";
  case CMPS_NEQ_F32: return "v_cmps_neq_f32";
  case CMPS_NLT_F32: return "v_cmps_nlt_f32";
  case CMPS_TRU_F32: return "v_cmps_tru_f32";
  case CMPSX_F_F32: return "v_cmpsx_f_f32";
  case CMPSX_LT_F32: return "v_cmpsx_lt_f32";
  case CMPSX_EQ_F32: return "v_cmpsx_eq_f32";
  case CMPSX_LE_F32: return "v_cmpsx_le_f32";
  case CMPSX_GT_F32: return "v_cmpsx_gt_f32";
  case CMPSX_LG_F32: return "v_cmpsx_lg_f32";
  case CMPSX_GE_F32: return "v_cmpsx_ge_f32";
  case CMPSX_O_F32: return "v_cmpsx_o_f32";
  case CMPSX_U_F32: return "v_cmpsx_u_f32";
  case CMPSX_NGE_F32: return "v_cmpsx_nge_f32";
  case CMPSX_NLG_F32: return "v_cmpsx_nlg_f32";
  case CMPSX_NGT_F32: return "v_cmpsx_ngt_f32";
  case CMPSX_NLE_F32: return "v_cmpsx_nle_f32";
  case CMPSX_NEQ_F32: return "v_cmpsx_neq_f32";
  case CMPSX_NLT_F32: return "v_cmpsx_nlt_f32";
  case CMPSX_TRU_F32: return "v_cmpsx_tru_f32";
  case CMPS_F_F64: return "v_cmps_f_f64";
  case CMPS_LT_F64: return "v_cmps_lt_f64";
  case CMPS_EQ_F64: return "v_cmps_eq_f64";
  case CMPS_LE_F64: return "v_cmps_le_f64";
  case CMPS_GT_F64: return "v_cmps_gt_f64";
  case CMPS_LG_F64: return "v_cmps_lg_f64";
  case CMPS_GE_F64: return "v_cmps_ge_f64";
  case CMPS_O_F64: return "v_cmps_o_f64";
  case CMPS_U_F64: return "v_cmps_u_f64";
  case CMPS_NGE_F64: return "v_cmps_nge_f64";
  case CMPS_NLG_F64: return "v_cmps_nlg_f64";
  case CMPS_NGT_F64: return "v_cmps_ngt_f64";
  case CMPS_NLE_F64: return "v_cmps_nle_f64";
  case CMPS_NEQ_F64: return "v_cmps_neq_f64";
  case CMPS_NLT_F64: return "v_cmps_nlt_f64";
  case CMPS_TRU_F64: return "v_cmps_tru_f64";
  case CMPSX_F_F64: return "v_cmpsx_f_f64";
  case CMPSX_LT_F64: return "v_cmpsx_lt_f64";
  case CMPSX_EQ_F64: return "v_cmpsx_eq_f64";
  case CMPSX_LE_F64: return "v_cmpsx_le_f64";
  case CMPSX_GT_F64: return "v_cmpsx_gt_f64";
  case CMPSX_LG_F64: return "v_cmpsx_lg_f64";
  case CMPSX_GE_F64: return "v_cmpsx_ge_f64";
  case CMPSX_O_F64: return "v_cmpsx_o_f64";
  case CMPSX_U_F64: return "v_cmpsx_u_f64";
  case CMPSX_NGE_F64: return "v_cmpsx_nge_f64";
  case CMPSX_NLG_F64: return "v_cmpsx_nlg_f64";
  case CMPSX_NGT_F64: return "v_cmpsx_ngt_f64";
  case CMPSX_NLE_F64: return "v_cmpsx_nle_f64";
  case CMPSX_NEQ_F64: return "v_cmpsx_neq_f64";
  case CMPSX_NLT_F64: return "v_cmpsx_nlt_f64";
  case CMPSX_TRU_F64: return "v_cmpsx_tru_f64";
  case CMP_F_I32: return "v_cmp_f_i32";
  case CMP_LT_I32: return "v_cmp_lt_i32";
  case CMP_EQ_I32: return "v_cmp_eq_i32";
  case CMP_LE_I32: return "v_cmp_le_i32";
  case CMP_GT_I32: return "v_cmp_gt_i32";
  case CMP_NE_I32: return "v_cmp_ne_i32";
  case CMP_GE_I32: return "v_cmp_ge_i32";
  case CMP_T_I32: return "v_cmp_t_i32";
  case CMP_CLASS_F32: return "v_cmp_class_f32";
  case CMP_LT_I16: return "v_cmp_lt_i16";
  case CMP_EQ_I16: return "v_cmp_eq_i16";
  case CMP_LE_I16: return "v_cmp_le_i16";
  case CMP_GT_I16: return "v_cmp_gt_i16";
  case CMP_NE_I16: return "v_cmp_ne_i16";
  case CMP_GE_I16: return "v_cmp_ge_i16";
  case CMP_CLASS_F16: return "v_cmp_class_f16";
  case CMPX_F_I32: return "v_cmpx_f_i32";
  case CMPX_LT_I32: return "v_cmpx_lt_i32";
  case CMPX_EQ_I32: return "v_cmpx_eq_i32";
  case CMPX_LE_I32: return "v_cmpx_le_i32";
  case CMPX_GT_I32: return "v_cmpx_gt_i32";
  case CMPX_NE_I32: return "v_cmpx_ne_i32";
  case CMPX_GE_I32: return "v_cmpx_ge_i32";
  case CMPX_T_I32: return "v_cmpx_t_i32";
  case CMPX_CLASS_F32: return "v_cmpx_class_f32";
  case CMPX_LT_I16: return "v_cmpx_lt_i16";
  case CMPX_EQ_I16: return "v_cmpx_eq_i16";
  case CMPX_LE_I16: return "v_cmpx_le_i16";
  case CMPX_GT_I16: return "v_cmpx_gt_i16";
  case CMPX_NE_I16: return "v_cmpx_ne_i16";
  case CMPX_GE_I16: return "v_cmpx_ge_i16";
  case CMPX_CLASS_F16: return "v_cmpx_class_f16";
  case CMP_F_I64: return "v_cmp_f_i64";
  case CMP_LT_I64: return "v_cmp_lt_i64";
  case CMP_EQ_I64: return "v_cmp_eq_i64";
  case CMP_LE_I64: return "v_cmp_le_i64";
  case CMP_GT_I64: return "v_cmp_gt_i64";
  case CMP_NE_I64: return "v_cmp_ne_i64";
  case CMP_GE_I64: return "v_cmp_ge_i64";
  case CMP_T_I64: return "v_cmp_t_i64";
  case CMP_CLASS_F64: return "v_cmp_class_f64";
  case CMP_LT_U16: return "v_cmp_lt_u16";
  case CMP_EQ_U16: return "v_cmp_eq_u16";
  case CMP_LE_U16: return "v_cmp_le_u16";
  case CMP_GT_U16: return "v_cmp_gt_u16";
  case CMP_NE_U16: return "v_cmp_ne_u16";
  case CMP_GE_U16: return "v_cmp_ge_u16";
  case CMPX_F_I64: return "v_cmpx_f_i64";
  case CMPX_LT_I64: return "v_cmpx_lt_i64";
  case CMPX_EQ_I64: return "v_cmpx_eq_i64";
  case CMPX_LE_I64: return "v_cmpx_le_i64";
  case CMPX_GT_I64: return "v_cmpx_gt_i64";
  case CMPX_NE_I64: return "v_cmpx_ne_i64";
  case CMPX_GE_I64: return "v_cmpx_ge_i64";
  case CMPX_T_I64: return "v_cmpx_t_i64";
  case CMPX_CLASS_F64: return "v_cmpx_class_f64";
  case CMPX_LT_U16: return "v_cmpx_lt_u16";
  case CMPX_EQ_U16: return "v_cmpx_eq_u16";
  case CMPX_LE_U16: return "v_cmpx_le_u16";
  case CMPX_GT_U16: return "v_cmpx_gt_u16";
  case CMPX_NE_U16: return "v_cmpx_ne_u16";
  case CMPX_GE_U16: return "v_cmpx_ge_u16";
  case CMP_F_U32: return "v_cmp_f_u32";
  case CMP_LT_U32: return "v_cmp_lt_u32";
  case CMP_EQ_U32: return "v_cmp_eq_u32";
  case CMP_LE_U32: return "v_cmp_le_u32";
  case CMP_GT_U32: return "v_cmp_gt_u32";
  case CMP_NE_U32: return "v_cmp_ne_u32";
  case CMP_GE_U32: return "v_cmp_ge_u32";
  case CMP_T_U32: return "v_cmp_t_u32";
  case CMP_F_F16: return "v_cmp_f_f16";
  case CMP_LT_F16: return "v_cmp_lt_f16";
  case CMP_EQ_F16: return "v_cmp_eq_f16";
  case CMP_LE_F16: return "v_cmp_le_f16";
  case CMP_GT_F16: return "v_cmp_gt_f16";
  case CMP_LG_F16: return "v_cmp_lg_f16";
  case CMP_GE_F16: return "v_cmp_ge_f16";
  case CMP_O_F16: return "v_cmp_o_f16";
  case CMPX_F_U32: return "v_cmpx_f_u32";
  case CMPX_LT_U32: return "v_cmpx_lt_u32";
  case CMPX_EQ_U32: return "v_cmpx_eq_u32";
  case CMPX_LE_U32: return "v_cmpx_le_u32";
  case CMPX_GT_U32: return "v_cmpx_gt_u32";
  case CMPX_NE_U32: return "v_cmpx_ne_u32";
  case CMPX_GE_U32: return "v_cmpx_ge_u32";
  case CMPX_T_U32: return "v_cmpx_t_u32";
  case CMPX_F_F16: return "v_cmpx_f_f16";
  case CMPX_LT_F16: return "v_cmpx_lt_f16";
  case CMPX_EQ_F16: return "v_cmpx_eq_f16";
  case CMPX_LE_F16: return "v_cmpx_le_f16";
  case CMPX_GT_F16: return "v_cmpx_gt_f16";
  case CMPX_LG_F16: return "v_cmpx_lg_f16";
  case CMPX_GE_F16: return "v_cmpx_ge_f16";
  case CMPX_O_F16: return "v_cmpx_o_f16";
  case CMP_F_U64: return "v_cmp_f_u64";
  case CMP_LT_U64: return "v_cmp_lt_u64";
  case CMP_EQ_U64: return "v_cmp_eq_u64";
  case CMP_LE_U64: return "v_cmp_le_u64";
  case CMP_GT_U64: return "v_cmp_gt_u64";
  case CMP_NE_U64: return "v_cmp_ne_u64";
  case CMP_GE_U64: return "v_cmp_ge_u64";
  case CMP_T_U64: return "v_cmp_t_u64";
  case CMP_U_F16: return "v_cmp_u_f16";
  case CMP_NGE_F16: return "v_cmp_nge_f16";
  case CMP_NLG_F16: return "v_cmp_nlg_f16";
  case CMP_NGT_F16: return "v_cmp_ngt_f16";
  case CMP_NLE_F16: return "v_cmp_nle_f16";
  case CMP_NEQ_F16: return "v_cmp_neq_f16";
  case CMP_NLT_F16: return "v_cmp_nlt_f16";
  case CMP_TRU_F16: return "v_cmp_tru_f16";
  case CMPX_F_U64: return "v_cmpx_f_u64";
  case CMPX_LT_U64: return "v_cmpx_lt_u64";
  case CMPX_EQ_U64: return "v_cmpx_eq_u64";
  case CMPX_LE_U64: return "v_cmpx_le_u64";
  case CMPX_GT_U64: return "v_cmpx_gt_u64";
  case CMPX_NE_U64: return "v_cmpx_ne_u64";
  case CMPX_GE_U64: return "v_cmpx_ge_u64";
  case CMPX_T_U64: return "v_cmpx_t_u64";
  case CMPX_U_F16: return "v_cmpx_u_f16";
  case CMPX_NGE_F16: return "v_cmpx_nge_f16";
  case CMPX_NLG_F16: return "v_cmpx_nlg_f16";
  case CMPX_NGT_F16: return "v_cmpx_ngt_f16";
  case CMPX_NLE_F16: return "v_cmpx_nle_f16";
  case CMPX_NEQ_F16: return "v_cmpx_neq_f16";
  case CMPX_NLT_F16: return "v_cmpx_nlt_f16";
  case CMPX_TRU_F16: return "v_cmpx_tru_f16";
  }
  return nullptr;
}
}
