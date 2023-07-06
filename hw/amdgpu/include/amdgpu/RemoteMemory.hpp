#pragma once
#include <cstdint>

namespace amdgpu {
struct RemoteMemory {
  char *shmPointer;

  template <typename T = void> T *getPointer(std::uint64_t address) const {
    return address ? reinterpret_cast<T *>(shmPointer + address - 0x40000)
                   : nullptr;
  }
};
} // namespace amdgpu
