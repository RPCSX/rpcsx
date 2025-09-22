#pragma once

#include "orbis/error.hpp"
#include "orbis/thread/RegisterId.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <sys/ucontext.h>
#include <type_traits>

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

using sshort = int16_t;
using ushort = uint16_t;

using uint = uint32_t;
using sint = int32_t;

using slong = int64_t;
using ulong = uint64_t;

using uintptr_t = uint64_t;
using intptr_t = int64_t;

template <typename T> using ptr = T *;
template <typename T> using cptr = T *const;

using caddr_t = ptr<char>;

static constexpr auto kMinAddress = 0x400000;
static constexpr auto kMaxAddress = 0x100'0000'0000;

[[nodiscard]] inline ErrorCode
ureadRaw(void *kernelAddress, ptr<const void> userAddress, size_t size) {
  if (size != 0) {
    auto addr = reinterpret_cast<std::uintptr_t>(userAddress);
    if (addr < kMinAddress || addr + size >= kMaxAddress || addr + size < addr)
      return ErrorCode::FAULT;
    std::memcpy(kernelAddress, userAddress, size);
  }
  return {};
}

[[nodiscard]] inline ErrorCode
uwriteRaw(ptr<void> userAddress, const void *kernelAddress, size_t size) {
  if (size != 0) {
    auto addr = reinterpret_cast<std::uintptr_t>(userAddress);
    if (addr < kMinAddress || addr + size > kMaxAddress || addr + size < addr)
      return ErrorCode::FAULT;
    std::memcpy(userAddress, kernelAddress, size);
  }
  return {};
}

[[nodiscard]] inline ErrorCode ureadString(char *kernelAddress, size_t size,
                                           ptr<const char> userAddress) {
  auto addr = reinterpret_cast<std::uintptr_t>(userAddress);
  if (addr < kMinAddress || addr + size > kMaxAddress || addr + size < addr)
    return ErrorCode::FAULT;
  std::strncpy(kernelAddress, userAddress, size);
  if (kernelAddress[size - 1] != '\0') {
    kernelAddress[size - 1] = '\0';
    return ErrorCode::NAMETOOLONG;
  }

  return {};
}

template <typename T>
[[nodiscard]] ErrorCode uread(T &result, ptr<const T> pointer) {
  return ureadRaw(&result, pointer, sizeof(T));
}

template <typename T> [[nodiscard]] ErrorCode uwrite(ptr<T> pointer, T data) {
  return uwriteRaw(pointer, &data, sizeof(T));
}

template <typename T, typename U>
  requires(std::is_arithmetic_v<T> && std::is_arithmetic_v<U> &&
           sizeof(T) > sizeof(U) && !std::is_same_v<std::remove_cv_t<T>, bool>)
[[nodiscard]] ErrorCode uwrite(ptr<T> pointer, U data) {
  T converted = data;
  return uwriteRaw(pointer, &converted, sizeof(T));
}

template <typename T>
[[nodiscard]] ErrorCode uread(T *result, ptr<const T> pointer,
                              std::size_t count) {
  return ureadRaw(result, pointer, sizeof(T) * count);
}

template <typename T>
[[nodiscard]] ErrorCode uwrite(ptr<T> pointer, const T *data,
                               std::size_t count) {
  return uwriteRaw(pointer, data, sizeof(T) * count);
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
  case RegisterId::rip:
    return c->gregs[REG_RIP];
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
  case RegisterId::rip:
    c->gregs[REG_RIP] = value;
    return;
  }
}

} // namespace orbis
