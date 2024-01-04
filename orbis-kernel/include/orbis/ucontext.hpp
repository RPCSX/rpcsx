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
  ulong bits[2];
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
} // namespace orbis
