#pragma once
#include "Cache.hpp"
#include "FlipPipeline.hpp"
#include "Pipe.hpp"
#include "amdgpu/bridge/bridge.hpp"
#include "amdgpu/tiler_vulkan.hpp"
#include "rx/MemoryTable.hpp"
#include "shader/SemanticInfo.hpp"
#include "shader/SpvConverter.hpp"
#include "shader/gcn.hpp"
#include <unordered_map>
#include <vulkan/vulkan_core.h>

namespace amdgpu {

struct VmMapSlot {
  int memoryType;
  int prot;
  std::int64_t offset;
  std::uint64_t baseAddress;

  auto operator<=>(const VmMapSlot &) const = default;
};

struct ProcessInfo {
  int vmId = -1;
  int vmFd = -1;
  amdgpu::bridge::CmdBufferAttribute bufferAttributes[10];
  amdgpu::bridge::CmdBuffer buffers[10];
  rx::MemoryTableWithPayload<VmMapSlot> vmTable;
};

struct RemoteMemory {
  int vmId;

  template <typename T = void> T *getPointer(std::uint64_t address) const {
    return address ? reinterpret_cast<T *>(
                         static_cast<std::uint64_t>(vmId) << 40 | address)
                   : nullptr;
  }
};

struct Device {
  static constexpr auto kComputePipeCount = 8;
  static constexpr auto kGfxPipeCount = 2;

  shader::SemanticInfo gcnSemantic;
  shader::spv::Context shaderSemanticContext;
  shader::gcn::SemanticModuleInfo gcnSemanticModuleInfo;
  amdgpu::bridge::BridgeHeader *bridge;

  Registers::Config config;

  GpuTiler tiler;
  GraphicsPipe graphicsPipes[kGfxPipeCount]{0, 1};
  // ComputePipe computePipes[kComputePipeCount]{0, 1, 2, 3, 4, 5, 6, 7};
  FlipPipeline flipPipeline;

  int dmemFd[3] = {-1, -1, -1};
  std::unordered_map<std::int64_t, ProcessInfo> processInfo;

  Cache caches[6]{
      {this, 0}, {this, 1}, {this, 2}, {this, 3}, {this, 4}, {this, 5},
  };

  Device();
  ~Device();

  Cache::Tag getCacheTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createTag(scheduler);
  }

  Cache::GraphicsTag getGraphicsTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createGraphicsTag(scheduler);
  }

  Cache::ComputeTag getComputeTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createComputeTag(scheduler);
  }

  void mapProcess(std::int64_t pid, int vmId, const char *shmName);
  void unmapProcess(std::int64_t pid);
  void protectMemory(int pid, std::uint64_t address, std::uint64_t size,
                     int prot);
  void onCommandBuffer(std::int64_t pid, int cmdHeader, std::uint64_t address,
                       std::uint64_t size);
  bool processPipes();
  bool flip(std::int64_t pid, int bufferIndex, std::uint64_t arg,
            VkImage swapchainImage, VkImageView swapchainImageView);
  void mapMemory(std::int64_t pid, std::uint64_t address, std::uint64_t size,
                 int memoryType, int dmemIndex, int prot, std::int64_t offset);
  void registerBuffer(std::int64_t pid, bridge::CmdBuffer buffer);
  void registerBufferAttribute(std::int64_t pid,
                               bridge::CmdBufferAttribute attr);
  void handleProtectChange(int vmId, std::uint64_t address, std::uint64_t size,
                           int prot);
};
} // namespace amdgpu
