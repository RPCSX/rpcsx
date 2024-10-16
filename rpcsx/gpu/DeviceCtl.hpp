#pragma once

#include "DeviceContext.hpp"
#include "orbis-config.hpp"
#include "orbis/utils/Rc.hpp"
#include <cstdint>
#include <span>

namespace amdgpu {
class Device;

class DeviceCtl {
  orbis::Ref<Device> mDevice;

public:
  DeviceCtl() noexcept;
  DeviceCtl(orbis::Ref<orbis::RcBase> device) noexcept;
  DeviceCtl(DeviceCtl &&) noexcept;
  DeviceCtl(const DeviceCtl &);
  DeviceCtl &operator=(DeviceCtl &&) noexcept;
  DeviceCtl &operator=(const DeviceCtl &);
  ~DeviceCtl();

  static DeviceCtl createDevice();
  DeviceContext &getContext();
  orbis::Ref<orbis::RcBase> getOpaque();

  void submitGfxCommand(int gfxPipe, int vmId,
                        std::span<const std::uint32_t> command);
  void submitSwitchBuffer(int gfxPipe);
  void submitFlip(int gfxPipe, std::uint32_t pid, int bufferIndex,
                  std::uint64_t flipArg);
  orbis::ErrorCode submitFlipOnEop(int gfxPipe, std::uint32_t pid,
                                   int bufferIndex, std::uint64_t flipArg);
  void submitMapMemory(int gfxPipe, std::uint32_t pid, std::uint64_t address,
                       std::uint64_t size, int memoryType, int dmemIndex,
                       int prot, std::int64_t offset);
  void submitUnmapMemory(int gfxPipe, std::uint32_t pid, std::uint64_t address,
                         std::uint64_t size);
  void submitMapProcess(int gfxPipe, std::uint32_t pid, int vmId);
  void submitUnmapProcess(int gfxPipe, std::uint32_t pid);
  void submitProtectMemory(int gfxPipe, std::uint32_t pid,
                           std::uint64_t address, std::uint64_t size, int prot);
  void registerBuffer(std::uint32_t pid, Buffer buffer);
  void registerBufferAttribute(std::uint32_t pid, BufferAttribute attr);

  void mapComputeQueue(int vmId, std::uint32_t meId, std::uint32_t pipeId,
                       std::uint32_t queueId, std::uint32_t vqueueId,
                       orbis::uint64_t ringBaseAddress,
                       orbis::uint64_t readPtrAddress, orbis::uint64_t doorbell,
                       orbis::uint64_t ringSize);
  void submitComputeQueue(std::uint32_t meId, std::uint32_t pipeId,
                          std::uint32_t queueId, std::uint64_t offset);
  void start();
  void waitForIdle();

  explicit operator bool() const { return mDevice != nullptr; }
};
} // namespace amdgpu
