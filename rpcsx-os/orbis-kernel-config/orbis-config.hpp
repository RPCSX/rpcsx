#pragma once

#include "orbis/error.hpp"
#include "orbis/thread/RegisterId.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <sys/ucontext.h>

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
template <typename T> using cptr = T *const;

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

inline ErrorCode ureadString(char *kernelAddress, size_t kernelSize,
                             ptr<const char> userAddress) {
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
  auto c = &reinterpret_cast<ucontext_t *>(context)->uc_mcontext;
  switch (id) {
  case RegisterId::r15:
    return c->gregs[REG_R15];
  case RegisterId::r14:
    return c->gregs[REG_R14];
  case RegisterId::r13:
    return c->gregs[REG_R13];
  case RegisterId::r12:
    return c->gregs[REG_R12];
  case RegisterId::r11:
    return c->gregs[REG_R11];
  case RegisterId::r10:
    return c->gregs[REG_R10];
  case RegisterId::r9:
    return c->gregs[REG_R9];
  case RegisterId::r8:
    return c->gregs[REG_R8];
  case RegisterId::rdi:
    return c->gregs[REG_RDI];
  case RegisterId::rsi:
    return c->gregs[REG_RSI];
  case RegisterId::rbp:
    return c->gregs[REG_RBP];
  case RegisterId::rbx:
    return c->gregs[REG_RBX];
  case RegisterId::rdx:
    return c->gregs[REG_RDX];
  case RegisterId::rcx:
    return c->gregs[REG_RCX];
  case RegisterId::rax:
    return c->gregs[REG_RAX];
  case RegisterId::rsp:
    return c->gregs[REG_RSP];
  case RegisterId::rflags:
    return c->gregs[REG_EFL];
  }
  std::fprintf(stderr, "***ERROR*** Unhandled RegisterId %d\n",
               static_cast<int>(id));
  std::abort();
}

inline void writeRegister(void *context, RegisterId id, uint64_t value) {
  auto c = &reinterpret_cast<ucontext_t *>(context)->uc_mcontext;
  switch (id) {
  case RegisterId::r15:
    c->gregs[REG_R15] = value;
    return;
  case RegisterId::r14:
    c->gregs[REG_R14] = value;
    return;
  case RegisterId::r13:
    c->gregs[REG_R13] = value;
    return;
  case RegisterId::r12:
    c->gregs[REG_R12] = value;
    return;
  case RegisterId::r11:
    c->gregs[REG_R11] = value;
    return;
  case RegisterId::r10:
    c->gregs[REG_R10] = value;
    return;
  case RegisterId::r9:
    c->gregs[REG_R9] = value;
    return;
  case RegisterId::r8:
    c->gregs[REG_R8] = value;
    return;
  case RegisterId::rdi:
    c->gregs[REG_RDI] = value;
    return;
  case RegisterId::rsi:
    c->gregs[REG_RSI] = value;
    return;
  case RegisterId::rbp:
    c->gregs[REG_RBP] = value;
    return;
  case RegisterId::rbx:
    c->gregs[REG_RBX] = value;
    return;
  case RegisterId::rdx:
    c->gregs[REG_RDX] = value;
    return;
  case RegisterId::rcx:
    c->gregs[REG_RCX] = value;
    return;
  case RegisterId::rax:
    c->gregs[REG_RAX] = value;
    return;
  case RegisterId::rsp:
    c->gregs[REG_RSP] = value;
    return;
  case RegisterId::rflags:
    c->gregs[REG_EFL] = value;
    return;
  }
}

} // namespace orbis
