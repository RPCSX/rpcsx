#pragma once

namespace shader::ir::sopp {
enum Op {
  NOP,
  ENDPGM,
  BRANCH,
  CBRANCH_SCC0 = 4,
  CBRANCH_SCC1,
  CBRANCH_VCCZ,
  CBRANCH_VCCNZ,
  CBRANCH_EXECZ,
  CBRANCH_EXECNZ,
  BARRIER,
  WAITCNT = 12,
  SETHALT,
  SLEEP,
  SETPRIO,
  SENDMSG,
  SENDMSGHALT,
  TRAP,
  ICACHE_INV,
  INCPERFLEVEL,
  DECPERFLEVEL,
  TTRACEDATA,
  CBRANCH_CDBGSYS = 23,
  CBRANCH_CDBGUSER = 24,
  CBRANCH_CDBGSYS_OR_USER = 25,
  CBRANCH_CDBGSYS_AND_USER = 26,

  OpCount
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case NOP:
    return "s_nop";
  case ENDPGM:
    return "s_endpgm";
  case BRANCH:
    return "s_branch";
  case CBRANCH_SCC0:
    return "s_cbranch_scc0";
  case CBRANCH_SCC1:
    return "s_cbranch_scc1";
  case CBRANCH_VCCZ:
    return "s_cbranch_vccz";
  case CBRANCH_VCCNZ:
    return "s_cbranch_vccnz";
  case CBRANCH_EXECZ:
    return "s_cbranch_execz";
  case CBRANCH_EXECNZ:
    return "s_cbranch_execnz";
  case BARRIER:
    return "s_barrier";
  case WAITCNT:
    return "s_waitcnt";
  case SETHALT:
    return "s_sethalt";
  case SLEEP:
    return "s_sleep";
  case SETPRIO:
    return "s_setprio";
  case SENDMSG:
    return "s_sendmsg";
  case SENDMSGHALT:
    return "s_sendmsghalt";
  case TRAP:
    return "s_trap";
  case ICACHE_INV:
    return "s_icache_inv";
  case INCPERFLEVEL:
    return "s_incperflevel";
  case DECPERFLEVEL:
    return "s_decperflevel";
  case TTRACEDATA:
    return "s_ttracedata";
  case CBRANCH_CDBGSYS:
    return "s_cbranch_cdbgsys";
  case CBRANCH_CDBGUSER:
    return "s_cbranch_cdbguser";
  case CBRANCH_CDBGSYS_OR_USER:
    return "s_cbranch_cdbgsys_or_user";
  case CBRANCH_CDBGSYS_AND_USER:
    return "s_cbranch_cdbgsys_and_user";
  }
  return nullptr;
}
} // namespace shader::ir::sopp
