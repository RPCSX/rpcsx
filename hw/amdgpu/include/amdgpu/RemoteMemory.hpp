#pragma once
#include <cstdint>

namespace amdgpu {
struct RemoteMemory {
  int vmId;

  template <typename T = void> T *getPointer(std::uint64_t address) const {
    return address ? reinterpret_cast<T *>(
                         static_cast<std::uint64_t>(vmId) << 40 | address)
                   : nullptr;
  }
};
} // namespace amdgpu
