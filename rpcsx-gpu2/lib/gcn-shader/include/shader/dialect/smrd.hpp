#pragma once

namespace shader::ir::smrd {
enum Op {
  LOAD_DWORD,
  LOAD_DWORDX2,
  LOAD_DWORDX4,
  LOAD_DWORDX8,
  LOAD_DWORDX16,
  BUFFER_LOAD_DWORD = 8,
  BUFFER_LOAD_DWORDX2,
  BUFFER_LOAD_DWORDX4,
  BUFFER_LOAD_DWORDX8,
  BUFFER_LOAD_DWORDX16,
  DCACHE_INV_VOL = 29,
  MEMTIME,
  DCACHE_INV,

  OpCount
};
inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case LOAD_DWORD: return "s_load_dword";
  case LOAD_DWORDX2: return "s_load_dwordx2";
  case LOAD_DWORDX4: return "s_load_dwordx4";
  case LOAD_DWORDX8: return "s_load_dwordx8";
  case LOAD_DWORDX16: return "s_load_dwordx16";
  case BUFFER_LOAD_DWORD: return "s_buffer_load_dword";
  case BUFFER_LOAD_DWORDX2: return "s_buffer_load_dwordx2";
  case BUFFER_LOAD_DWORDX4: return "s_buffer_load_dwordx4";
  case BUFFER_LOAD_DWORDX8: return "s_buffer_load_dwordx8";
  case BUFFER_LOAD_DWORDX16: return "s_buffer_load_dwordx16";
  case DCACHE_INV_VOL: return "s_dcache_inv_vol";
  case MEMTIME: return "s_memtime";
  case DCACHE_INV: return "s_dcache_inv";
  }
  return nullptr;
}
} // namespace shader::ir::smrd
