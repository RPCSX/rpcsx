#pragma once

namespace shader::ir::mimg {
enum Op {
  LOAD,
  LOAD_MIP,
  LOAD_PCK,
  LOAD_PCK_SGN,
  LOAD_MIP_PCK,
  LOAD_MIP_PCK_SGN,
  STORE = 8,
  STORE_MIP,
  STORE_PCK,
  STORE_MIP_PCK,
  GET_RESINFO = 14,
  ATOMIC_SWAP,
  ATOMIC_CMPSWAP,
  ATOMIC_ADD,
  ATOMIC_SUB,
  ATOMIC_RSUB,
  ATOMIC_SMIN,
  ATOMIC_UMIN,
  ATOMIC_SMAX,
  ATOMIC_UMAX,
  ATOMIC_AND,
  ATOMIC_OR,
  ATOMIC_XOR,
  ATOMIC_INC,
  ATOMIC_DEC,
  ATOMIC_FCMPSWAP,
  ATOMIC_FMIN,
  ATOMIC_FMAX,
  SAMPLE,
  SAMPLE_CL,
  SAMPLE_D,
  SAMPLE_D_CL,
  SAMPLE_L,
  SAMPLE_B,
  SAMPLE_B_CL,
  SAMPLE_LZ,
  SAMPLE_C,
  SAMPLE_C_CL,
  SAMPLE_C_D,
  SAMPLE_C_D_CL,
  SAMPLE_C_L,
  SAMPLE_C_B,
  SAMPLE_C_B_CL,
  SAMPLE_C_LZ,
  SAMPLE_O,
  SAMPLE_CL_O,
  SAMPLE_D_O,
  SAMPLE_D_CL_O,
  SAMPLE_L_O,
  SAMPLE_B_O,
  SAMPLE_B_CL_O,
  SAMPLE_LZ_O,
  SAMPLE_C_O,
  SAMPLE_C_CL_O,
  SAMPLE_C_D_O,
  SAMPLE_C_D_CL_O,
  SAMPLE_C_L_O,
  SAMPLE_C_B_O,
  SAMPLE_C_B_CL_O,
  SAMPLE_C_LZ_O,
  GATHER4,
  GATHER4_CL,
  GATHER4_L = 68,
  GATHER4_B,
  GATHER4_B_CL,
  GATHER4_LZ,
  GATHER4_C,
  GATHER4_C_CL,
  GATHER4_C_L = 76,
  GATHER4_C_B,
  GATHER4_C_B_CL,
  GATHER4_C_LZ,
  GATHER4_O,
  GATHER4_CL_O,
  GATHER4_L_O = 84,
  GATHER4_B_O,
  GATHER4_B_CL_O,
  GATHER4_LZ_O,
  GATHER4_C_O,
  GATHER4_C_CL_O,
  GATHER4_C_L_O = 92,
  GATHER4_C_B_O,
  GATHER4_C_B_CL_O,
  GATHER4_C_LZ_O,
  GET_LOD,
  SAMPLE_CD = 104,
  SAMPLE_CD_CL,
  SAMPLE_C_CD,
  SAMPLE_C_CD_CL,
  SAMPLE_CD_O,
  SAMPLE_CD_CL_O,
  SAMPLE_C_CD_O,
  SAMPLE_C_CD_CL_O,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case LOAD: return "image_load";
  case LOAD_MIP: return "image_load_mip";
  case LOAD_PCK: return "image_load_pck";
  case LOAD_PCK_SGN: return "image_load_pck_sgn";
  case LOAD_MIP_PCK: return "image_load_mip_pck";
  case LOAD_MIP_PCK_SGN: return "image_load_mip_pck_sgn";
  case STORE: return "image_store";
  case STORE_MIP: return "image_store_mip";
  case STORE_PCK: return "image_store_pck";
  case STORE_MIP_PCK: return "image_store_mip_pck";
  case GET_RESINFO: return "image_get_resinfo";
  case ATOMIC_SWAP: return "image_atomic_swap";
  case ATOMIC_CMPSWAP: return "image_atomic_cmpswap";
  case ATOMIC_ADD: return "image_atomic_add";
  case ATOMIC_SUB: return "image_atomic_sub";
  case ATOMIC_RSUB: return "image_atomic_rsub";
  case ATOMIC_SMIN: return "image_atomic_smin";
  case ATOMIC_UMIN: return "image_atomic_umin";
  case ATOMIC_SMAX: return "image_atomic_smax";
  case ATOMIC_UMAX: return "image_atomic_umax";
  case ATOMIC_AND: return "image_atomic_and";
  case ATOMIC_OR: return "image_atomic_or";
  case ATOMIC_XOR: return "image_atomic_xor";
  case ATOMIC_INC: return "image_atomic_inc";
  case ATOMIC_DEC: return "image_atomic_dec";
  case ATOMIC_FCMPSWAP: return "image_atomic_fcmpswap";
  case ATOMIC_FMIN: return "image_atomic_fmin";
  case ATOMIC_FMAX: return "image_atomic_fmax";
  case SAMPLE: return "image_sample";
  case SAMPLE_CL: return "image_sample_cl";
  case SAMPLE_D: return "image_sample_d";
  case SAMPLE_D_CL: return "image_sample_d_cl";
  case SAMPLE_L: return "image_sample_l";
  case SAMPLE_B: return "image_sample_b";
  case SAMPLE_B_CL: return "image_sample_b_cl";
  case SAMPLE_LZ: return "image_sample_lz";
  case SAMPLE_C: return "image_sample_c";
  case SAMPLE_C_CL: return "image_sample_c_cl";
  case SAMPLE_C_D: return "image_sample_c_d";
  case SAMPLE_C_D_CL: return "image_sample_c_d_cl";
  case SAMPLE_C_L: return "image_sample_c_l";
  case SAMPLE_C_B: return "image_sample_c_b";
  case SAMPLE_C_B_CL: return "image_sample_c_b_cl";
  case SAMPLE_C_LZ: return "image_sample_c_lz";
  case SAMPLE_O: return "image_sample_o";
  case SAMPLE_CL_O: return "image_sample_cl_o";
  case SAMPLE_D_O: return "image_sample_d_o";
  case SAMPLE_D_CL_O: return "image_sample_d_cl_o";
  case SAMPLE_L_O: return "image_sample_l_o";
  case SAMPLE_B_O: return "image_sample_b_o";
  case SAMPLE_B_CL_O: return "image_sample_b_cl_o";
  case SAMPLE_LZ_O: return "image_sample_lz_o";
  case SAMPLE_C_O: return "image_sample_c_o";
  case SAMPLE_C_CL_O: return "image_sample_c_cl_o";
  case SAMPLE_C_D_O: return "image_sample_c_d_o";
  case SAMPLE_C_D_CL_O: return "image_sample_c_d_cl_o";
  case SAMPLE_C_L_O: return "image_sample_c_l_o";
  case SAMPLE_C_B_O: return "image_sample_c_b_o";
  case SAMPLE_C_B_CL_O: return "image_sample_c_b_cl_o";
  case SAMPLE_C_LZ_O: return "image_sample_c_lz_o";
  case GATHER4: return "image_gather4";
  case GATHER4_CL: return "image_gather4_cl";
  case GATHER4_L: return "image_gather4_l";
  case GATHER4_B: return "image_gather4_b";
  case GATHER4_B_CL: return "image_gather4_b_cl";
  case GATHER4_LZ: return "image_gather4_lz";
  case GATHER4_C: return "image_gather4_c";
  case GATHER4_C_CL: return "image_gather4_c_cl";
  case GATHER4_C_L: return "image_gather4_c_l";
  case GATHER4_C_B: return "image_gather4_c_b";
  case GATHER4_C_B_CL: return "image_gather4_c_b_cl";
  case GATHER4_C_LZ: return "image_gather4_c_lz";
  case GATHER4_O: return "image_gather4_o";
  case GATHER4_CL_O: return "image_gather4_cl_o";
  case GATHER4_L_O: return "image_gather4_l_o";
  case GATHER4_B_O: return "image_gather4_b_o";
  case GATHER4_B_CL_O: return "image_gather4_b_cl_o";
  case GATHER4_LZ_O: return "image_gather4_lz_o";
  case GATHER4_C_O: return "image_gather4_c_o";
  case GATHER4_C_CL_O: return "image_gather4_c_cl_o";
  case GATHER4_C_L_O: return "image_gather4_c_l_o";
  case GATHER4_C_B_O: return "image_gather4_c_b_o";
  case GATHER4_C_B_CL_O: return "image_gather4_c_b_cl_o";
  case GATHER4_C_LZ_O: return "image_gather4_c_lz_o";
  case GET_LOD: return "image_get_lod";
  case SAMPLE_CD: return "image_sample_cd";
  case SAMPLE_CD_CL: return "image_sample_cd_cl";
  case SAMPLE_C_CD: return "image_sample_c_cd";
  case SAMPLE_C_CD_CL: return "image_sample_c_cd_cl";
  case SAMPLE_CD_O: return "image_sample_cd_o";
  case SAMPLE_CD_CL_O: return "image_sample_cd_cl_o";
  case SAMPLE_C_CD_O: return "image_sample_c_cd_o";
  case SAMPLE_C_CD_CL_O: return "image_sample_c_cd_cl_o";
  }
  return nullptr;
}
} // namespace shader::ir::mimg
