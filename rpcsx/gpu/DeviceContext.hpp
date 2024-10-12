#pragma once

#include <atomic>
#include <cstdint>

namespace amdgpu {
struct BufferAttribute {
  std::uint8_t attrId;
  std::uint8_t submit;
  std::uint64_t canary;
  std::uint32_t pixelFormat;
  std::uint32_t tilingMode;
  std::uint32_t pitch;
  std::uint32_t width;
  std::uint32_t height;
};

struct Buffer {
  std::uint64_t canary;
  std::uint32_t index;
  std::uint32_t attrId;
  std::uint64_t address;
  std::uint64_t address2;
};

enum {
  kPageWriteWatch = 1 << 0,
  kPageReadWriteLock = 1 << 1,
  kPageInvalidated = 1 << 2,
  kPageLazyLock = 1 << 3
};

struct PadState {
  std::uint64_t timestamp;
  std::uint32_t unk;
  std::uint32_t buttons;
  std::uint8_t leftStickX;
  std::uint8_t leftStickY;
  std::uint8_t rightStickX;
  std::uint8_t rightStickY;
  std::uint8_t l2;
  std::uint8_t r2;
};

enum {
  kPadBtnL3 = 1 << 1,
  kPadBtnR3 = 1 << 2,
  kPadBtnOptions = 1 << 3,
  kPadBtnUp = 1 << 4,
  kPadBtnRight = 1 << 5,
  kPadBtnDown = 1 << 6,
  kPadBtnLeft = 1 << 7,
  kPadBtnL2 = 1 << 8,
  kPadBtnR2 = 1 << 9,
  kPadBtnL1 = 1 << 10,
  kPadBtnR1 = 1 << 11,
  kPadBtnTriangle = 1 << 12,
  kPadBtnCircle = 1 << 13,
  kPadBtnCross = 1 << 14,
  kPadBtnSquare = 1 << 15,
  kPadBtnPs = 1 << 16,
  kPadBtnTouchPad = 1 << 20,
  kPadBtnIntercepted = 1 << 31,
};

struct DeviceContext {
  static constexpr auto kMaxProcessCount = 6;

  PadState kbPadState;
  std::atomic<std::uint64_t> cacheCommands[kMaxProcessCount][4];
  std::atomic<std::uint32_t> gpuCacheCommand[kMaxProcessCount];
  std::atomic<std::uint8_t> *cachePages[kMaxProcessCount];

  volatile std::uint32_t flipBuffer[kMaxProcessCount];
  volatile std::uint64_t flipArg[kMaxProcessCount];
  volatile std::uint64_t flipCount[kMaxProcessCount];
  volatile std::uint64_t bufferInUseAddress[kMaxProcessCount];
};
} // namespace amdgpu