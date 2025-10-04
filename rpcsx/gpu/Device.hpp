#pragma once
#include "Cache.hpp"
#include "DeviceContext.hpp"
#include "FlipPipeline.hpp"
#include "Pipe.hpp"
#include "amdgpu/tiler_vulkan.hpp"
#include "orbis/KernelAllocator.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/Rc.hpp"
#include "rx/SharedMutex.hpp"
#include "shader/SemanticInfo.hpp"
#include "shader/SpvConverter.hpp"
#include "shader/gcn.hpp"
#include <array>
#include <thread>
#include <vulkan/vulkan_core.h>

struct GLFWwindow;

namespace amdgpu {

enum : std::uint8_t {
  IT_FLIP = 0xF0,
  IT_MAP_MEMORY,
  IT_UNMAP_MEMORY,
  IT_PROTECT_MEMORY,
  IT_UNMAP_PROCESS,
};

template <typename... T>
  requires(sizeof...(T) > 0)
std::array<std::uint32_t, sizeof...(T) + 1> createPm4Packet(std::uint32_t op,
                                                            T... data) {
  return {static_cast<std::uint32_t>((3 << 30) | (op << 8) |
                                     ((sizeof...(T) - 1) << 16)),
          static_cast<std::uint32_t>(data)...};
}

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
  BufferAttribute bufferAttributes[10];
  Buffer buffers[10];
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

struct Device : rx::RcBase, DeviceContext {
  static constexpr auto kComputePipeCount = 8;
  static constexpr auto kGfxPipeCount = 2;

  shader::SemanticInfo gcnSemantic;
  shader::spv::Context shaderSemanticContext;
  shader::gcn::SemanticModuleInfo gcnSemanticModuleInfo;
  Registers::Config config;
  GLFWwindow *window = nullptr;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
  vk::Context vkContext;

  GpuTiler tiler;
  GraphicsPipe graphicsPipes[kGfxPipeCount]{0, 1};
  ComputePipe computePipes[kComputePipeCount]{0, 1, 2, 3, 4, 5, 6, 7};
  CommandPipe commandPipe;
  FlipPipeline flipPipeline;

  rx::shared_mutex writeCommandMtx;
  uint32_t imageIndex = 0;
  bool isImageAcquired = false;

  std::jthread cacheUpdateThread;

  int dmemFd[3] = {-1, -1, -1};
  orbis::kmap<std::int32_t, ProcessInfo> processInfo;

  Cache caches[kMaxProcessCount]{
      {this, 0}, {this, 1}, {this, 2}, {this, 3}, {this, 4}, {this, 5},
  };

  std::uint32_t mainGfxRings[kGfxPipeCount][0x4000 / sizeof(std::uint32_t)];
  std::uint32_t cmdRing[0x4000 / sizeof(std::uint32_t)];

  Device();
  ~Device();

  void start();

  Cache::Tag getCacheTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createTag(scheduler);
  }

  Cache::GraphicsTag getGraphicsTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createGraphicsTag(scheduler);
  }

  Cache::ComputeTag getComputeTag(int vmId, Scheduler &scheduler) {
    return caches[vmId].createComputeTag(scheduler);
  }

  void submitCommand(Ring &ring, std::span<const std::uint32_t> command);
  void submitGfxCommand(int gfxPipe, std::span<const std::uint32_t> command);

  void mapProcess(std::uint32_t pid, int vmId);
  void unmapProcess(std::uint32_t pid);
  void protectMemory(std::uint32_t pid, std::uint64_t address,
                     std::uint64_t size, int prot);
  void onCommandBuffer(std::uint32_t pid, int cmdHeader, std::uint64_t address,
                       std::uint64_t size);
  bool processPipes();
  bool flip(std::uint32_t pid, int bufferIndex, std::uint64_t arg,
            VkImage swapchainImage, VkImageView swapchainImageView);
  void flip(std::uint32_t pid, int bufferIndex, std::uint64_t arg);
  void waitForIdle();
  void mapMemory(std::uint32_t pid, std::uint64_t address, std::uint64_t size,
                 int memoryType, int dmemIndex, int prot, std::int64_t offset);
  void unmapMemory(std::uint32_t pid, std::uint64_t address,
                   std::uint64_t size);
  void watchWrites(int vmId, std::uint64_t address, std::uint64_t size);
  void lockReadWrite(int vmId, std::uint64_t address, std::uint64_t size,
                     bool isLazy);
  void unlockReadWrite(int vmId, std::uint64_t address, std::uint64_t size);
};
} // namespace amdgpu
