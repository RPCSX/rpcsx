#pragma once

#include "orbis/error.hpp"
#include "orbis/thread/RegisterId.hpp"
#include <cstdint>
#include <cstring>
#include <sys/ucontext.h>
#include <immintrin.h>

namespace orbis {
using int8_t = std::int8_t;
using int16_t = std::int16_t;
using int32_t = std::int32_t;
using int64_t = std::int64_t;

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;
using uint64_t = std::uint64_t;

using size_t = uint64_t;
using ssize_t = int64_t;
using off_t = int64_t;

using uint = uint32_t;
using sint = int32_t;

using slong = int64_t;
using ulong = uint64_t;

template <typename T> using ptr = T *;
template <typename T> using cptr = T * const;

using caddr_t = ptr<char>;

inline ErrorCode uread(void *kernelAddress, ptr<const void> userAddress,
                  size_t size) {
  std::memcpy(kernelAddress, userAddress, size);
  return {};
}

inline ErrorCode uwrite(ptr<void> userAddress, const void *kernelAddress,
                   size_t size) {
  std::memcpy(userAddress, kernelAddress, size);
  return {};
}

inline ErrorCode ureadString(char *kernelAddress, size_t kernelSize, ptr<const char> userAddress) {
  std::strncpy(kernelAddress, userAddress, kernelSize);
  if (kernelAddress[kernelSize - 1] != '\0') {
    kernelAddress[kernelSize - 1] = '\0';
    return ErrorCode::NAMETOOLONG;
  }

  return {};
}

template <typename T> T uread(ptr<T> pointer) {
  T result;
  uread(&result, pointer, sizeof(T));
  return result;
}

template <typename T> void uwrite(ptr<T> pointer, T data) {
  uwrite(pointer, &data, sizeof(T));
}

inline uint64_t readRegister(void *context, RegisterId id) {
#if defined(LINUX)
  auto c = &reinterpret_cast<ucontext_t *>(context)->uc_mcontext;
    switch (id) {
    case RegisterId::r15: return c->gregs[REG_R15];
    case RegisterId::r14: return c->gregs[REG_R14];
    case RegisterId::r13: return c->gregs[REG_R13];
    case RegisterId::r12: return c->gregs[REG_R12];
    case RegisterId::r11: return c->gregs[REG_R11];
    case RegisterId::r10: return c->gregs[REG_R10];
    case RegisterId::r9: return c->gregs[REG_R9];
    case RegisterId::r8: return c->gregs[REG_R8];
    case RegisterId::rdi: return c->gregs[REG_RDI];
    case RegisterId::rsi: return c->gregs[REG_RSI];
    case RegisterId::rbp: return c->gregs[REG_RBP];
    case RegisterId::rbx: return c->gregs[REG_RBX];
    case RegisterId::rdx: return c->gregs[REG_RDX];
    case RegisterId::rcx: return c->gregs[REG_RCX];
    case RegisterId::rax: return c->gregs[REG_RAX];
    case RegisterId::rsp: return c->gregs[REG_RSP];
    case RegisterId::rflags: return c->gregs[REG_EFL];
  }
#elif defined(MAC)
  auto c = reinterpret_cast<ucontext_t *>(context);
    switch (id) {
    case RegisterId::r15: return c->uc_mcontext->__ss.__r15;
    case RegisterId::r14: return c->uc_mcontext->__ss.__r14;
    case RegisterId::r13: return c->uc_mcontext->__ss.__r13;
    case RegisterId::r12: return c->uc_mcontext->__ss.__r12;
    case RegisterId::r11: return c->uc_mcontext->__ss.__r11;
    case RegisterId::r10: return c->uc_mcontext->__ss.__r10;
    case RegisterId::r9: return c->uc_mcontext->__ss.__r9;
    case RegisterId::r8: return c->uc_mcontext->__ss.__r8;
    case RegisterId::rdi: return c->uc_mcontext->__ss.__rdi;
    case RegisterId::rsi: return c->uc_mcontext->__ss.__rsi;
    case RegisterId::rbp: return c->uc_mcontext->__ss.__rbp;
    case RegisterId::rbx: return c->uc_mcontext->__ss.__rbx;
    case RegisterId::rdx: return c->uc_mcontext->__ss.__rdx;
    case RegisterId::rcx: return c->uc_mcontext->__ss.__rcx;
    case RegisterId::rax: return c->uc_mcontext->__ss.__rax;
    case RegisterId::rsp: return c->uc_mcontext->__ss.__rsp;
    case RegisterId::rflags: return c->uc_mcontext->__ss.__rflags;
  }
#endif
}

inline void writeRegister(void *context, RegisterId id, uint64_t value)
{
#if defined(LINUX)
  auto c = &reinterpret_cast<ucontext_t *>(context)->uc_mcontext;
  switch (id) {
    case RegisterId::r15: c->gregs[REG_R15] = value; return;
    case RegisterId::r14: c->gregs[REG_R14] = value; return;
    case RegisterId::r13: c->gregs[REG_R13] = value; return;
    case RegisterId::r12: c->gregs[REG_R12] = value; return;
    case RegisterId::r11: c->gregs[REG_R11] = value; return;
    case RegisterId::r10: c->gregs[REG_R10] = value; return;
    case RegisterId::r9: c->gregs[REG_R9] = value; return;
    case RegisterId::r8: c->gregs[REG_R8] = value; return;
    case RegisterId::rdi: c->gregs[REG_RDI] = value; return;
    case RegisterId::rsi: c->gregs[REG_RSI] = value; return;
    case RegisterId::rbp: c->gregs[REG_RBP] = value; return;
    case RegisterId::rbx: c->gregs[REG_RBX] = value; return;
    case RegisterId::rdx: c->gregs[REG_RDX] = value; return;
    case RegisterId::rcx: c->gregs[REG_RCX] = value; return;
    case RegisterId::rax: c->gregs[REG_RAX] = value; return;
    case RegisterId::rsp: c->gregs[REG_RSP] = value; return;
    case RegisterId::rflags: c->gregs[REG_EFL] = value; return;
  }
#elif defined(MAC)
  auto c = reinterpret_cast<ucontext_t *>(context);
  switch (id) {
    case RegisterId::r15: c->uc_mcontext->__ss.__r15 = value; return;
    case RegisterId::r14: c->uc_mcontext->__ss.__r14 = value; return;
    case RegisterId::r13: c->uc_mcontext->__ss.__r13 = value; return;
    case RegisterId::r12: c->uc_mcontext->__ss.__r12 = value; return;
    case RegisterId::r11: c->uc_mcontext->__ss.__r11 = value; return; 
    case RegisterId::r10: c->uc_mcontext->__ss.__r10 = value; return;
    case RegisterId::r9: c->uc_mcontext->__ss.__r9 = value; return;
    case RegisterId::r8: c->uc_mcontext->__ss.__r8 = value; return;
    case RegisterId::rdi: c->uc_mcontext->__ss.__rdi = value; return;
    case RegisterId::rsi: c->uc_mcontext->__ss.__rsi = value; return;
    case RegisterId::rbp: c->uc_mcontext->__ss.__rbp = value; return;
    case RegisterId::rbx: c->uc_mcontext->__ss.__rbx = value; return;
    case RegisterId::rdx: c->uc_mcontext->__ss.__rdx = value; return;
    case RegisterId::rcx: c->uc_mcontext->__ss.__rcx = value; return;
    case RegisterId::rax: c->uc_mcontext->__ss.__rax = value; return;
    case RegisterId::rsp: c->uc_mcontext->__ss.__rsp = value; return;
    case RegisterId::rflags: c->uc_mcontext->__ss.__rflags = value; return;
  }
#endif
}
} // namespace orbis
