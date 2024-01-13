#pragma once

#include "orbis-config.hpp"
namespace orbis {

struct MContext {
  ulong onstack;
  ulong rdi;
  ulong rsi;
  ulong rdx;
  ulong rcx;
  ulong r8;
  ulong r9;
  ulong rax;
  ulong rbx;
  ulong rbp;
  ulong r10;
  ulong r11;
  ulong r12;
  ulong r13;
  ulong r14;
  ulong r15;
  uint trapno;
  ushort fs;
  ushort gs;
  ulong addr;
  uint flags;
  ushort es;
  ushort ds;
  ulong err;
  ulong rip;
  ulong cs;
  ulong rflags;
  ulong rsp;
  ulong ss;
  ulong len;
  ulong fpformat;
  ulong ownedfp;
  ulong lbrfrom;
  ulong lbrto;
  ulong aux1;
  ulong aux2;
  ulong fpstate[104];
  ulong fsbase;
  ulong gsbase;
  ulong spare[6];
};

struct Stack {
  ptr<void> sp;
  size_t size;
  sint flags;
  sint align;
};

struct SigSet {
  static constexpr auto min = 1;
  static constexpr auto max = 128;

  uint bits[4];

  bool test(unsigned signal) const {
    return (bits[(signal - 1) >> 5] & (1 << ((signal - 1) & 31))) != 0;
  }
  void set(unsigned signal) {
    bits[(signal - 1) >> 5] |= (1 << ((signal - 1) & 31));
  }
  void clear(unsigned signal) {
    bits[(signal - 1) >> 5] &= ~(1 << ((signal - 1) & 31));
  }
};

struct UContext {
  SigSet sigmask;
  sint unk0[12];
  MContext mcontext;
  ptr<UContext> link;
  Stack stack;
  sint uc_flags;
  sint spare[4];
  sint unk1[3];
};

static_assert(sizeof(UContext) == 0x500);


enum Signal {
  kSigHup = 1,
  kSigInt = 2,
  kSigQuit = 3,
  kSigIll = 4,
  kSigTrap = 5,
  kSigAbrt = 6,
  kSigEmt = 7,
  kSigFpe = 8,
  kSigKill = 9,
  kSigBus = 10,
  kSigSegv = 11,
  kSigSys = 12,
  kSigPipe = 13,
  kSigAlrm = 14,
  kSigUrg = 16,
  kSigStop = 17,
  kSigTstp = 18,
  kSigCont = 19,
  kSigChld = 20,
  kSigTtin = 21,
  kSigTtou = 22,
  kSigIo = 23,
  kSigXcpu = 24,
  kSigXfsz = 25,
  kSigVtalrm = 26,
  kSigProf = 27,
  kSigWinch = 28,
  kSigInfo = 29,
  kSigUsr1 = 30,
  kSigUsr2 = 31,
  kSigThr = 32,
};

struct SigAction {
  ptr<void(int32_t, void *, void *)> handler;
  sint flags;
  SigSet mask;
};

union SigVal {
  sint integer;
  ptr<void> pointer;
};

struct SigInfo {
  sint signo;
  sint errno_;
  sint code;
  sint pid;
  slong uid;
  sint status;
  ptr<void> addr;
  SigVal value;

  union {
    struct {
      sint trapno;
    } fault;

    struct {
      sint timerid;
      sint overrun;
    } timer;

    struct {
      sint mqd;
    } mesgq;

    struct {
      slong band;
    } poll;

    struct {
      slong spare1;
      sint spare2[7];
    } spare;
  } reason;
};
} // namespace orbis
