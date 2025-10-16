#pragma once

#include "AddressRange.hpp"
#include "EnumBitSet.hpp"
#include <system_error>
#include <utility>

namespace rx {
enum class Protection {
  R,
  W,
  X,

  bitset_last = X
};

std::errc reserveVirtualSpace(rx::AddressRange range);
std::errc releaseVirtualSpace(rx::AddressRange range, std::size_t alignment);

class Mappable {
#ifdef _WIN32
  using NativeHandle = void *;
  static constexpr NativeHandle kInvalidHandle = nullptr;
#else
  using NativeHandle = int;
  static constexpr auto kInvalidHandle = NativeHandle(-1);
#endif

  NativeHandle m_handle = kInvalidHandle;

public:
  Mappable() = default;
  Mappable(Mappable &&other) noexcept { *this = std::move(other); }
  Mappable(const Mappable &) = delete;
  Mappable &operator=(Mappable &&other) noexcept {
    std::swap(m_handle, other.m_handle);
    return *this;
  }
  Mappable &operator=(const Mappable &) = delete;
  ~Mappable() noexcept {
    if (m_handle != kInvalidHandle) {
      destroy();
    }
  }

  static std::pair<Mappable, std::errc> CreateMemory(std::size_t size);
  static std::pair<Mappable, std::errc> CreateSwap(std::size_t size);
  std::errc map(rx::AddressRange virtualRange, std::size_t offset,
                rx::EnumBitSet<Protection> protection, std::size_t alignment);

  [[nodiscard]] NativeHandle release() {
    return std::exchange(m_handle, kInvalidHandle);
  }
  [[nodiscard]] NativeHandle native_handle() const { return m_handle; }
  explicit operator bool() const { return m_handle != kInvalidHandle; }

private:
  void destroy();
};
} // namespace rx
