#pragma once

#include "orbis/error.hpp"
#include "orbis/thread/RegisterId.hpp"
#include <cstdint>
#include <cstring>

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

uint64_t readRegister(void *context, RegisterId id);
void writeRegister(void *context, RegisterId id, uint64_t value);
} // namespace orbis
