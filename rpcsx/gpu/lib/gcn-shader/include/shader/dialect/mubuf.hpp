#pragma once

namespace shader::ir::mubuf {
enum Op {
  LOAD_FORMAT_X,
  LOAD_FORMAT_XY,
  LOAD_FORMAT_XYZ,
  LOAD_FORMAT_XYZW,
  STORE_FORMAT_X,
  STORE_FORMAT_XY,
  STORE_FORMAT_XYZ,
  STORE_FORMAT_XYZW,
  LOAD_UBYTE,
  LOAD_SBYTE,
  LOAD_USHORT,
  LOAD_SSHORT,
  LOAD_DWORD,
  LOAD_DWORDX2,
  LOAD_DWORDX4,
  LOAD_DWORDX3,
  STORE_BYTE = 24,
  STORE_SHORT = 26,
  STORE_DWORD = 28,
  STORE_DWORDX2,
  STORE_DWORDX4,
  STORE_DWORDX3,
  ATOMIC_SWAP = 48,
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
  ATOMIC_SWAP_X2 = 80,
  ATOMIC_CMPSWAP_X2,
  ATOMIC_ADD_X2,
  ATOMIC_SUB_X2,
  ATOMIC_RSUB_X2,
  ATOMIC_SMIN_X2,
  ATOMIC_UMIN_X2,
  ATOMIC_SMAX_X2,
  ATOMIC_UMAX_X2,
  ATOMIC_AND_X2,
  ATOMIC_OR_X2,
  ATOMIC_XOR_X2,
  ATOMIC_INC_X2,
  ATOMIC_DEC_X2,
  ATOMIC_FCMPSWAP_X2,
  ATOMIC_FMIN_X2,
  ATOMIC_FMAX_X2,
  WBINVL1_SC_VOL = 112,
  WBINVL1,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case LOAD_FORMAT_X:return "buffer_load_format_x";
  case LOAD_FORMAT_XY:return "buffer_load_format_xy";
  case LOAD_FORMAT_XYZ:return "buffer_load_format_xyz";
  case LOAD_FORMAT_XYZW:return "buffer_load_format_xyzw";
  case STORE_FORMAT_X:return "buffer_store_format_x";
  case STORE_FORMAT_XY:return "buffer_store_format_xy";
  case STORE_FORMAT_XYZ:return "buffer_store_format_xyz";
  case STORE_FORMAT_XYZW:return "buffer_store_format_xyzw";
  case LOAD_UBYTE:return "buffer_load_ubyte";
  case LOAD_SBYTE:return "buffer_load_sbyte";
  case LOAD_USHORT:return "buffer_load_ushort";
  case LOAD_SSHORT:return "buffer_load_sshort";
  case LOAD_DWORD:return "buffer_load_dword";
  case LOAD_DWORDX2:return "buffer_load_dwordx2";
  case LOAD_DWORDX4:return "buffer_load_dwordx4";
  case LOAD_DWORDX3:return "buffer_load_dwordx3";
  case STORE_BYTE:return "buffer_store_byte";
  case STORE_SHORT:return "buffer_store_short";
  case STORE_DWORD:return "buffer_store_dword";
  case STORE_DWORDX2:return "buffer_store_dwordx2";
  case STORE_DWORDX4:return "buffer_store_dwordx4";
  case STORE_DWORDX3:return "buffer_store_dwordx3";
  case ATOMIC_SWAP:return "buffer_atomic_swap";
  case ATOMIC_CMPSWAP:return "buffer_atomic_cmpswap";
  case ATOMIC_ADD:return "buffer_atomic_add";
  case ATOMIC_SUB:return "buffer_atomic_sub";
  case ATOMIC_RSUB:return "buffer_atomic_rsub";
  case ATOMIC_SMIN:return "buffer_atomic_smin";
  case ATOMIC_UMIN:return "buffer_atomic_umin";
  case ATOMIC_SMAX:return "buffer_atomic_smax";
  case ATOMIC_UMAX:return "buffer_atomic_umax";
  case ATOMIC_AND:return "buffer_atomic_and";
  case ATOMIC_OR:return "buffer_atomic_or";
  case ATOMIC_XOR:return "buffer_atomic_xor";
  case ATOMIC_INC:return "buffer_atomic_inc";
  case ATOMIC_DEC:return "buffer_atomic_dec";
  case ATOMIC_FCMPSWAP:return "buffer_atomic_fcmpswap";
  case ATOMIC_FMIN:return "buffer_atomic_fmin";
  case ATOMIC_FMAX:return "buffer_atomic_fmax";
  case ATOMIC_SWAP_X2:return "buffer_atomic_swap_x2";
  case ATOMIC_CMPSWAP_X2:return "buffer_atomic_cmpswap_x2";
  case ATOMIC_ADD_X2:return "buffer_atomic_add_x2";
  case ATOMIC_SUB_X2:return "buffer_atomic_sub_x2";
  case ATOMIC_RSUB_X2:return "buffer_atomic_rsub_x2";
  case ATOMIC_SMIN_X2:return "buffer_atomic_smin_x2";
  case ATOMIC_UMIN_X2:return "buffer_atomic_umin_x2";
  case ATOMIC_SMAX_X2:return "buffer_atomic_smax_x2";
  case ATOMIC_UMAX_X2:return "buffer_atomic_umax_x2";
  case ATOMIC_AND_X2:return "buffer_atomic_and_x2";
  case ATOMIC_OR_X2:return "buffer_atomic_or_x2";
  case ATOMIC_XOR_X2:return "buffer_atomic_xor_x2";
  case ATOMIC_INC_X2:return "buffer_atomic_inc_x2";
  case ATOMIC_DEC_X2:return "buffer_atomic_dec_x2";
  case ATOMIC_FCMPSWAP_X2:return "buffer_atomic_fcmpswap_x2";
  case ATOMIC_FMIN_X2:return "buffer_atomic_fmin_x2";
  case ATOMIC_FMAX_X2:return "buffer_atomic_fmax_x2";
  case WBINVL1_SC_VOL:return "buffer_wbinvl1_sc_vol";
  case WBINVL1:return "buffer_wbinvl1";
  }
  return nullptr;
}
} // namespace shader::ir::mubuf
