#include "device.hpp"
#include "amdgpu/bridge/bridge.hpp"
#include "amdgpu/shader/AccessOp.hpp"
#include "amdgpu/shader/Converter.hpp"
#include "scheduler.hpp"
#include "tiler.hpp"

#include "spirv-tools/optimizer.hpp"
#include "util/area.hpp"
#include "util/unreachable.hpp"
#include "vk.hpp"
#include <amdgpu/shader/UniformBindings.hpp>
#include <atomic>
#include <bit>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <forward_list>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <shaders/rect_list.geom.h>
#include <span>
#include <spirv_cross/spirv_glsl.hpp>
#include <sys/mman.h>
#include <unistd.h>
#include <util/VerifyVulkan.hpp>
#include <utility>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;
using namespace amdgpu::device;

static const bool kUseDirectMemory = false;
static amdgpu::bridge::BridgeHeader *g_bridge;

void *g_rwMemory;
std::size_t g_memorySize;
std::uint64_t g_memoryBase;
RemoteMemory g_hostMemory;

namespace amdgpu::device::vk {
VkDevice g_vkDevice = VK_NULL_HANDLE;
VkAllocationCallbacks *g_vkAllocator = nullptr;
std::vector<std::pair<VkQueue, unsigned>> g_computeQueues;
std::vector<std::pair<VkQueue, unsigned>> g_transferQueues;
std::vector<std::pair<VkQueue, unsigned>> g_graphicsQueues;

static VkPhysicalDeviceMemoryProperties g_physicalMemoryProperties;
static VkPhysicalDeviceProperties g_physicalDeviceProperties;
std::uint32_t findPhysicalMemoryTypeIndex(std::uint32_t typeBits,
                                          VkMemoryPropertyFlags properties) {
  typeBits &= (1 << g_physicalMemoryProperties.memoryTypeCount) - 1;

  while (typeBits != 0) {
    auto typeIndex = std::countr_zero(typeBits);

    if ((g_physicalMemoryProperties.memoryTypes[typeIndex].propertyFlags &
         properties) == properties) {
      return typeIndex;
    }

    typeBits &= ~(1 << typeIndex);
  }

  util::unreachable("Failed to find memory type with properties %x",
                    properties);
}
} // namespace amdgpu::device::vk

static Scheduler g_scheduler{4};

static VkResult _vkCreateShadersEXT(VkDevice device, uint32_t createInfoCount,
                                    const VkShaderCreateInfoEXT *pCreateInfos,
                                    const VkAllocationCallbacks *pAllocator,
                                    VkShaderEXT *pShaders) {
  static PFN_vkCreateShadersEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCreateShadersEXT)vkGetDeviceProcAddr(vk::g_vkDevice,
                                                     "vkCreateShadersEXT");
  }

  return fn(device, createInfoCount, pCreateInfos, pAllocator, pShaders);
}

static void _vkDestroyShaderEXT(VkDevice device, VkShaderEXT shader,
                                const VkAllocationCallbacks *pAllocator) {
  static PFN_vkDestroyShaderEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkDestroyShaderEXT)vkGetDeviceProcAddr(vk::g_vkDevice,
                                                     "vkDestroyShaderEXT");
  }

  fn(device, shader, pAllocator);
}

static void _vkCmdBindShadersEXT(VkCommandBuffer commandBuffer,
                                 uint32_t stageCount,
                                 const VkShaderStageFlagBits *pStages,
                                 const VkShaderEXT *pShaders) {
  static PFN_vkCmdBindShadersEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdBindShadersEXT)vkGetDeviceProcAddr(vk::g_vkDevice,
                                                      "vkCmdBindShadersEXT");
  }

  return fn(commandBuffer, stageCount, pStages, pShaders);
}

static void _vkCmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer,
                                         uint32_t firstAttachment,
                                         uint32_t attachmentCount,
                                         const VkBool32 *pColorBlendEnables) {
  static PFN_vkCmdSetColorBlendEnableEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdSetColorBlendEnableEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetColorBlendEnableEXT");
  }

  return fn(commandBuffer, firstAttachment, attachmentCount,
            pColorBlendEnables);
}
static void _vkCmdSetColorBlendEquationEXT(
    VkCommandBuffer commandBuffer, uint32_t firstAttachment,
    uint32_t attachmentCount,
    const VkColorBlendEquationEXT *pColorBlendEquations) {
  static PFN_vkCmdSetColorBlendEquationEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdSetColorBlendEquationEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetColorBlendEquationEXT");
  }

  return fn(commandBuffer, firstAttachment, attachmentCount,
            pColorBlendEquations);
}

static void _vkCmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer,
                                         VkBool32 depthClampEnable) {
  static PFN_vkCmdSetDepthClampEnableEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdSetDepthClampEnableEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetDepthClampEnableEXT");
  }

  return fn(commandBuffer, depthClampEnable);
}

static void _vkCmdSetLogicOpEXT(VkCommandBuffer commandBuffer,
                                VkLogicOp logicOp) {
  static PFN_vkCmdSetLogicOpEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdSetLogicOpEXT)vkGetDeviceProcAddr(vk::g_vkDevice,
                                                     "vkCmdSetLogicOpEXT");
  }

  return fn(commandBuffer, logicOp);
}

static void _vkCmdSetPolygonModeEXT(VkCommandBuffer commandBuffer,
                                    VkPolygonMode polygonMode) {
  static PFN_vkCmdSetPolygonModeEXT fn;

  if (fn == nullptr) {
    fn = (PFN_vkCmdSetPolygonModeEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetPolygonModeEXT");
  }

  return fn(commandBuffer, polygonMode);
}

static void _vkCmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer,
                                      VkBool32 logicOpEnable) {
  static PFN_vkCmdSetLogicOpEnableEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetLogicOpEnableEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetLogicOpEnableEXT");
  }

  return fn(commandBuffer, logicOpEnable);
}
static void
_vkCmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer,
                                 VkSampleCountFlagBits rasterizationSamples) {
  static PFN_vkCmdSetRasterizationSamplesEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetRasterizationSamplesEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetRasterizationSamplesEXT");
  }

  return fn(commandBuffer, rasterizationSamples);
}
static void _vkCmdSetSampleMaskEXT(VkCommandBuffer commandBuffer,
                                   VkSampleCountFlagBits samples,
                                   const VkSampleMask *pSampleMask) {
  static PFN_vkCmdSetSampleMaskEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetSampleMaskEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetSampleMaskEXT");
  }

  return fn(commandBuffer, samples, pSampleMask);
}
static void
_vkCmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer,
                                     VkTessellationDomainOrigin domainOrigin) {
  static PFN_vkCmdSetTessellationDomainOriginEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetTessellationDomainOriginEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetTessellationDomainOriginEXT");
  }

  return fn(commandBuffer, domainOrigin);
}
static void _vkCmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer,
                                              VkBool32 alphaToCoverageEnable) {
  static PFN_vkCmdSetAlphaToCoverageEnableEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetAlphaToCoverageEnableEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetAlphaToCoverageEnableEXT");
  }

  return fn(commandBuffer, alphaToCoverageEnable);
}
static void _vkCmdSetVertexInputEXT(
    VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions) {
  static PFN_vkCmdSetVertexInputEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetVertexInputEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetVertexInputEXT");
  }

  return fn(commandBuffer, vertexBindingDescriptionCount,
            pVertexBindingDescriptions, vertexAttributeDescriptionCount,
            pVertexAttributeDescriptions);
}
static void
_vkCmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer,
                           uint32_t firstAttachment, uint32_t attachmentCount,
                           const VkColorComponentFlags *pColorWriteMasks) {
  static PFN_vkCmdSetColorWriteMaskEXT fn;
  if (fn == nullptr) {
    fn = (PFN_vkCmdSetColorWriteMaskEXT)vkGetDeviceProcAddr(
        vk::g_vkDevice, "vkCmdSetColorWriteMaskEXT");
  }

  return fn(commandBuffer, firstAttachment, attachmentCount, pColorWriteMasks);
}
static void submitToQueue(VkQueue queue, VkCommandBuffer cmdBuffer,
                          vk::Semaphore &sem, std::uint64_t signalValue) {
  VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signalValue,
  };

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSubmitInfo,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &sem.mSemaphore,
  };

  Verify() << vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
}

static void submitToQueue(VkQueue queue, VkCommandBuffer cmdBuffer,
                          vk::Semaphore &sem, std::uint64_t waitValue,
                          std::uint64_t signalValue,
                          VkPipelineStageFlags waitDstStage) {
  VkTimelineSemaphoreSubmitInfo timelineSubmitInfo{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .waitSemaphoreValueCount = 1,
      .pWaitSemaphoreValues = &waitValue,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &signalValue,
  };

  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = &timelineSubmitInfo,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &sem.mSemaphore,
      .pWaitDstStageMask = &waitDstStage,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &sem.mSemaphore,
  };

  Verify() << vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
}

struct GpuTaskCtl {
  std::atomic<unsigned> refs{0};
  vk::Semaphore semaphore = vk::Semaphore::Create();

  virtual ~GpuTaskCtl() = default;

  void incRef() { refs.fetch_add(1, std::memory_order::relaxed); }
  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::relaxed) == 1) {
      delete this;
    }
  }

  bool isComplete() { return semaphore.getCounterValue() == 1; }
  bool isInProgress() { return semaphore.getCounterValue() == 0; }

  void wait() { semaphore.wait(1, UINTMAX_MAX); }
  virtual void invoke(VkCommandBuffer commandBuffer) = 0;
};

template <typename T> struct GpuTask;

template <typename T>
  requires(std::is_invocable_r_v<void, T, VkCommandBuffer> &&
           detail::LambdaWithoutClosure<T>)
struct GpuTask<T> : GpuTaskCtl {
  static constexpr void (*fn)(VkCommandBuffer) = +std::declval<T>();

  GpuTask() = default;
  GpuTask(T &&) {}

  void invoke(VkCommandBuffer commandBuffer) override { fn(commandBuffer); }
};

template <typename T>
  requires(std::is_invocable_r_v<void, T, VkCommandBuffer> &&
           !detail::LambdaWithoutClosure<T>)
struct GpuTask<T> : GpuTaskCtl {
  alignas(T) std::byte taskStorage[sizeof(T)];

  GpuTask() = default;
  GpuTask(T &&t) { new (taskStorage) T(std::forward<T>(t)); }
  GpuTask &operator=(T &&t) {
    new (taskStorage) T(std::forward<T>(t));
    return *this;
  }

  void invoke(VkCommandBuffer commandBuffer) override {
    auto &lambda = *std::bit_cast<T *>(&taskStorage);
    lambda(commandBuffer);
    std::bit_cast<T *>(&taskStorage)->~T();
  }
};

template <typename T>
  requires std::is_invocable_r_v<void, T, VkCommandBuffer>
Ref<GpuTaskCtl> createGpuTask(T &&task) {
  return Ref<GpuTaskCtl>(new GpuTask<T>(std::forward<T>(task)));
}

struct GpuScheduler {
  std::vector<std::thread> threads;

  std::vector<std::thread> workThreads;
  std::vector<Ref<GpuTaskCtl>> tasks;
  std::mutex taskMtx;
  std::condition_variable taskCv;
  std::atomic<bool> exit{false};

public:
  explicit GpuScheduler(std::span<std::pair<VkQueue, std::uint32_t>> queues) {
    for (auto [queue, queueFamilyIndex] : queues) {
      workThreads.push_back(
          std::thread{[=, this] { entry(queue, queueFamilyIndex); }});
    }
  }

  ~GpuScheduler() {
    exit = true;
    taskCv.notify_all();

    for (auto &thread : workThreads) {
      thread.join();
    }
  }

  template <typename T>
    requires std::is_invocable_r_v<void, T, VkCommandBuffer>
  Ref<GpuTaskCtl> enqueue(T &&task) {
    auto taskHandle = createGpuTask(std::forward<T>(task));
    enqueue(taskHandle);
    return taskHandle;
  }

  void enqueue(Ref<GpuTaskCtl> task) {
    std::lock_guard lock(taskMtx);
    tasks.push_back(std::move(task));
    taskCv.notify_one();
  }

  template <typename T>
    requires std::is_invocable_r_v<bool, T, const AsyncTaskCtl &>
  void enqueue(TaskSet &set, T &&task) {
    auto taskCtl = enqueue(std::forward<T>(task));
    set.append(taskCtl);
  }

private:
  void entry(VkQueue queue, std::uint32_t queueFamilyIndex) {
    VkCommandPool pool;
    {
      VkCommandPoolCreateInfo poolCreateInfo{
          .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndex};

      Verify() << vkCreateCommandPool(vk::g_vkDevice, &poolCreateInfo,
                                      vk::g_vkAllocator, &pool);
    }

    while (!exit.load(std::memory_order::relaxed)) {
      Ref<GpuTaskCtl> task;

      {
        std::unique_lock lock(taskMtx);

        if (tasks.empty()) {
          taskCv.wait(lock);
        }

        if (tasks.empty()) {
          continue;
        }

        task = std::move(tasks.back());
        tasks.pop_back();
      }

      VkCommandBuffer cmdBuffer;
      {
        VkCommandBufferAllocateInfo allocateInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };
        Verify() << vkAllocateCommandBuffers(vk::g_vkDevice, &allocateInfo,
                                             &cmdBuffer);

        VkCommandBufferBeginInfo beginInfo{
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        };

        vkBeginCommandBuffer(cmdBuffer, &beginInfo);
      }

      task->invoke(cmdBuffer);
      vkEndCommandBuffer(cmdBuffer);

      submitToQueue(queue, cmdBuffer, task->semaphore, 1);
    }

    vkDestroyCommandPool(vk::g_vkDevice, pool, vk::g_vkAllocator);
  }
};

static GpuScheduler &getTransferQueueScheduler() {
  static GpuScheduler result{vk::g_transferQueues};
  return result;
}
static GpuScheduler &getComputeQueueScheduler() {
  static GpuScheduler result{vk::g_computeQueues};
  return result;
}
static GpuScheduler &getGraphicsQueueScheduler() {
  static GpuScheduler result{vk::g_graphicsQueues};
  return result;
}

static util::MemoryAreaTable<util::StdSetInvalidationHandle> memoryAreaTable;

void device::setVkDevice(VkDevice device,
                         VkPhysicalDeviceMemoryProperties memProperties,
                         VkPhysicalDeviceProperties devProperties) {
  vk::g_vkDevice = device;
  vk::g_physicalMemoryProperties = memProperties;
  vk::g_physicalDeviceProperties = devProperties;
}

static VkBlendFactor blendMultiplierToVkBlendFactor(BlendMultiplier mul) {
  switch (mul) {
  case kBlendMultiplierZero:
    return VK_BLEND_FACTOR_ZERO;
  case kBlendMultiplierOne:
    return VK_BLEND_FACTOR_ONE;
  case kBlendMultiplierSrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case kBlendMultiplierOneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case kBlendMultiplierSrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case kBlendMultiplierOneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case kBlendMultiplierDestAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case kBlendMultiplierOneMinusDestAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case kBlendMultiplierDestColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case kBlendMultiplierOneMinusDestColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case kBlendMultiplierSrcAlphaSaturate:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case kBlendMultiplierConstantColor:
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case kBlendMultiplierOneMinusConstantColor:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case kBlendMultiplierSrc1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case kBlendMultiplierInverseSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case kBlendMultiplierSrc1Alpha:
    return VK_BLEND_FACTOR_SRC1_ALPHA;
  case kBlendMultiplierInverseSrc1Alpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  case kBlendMultiplierConstantAlpha:
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case kBlendMultiplierOneMinusConstantAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  }

  util::unreachable();
}

static VkBlendOp blendFuncToVkBlendOp(BlendFunc func) {
  switch (func) {
  case kBlendFuncAdd:
    return VK_BLEND_OP_ADD;
  case kBlendFuncSubtract:
    return VK_BLEND_OP_SUBTRACT;
  case kBlendFuncMin:
    return VK_BLEND_OP_MIN;
  case kBlendFuncMax:
    return VK_BLEND_OP_MAX;
  case kBlendFuncReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  }

  util::unreachable();
}

#define GNM_GET_FIELD(src, registername, field)                                \
  (((src) & (GNM_##registername##__##field##__MASK)) >>                        \
   (GNM_##registername##__##field##__SHIFT))

#define mmSQ_BUF_RSRC_WORD0 0x23C0
#define GNM_SQ_BUF_RSRC_WORD0__BASE_ADDRESS__MASK 0xffffffffL // size:32
#define GNM_SQ_BUF_RSRC_WORD0__BASE_ADDRESS__SHIFT 0

#define mmSQ_BUF_RSRC_WORD1 0x23C1
#define GNM_SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__MASK 0x00000fffL // size:12
#define GNM_SQ_BUF_RSRC_WORD1__STRIDE__MASK 0x3fff0000L          // size:14
#define GNM_SQ_BUF_RSRC_WORD1__SWIZZLE_ENABLE__MASK 0x80000000L  // size: 1
#define GNM_SQ_BUF_RSRC_WORD1__BASE_ADDRESS_HI__SHIFT 0
#define GNM_SQ_BUF_RSRC_WORD1__STRIDE__SHIFT 16
#define GNM_SQ_BUF_RSRC_WORD1__SWIZZLE_ENABLE__SHIFT 31

#define mmSQ_BUF_RSRC_WORD2 0x23C2
#define GNM_SQ_BUF_RSRC_WORD2__NUM_RECORDS__MASK 0xffffffffL // size:32
#define GNM_SQ_BUF_RSRC_WORD2__NUM_RECORDS__SHIFT 0

#define mmSQ_BUF_RSRC_WORD3 0x23C3
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_X__MASK 0x00000007L    // size: 3
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_Y__MASK 0x00000038L    // size: 3
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_Z__MASK 0x000001c0L    // size: 3
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_W__MASK 0x00000e00L    // size: 3
#define GNM_SQ_BUF_RSRC_WORD3__ELEMENT_SIZE__MASK 0x00180000L // size: 2
#define GNM_SQ_BUF_RSRC_WORD3__INDEX_STRIDE__MASK 0x00600000L // size: 2
#define GNM_SQ_BUF_RSRC_WORD3__TYPE__MASK 0xc0000000L         // size: 2
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_X__SHIFT 0
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_Y__SHIFT 3
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_Z__SHIFT 6
#define GNM_SQ_BUF_RSRC_WORD3__DST_SEL_W__SHIFT 9
#define GNM_SQ_BUF_RSRC_WORD3__ELEMENT_SIZE__SHIFT 19
#define GNM_SQ_BUF_RSRC_WORD3__INDEX_STRIDE__SHIFT 21
#define GNM_SQ_BUF_RSRC_WORD3__TYPE__SHIFT 30

#define mmCB_COLOR0_PITCH 0xA319
#define GNM_CB_COLOR0_PITCH__TILE_MAX__MASK 0x000007ffL       // size:11
#define GNM_CB_COLOR0_PITCH__FMASK_TILE_MAX__MASK 0x7ff00000L // size:11
#define GNM_CB_COLOR0_PITCH__TILE_MAX__SHIFT 0
#define GNM_CB_COLOR0_PITCH__FMASK_TILE_MAX__SHIFT 20

#define mmCB_COLOR0_SLICE 0xA31A
#define GNM_CB_COLOR0_SLICE__TILE_MAX__MASK 0x003fffffL // size:22
#define GNM_CB_COLOR0_SLICE__TILE_MAX__SHIFT 0

#define mmCB_COLOR0_VIEW 0xA31B
#define GNM_CB_COLOR0_VIEW__SLICE_START__MASK 0x000007ffL // size:11
#define GNM_CB_COLOR0_VIEW__SLICE_MAX__MASK 0x00ffe000L   // size:11
#define GNM_CB_COLOR0_VIEW__SLICE_START__SHIFT 0
#define GNM_CB_COLOR0_VIEW__SLICE_MAX__SHIFT 13

#define mmCB_COLOR0_INFO 0xA31C
#define GNM_CB_COLOR0_INFO__FAST_CLEAR__MASK 0x00002000L             // size: 1
#define GNM_CB_COLOR0_INFO__COMPRESSION__MASK 0x00004000L            // size: 1
#define GNM_CB_COLOR0_INFO__CMASK_IS_LINEAR__MASK 0x00080000L        // size: 1
#define GNM_CB_COLOR0_INFO__FMASK_COMPRESSION_MODE__MASK 0x0C000000L // size: 2
#define GNM_CB_COLOR0_INFO__DCC_ENABLE__MASK 0x10000000L             // size: 1
#define GNM_CB_COLOR0_INFO__CMASK_ADDR_TYPE__MASK 0x60000000L        // size: 2
#define GNM_CB_COLOR0_INFO__ALT_TILE_MODE__MASK 0x80000000L          // size: 1
#define GNM_CB_COLOR0_INFO__FAST_CLEAR__SHIFT 13
#define GNM_CB_COLOR0_INFO__COMPRESSION__SHIFT 14
#define GNM_CB_COLOR0_INFO__CMASK_IS_LINEAR__SHIFT 19
#define GNM_CB_COLOR0_INFO__FMASK_COMPRESSION_MODE__SHIFT 26
#define GNM_CB_COLOR0_INFO__DCC_ENABLE__SHIFT 28
#define GNM_CB_COLOR0_INFO__CMASK_ADDR_TYPE__SHIFT 29
#define GNM_CB_COLOR0_INFO__ALT_TILE_MODE__SHIFT 31
#define GNM_CB_COLOR0_INFO__FORMAT__MASK 0x3f << 2
#define GNM_CB_COLOR0_INFO__FORMAT__SHIFT 2

#define GNM_CB_COLOR0_INFO__ARRAY_MODE__MASK 0x0f << 8
#define GNM_CB_COLOR0_INFO__ARRAY_MODE__SHIFT 8

enum {
  ARRAY_LINEAR_GENERAL = 0x00, // Unaligned linear array
  ARRAY_LINEAR_ALIGNED = 0x01, // Aligned linear array
};

#define GNM_CB_COLOR0_INFO__NUMBER_TYPE__MASK 0x07 << 12
#define GNM_CB_COLOR0_INFO__NUMBER_TYPE__SHIFT 12

enum {
  NUMBER_UNORM = 0x00, // unsigned repeating fraction (urf): range [0..1], scale
                       // factor (2^n)-1
  NUMBER_SNORM = 0x01, // Microsoft-style signed rf: range [-1..1], scale factor
                       // (2^(n-1))-1
  NUMBER_USCALED = 0x02, // unsigned integer, converted to float in shader:
                         // range [0..(2^n)-1]
  NUMBER_SSCALED = 0x03, // signed integer, converted to float in shader: range
                         // [-2^(n-1)..2^(n-1)-1]
  NUMBER_UINT = 0x04, // zero-extended bit field, int in shader: not blendable
                      // or filterable
  NUMBER_SINT = 0x05, // sign-extended bit field, int in shader: not blendable
                      // or filterable
  NUMBER_SRGB = 0x06, // gamma corrected, range [0..1] (only suported for 8-bit
                      // components (always rounds color channels)
  NUMBER_FLOAT =
      0x07, // floating point, depends on component size: 32-bit: IEEE float,
            // SE8M23, bias 127, range (- 2^129..2^129) 24-bit: Depth float,
            // E4M20, bias 15, range [0..1] 16-bit: Short float SE5M10, bias 15,
            // range (-2^17..2^17) 11-bit: Packed float, E5M6 bias 15, range
            // [0..2^17) 10-bit: Packed float, E5M5 bias 15, range [0..2^17) all
            // other component sizes are treated as UINT
};

#define GNM_CB_COLOR0_INFO__READ_SIZE__MASK 1 << 15
#define GNM_CB_COLOR0_INFO__READ_SIZE__SHIFT 15

// Specifies how to map the red, green, blue, and alpha components from the
// shader to the components in the frame buffer pixel format. There are four
// choices for each number of components. With one component, the four modes
// select any one component. With 2-4 components, SWAP_STD selects the low order
// shader components in little-endian order; SWAP_ALT selects an alternate order
// (for 4 compoents) or inclusion of alpha (for 2 or 3 components); and the
// other two reverse the component orders for use on big-endian machines. The
// following table specifies the exact component mappings:
//
// 1 comp      std     alt     std_rev alt_rev
// ----------- ------- ------- ------- -------
// comp 0:     red     green   blue    alpha
//
// 3 comps     std     alt     std_rev alt_rev
// ----------- ------- ------- ------- -------
// comp 0:     red     red     green   alpha
// comp 1:     green   alpha   red     red
//
// 3 comps     std     alt     std_rev alt_rev
// ----------- ------- ------- ------- -------
// comp 0:     red     red     blue    alpha
// comp 1:     green   green   green   green
// comp 2:     blue    alpha   red     red
//
// 4 comps     std     alt     std_rev alt_rev
// ----------- ------- ------- ------- -------
// comp 0:     red     blue    alpha   alpha
// comp 1:     green   green   blue    red
// comp 2:     blue    red     green   green
// comp 3:     alpha   alpha   red     blue
//
#define GNM_CB_COLOR0_INFO__COMP_SWAP__MASK 0x03 << 16
#define GNM_CB_COLOR0_INFO__COMP_SWAP__SHIFT 16
enum {
  SWAP_STD = 0x00,     // standard little-endian comp order
  SWAP_ALT = 0x01,     // alternate components or order
  SWAP_STD_REV = 0x02, // reverses SWAP_STD order
  SWAP_ALT_REV = 0x03, // reverses SWAP_ALT order
};

// Specifies whether to clamp source data to the render target range prior to
// blending, in addition to the post- blend clamp. This bit must be zero for
// uscaled, sscaled and float number types and when blend_bypass is set.
#define GNM_CB_COLOR0_INFO__BLEND_CLAMP__MASK 1 << 20
#define GNM_CB_COLOR0_INFO__BLEND_CLAMP__SHIFT 20

// If false, use RGB=0.0 and A=1.0 (0x3f800000) to expand fast-cleared tiles. If
// true, use the CB_CLEAR register values to expand fast-cleared tiles.
#define GNM_CB_COLOR0_INFO__CLEAR_COLOR__MASK 1 << 21
#define GNM_CB_COLOR0_INFO__CLEAR_COLOR__SHIFT 21

// If false, blending occurs normaly as specified in CB_BLEND#_CONTROL. If true,
// blending (but not fog) is disabled. This must be set for the 24_8 and 8_24
// formats and when the number type is uint or sint. It should also be set for
// number types that are required to ignore the blend state in a specific
// aplication interface.
#define GNM_CB_COLOR0_INFO__BLEND_BYPASS__MASK 1 << 22
#define GNM_CB_COLOR0_INFO__BLEND_BYPASS__SHIFT 22

// If true, use 32-bit float precision for source colors, else truncate to
// 12-bit mantissa precision. This applies even if blending is disabled so that
// a null blend and blend disable produce the same result. This field is ignored
// for NUMBER_UINT and NUMBER_SINT. It must be one for floating point components
// larger than 16-bits or non- floating components larger than 12-bits,
// otherwise it must be 0.
#define GNM_CB_COLOR0_INFO__BLEND_FLOAT32__MASK 1 << 23
#define GNM_CB_COLOR0_INFO__BLEND_FLOAT32__SHIFT 23

// If false, floating point processing follows full IEEE rules for INF, NaN, and
// -0. If true, 0*anything produces 0 and no operation produces -0.
#define GNM_CB_COLOR0_INFO__SIMPLE_FLOAT__MASK 1 << 24
#define GNM_CB_COLOR0_INFO__SIMPLE_FLOAT__SHIFT 24

// This field selects between truncating (standard for floats) and rounding
// (standard for most other cases) to convert blender results to frame buffer
// components. The ROUND_BY_HALF setting can be over-riden by the DITHER_ENABLE
// field in CB_COLOR_CONTROL.
#define GNM_CB_COLOR0_INFO__ROUND_MODE__MASK 1 << 25
#define GNM_CB_COLOR0_INFO__ROUND_MODE__SHIFT 25

// This field indicates the allowed format for color data being exported from
// the pixel shader into the output merge block. This field may only be set to
// EXPORT_NORM if BLEND_CLAMP is enabled, BLEND_FLOAT32 is disabled, and the
// render target has only 11-bit or smaller UNORM or SNORM components. Selecting
// EXPORT_NORM flushes to zero values with exponent less than 0x70 (values less
// than 2^-15).
#define GNM_CB_COLOR0_INFO__SOURCE_FORMAT__MASK 1 << 27
#define GNM_CB_COLOR0_INFO__SOURCE_FORMAT__SHIFT 27

#define mmCB_COLOR0_ATTRIB 0xA31D
#define GNM_CB_COLOR0_ATTRIB__TILE_MODE_INDEX__MASK 0x0000001fL       // size: 5
#define GNM_CB_COLOR0_ATTRIB__FMASK_TILE_MODE_INDEX__MASK 0x000003e0L // size: 5
#define GNM_CB_COLOR0_ATTRIB__NUM_SAMPLES__MASK 0x00007000L           // size: 3
#define GNM_CB_COLOR0_ATTRIB__NUM_FRAGMENTS__MASK 0x00018000L         // size: 2
#define GNM_CB_COLOR0_ATTRIB__FORCE_DST_ALPHA_1__MASK 0x00020000L     // size: 1
#define GNM_CB_COLOR0_ATTRIB__TILE_MODE_INDEX__SHIFT 0
#define GNM_CB_COLOR0_ATTRIB__FMASK_TILE_MODE_INDEX__SHIFT 5
#define GNM_CB_COLOR0_ATTRIB__NUM_SAMPLES__SHIFT 12
#define GNM_CB_COLOR0_ATTRIB__NUM_FRAGMENTS__SHIFT 15
#define GNM_CB_COLOR0_ATTRIB__FORCE_DST_ALPHA_1__SHIFT 17

#define mmCB_COLOR0_DCC_CONTROL 0xA31E
#define GNM_CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE__MASK            \
  0x00000001L // size: 1
#define GNM_CB_COLOR0_DCC_CONTROL__MAX_UNCOMPRESSED_BLOCK_SIZE__MASK           \
  0x0000000cL // size: 2
#define GNM_CB_COLOR0_DCC_CONTROL__MIN_COMPRESSED_BLOCK_SIZE__MASK             \
  0x00000010L // size: 1
#define GNM_CB_COLOR0_DCC_CONTROL__MAX_COMPRESSED_BLOCK_SIZE__MASK             \
  0x00000060L                                                        // size: 2
#define GNM_CB_COLOR0_DCC_CONTROL__COLOR_TRANSFORM__MASK 0x00000180L // size: 2
#define GNM_CB_COLOR0_DCC_CONTROL__INDEPENDENT_64B_BLOCKS__MASK                \
  0x00000200L // size: 1
#define GNM_CB_COLOR0_DCC_CONTROL__OVERWRITE_COMBINER_DISABLE__SHIFT 0
#define GNM_CB_COLOR0_DCC_CONTROL__MAX_UNCOMPRESSED_BLOCK_SIZE__SHIFT 2
#define GNM_CB_COLOR0_DCC_CONTROL__MIN_COMPRESSED_BLOCK_SIZE__SHIFT 4
#define GNM_CB_COLOR0_DCC_CONTROL__MAX_COMPRESSED_BLOCK_SIZE__SHIFT 5
#define GNM_CB_COLOR0_DCC_CONTROL__COLOR_TRANSFORM__SHIFT 7
#define GNM_CB_COLOR0_DCC_CONTROL__INDEPENDENT_64B_BLOCKS__SHIFT 9

#define mmCB_COLOR0_CMASK 0xA31F
#define GNM_CB_COLOR0_CMASK__BASE_256B__MASK 0xffffffffL // size:32
#define GNM_CB_COLOR0_CMASK__BASE_256B__SHIFT 0

#define mmCB_COLOR0_CMASK_SLICE 0xA320
#define GNM_CB_COLOR0_CMASK_SLICE__TILE_MAX__MASK 0x00003fffL // size:14
#define GNM_CB_COLOR0_CMASK_SLICE__TILE_MAX__SHIFT 0

#define mmCB_COLOR0_FMASK 0xA321
#define GNM_CB_COLOR0_FMASK__BASE_256B__MASK 0xffffffffL // size:32
#define GNM_CB_COLOR0_FMASK__BASE_256B__SHIFT 0

#define mmCB_COLOR0_FMASK_SLICE 0xA322
#define GNM_CB_COLOR0_FMASK_SLICE__TILE_MAX__MASK 0x003fffffL // size:22
#define GNM_CB_COLOR0_FMASK_SLICE__TILE_MAX__SHIFT 0

#define mmCB_COLOR0_CLEAR_WORD0 0xA323
#define GNM_CB_COLOR0_CLEAR_WORD0__CLEAR_WORD0__MASK 0xffffffffL // size:32
#define GNM_CB_COLOR0_CLEAR_WORD0__CLEAR_WORD0__SHIFT 0

#define mmCB_COLOR0_CLEAR_WORD1 0xA324
#define GNM_CB_COLOR0_CLEAR_WORD1__CLEAR_WORD1__MASK 0xffffffffL // size:32
#define GNM_CB_COLOR0_CLEAR_WORD1__CLEAR_WORD1__SHIFT 0

#define mmCB_COLOR0_DCC_BASE 0xA325
#define GNM_CB_COLOR0_DCC_BASE__BASE_256B__MASK 0xffffffffL // size:32
#define GNM_CB_COLOR0_DCC_BASE__BASE_256B__SHIFT 0

static constexpr auto CB_BLEND0_CONTROL_COLOR_SRCBLEND_MASK = genMask(0, 5);
static constexpr auto CB_BLEND0_CONTROL_COLOR_COMB_FCN_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_COLOR_SRCBLEND_MASK), 3);
static constexpr auto CB_BLEND0_CONTROL_COLOR_DESTBLEND_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_COLOR_COMB_FCN_MASK), 5);
static constexpr auto CB_BLEND0_CONTROL_OPACITY_WEIGHT_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_COLOR_DESTBLEND_MASK), 1);
static constexpr auto CB_BLEND0_CONTROL_ALPHA_SRCBLEND_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_OPACITY_WEIGHT_MASK) + 2, 5);
static constexpr auto CB_BLEND0_CONTROL_ALPHA_COMB_FCN_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_ALPHA_SRCBLEND_MASK), 3);
static constexpr auto CB_BLEND0_CONTROL_ALPHA_DESTBLEND_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_ALPHA_COMB_FCN_MASK), 5);
static constexpr auto CB_BLEND0_CONTROL_SEPARATE_ALPHA_BLEND_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_ALPHA_DESTBLEND_MASK), 1);
static constexpr auto CB_BLEND0_CONTROL_BLEND_ENABLE_MASK =
    genMask(getMaskEnd(CB_BLEND0_CONTROL_SEPARATE_ALPHA_BLEND_MASK), 1);

struct ColorBuffer {
  std::uint64_t base;
  std::uint8_t format;
  std::uint8_t tileModeIndex;

  void setRegister(unsigned index, std::uint32_t value) {
    switch (index) {
    case CB_COLOR0_BASE - CB_COLOR0_BASE:
      base = static_cast<std::uint64_t>(value) << 8;
      // std::printf("  * base = %lx\n", base);
      break;

    case CB_COLOR0_PITCH - CB_COLOR0_BASE: {
      auto pitchTileMax = GNM_GET_FIELD(value, CB_COLOR0_PITCH, TILE_MAX);
      auto pitchFmaskTileMax =
          GNM_GET_FIELD(value, CB_COLOR0_PITCH, FMASK_TILE_MAX);
      // std::printf("  * TILE_MAX = %lx\n", pitchTileMax);
      // std::printf("  * FMASK_TILE_MAX = %lx\n", pitchFmaskTileMax);
      break;
    }
    case CB_COLOR0_SLICE - CB_COLOR0_BASE: { // SLICE
      auto sliceTileMax = GNM_GET_FIELD(value, CB_COLOR0_SLICE, TILE_MAX);
      // std::printf("  * TILE_MAX = %lx\n", sliceTileMax);
      break;
    }
    case CB_COLOR0_VIEW - CB_COLOR0_BASE: { // VIEW
      auto viewSliceStart = GNM_GET_FIELD(value, CB_COLOR0_VIEW, SLICE_START);
      auto viewSliceMax = GNM_GET_FIELD(value, CB_COLOR0_VIEW, SLICE_MAX);

      // std::printf("  * SLICE_START = %lx\n", viewSliceStart);
      // std::printf("  * SLICE_MAX = %lx\n", viewSliceMax);
      break;
    }
    case CB_COLOR0_INFO - CB_COLOR0_BASE: { // INFO
      auto fastClear = GNM_GET_FIELD(value, CB_COLOR0_INFO, FAST_CLEAR);
      auto compression = GNM_GET_FIELD(value, CB_COLOR0_INFO, COMPRESSION);
      auto cmaskIsLinear =
          GNM_GET_FIELD(value, CB_COLOR0_INFO, CMASK_IS_LINEAR);
      auto fmaskCompressionMode =
          GNM_GET_FIELD(value, CB_COLOR0_INFO, FMASK_COMPRESSION_MODE);
      auto dccEnable = GNM_GET_FIELD(value, CB_COLOR0_INFO, DCC_ENABLE);
      auto cmaskAddrType =
          GNM_GET_FIELD(value, CB_COLOR0_INFO, CMASK_ADDR_TYPE);
      auto altTileMode = GNM_GET_FIELD(value, CB_COLOR0_INFO, ALT_TILE_MODE);
      format = GNM_GET_FIELD(value, CB_COLOR0_INFO, FORMAT);
      auto arrayMode = GNM_GET_FIELD(value, CB_COLOR0_INFO, ARRAY_MODE);
      auto numberType = GNM_GET_FIELD(value, CB_COLOR0_INFO, NUMBER_TYPE);
      auto readSize = GNM_GET_FIELD(value, CB_COLOR0_INFO, READ_SIZE);
      auto compSwap = GNM_GET_FIELD(value, CB_COLOR0_INFO, COMP_SWAP);
      auto blendClamp = GNM_GET_FIELD(value, CB_COLOR0_INFO, BLEND_CLAMP);
      auto clearColor = GNM_GET_FIELD(value, CB_COLOR0_INFO, CLEAR_COLOR);
      auto blendBypass = GNM_GET_FIELD(value, CB_COLOR0_INFO, BLEND_BYPASS);
      auto blendFloat32 = GNM_GET_FIELD(value, CB_COLOR0_INFO, BLEND_FLOAT32);
      auto simpleFloat = GNM_GET_FIELD(value, CB_COLOR0_INFO, SIMPLE_FLOAT);
      auto roundMode = GNM_GET_FIELD(value, CB_COLOR0_INFO, ROUND_MODE);
      auto sourceFormat = GNM_GET_FIELD(value, CB_COLOR0_INFO, SOURCE_FORMAT);

      // std::printf("  * FAST_CLEAR = %lu\n", fastClear);
      // std::printf("  * COMPRESSION = %lu\n", compression);
      // std::printf("  * CMASK_IS_LINEAR = %lu\n", cmaskIsLinear);
      // std::printf("  * FMASK_COMPRESSION_MODE = %lu\n",
      // fmaskCompressionMode); std::printf("  * DCC_ENABLE = %lu\n",
      // dccEnable); std::printf("  * CMASK_ADDR_TYPE = %lu\n", cmaskAddrType);
      // std::printf("  * ALT_TILE_MODE = %lu\n", altTileMode);
      // std::printf("  * FORMAT = %x\n", format);
      // std::printf("  * ARRAY_MODE = %u\n", arrayMode);
      // std::printf("  * NUMBER_TYPE = %u\n", numberType);
      // std::printf("  * READ_SIZE = %u\n", readSize);
      // std::printf("  * COMP_SWAP = %u\n", compSwap);
      // std::printf("  * BLEND_CLAMP = %u\n", blendClamp);
      // std::printf("  * CLEAR_COLOR = %u\n", clearColor);
      // std::printf("  * BLEND_BYPASS = %u\n", blendBypass);
      // std::printf("  * BLEND_FLOAT32 = %u\n", blendFloat32);
      // std::printf("  * SIMPLE_FLOAT = %u\n", simpleFloat);
      // std::printf("  * ROUND_MODE = %u\n", roundMode);
      // std::printf("  * SOURCE_FORMAT = %u\n", sourceFormat);
      break;
    }

    case CB_COLOR0_ATTRIB - CB_COLOR0_BASE: { // ATTRIB
      tileModeIndex = GNM_GET_FIELD(value, CB_COLOR0_ATTRIB, TILE_MODE_INDEX);
      auto fmaskTileModeIndex =
          GNM_GET_FIELD(value, CB_COLOR0_ATTRIB, FMASK_TILE_MODE_INDEX);
      auto numSamples = GNM_GET_FIELD(value, CB_COLOR0_ATTRIB, NUM_SAMPLES);
      auto numFragments = GNM_GET_FIELD(value, CB_COLOR0_ATTRIB, NUM_FRAGMENTS);
      auto forceDstAlpha1 =
          GNM_GET_FIELD(value, CB_COLOR0_ATTRIB, FORCE_DST_ALPHA_1);

      // std::printf("  * TILE_MODE_INDEX = %u\n", tileModeIndex);
      // std::printf("  * FMASK_TILE_MODE_INDEX = %lu\n", fmaskTileModeIndex);
      // std::printf("  * NUM_SAMPLES = %lu\n", numSamples);
      // std::printf("  * NUM_FRAGMENTS = %lu\n", numFragments);
      // std::printf("  * FORCE_DST_ALPHA_1 = %lu\n", forceDstAlpha1);
      break;
    }
    case CB_COLOR0_CMASK - CB_COLOR0_BASE: { // CMASK
      auto cmaskBase = GNM_GET_FIELD(value, CB_COLOR0_CMASK, BASE_256B) << 8;
      // std::printf("  * cmaskBase = %lx\n", cmaskBase);
      break;
    }
    case CB_COLOR0_CMASK_SLICE - CB_COLOR0_BASE: { // CMASK_SLICE
      auto cmaskSliceTileMax =
          GNM_GET_FIELD(value, CB_COLOR0_CMASK_SLICE, TILE_MAX);
      // std::printf("  * cmaskSliceTileMax = %lx\n", cmaskSliceTileMax);
      break;
    }
    case CB_COLOR0_FMASK - CB_COLOR0_BASE: { // FMASK
      auto fmaskBase = GNM_GET_FIELD(value, CB_COLOR0_FMASK, BASE_256B) << 8;
      // std::printf("  * fmaskBase = %lx\n", fmaskBase);
      break;
    }
    case CB_COLOR0_FMASK_SLICE - CB_COLOR0_BASE: { // FMASK_SLICE
      auto fmaskSliceTileMax =
          GNM_GET_FIELD(value, CB_COLOR0_FMASK_SLICE, TILE_MAX);
      // std::printf("  * fmaskSliceTileMax = %lx\n", fmaskSliceTileMax);
      break;
    }
    case CB_COLOR0_CLEAR_WORD0 - CB_COLOR0_BASE: // CLEAR_WORD0
      break;
    case CB_COLOR1_CLEAR_WORD0 - CB_COLOR0_BASE: // CLEAR_WORD1
      break;
    }
  }
};

static constexpr std::size_t colorBuffersCount = 6;

enum class CbRasterOp {
  Blackness = 0x00,
  Nor = 0x05,          // ~(src | dst)
  AndInverted = 0x0a,  // ~src & dst
  CopyInverted = 0x0f, // ~src
  NotSrcErase = 0x11,  // ~src & ~dst
  SrcErase = 0x44,     // src & ~dst
  DstInvert = 0x55,    // ~dst
  Xor = 0x5a,          // src ^ dst
  Nand = 0x5f,         // ~(src & dst)
  And = 0x88,          // src & dst
  Equiv = 0x99,        // ~(src ^ dst)
  Noop = 0xaa,         // dst
  OrInverted = 0xaf,   // ~src | dst
  Copy = 0xcc,         // src
  OrReverse = 0xdd,    // src | ~dst
  Or = 0xEE,           // src | dst
  Whiteness = 0xff,
};

enum class CbColorFormat {
  /*
        00 - CB_DISABLE: Disables drawing to color
        buffer. Causes DB to not send tiles/quads to CB. CB
        itself ignores this field.
        01 - CB_NORMAL: Normal rendering mode. DB
        should send tiles and quads for pixel exports or just
        quads for compute exports.
        02 - CB_ELIMINATE_FAST_CLEAR: Fill fast
        cleared color surface locations with clear color. DB
        should send only tiles.
        03 - CB_RESOLVE: Read from MRT0, average all
        samples, and write to MRT1, which is one-sample. DB
        should send only tiles.
        04 - CB_DECOMPRESS: Decompress MRT0 to a
  */
  Disable,
  Normal,
  EliminateFastClear,
  Resolve,
};

struct QueueRegisters {
  std::uint64_t pgmPsAddress = 0;
  std::uint64_t pgmVsAddress = 0;
  std::uint64_t pgmComputeAddress = 0;
  std::uint32_t userVsData[16];
  std::uint32_t userPsData[16];
  std::uint32_t userComputeData[16];
  std::uint32_t computeNumThreadX = 1;
  std::uint32_t computeNumThreadY = 1;
  std::uint32_t computeNumThreadZ = 1;
  std::uint8_t psUserSpgrs;
  std::uint8_t vsUserSpgrs;
  std::uint8_t computeUserSpgrs;

  ColorBuffer colorBuffers[colorBuffersCount];

  std::uint32_t indexType;

  std::uint32_t screenScissorX = 0;
  std::uint32_t screenScissorY = 0;
  std::uint32_t screenScissorW = 0;
  std::uint32_t screenScissorH = 0;

  CbColorFormat cbColorFormat = CbColorFormat::Normal;

  CbRasterOp cbRasterOp = CbRasterOp::Copy;

  std::uint32_t vgtPrimitiveType = 0;
  bool stencilEnable = false;
  bool depthEnable = false;
  bool depthWriteEnable = false;
  bool depthBoundsEnable = false;
  int zFunc = 0;
  bool backFaceEnable = false;
  int stencilFunc = 0;
  int stencilFuncBackFace = 0;

  float depthClear = 1.f;

  bool cullFront = false;
  bool cullBack = false;
  int face = 0; // 0 - CCW, 1 - CW
  bool polyMode = false;
  int polyModeFrontPType = 0;
  int polyModeBackPType = 0;
  bool polyOffsetFrontEnable = false;
  bool polyOffsetBackEnable = false;
  bool polyOffsetParaEnable = false;
  bool vtxWindowOffsetEnable = false;
  bool provokingVtxLast = false;
  bool erspCorrDis = false;
  bool multiPrimIbEna = false;

  bool depthClearEnable = false;
  bool stencilClearEnable = false;
  bool depthCopy = false;
  bool stencilCopy = false;
  bool resummarizeEnable = false;
  bool stencilCompressDisable = false;
  bool depthCompressDisable = false;
  bool copyCentroid = false;
  int copySample = 0;
  bool zpassIncrementDisable = false;

  std::uint64_t zReadBase = 0;
  std::uint64_t zWriteBase = 0;

  BlendMultiplier blendColorSrc = {};
  BlendFunc blendColorFn = {};
  BlendMultiplier blendColorDst = {};
  BlendMultiplier blendAlphaSrc = {};
  BlendFunc blendAlphaFn = {};
  BlendMultiplier blendAlphaDst = {};
  bool blendSeparateAlpha = false;
  bool blendEnable = false;
  std::uint32_t cbRenderTargetMask = 0;

  void setRegister(std::uint32_t regId, std::uint32_t value) {
    switch (regId) {
    case SPI_SHADER_PGM_LO_PS:
      pgmPsAddress &= ~((1ull << 40) - 1);
      pgmPsAddress |= static_cast<std::uint64_t>(value) << 8;
      break;
    case SPI_SHADER_PGM_HI_PS:
      pgmPsAddress &= (1ull << 40) - 1;
      pgmPsAddress |= static_cast<std::uint64_t>(value) << 40;
      break;
    case SPI_SHADER_PGM_LO_VS:
      pgmVsAddress &= ~((1ull << 40) - 1);
      pgmVsAddress |= static_cast<std::uint64_t>(value) << 8;
      break;
    case SPI_SHADER_PGM_HI_VS:
      pgmVsAddress &= (1ull << 40) - 1;
      pgmVsAddress |= static_cast<std::uint64_t>(value) << 40;
      break;

    case SPI_SHADER_USER_DATA_VS_0:
    case SPI_SHADER_USER_DATA_VS_1:
    case SPI_SHADER_USER_DATA_VS_2:
    case SPI_SHADER_USER_DATA_VS_3:
    case SPI_SHADER_USER_DATA_VS_4:
    case SPI_SHADER_USER_DATA_VS_5:
    case SPI_SHADER_USER_DATA_VS_6:
    case SPI_SHADER_USER_DATA_VS_7:
    case SPI_SHADER_USER_DATA_VS_8:
    case SPI_SHADER_USER_DATA_VS_9:
    case SPI_SHADER_USER_DATA_VS_10:
    case SPI_SHADER_USER_DATA_VS_11:
    case SPI_SHADER_USER_DATA_VS_12:
    case SPI_SHADER_USER_DATA_VS_13:
    case SPI_SHADER_USER_DATA_VS_14:
    case SPI_SHADER_USER_DATA_VS_15:
      userVsData[regId - SPI_SHADER_USER_DATA_VS_0] = value;
      break;

    case SPI_SHADER_USER_DATA_PS_0:
    case SPI_SHADER_USER_DATA_PS_1:
    case SPI_SHADER_USER_DATA_PS_2:
    case SPI_SHADER_USER_DATA_PS_3:
    case SPI_SHADER_USER_DATA_PS_4:
    case SPI_SHADER_USER_DATA_PS_5:
    case SPI_SHADER_USER_DATA_PS_6:
    case SPI_SHADER_USER_DATA_PS_7:
    case SPI_SHADER_USER_DATA_PS_8:
    case SPI_SHADER_USER_DATA_PS_9:
    case SPI_SHADER_USER_DATA_PS_10:
    case SPI_SHADER_USER_DATA_PS_11:
    case SPI_SHADER_USER_DATA_PS_12:
    case SPI_SHADER_USER_DATA_PS_13:
    case SPI_SHADER_USER_DATA_PS_14:
    case SPI_SHADER_USER_DATA_PS_15:
      userPsData[regId - SPI_SHADER_USER_DATA_PS_0] = value;
      break;

    case SPI_SHADER_PGM_RSRC2_PS:
      psUserSpgrs = (value >> 1) & 0x1f;
      break;

    case SPI_SHADER_PGM_RSRC2_VS:
      vsUserSpgrs = (value >> 1) & 0x1f;
      break;

    case CB_COLOR0_BASE ... CB_COLOR6_DCC_BASE: {
      auto buffer =
          (regId - CB_COLOR0_BASE) / (CB_COLOR1_BASE - CB_COLOR0_BASE);
      auto index = (regId - CB_COLOR0_BASE) % (CB_COLOR1_BASE - CB_COLOR0_BASE);
      colorBuffers[buffer].setRegister(index, value);
      break;
    }

    case DB_RENDER_CONTROL:
      depthClearEnable = getBit(value, 0);
      stencilClearEnable = getBit(value, 1);
      depthCopy = getBit(value, 2);
      stencilCopy = getBit(value, 3);
      resummarizeEnable = getBit(value, 4);
      stencilCompressDisable = getBit(value, 5);
      depthCompressDisable = getBit(value, 6);
      copyCentroid = getBit(value, 7);
      copySample = getBits(value, 10, 8);
      zpassIncrementDisable = getBit(value, 11);
      break;

    case DB_Z_READ_BASE:
      zReadBase = static_cast<std::uint64_t>(value) << 8;
      break;

    case DB_Z_WRITE_BASE:
      zWriteBase = static_cast<std::uint64_t>(value) << 8;
      break;

    case DB_DEPTH_CLEAR:
      depthClear = std::bit_cast<float>(value);
      break;

    case DB_DEPTH_CONTROL:
      stencilEnable = getBit(value, 0) != 0;
      depthEnable = getBit(value, 1) != 0;
      depthWriteEnable = getBit(value, 2) != 0;
      depthBoundsEnable = getBit(value, 3) != 0;
      zFunc = getBits(value, 6, 4);
      backFaceEnable = getBit(value, 7);
      stencilFunc = getBits(value, 11, 8);
      stencilFuncBackFace = getBits(value, 23, 20);

      // std::printf("stencilEnable=%u, depthEnable=%u, depthWriteEnable=%u, "
      //             "depthBoundsEnable=%u, zFunc=%u, backFaceEnable=%u, "
      //             "stencilFunc=%u, stencilFuncBackFace=%u\n",
      //             stencilEnable, depthEnable, depthWriteEnable,
      //             depthBoundsEnable, zFunc, backFaceEnable, stencilFunc,
      //             stencilFuncBackFace);
      break;

    case CB_TARGET_MASK: {
      cbRenderTargetMask = value;
      break;
    }

    case CB_COLOR_CONTROL: {
      /*
        If true, then each UNORM format COLOR_8_8_8_8
        MRT is treated as an SRGB format instead. This affects
        both normal draw and resolve. This bit exists for
        compatibility with older architectures that did not have
        an SRGB number type.
      */
      auto degammaEnable = getBits(value, 3, 0);

      /*
        This field selects standard color processing or one of
        several major operation modes.

        POSSIBLE VALUES:
        00 - CB_DISABLE: Disables drawing to color
        buffer. Causes DB to not send tiles/quads to CB. CB
        itself ignores this field.
        01 - CB_NORMAL: Normal rendering mode. DB
        should send tiles and quads for pixel exports or just
        quads for compute exports.
        02 - CB_ELIMINATE_FAST_CLEAR: Fill fast
        cleared color surface locations with clear color. DB
        should send only tiles.
        03 - CB_RESOLVE: Read from MRT0, average all
        samples, and write to MRT1, which is one-sample. DB
        should send only tiles.
        04 - CB_DECOMPRESS: Decompress MRT0 to a
        uncompressed color format. This is required before a
        multisampled surface is accessed by the CPU, or used as
        a texture. This also decompresses the FMASK buffer. A
        CB_ELIMINATE_FAST_CLEAR pass before this is
        unnecessary. DB should send tiles and quads.
        05 - CB_FMASK_DECOMPRESS: Decompress the
        FMASK buffer into a texture readable format. A
        CB_ELIMINATE_FAST_CLEAR pass before this is
        unnecessary. DB should send only tiles.
      */
      auto mode = getBits(value, 6, 4);

      /*
        This field supports the 28 boolean ops that combine
        either source and dest or brush and dest, with brush
        provided by the shader in place of source. The code
        0xCC (11001100) copies the source to the destination,
        which disables the ROP function. ROP must be disabled
        if any MRT enables blending.

        POSSIBLE VALUES:
        00 - 0x00: BLACKNESS
        05 - 0x05
        10 - 0x0A
        15 - 0x0F
        17 - 0x11: NOTSRCERASE
        34 - 0x22
        51 - 0x33: NOTSRCCOPY
        68 - 0x44: SRCERASE
        80 - 0x50
        85 - 0x55: DSTINVERT
        90 - 0x5A: PATINVERT
        95 - 0x5F
        102 - 0x66: SRCINVERT
        119 - 0x77
        136 - 0x88: SRCAND
        153 - 0x99
        160 - 0xA0
        165 - 0xA5
        170 - 0xAA
        175 - 0xAF
        187 - 0xBB: MERGEPAINT
        204 - 0xCC: SRCCOPY
        221 - 0xDD
        238 - 0xEE: SRCPAINT
        240 - 0xF0: PATCOPY
        245 - 0xF5
        250 - 0xFA
        255 - 0xFF: WHITENESS
      */
      auto rop3 = getBits(value, 23, 16);

      // std::printf("  * degammaEnable = %x\n", degammaEnable);
      // std::printf("  * mode = %x\n", mode);
      // std::printf("  * rop3 = %x\n", rop3);

      cbColorFormat = static_cast<CbColorFormat>(mode);
      cbRasterOp = static_cast<CbRasterOp>(rop3);
      break;
    }

    case PA_CL_CLIP_CNTL:
      cullFront = getBit(value, 0);
      cullBack = getBit(value, 1);
      face = getBit(value, 2);
      polyMode = getBits(value, 4, 3);
      polyModeFrontPType = getBits(value, 7, 5);
      polyModeBackPType = getBits(value, 10, 8);
      polyOffsetFrontEnable = getBit(value, 11);
      polyOffsetBackEnable = getBit(value, 12);
      polyOffsetParaEnable = getBit(value, 13);
      vtxWindowOffsetEnable = getBit(value, 16);
      provokingVtxLast = getBit(value, 19);
      erspCorrDis = getBit(value, 20);
      multiPrimIbEna = getBit(value, 21);
      break;

    case PA_SC_SCREEN_SCISSOR_TL:
      screenScissorX = static_cast<std::uint16_t>(value);
      screenScissorY = static_cast<std::uint16_t>(value >> 16);
      break;

    case PA_SC_SCREEN_SCISSOR_BR:
      screenScissorW = static_cast<std::uint16_t>(value) - screenScissorX;
      screenScissorH = static_cast<std::uint16_t>(value >> 16) - screenScissorY;
      break;

    case VGT_PRIMITIVE_TYPE:
      vgtPrimitiveType = value;
      break;

    case COMPUTE_NUM_THREAD_X:
      computeNumThreadX = value;
      break;

    case COMPUTE_NUM_THREAD_Y:
      computeNumThreadY = value;
      break;

    case COMPUTE_NUM_THREAD_Z:
      computeNumThreadZ = value;
      break;

    case COMPUTE_PGM_LO:
      pgmComputeAddress &= ~((1ull << 40) - 1);
      pgmComputeAddress |= static_cast<std::uint64_t>(value) << 8;
      break;

    case COMPUTE_PGM_HI:
      pgmComputeAddress &= (1ull << 40) - 1;
      pgmComputeAddress |= static_cast<std::uint64_t>(value) << 40;
      break;

    case COMPUTE_PGM_RSRC1:
      break;
    case COMPUTE_PGM_RSRC2:
      computeUserSpgrs = (value >> 1) & 0x1f;
      break;

    case COMPUTE_USER_DATA_0:
    case COMPUTE_USER_DATA_1:
    case COMPUTE_USER_DATA_2:
    case COMPUTE_USER_DATA_3:
    case COMPUTE_USER_DATA_4:
    case COMPUTE_USER_DATA_5:
    case COMPUTE_USER_DATA_6:
    case COMPUTE_USER_DATA_7:
    case COMPUTE_USER_DATA_8:
    case COMPUTE_USER_DATA_9:
    case COMPUTE_USER_DATA_10:
    case COMPUTE_USER_DATA_11:
    case COMPUTE_USER_DATA_12:
    case COMPUTE_USER_DATA_13:
    case COMPUTE_USER_DATA_14:
    case COMPUTE_USER_DATA_15:
      userComputeData[regId - COMPUTE_USER_DATA_0] = value;
      break;

    case CB_BLEND0_CONTROL: {
      blendColorSrc = (BlendMultiplier)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_COLOR_SRCBLEND_MASK);
      blendColorFn = (BlendFunc)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_COLOR_COMB_FCN_MASK);
      blendColorDst = (BlendMultiplier)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_COLOR_DESTBLEND_MASK);
      auto opacity_weight =
          fetchMaskedValue(value, CB_BLEND0_CONTROL_OPACITY_WEIGHT_MASK);
      blendAlphaSrc = (BlendMultiplier)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_ALPHA_SRCBLEND_MASK);
      blendAlphaFn = (BlendFunc)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_ALPHA_COMB_FCN_MASK);
      blendAlphaDst = (BlendMultiplier)fetchMaskedValue(
          value, CB_BLEND0_CONTROL_ALPHA_DESTBLEND_MASK);
      blendSeparateAlpha =
          fetchMaskedValue(value,
                           CB_BLEND0_CONTROL_SEPARATE_ALPHA_BLEND_MASK) != 0;
      blendEnable =
          fetchMaskedValue(value, CB_BLEND0_CONTROL_BLEND_ENABLE_MASK) != 0;

      // std::printf("  * COLOR_SRCBLEND = %x\n", blendColorSrc);
      // std::printf("  * COLOR_COMB_FCN = %x\n", blendColorFn);
      // std::printf("  * COLOR_DESTBLEND = %x\n", blendColorDst);
      // std::printf("  * OPACITY_WEIGHT = %x\n", opacity_weight);
      // std::printf("  * ALPHA_SRCBLEND = %x\n", blendAlphaSrc);
      // std::printf("  * ALPHA_COMB_FCN = %x\n", blendAlphaFn);
      // std::printf("  * ALPHA_DESTBLEND = %x\n", blendAlphaDst);
      // std::printf("  * SEPARATE_ALPHA_BLEND = %x\n", blendSeparateAlpha);
      // std::printf("  * BLEND_ENABLE = %x\n", blendEnable);
      break;
    }
    }
  }
};

static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                                  VkImageAspectFlags aspectFlags,
                                  VkImageLayout oldLayout,
                                  VkImageLayout newLayout) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = aspectFlags;
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  auto layoutToStageAccess = [](VkImageLayout layout)
      -> std::pair<VkPipelineStageFlags, VkAccessFlags> {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      return {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0};

    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT};

    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return {VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_READ_BIT};

    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT};

    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
              VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT |
                  VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT};

    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
              VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                  VK_ACCESS_COLOR_ATTACHMENT_READ_BIT};

    default:
      util::unreachable("unsupported layout transition! %d", layout);
    }
  };

  auto [sourceStage, sourceAccess] = layoutToStageAccess(oldLayout);
  auto [destinationStage, destinationAccess] = layoutToStageAccess(newLayout);

  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

static int getBitWidthOfSurfaceFormat(SurfaceFormat format) {
  switch (format) {
  case kSurfaceFormatInvalid:
    return 0;
  case kSurfaceFormat8:
    return 8;
  case kSurfaceFormat16:
    return 16;
  case kSurfaceFormat8_8:
    return 8 + 8;
  case kSurfaceFormat32:
    return 32;
  case kSurfaceFormat16_16:
    return 16 + 16;
  case kSurfaceFormat10_11_11:
    return 10 + 11 + 11;
  case kSurfaceFormat11_11_10:
    return 11 + 11 + 10;
  case kSurfaceFormat10_10_10_2:
    return 10 + 10 + 10 + 2;
  case kSurfaceFormat2_10_10_10:
    return 2 + 10 + 10 + 10;
  case kSurfaceFormat8_8_8_8:
    return 8 + 8 + 8 + 8;
  case kSurfaceFormat32_32:
    return 32 + 32;
  case kSurfaceFormat16_16_16_16:
    return 16 + 16 + 16 + 16;
  case kSurfaceFormat32_32_32:
    return 32 + 32 + 32;
  case kSurfaceFormat32_32_32_32:
    return 32 + 32 + 32 + 32;
  case kSurfaceFormat5_6_5:
    return 5 + 6 + 5;
  case kSurfaceFormat1_5_5_5:
    return 1 + 5 + 5 + 5;
  case kSurfaceFormat5_5_5_1:
    return 5 + 5 + 5 + 1;
  case kSurfaceFormat4_4_4_4:
    return 4 + 4 + 4 + 4;
  case kSurfaceFormat8_24:
    return 8 + 24;
  case kSurfaceFormat24_8:
    return 24 + 8;
  case kSurfaceFormatX24_8_32:
    return 24 + 8 + 32;
  case kSurfaceFormatGB_GR:
    return 2 + 2;
  case kSurfaceFormatBG_RG:
    return 0;
  case kSurfaceFormat5_9_9_9:
    return 5 + 9 + 9 + 9;
  case kSurfaceFormatBc1:
    return 8;
  case kSurfaceFormatBc2:
    return 8;
  case kSurfaceFormatBc3:
    return 8;
  case kSurfaceFormatBc4:
    return 8;
  case kSurfaceFormatBc5:
    return 8;
  case kSurfaceFormatBc6:
    return 8;
  case kSurfaceFormatBc7:
    return 8;
  case kSurfaceFormatFmask8_S2_F1:
    return 0;
  case kSurfaceFormatFmask8_S4_F1:
    return 0;
  case kSurfaceFormatFmask8_S8_F1:
    return 0;
  case kSurfaceFormatFmask8_S2_F2:
    return 0;
  case kSurfaceFormatFmask8_S4_F2:
    return 0;
  case kSurfaceFormatFmask8_S4_F4:
    return 0;
  case kSurfaceFormatFmask16_S16_F1:
    return 0;
  case kSurfaceFormatFmask16_S8_F2:
    return 0;
  case kSurfaceFormatFmask32_S16_F2:
    return 0;
  case kSurfaceFormatFmask32_S8_F4:
    return 0;
  case kSurfaceFormatFmask32_S8_F8:
    return 0;
  case kSurfaceFormatFmask64_S16_F4:
    return 0;
  case kSurfaceFormatFmask64_S16_F8:
    return 0;
  case kSurfaceFormat4_4:
    return 4 + 4;
  case kSurfaceFormat6_5_5:
    return 6 + 5 + 5;
  case kSurfaceFormat1:
    return 1;
  case kSurfaceFormat1Reversed:
    return 0;
  }

  return 0;
}

static VkFormat surfaceFormatToVkFormat(SurfaceFormat surface,
                                        TextureChannelType channel) {
  switch (surface) {
  case kSurfaceFormat8: {
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_R8_UNORM;
    case kTextureChannelTypeSNorm:
      return VK_FORMAT_R8_SNORM;
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R8_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R8_SINT;
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_R8_SRGB;
    default:
      break;
    }

    break;
  }
  case kSurfaceFormat32:
    switch (channel) {
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R32_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R32_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R32_SFLOAT;
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_R32_UINT; // FIXME
    default:
      break;
    }
    break;

  case kSurfaceFormat8_8:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_R8G8_UNORM;
    case kTextureChannelTypeSNorm:
      return VK_FORMAT_R8G8_SNORM;
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R8G8_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R8G8_SINT;
    default:
      break;
    }
    break;

  case kSurfaceFormat5_6_5:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_R5G6B5_UNORM_PACK16;

    default:
      break;
    }
    break;

  case kSurfaceFormat16_16:
    switch (channel) {
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R16G16_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R16G16_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R16G16_SFLOAT;
    default:
      break;
    }
    break;

  case kSurfaceFormat32_32:
    switch (channel) {
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R32G32_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R32G32_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R32G32_SFLOAT;
    default:
      break;
    }
    break;

  case kSurfaceFormat16_16_16_16:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case kTextureChannelTypeSNorm:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case kTextureChannelTypeUScaled:
      return VK_FORMAT_R16G16B16A16_USCALED;
    case kTextureChannelTypeSScaled:
      return VK_FORMAT_R16G16B16A16_SSCALED;
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R16G16B16A16_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R16G16B16A16_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_R16G16B16A16_UNORM; // FIXME: wrong

    default:
      break;
    }
    break;

  case kSurfaceFormat32_32_32:
    switch (channel) {
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R32G32B32_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R32G32B32_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R32G32B32_SFLOAT;
    default:
      break;
    }
    break;
  case kSurfaceFormat32_32_32_32:
    switch (channel) {
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R32G32B32A32_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R32G32B32A32_SINT;
    case kTextureChannelTypeFloat:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      break;
    }
    break;

  case kSurfaceFormat24_8:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_D32_SFLOAT_S8_UINT; // HACK for amdgpu

    default:
      break;
    }

    break;

  case kSurfaceFormat8_8_8_8:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case kTextureChannelTypeSNorm:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case kTextureChannelTypeUScaled:
      return VK_FORMAT_R8G8B8A8_USCALED;
    case kTextureChannelTypeSScaled:
      return VK_FORMAT_R8G8B8A8_SSCALED;
    case kTextureChannelTypeUInt:
      return VK_FORMAT_R8G8B8A8_UINT;
    case kTextureChannelTypeSInt:
      return VK_FORMAT_R8G8B8A8_SINT;
    // case kTextureChannelTypeSNormNoZero:
    //   return VK_FORMAT_R8G8B8A8_SNORM;
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_R8G8B8A8_SRGB;
      // case kTextureChannelTypeUBNorm:
      //   return VK_FORMAT_R8G8B8A8_UNORM;
      // case kTextureChannelTypeUBNormNoZero:
      //   return VK_FORMAT_R8G8B8A8_UNORM;
      // case kTextureChannelTypeUBInt:
      //   return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
      // case kTextureChannelTypeUBScaled:
      //   return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;

    default:
      break;
    }
    break;

  case kSurfaceFormatBc1:
    switch (channel) {
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    default:
      break;
    }

  case kSurfaceFormatBc3:
    switch (channel) {
    case kTextureChannelTypeSrgb:
      return VK_FORMAT_BC3_SRGB_BLOCK;
    default:
      break;
    }

  case kSurfaceFormatBc4:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_BC4_UNORM_BLOCK;

    case kTextureChannelTypeSNorm:
      return VK_FORMAT_BC4_SNORM_BLOCK;

    default:
      break;
    }
  case kSurfaceFormatBc5:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_BC5_UNORM_BLOCK;

    case kTextureChannelTypeSNorm:
      return VK_FORMAT_BC5_SNORM_BLOCK;

    default:
      break;
    }

  case kSurfaceFormatBc7:
    switch (channel) {
    case kTextureChannelTypeUNorm:
      return VK_FORMAT_BC7_UNORM_BLOCK;

    case kTextureChannelTypeSrgb:
      return VK_FORMAT_BC7_SRGB_BLOCK;

    default:
      break;
    }

  default:
    break;
  }

  util::unreachable("unimplemented surface format. %x.%x\n", (int)surface,
                    (int)channel);
}

static VkPrimitiveTopology getVkPrimitiveType(PrimitiveType type) {
  switch (type) {
  case kPrimitiveTypePointList:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case kPrimitiveTypeLineList:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case kPrimitiveTypeLineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case kPrimitiveTypeTriList:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case kPrimitiveTypeTriFan:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  case kPrimitiveTypeTriStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  case kPrimitiveTypePatch:
    return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  case kPrimitiveTypeLineListAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
  case kPrimitiveTypeLineStripAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
  case kPrimitiveTypeTriListAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
  case kPrimitiveTypeTriStripAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
  case kPrimitiveTypeLineLoop:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; // FIXME

  case kPrimitiveTypeRectList:
  case kPrimitiveTypeQuadList:
  case kPrimitiveTypeQuadStrip:
  case kPrimitiveTypePolygon:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  default:
    util::unreachable();
  }
}

static std::pair<std::uint64_t, std::uint64_t>
quadListPrimConverter(std::uint64_t index) {
  static constexpr int indecies[] = {0, 1, 2, 2, 3, 0};
  return {index, index / 6 + indecies[index % 6]};
}

static std::pair<std::uint64_t, std::uint64_t>
quadStripPrimConverter(std::uint64_t index) {
  static constexpr int indecies[] = {0, 1, 3, 0, 3, 2};
  return {index, (index / 6) * 4 + indecies[index % 6]};
}

using ConverterFn =
    std::pair<std::uint64_t, std::uint64_t>(std::uint64_t index);

static ConverterFn *getPrimConverterFn(PrimitiveType primType,
                                       std::uint32_t *count) {
  switch (primType) {
  case kPrimitiveTypeQuadList:
    *count = *count / 4 * 6;
    return quadListPrimConverter;

  case kPrimitiveTypeQuadStrip:
    *count = *count / 4 * 6;
    return quadStripPrimConverter;

  default:
    util::unreachable();
  }
}

static bool isPrimRequiresConversion(PrimitiveType primType) {
  switch (primType) {
  case kPrimitiveTypePointList:
  case kPrimitiveTypeLineList:
  case kPrimitiveTypeLineStrip:
  case kPrimitiveTypeTriList:
  case kPrimitiveTypeTriFan:
  case kPrimitiveTypeTriStrip:
  case kPrimitiveTypePatch:
  case kPrimitiveTypeLineListAdjacency:
  case kPrimitiveTypeLineStripAdjacency:
  case kPrimitiveTypeTriListAdjacency:
  case kPrimitiveTypeTriStripAdjacency:
    return false;
  case kPrimitiveTypeLineLoop: // FIXME
    util::unreachable();
    return false;

  case kPrimitiveTypeRectList:
    return false; // handled by geometry shader

  case kPrimitiveTypeQuadList:
  case kPrimitiveTypeQuadStrip:
  case kPrimitiveTypePolygon:
    return true;

  default:
    util::unreachable();
  }
}

static bool validateSpirv(const std::vector<uint32_t> &bin) {
  spv_target_env target_env = SPV_ENV_VULKAN_1_3;
  spv_context spvContext = spvContextCreate(target_env);
  spv_diagnostic diagnostic = nullptr;
  spv_const_binary_t binary = {bin.data(), bin.size()};
  spv_result_t error = spvValidate(spvContext, &binary, &diagnostic);
  if (error != 0)
    spvDiagnosticPrint(diagnostic);
  spvDiagnosticDestroy(diagnostic);
  spvContextDestroy(spvContext);
  return error == 0;
}

static void printSpirv(const std::vector<uint32_t> &bin) {
  spv_target_env target_env = SPV_ENV_VULKAN_1_3;
  spv_context spvContext = spvContextCreate(target_env);
  spv_diagnostic diagnostic = nullptr;

  spv_result_t error = spvBinaryToText(
      spvContext, bin.data(), bin.size(),
      SPV_BINARY_TO_TEXT_OPTION_PRINT | // SPV_BINARY_TO_TEXT_OPTION_COLOR |
          SPV_BINARY_TO_TEXT_OPTION_FRIENDLY_NAMES |
          SPV_BINARY_TO_TEXT_OPTION_COMMENT | SPV_BINARY_TO_TEXT_OPTION_INDENT,
      nullptr, &diagnostic);

  if (error != 0) {
    spvDiagnosticPrint(diagnostic);
  }

  spvDiagnosticDestroy(diagnostic);
  spvContextDestroy(spvContext);

  if (error != 0) {
    return;
  }

  spirv_cross::CompilerGLSL glsl(bin);
  spirv_cross::CompilerGLSL::Options options;
  options.version = 460;
  options.es = false;
  options.vulkan_semantics = true;
  glsl.set_common_options(options);
  std::printf("%s\n", glsl.compile().c_str());
}

static std::optional<std::vector<uint32_t>>
optimizeSpirv(std::span<const std::uint32_t> spirv) {
  spvtools::Optimizer optimizer(SPV_ENV_VULKAN_1_3);
  optimizer.RegisterPerformancePasses();
  optimizer.RegisterPass(spvtools::CreateSimplificationPass());

  std::vector<uint32_t> result;
  if (optimizer.Run(spirv.data(), spirv.size(), &result)) {
    return result;
  }

  util::unreachable();
  return {};
}

static VkShaderStageFlagBits shaderStageToVk(amdgpu::shader::Stage stage) {
  switch (stage) {
  case amdgpu::shader::Stage::None:
    break;
  case amdgpu::shader::Stage::Fragment:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case amdgpu::shader::Stage::Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case amdgpu::shader::Stage::Geometry:
    return VK_SHADER_STAGE_GEOMETRY_BIT;
  case amdgpu::shader::Stage::Compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  }

  return VK_SHADER_STAGE_ALL;
}

static vk::MemoryResource hostVisibleMemory;
static vk::MemoryResource deviceLocalMemory;

static vk::MemoryResource &getHostVisibleMemory() {
  if (!hostVisibleMemory) {
    hostVisibleMemory.initHostVisible(1024 * 1024 * 512);
  }

  return hostVisibleMemory;
}

static vk::MemoryResource &getDeviceLocalMemory() {
  if (!deviceLocalMemory) {
    deviceLocalMemory.initDeviceLocal(1024 * 1024 * 512);
  }

  return deviceLocalMemory;
}

static void saveImage(const char *name, vk::Image2D &image,
                      VkImageLayout layout) {
  auto imageRef = vk::ImageRef(image);
  vk::Image2D transferImage(imageRef.getWidth(), imageRef.getHeight(),
                            VK_FORMAT_R8G8B8A8_UNORM,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL |
                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  auto transferImageMemory =
      vk::DeviceMemory::Allocate(transferImage.getMemoryRequirements(),
                                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  transferImage.bindMemory(vk::DeviceMemoryRef{
      .deviceMemory = transferImageMemory.getHandle(),
      .offset = 0,
      .size = transferImageMemory.getSize(),
  });

  auto transferImageRef = vk::ImageRef(transferImage);

  auto imageSize = transferImageRef.getMemoryRequirements().size;

  auto transferBuffer = vk::Buffer::Allocate(
      getHostVisibleMemory(), imageSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

  getGraphicsQueueScheduler()
      .enqueue([&](VkCommandBuffer commandBuffer) {
        VkImageBlit region{
            .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .mipLevel = 0,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
            .srcOffsets = {{},
                           {static_cast<int32_t>(imageRef.getWidth()),
                            static_cast<int32_t>(imageRef.getHeight()), 1}},
            .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                               .mipLevel = 0,
                               .baseArrayLayer = 0,
                               .layerCount = 1},
            .dstOffsets = {{},
                           {static_cast<int32_t>(imageRef.getWidth()),
                            static_cast<int32_t>(imageRef.getHeight()), 1}},
        };

        imageRef.transitionLayout(commandBuffer,
                                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        transferImageRef.transitionLayout(commandBuffer,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        vkCmdBlitImage(
            commandBuffer, imageRef.getHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, transferImage.getHandle(),
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_LINEAR);

        transferImageRef.transitionLayout(commandBuffer,
                                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        transferImageRef.writeToBuffer(commandBuffer,
                                       transferBuffer.getHandle(),
                                       VK_IMAGE_ASPECT_COLOR_BIT);

        imageRef.transitionLayout(commandBuffer, layout);
      })
      ->wait();

  std::ofstream file(name, std::ios::out | std::ios::binary);
  auto data = (unsigned int *)transferBuffer.getData();

  file << "P6\n"
       << transferImageRef.getWidth() << "\n"
       << transferImageRef.getHeight() << "\n"
       << 255 << "\n";

  for (uint32_t y = 0; y < transferImageRef.getHeight(); y++) {
    for (uint32_t x = 0; x < transferImageRef.getWidth(); x++) {
      file.write((char *)data, 3);
      data++;
    }
  }
  file.close();
}

struct BufferRef {
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceSize offset = 0;
  VkDeviceSize size = 0;
};

static constexpr bool isAligned(std::uint64_t offset, std::uint64_t alignment) {
  return (offset & (alignment - 1)) == 0;
}

static void
fillStageBindings(std::vector<VkDescriptorSetLayoutBinding> &bindings,
                  shader::Stage stage) {
  for (std::size_t i = 0; i < shader::UniformBindings::kBufferSlots; ++i) {
    auto binding = shader::UniformBindings::getBufferBinding(stage, i);
    bindings[binding] = VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = shaderStageToVk(stage),
        .pImmutableSamplers = nullptr};
  }

  for (std::size_t i = 0; i < shader::UniformBindings::kImageSlots; ++i) {
    auto binding = shader::UniformBindings::getImageBinding(stage, i);
    bindings[binding] = VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = shaderStageToVk(stage),
        .pImmutableSamplers = nullptr};
  }

  for (std::size_t i = 0; i < shader::UniformBindings::kSamplerSlots; ++i) {
    auto binding = shader::UniformBindings::getSamplerBinding(stage, i);
    bindings[binding] = VkDescriptorSetLayoutBinding{
        .binding = binding,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = shaderStageToVk(stage),
        .pImmutableSamplers = nullptr};
  }
}
static std::pair<VkDescriptorSetLayout, VkPipelineLayout> getGraphicsLayout() {
  static std::pair<VkDescriptorSetLayout, VkPipelineLayout> result{};

  if (result.first != VK_NULL_HANDLE) {
    return result;
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings(
      shader::UniformBindings::kStageSize * 2);

  for (auto stage : {shader::Stage::Vertex, shader::Stage::Fragment}) {
    fillStageBindings(bindings, stage);
  }

  VkDescriptorSetLayoutCreateInfo descLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  Verify() << vkCreateDescriptorSetLayout(vk::g_vkDevice, &descLayoutInfo,
                                          vk::g_vkAllocator, &result.first);

  VkPipelineLayoutCreateInfo piplineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &result.first,
  };

  Verify() << vkCreatePipelineLayout(vk::g_vkDevice, &piplineLayoutInfo,
                                     vk::g_vkAllocator, &result.second);

  return result;
}

static std::pair<VkDescriptorSetLayout, VkPipelineLayout> getComputeLayout() {
  static std::pair<VkDescriptorSetLayout, VkPipelineLayout> result{};

  if (result.first != VK_NULL_HANDLE) {
    return result;
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings(
      shader::UniformBindings::kStageSize);

  fillStageBindings(bindings, shader::Stage::Compute);

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(bindings.size()),
      .pBindings = bindings.data(),
  };

  Verify() << vkCreateDescriptorSetLayout(vk::g_vkDevice, &layoutInfo, nullptr,
                                          &result.first);

  VkPipelineLayoutCreateInfo piplineLayoutInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 1,
      .pSetLayouts = &result.first,
  };

  Verify() << vkCreatePipelineLayout(vk::g_vkDevice, &piplineLayoutInfo,
                                     vk::g_vkAllocator, &result.second);
  return result;
}

struct ShaderKey {
  std::uint64_t address;
  std::uint16_t dimX;
  std::uint16_t dimY;
  std::uint16_t dimZ;
  shader::Stage stage;
  std::uint8_t userSgprCount;
  std::uint32_t userSgprs[16];

  auto operator<=>(const ShaderKey &other) const {
    auto result = address <=> other.address;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    result = dimX <=> other.dimX;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    result = dimY <=> other.dimY;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    result = dimZ <=> other.dimZ;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    result = stage <=> other.stage;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    result = userSgprCount <=> other.userSgprCount;
    if (result != std::strong_ordering::equal) {
      return result;
    }

    for (std::size_t i = 0; i < std::size(userSgprs); ++i) {
      if (i >= userSgprCount) {
        break;
      }

      result = userSgprs[i] <=> other.userSgprs[i];
      if (result != std::strong_ordering::equal) {
        return result;
      }
    }

    return result;
  }
};

struct CachedShader {
  std::map<std::uint64_t, std::vector<char>> cachedData;
  shader::Shader info;
  VkShaderEXT shader;

  ~CachedShader() {
    _vkDestroyShaderEXT(vk::g_vkDevice, shader, vk::g_vkAllocator);
  }
};

struct CacheOverlayBase;
struct CacheBufferOverlay;
struct CacheImageOverlay;

struct CacheSyncEntry {
  std::uint64_t tag;
  Ref<CacheOverlayBase> overlay;

  auto operator<=>(const CacheSyncEntry &) const = default;
};

struct CacheOverlayBase {
  std::mutex mtx;
  util::MemoryAreaTable<> loadMemorySyncTable;
  util::MemoryTableWithPayload<std::uint64_t> syncState;

  std::atomic<unsigned> refs{0};
  virtual ~CacheOverlayBase() = default;

  void incRef() { refs.fetch_add(1, std::memory_order::relaxed); }
  void decRef() {
    if (refs.fetch_sub(1, std::memory_order::relaxed) == 1) {
      delete this;
    }
  }

  std::optional<decltype(syncState)::AreaInfo> getSyncTag(std::uint64_t address,
                                                          std::uint64_t size) {
    std::lock_guard lock(mtx);
    auto it = syncState.queryArea(address);
    if (it == syncState.end()) {
      return {};
    }

    auto state = *it;

    if (state.endAddress < address + size || state.beginAddress > address) {
      // has no single sync state
      return {};
    }

    return state;
  }

  bool isInSync(util::MemoryTableWithPayload<CacheSyncEntry> &table,
                std::mutex &tableMutex, std::uint64_t address,
                std::uint64_t size) {
    auto optSyncTag = getSyncTag(address, size);
    if (!optSyncTag) {
      return false;
    }

    auto syncTag = *optSyncTag;

    std::lock_guard lock(tableMutex);
    auto tableArea = table.queryArea(address);

    if (tableArea == table.end()) {
      return false;
    }

    auto tableTag = *tableArea;

    if (tableTag.beginAddress > address ||
        tableTag.endAddress < address + size) {
      return false;
    }

    return tableTag.payload.tag == syncTag.payload;
  }

  virtual void writeBuffer(Ref<CacheBufferOverlay> sourceBuffer,
                           std::uint64_t address, std::uint64_t size) {
    std::printf("cache: unimplemented buffer write to %lx-%lx\n", address,
                address + size);
  }

  virtual void readBuffer(Ref<CacheBufferOverlay> targetBuffer,
                          std::uint64_t address, std::uint64_t size) {
    std::printf("cache: unimplemented buffer read from %lx-%lx\n", address,
                address + size);
  }
};

struct CacheBufferOverlay : CacheOverlayBase {
  vk::Buffer buffer;
  std::uint64_t bufferAddress;

  void read(util::MemoryTableWithPayload<CacheSyncEntry> &table,
            std::mutex &tableMtx, std::uint64_t address,
            std::uint32_t elementCount, std::uint32_t stride,
            std::uint32_t elementSize, bool cache) {
    std::lock_guard lock(mtx);
    auto size = stride == 0
                    ? static_cast<std::uint64_t>(elementCount) * elementSize
                    : static_cast<std::uint64_t>(elementCount) * stride;
    auto doRead = [&](std::uint64_t address, std::uint64_t size,
                      std::uint64_t tag, Ref<CacheOverlayBase> overlay) {
      overlay->readBuffer(this, address, size);
      syncState.map(address, address + size, tag);
    };

    auto getAreaInfo = [&](std::uint64_t address) {
      std::lock_guard lock(tableMtx);
      auto it = table.queryArea(address);
      if (it == table.end()) {
        util::unreachable();
      }
      return *it;
    };

    while (size > 0) {
      auto state = getAreaInfo(address);

      assert(state.endAddress > address);
      auto origAreaSize = std::min(state.endAddress - address, size);
      auto areaSize = origAreaSize;

      if (!cache) {
        state.payload.overlay->readBuffer(this, address, areaSize);
        size -= areaSize;
        address += areaSize;
        continue;
      }

      while (areaSize > 0) {
        auto blockSyncStateIt = syncState.queryArea(address);

        if (blockSyncStateIt == syncState.end()) {
          doRead(address, areaSize, state.payload.tag, state.payload.overlay);
          address += areaSize;
          break;
        }

        auto blockSyncState = *blockSyncStateIt;
        auto blockSize =
            std::min(blockSyncState.endAddress - address, areaSize);

        if (blockSyncState.payload != state.payload.tag) {
          doRead(address, areaSize, state.payload.tag, state.payload.overlay);
        }

        areaSize -= blockSize;
        address += blockSize;
      }

      size -= origAreaSize;
    }
  }

  void readBuffer(Ref<CacheBufferOverlay> targetBuffer, std::uint64_t address,
                  std::uint64_t size) override {
    auto targetOffset = address - targetBuffer->bufferAddress;
    auto sourceOffset = address - bufferAddress;
    std::memcpy((char *)targetBuffer->buffer.getData() + targetOffset,
                (char *)buffer.getData() + sourceOffset, size);
  }
};

struct CacheImageOverlay : CacheOverlayBase {
  vk::Image2D image;

  vk::Buffer trasferBuffer; // TODO: remove
  VkImageView view = VK_NULL_HANDLE;
  std::uint32_t dataWidth;
  std::uint32_t dataPitch;
  std::uint32_t dataHeight;
  std::uint8_t bpp;
  TileMode tileMode;
  VkImageAspectFlags aspect;
  Ref<CacheBufferOverlay> usedBuffer;

  ~CacheImageOverlay() {
    if (view != VK_NULL_HANDLE) {
      vkDestroyImageView(vk::g_vkDevice, view, vk::g_vkAllocator);
    }
  }

  void read(VkCommandBuffer commandBuffer, std::uint64_t address,
            Ref<CacheBufferOverlay> srcBuffer) {
    if (usedBuffer != nullptr) {
      util::unreachable();
    }

    usedBuffer = srcBuffer;
    auto offset = address - srcBuffer->bufferAddress;

    vk::ImageRef imageRef(image);
    auto size = dataHeight * dataPitch * bpp;

    if (dataPitch == dataWidth &&
        (tileMode == kTileModeDisplay_2dThin ||
         tileMode == kTileModeDisplay_LinearAligned)) {
      imageRef.transitionLayout(commandBuffer,
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      VkBufferImageCopy region{
          .bufferOffset = offset,
          .bufferRowLength = 0,
          .bufferImageHeight = 0,
          .imageSubresource =
              {
                  .aspectMask = aspect,
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
          .imageOffset = {0, 0, 0},
          .imageExtent = {imageRef.getWidth(), imageRef.getHeight(), 1},
      };

      vkCmdCopyBufferToImage(commandBuffer, srcBuffer->buffer.getHandle(),
                             image.getHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
      auto tag = *srcBuffer->getSyncTag(address, size);
      std::lock_guard lock(mtx);
      syncState.map(address, address + size, tag.payload);
      return;
    }

    auto bufferData = (char *)srcBuffer->buffer.getData() + offset;

    trasferBuffer.readFromImage(bufferData, bpp, tileMode, dataWidth,
                                dataHeight, 1, dataPitch);

    imageRef.readFromBuffer(commandBuffer, trasferBuffer.getHandle(), aspect);
    auto tag = *srcBuffer->getSyncTag(address, size);
    std::lock_guard lock(mtx);
    syncState.map(address, address + size, tag.payload);
  }

  void readBuffer(Ref<CacheBufferOverlay> targetBuffer, std::uint64_t address,
                  std::uint64_t size) override {
    auto offset = address - targetBuffer->bufferAddress;

    if (dataPitch == dataWidth &&
        (tileMode == kTileModeDisplay_2dThin ||
         tileMode == kTileModeDisplay_LinearAligned)) {
      auto uploadTaskCtl = getGraphicsQueueScheduler().enqueue(
          [=, self = Ref(this)](VkCommandBuffer commandBuffer) {
            vk::ImageRef imageRef(self->image);

            imageRef.transitionLayout(commandBuffer,
                                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            VkBufferImageCopy region{
                .bufferOffset = offset,
                .bufferRowLength = 0,
                .bufferImageHeight = 0,
                .imageSubresource =
                    {
                        .aspectMask = self->aspect,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .imageOffset = {0, 0, 0},
                .imageExtent = {imageRef.getWidth(), imageRef.getHeight(),
                                imageRef.getDepth()},
            };

            vkCmdCopyImageToBuffer(commandBuffer, imageRef.getHandle(),
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   targetBuffer->buffer.getHandle(), 1,
                                   &region);
          });

      uploadTaskCtl->wait();
      return;
    }

    auto uploadTaskCtl = getGraphicsQueueScheduler().enqueue(
        [=, self = Ref(this)](VkCommandBuffer commandBuffer) {
          vk::ImageRef imageRef(self->image);
          imageRef.writeToBuffer(commandBuffer, self->trasferBuffer.getHandle(),
                                 self->aspect);
          return true;
        });

    uploadTaskCtl->wait();

    auto targetData = (char *)targetBuffer->buffer.getData() + offset;
    trasferBuffer.writeAsImageTo(targetData, bpp, tileMode, dataWidth,
                                 dataHeight, 1, dataPitch);
  }
};

struct MemoryOverlay : CacheOverlayBase {
  void readBuffer(Ref<CacheBufferOverlay> targetBuffer, std::uint64_t address,
                  std::uint64_t size) override {
    auto offset = address - targetBuffer->bufferAddress;
    auto targetData = (char *)targetBuffer->buffer.getData() + offset;

    std::memcpy(targetData, g_hostMemory.getPointer(address), size);
  }

  void writeBuffer(Ref<CacheBufferOverlay> sourceBuffer, std::uint64_t address,
                   std::uint64_t size) override {
    auto offset = address - sourceBuffer->bufferAddress;
    auto sourceData = (char *)sourceBuffer->buffer.getData() + offset;

    std::memcpy(g_hostMemory.getPointer(address), sourceData, size);
  }
};

static void notifyPageChanges(std::uint32_t firstPage,
                              std::uint32_t pageCount) {
  std::uint64_t command =
      (static_cast<std::uint64_t>(pageCount - 1) << 32) | firstPage;

  while (true) {
    for (std::size_t i = 0; i < std::size(g_bridge->cacheCommands); ++i) {
      std::uint64_t expCommand = 0;
      if (g_bridge->cacheCommands[i].compare_exchange_strong(
              expCommand, command, std::memory_order::acquire,
              std::memory_order::relaxed)) {
        return;
      }
    }
  }
}

static void modifyWatchFlags(std::uint64_t address, std::uint64_t size,
                             std::uint8_t addFlags, std::uint8_t removeFlags) {
  auto firstPage = address / bridge::kHostPageSize;
  auto lastPage =
      (address + size + bridge::kHostPageSize - 1) / bridge::kHostPageSize;
  bool hasChanges = false;
  for (auto page = firstPage; page < lastPage; ++page) {
    auto prevValue =
        g_bridge->cachePages[page].load(std::memory_order::relaxed);
    auto newValue = (prevValue & ~removeFlags) | addFlags;

    if (newValue == prevValue) {
      continue;
    }

    while (!g_bridge->cachePages[page].compare_exchange_weak(
        prevValue, newValue, std::memory_order::relaxed)) {
      newValue = (prevValue & ~removeFlags) | addFlags;
    }

    if (newValue != prevValue) {
      hasChanges = true;
    }
  }

  if (hasChanges) {
    notifyPageChanges(firstPage, lastPage - firstPage);
  }
}

static void watchWrites(std::uint64_t address, std::uint64_t size) {
  modifyWatchFlags(address, size, bridge::kPageWriteWatch,
                   bridge::kPageInvalidated);
}
static void lockReadWrite(std::uint64_t address, std::uint64_t size) {
  modifyWatchFlags(address, size, bridge::kPageReadWriteLock,
                   bridge::kPageInvalidated);
}
static void unlockReadWrite(std::uint64_t address, std::uint64_t size) {
  modifyWatchFlags(address, size, bridge::kPageWriteWatch,
                   bridge::kPageReadWriteLock);
}

struct CacheLine {
  std::uint64_t areaAddress;
  std::uint64_t areaSize;

  Ref<CacheOverlayBase> memoryOverlay;

  // TODO: flat image storage
  struct ImageKey {
    std::uint64_t address;
    SurfaceFormat dataFormat;
    TextureChannelType channelType;
    TileMode tileMode;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
    std::uint32_t pitch;

    auto operator<=>(const ImageKey &other) const = default;
  };

  std::mutex hostSyncMtx;
  util::MemoryTableWithPayload<CacheSyncEntry> hostSyncTable;

  std::mutex bufferTableMtx;
  std::unordered_map<std::uint64_t,
                     util::MemoryTableWithPayload<Ref<CacheBufferOverlay>>>
      bufferTable;

  std::mutex imageTableMtx;
  std::map<ImageKey, Ref<CacheImageOverlay>> imageTable;

  std::mutex writeBackTableMtx;
  util::MemoryTableWithPayload<Ref<AsyncTaskCtl>> writeBackTable;

  CacheLine(std::uint64_t areaAddress, std::uint64_t areaSize)
      : areaAddress(areaAddress), areaSize(areaSize) {
    memoryOverlay = new MemoryOverlay();
    hostSyncTable.map(areaAddress, areaAddress + areaSize, {1, memoryOverlay});
  }

  void markHostInvalidated(std::uint64_t tag, std::uint64_t address,
                           std::uint64_t size) {
    std::lock_guard lock(hostSyncMtx);
    hostSyncTable.map(address, address + size, {tag, memoryOverlay});
  }

  bool handleHostInvalidations(std::uint64_t tag, std::uint64_t address,
                               std::uint64_t size) {
    auto firstPage = address / bridge::kHostPageSize;
    auto lastPage =
        (address + size + bridge::kHostPageSize - 1) / bridge::kHostPageSize;

    bool hasInvalidations = false;

    for (auto page = firstPage; page < lastPage; ++page) {
      auto prevValue =
          g_bridge->cachePages[page].load(std::memory_order::relaxed);

      if (~prevValue & bridge::kPageInvalidated) {
        continue;
      }

      while (!g_bridge->cachePages[page].compare_exchange_weak(
          prevValue, prevValue & ~bridge::kPageInvalidated,
          std::memory_order::relaxed)) {
      }

      markHostInvalidated(tag, page * bridge::kHostPageSize,
                          bridge::kHostPageSize);
      hasInvalidations = true;
    }

    return hasInvalidations;
  }

  void trackCacheRead(std::uint64_t address, std::uint64_t size) {
    watchWrites(address, size);
  }

  void setWriteBackTask(std::uint64_t address, std::uint64_t size,
                        Ref<AsyncTaskCtl> task) {
    std::lock_guard lock(writeBackTableMtx);
    auto it = writeBackTable.queryArea(address);

    while (it != writeBackTable.end()) {
      auto taskInfo = *it;

      if (taskInfo.beginAddress >= address + size) {
        break;
      }

      if (taskInfo.beginAddress >= address &&
          taskInfo.endAddress <= address + size) {
        if (taskInfo.payload != nullptr) {
          // another task with smaller range already in progress, we can cancel
          // it
          std::printf("prev upload task cancelation\n");
          taskInfo.payload->cancel();
        }
      }

      ++it;
    }

    g_scheduler.enqueue(task);
    writeBackTable.map(address, address + size, std::move(task));
  }

  void trackCacheWrite(std::uint64_t address, std::uint64_t size,
                       std::uint64_t tag, Ref<CacheOverlayBase> entry) {
    lockReadWrite(address, size);

    auto writeBackTask = createTask([=, this](const AsyncTaskCtl &ctl) {
      if (ctl.isCanceled()) {
        return false;
      }

      auto buffer = getBufferInternal(address, size);

      if (ctl.isCanceled()) {
        return false;
      }

      buffer->read(hostSyncTable, hostSyncMtx, address, size, 0, 1, true);

      if (ctl.isCanceled()) {
        return false;
      }

      memoryOverlay->writeBuffer(buffer, address, size);
      unlockReadWrite(address, size);

      std::lock_guard lock(writeBackTableMtx);
      writeBackTable.map(address, address + size, nullptr);
      return true;
    });

    setWriteBackTask(address, size, writeBackTask);
    std::lock_guard lock(hostSyncMtx);
    hostSyncTable.map(address, address + size, {.tag = tag, .overlay = entry});
    entry->syncState.map(address, address + size, tag);
  }

  Ref<CacheBufferOverlay>
  getBuffer(std::uint64_t writeTag, TaskSet &readTaskSet, TaskSet &writeTaskSet,
            std::uint64_t address, std::uint32_t elementCount,
            std::uint32_t stride, std::uint32_t elementSize,
            shader::AccessOp access) {
    auto size = stride == 0
                    ? static_cast<std::uint64_t>(elementCount) * elementSize
                    : static_cast<std::uint64_t>(elementCount) * stride;

    auto result = getBufferInternal(address, size);
    bool cacheValue =
        size >= bridge::kHostPageSize ||
        (access & shader::AccessOp::Store) == shader::AccessOp::Store;

    if ((access & shader::AccessOp::Load) == shader::AccessOp::Load) {
      if (!cacheValue || handleHostInvalidations(writeTag - 1, address, size) ||
          !result->isInSync(hostSyncTable, hostSyncMtx, address, size)) {
        g_scheduler.enqueue(readTaskSet, [=, this](const AsyncTaskCtl &) {
          result->read(hostSyncTable, hostSyncMtx, address, elementCount,
                       stride, elementSize, cacheValue);
          if (cacheValue) {
            // std::printf("caching %lx-%lx\n", address, size);
            trackCacheRead(address, size);
          }
          return true;
        });
      }
    }

    if ((access & shader::AccessOp::Store) == shader::AccessOp::Store) {
      auto writeTask = createTask([=, this](const AsyncTaskCtl &) {
        trackCacheWrite(address, size, writeTag, result);
        return true;
      });

      writeTaskSet.append(writeTask);
    }

    return result;
  }

  Ref<CacheImageOverlay>
  getImage(std::uint64_t writeTag, TaskSet &readTaskSet, TaskSet &writeTaskSet,
           VkCommandBuffer readCommandBuffer, std::uint64_t address,
           SurfaceFormat dataFormat, TextureChannelType channelType,
           TileMode tileMode, std::uint32_t width, std::uint32_t height,
           std::uint32_t depth, std::uint32_t pitch, shader::AccessOp access,
           VkImageLayout layout, bool isColor = true) {
    auto result = getImageInternal(address, dataFormat, channelType, tileMode,
                                   width, height, depth, pitch, isColor);

    auto size = result->bpp * result->dataHeight * result->dataPitch;

    auto cacheBeginAddress =
        (address + bridge::kHostPageSize - 1) & ~(bridge::kHostPageSize - 1);
    auto cacheEndAddress = (address + size) & ~(bridge::kHostPageSize - 1);

    if (cacheBeginAddress == cacheEndAddress) {
      cacheBeginAddress = address;
      cacheEndAddress = address + size;
    }

    auto cacheSize = cacheEndAddress - cacheBeginAddress;

    if ((access & shader::AccessOp::Load) == shader::AccessOp::Load) {
      // TODO
      handleHostInvalidations(writeTag - 1, cacheBeginAddress, cacheSize);

      if (!result->isInSync(hostSyncTable, hostSyncMtx, address, size)) {
        auto buffer = getBufferInternal(address, size);

        if (!buffer->isInSync(hostSyncTable, hostSyncMtx, address, size)) {
          buffer->read(hostSyncTable, hostSyncMtx, address, size, 0, 1, true);
        }

        result->read(readCommandBuffer, address, buffer);
        trackCacheRead(cacheBeginAddress, cacheSize);
      }
    }

    vk::ImageRef(result->image).transitionLayout(readCommandBuffer, layout);

    auto writeTask = createTask([=, this](const AsyncTaskCtl &) {
      result->usedBuffer = nullptr;

      if ((access & shader::AccessOp::Store) == shader::AccessOp::Store) {
        trackCacheWrite(address, size, writeTag, result);
      }

      if (false && isColor) {
        static unsigned index = 0;
        saveImage(("images/" + std::to_string(index++) + ".ppm").c_str(),
                  result->image, layout);
      }
      return true;
    });

    writeTaskSet.append(writeTask);

    return result;
  }

private:
  Ref<CacheBufferOverlay> getBufferInternal(std::uint64_t address,
                                            std::uint64_t size) {
    auto alignment =
        vk::g_physicalDeviceProperties.limits.minStorageBufferOffsetAlignment;

    if (address + size > areaAddress + areaSize) {
      util::unreachable();
    }

    auto offset = (address - areaAddress) & (alignment - 1);

    std::lock_guard lock(bufferTableMtx);
    auto &table = bufferTable[offset];

    if (auto it = table.queryArea(address); it != table.end()) {
      auto bufferInfo = *it;

      if (bufferInfo.beginAddress <= address &&
          bufferInfo.endAddress >= address + size) {
        if (!isAligned(address - bufferInfo.beginAddress, alignment)) {
          util::unreachable();
        }

        return bufferInfo.payload;
      }

      assert(bufferInfo.beginAddress <= address);

      auto endAddress = std::max(bufferInfo.endAddress, address + size);
      address = bufferInfo.beginAddress;

      while (it != table.end()) {
        bufferInfo = *it;
        if (endAddress > bufferInfo.endAddress) {
          auto nextIt = it;
          if (++nextIt != table.end()) {
            auto nextInfo = *nextIt;
            if (nextInfo.beginAddress >= endAddress) {
              break;
            }
            endAddress = nextInfo.endAddress;
          }
        }
        ++it;
      }

      size = endAddress - address;
    }

    auto bufferOverlay = new CacheBufferOverlay();
    bufferOverlay->buffer = vk::Buffer::Allocate(
        getHostVisibleMemory(), size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    bufferOverlay->bufferAddress = address;

    table.map(address, address + size, bufferOverlay);
    return bufferOverlay;
  }

  Ref<CacheImageOverlay>
  getImageInternal(std::uint64_t address, SurfaceFormat dataFormat,
                   TextureChannelType channelType, TileMode tileMode,
                   std::uint32_t width, std::uint32_t height,
                   std::uint32_t depth, std::uint32_t pitch, bool isColor) {
    ImageKey key{
        .address = address,
        .dataFormat = dataFormat,
        .channelType = channelType,
        .tileMode = tileMode,
        .width = width,
        .height = height,
        .depth = depth,
        .pitch = pitch,
    };

    decltype(imageTable)::iterator it;
    {
      std::lock_guard lock(imageTableMtx);

      auto [emplacedIt, inserted] =
          imageTable.try_emplace(key, Ref<CacheImageOverlay>{});

      if (!inserted) {
        return emplacedIt->second;
      }

      it = emplacedIt;
    }

    std::printf(
        "Image cache miss: address: %lx, dataFormat: %u, channelType: %u, "
        "tileMode: %u, width: %u, height: %u, depth: %u, pitch: %u\n",
        address, dataFormat, channelType, tileMode, width, height, depth,
        pitch);

    auto colorFormat = surfaceFormatToVkFormat(dataFormat, channelType);
    auto usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                 VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    bool isCompressed =
        dataFormat == kSurfaceFormatBc1 || dataFormat == kSurfaceFormatBc2 ||
        dataFormat == kSurfaceFormatBc3 || dataFormat == kSurfaceFormatBc4 ||
        dataFormat == kSurfaceFormatBc5 || dataFormat == kSurfaceFormatBc6 ||
        dataFormat == kSurfaceFormatBc7;
    if (!isCompressed) {
      if (isColor) {
        usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
      } else {
        usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
      }
    }

    auto newOverlay = new CacheImageOverlay();

    newOverlay->image = vk::Image2D::Allocate(getDeviceLocalMemory(), width,
                                              height, colorFormat, usage);

    auto bpp = getBitWidthOfSurfaceFormat(dataFormat) / 8;

    std::uint32_t dataWidth = width;
    std::uint32_t dataPitch = pitch;
    std::uint32_t dataHeight = height;

    /*if (dataFormat == kSurfaceFormatBc1) {
      width = (width + 7) / 8;
      height = (height + 7) / 8;
      pitch = (pitch + 7) / 8;
      bpp = 8;
    } else */
    if (isCompressed) {
      dataWidth = (width + 3) / 4;
      dataPitch = (height + 3) / 4;
      dataHeight = (pitch + 3) / 4;
      bpp = 16;
    }

    auto memSize = vk::ImageRef(newOverlay->image).getMemoryRequirements().size;

    newOverlay->trasferBuffer = vk::Buffer::Allocate(
        getHostVisibleMemory(), memSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

    newOverlay->dataWidth = dataWidth;
    newOverlay->dataPitch = dataPitch;
    newOverlay->dataHeight = dataHeight;
    newOverlay->bpp = bpp;
    newOverlay->tileMode = tileMode;

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = newOverlay->image.getHandle(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = colorFormat,
        .components = {}, // TODO
        .subresourceRange =
            {
                .aspectMask = static_cast<VkImageAspectFlags>(
                    isColor ? VK_IMAGE_ASPECT_COLOR_BIT
                            : VK_IMAGE_ASPECT_DEPTH_BIT |
                                  VK_IMAGE_ASPECT_STENCIL_BIT),
                .baseMipLevel = 0, // TODO
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    Verify() << vkCreateImageView(vk::g_vkDevice, &viewInfo, nullptr,
                                  &newOverlay->view);

    newOverlay->aspect =
        isColor ? VK_IMAGE_ASPECT_COLOR_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    it->second = newOverlay;
    return it->second;
  }
};

struct Cache {
  // TODO: use descriptor buffer instead
  VkDescriptorPool graphicsDescriptorPool{};
  VkDescriptorPool computeDescriptorPool{};
  std::vector<VkDescriptorSet> graphicsDecsriptorSets;
  std::vector<VkDescriptorSet> computeDecsriptorSets;

  struct DetachedImageKey {
    SurfaceFormat dataFormat;
    TextureChannelType channelType;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;

    auto operator<=>(const DetachedImageKey &other) const = default;
  };

  // TODO: read S#
  VkSampler sampler{};
  std::map<DetachedImageKey, Ref<CacheImageOverlay>> datachedImages;
  std::map<std::uint64_t, CacheLine, std::greater<>> cacheLines;
  std::atomic<std::uint64_t> nextTag{2};
  std::map<ShaderKey, std::forward_list<CachedShader>> shaders;

  std::mutex mtx;

  void clear() {
    vkDestroyDescriptorPool(vk::g_vkDevice, graphicsDescriptorPool,
                            vk::g_vkAllocator);
    vkDestroyDescriptorPool(vk::g_vkDevice, computeDescriptorPool,
                            vk::g_vkAllocator);
    vkDestroySampler(vk::g_vkDevice, sampler, vk::g_vkAllocator);
    graphicsDescriptorPool = VK_NULL_HANDLE;
    computeDescriptorPool = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;

    graphicsDecsriptorSets.clear();
    computeDecsriptorSets.clear();
    datachedImages.clear();
    cacheLines.clear();
    nextTag = 2;
  }

  void syncLines() {
    std::lock_guard lock(mtx);

    auto areas = std::exchange(memoryAreaTable.invalidated, {});
    auto it = cacheLines.begin();

    if (it == cacheLines.end()) {
      return;
    }

    for (auto area : areas) {
      while (it->first > area) {
        if (++it == cacheLines.end()) {
          return;
        }
      }

      if (it->first == area) {
        it = cacheLines.erase(it);

        if (it == cacheLines.end()) {
          return;
        }
      }
    }
  }

  VkDescriptorSet getComputeDescriptorSet() {
    {
      std::lock_guard lock(mtx);
      if (computeDescriptorPool == nullptr) {
        VkDescriptorPoolSize poolSizes[]{
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = shader::UniformBindings::kImageSlots,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = shader::UniformBindings::kSamplerSlots,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = shader::UniformBindings::kBufferSlots,
            },
        };

        VkDescriptorPoolCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 32,
            .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
            .pPoolSizes = poolSizes,
        };

        Verify() << vkCreateDescriptorPool(
            vk::g_vkDevice, &info, vk::g_vkAllocator, &computeDescriptorPool);
      }

      if (!computeDecsriptorSets.empty()) {
        auto result = computeDecsriptorSets.back();
        computeDecsriptorSets.pop_back();
        return result;
      }
    }

    auto layout = getComputeLayout().first;

    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = computeDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet result;
    Verify() << vkAllocateDescriptorSets(vk::g_vkDevice, &info, &result);
    return result;
  }

  std::uint64_t createTag() { return nextTag.fetch_add(2); }

  VkSampler getSampler() {
    if (sampler != VK_NULL_HANDLE) {
      return sampler;
    }

    std::lock_guard lock(mtx);
    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 1.0,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
    };

    Verify() << vkCreateSampler(vk::g_vkDevice, &samplerInfo, nullptr,
                                &sampler);
    return sampler;
  }

  VkDescriptorSet getGraphicsDescriptorSet() {
    {
      std::lock_guard lock(mtx);
      if (graphicsDescriptorPool == nullptr) {
        VkDescriptorPoolSize poolSizes[]{
            {
                .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = shader::UniformBindings::kBufferSlots * 2,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                .descriptorCount = shader::UniformBindings::kImageSlots * 2,
            },
            {
                .type = VK_DESCRIPTOR_TYPE_SAMPLER,
                .descriptorCount = shader::UniformBindings::kSamplerSlots * 2,
            },
        };

        VkDescriptorPoolCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 32,
            .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
            .pPoolSizes = poolSizes,
        };

        Verify() << vkCreateDescriptorPool(
            vk::g_vkDevice, &info, vk::g_vkAllocator, &graphicsDescriptorPool);
      }

      if (!graphicsDecsriptorSets.empty()) {
        auto result = graphicsDecsriptorSets.back();
        graphicsDecsriptorSets.pop_back();
        return result;
      }
    }

    auto layout = getGraphicsLayout().first;

    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = graphicsDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout,
    };

    VkDescriptorSet result;
    Verify() << vkAllocateDescriptorSets(vk::g_vkDevice, &info, &result);
    return result;
  }

  void releaseGraphicsDescriptorSet(VkDescriptorSet descSet) {
    std::lock_guard lock(mtx);
    graphicsDecsriptorSets.push_back(descSet);
  }

  void releaseComputeDescriptorSet(VkDescriptorSet descSet) {
    std::lock_guard lock(mtx);
    computeDecsriptorSets.push_back(descSet);
  }

  const CachedShader &getShader(TaskSet &taskSet,
                                VkDescriptorSetLayout descriptorSetLayout,
                                shader::Stage stage, std::uint64_t address,
                                std::uint32_t *userSgprs,
                                std::uint8_t userSgprsCount,
                                std::uint16_t dimX = 1, std::uint16_t dimY = 1,
                                std::uint16_t dimZ = 1) {
    ShaderKey key{.address = address,
                  .dimX = dimX,
                  .dimY = dimY,
                  .dimZ = dimZ,
                  .stage = stage,
                  .userSgprCount = userSgprsCount};

    std::memcpy(key.userSgprs, userSgprs,
                userSgprsCount * sizeof(std::uint32_t));

    decltype(shaders)::iterator it;
    CachedShader *entry;
    {
      std::lock_guard lock(mtx);

      auto [emplacedIt, inserted] =
          shaders.try_emplace(key, std::forward_list<CachedShader>{});

      if (!inserted) {
        for (auto &shader : emplacedIt->second) {
          bool isAllSame = true;
          for (auto &[startAddress, bytes] : shader.cachedData) {
            if (std::memcmp(g_hostMemory.getPointer(startAddress), bytes.data(),
                            bytes.size()) != 0) {
              isAllSame = false;
              break;
            }
          }

          if (isAllSame) {
            return shader;
          }
        }

        std::printf("cache: found shader with different data, recompiling\n");
      }

      it = emplacedIt;
      entry = &it->second.emplace_front();
    }

    g_scheduler.enqueue(taskSet, [=](const AsyncTaskCtl &) {
      util::MemoryAreaTable<> dependencies;
      auto info = shader::convert(
          g_hostMemory, stage, address,
          std::span<const std::uint32_t>(userSgprs, userSgprsCount), dimX, dimY,
          dimZ, dependencies);

      if (!validateSpirv(info.spirv)) {
        printSpirv(info.spirv);
        dumpShader(g_hostMemory.getPointer<std::uint32_t>(address));
        util::unreachable();
      }

      // printSpirv(info.spirv);

      for (auto [startAddress, endAddress] : dependencies) {
        auto ptr = g_hostMemory.getPointer(startAddress);
        auto &target = entry->cachedData[startAddress];
        target.resize(endAddress - startAddress);

        // std::printf("shader dependency %lx-%lx\n", startAddress, endAddress);
        std::memcpy(target.data(), ptr, target.size());
      }

      VkShaderCreateInfoEXT createInfo{
          .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
          .flags = 0,
          .stage = shaderStageToVk(stage),
          .nextStage = 0,
          .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
          .codeSize = info.spirv.size() * sizeof(info.spirv[0]),
          .pCode = info.spirv.data(),
          .pName = "main",
          .setLayoutCount = 1,
          .pSetLayouts = &descriptorSetLayout,
      };

      VkShaderEXT shader;
      Verify() << _vkCreateShadersEXT(vk::g_vkDevice, 1, &createInfo,
                                      vk::g_vkAllocator, &shader);
      entry->info = std::move(info);
      entry->shader = shader;
      return true;
    });

    return *entry;
  }

  Ref<CacheImageOverlay>
  getImage(std::uint64_t writeTag, TaskSet &readTaskSet, TaskSet &writeTaskSet,
           VkCommandBuffer readCommandBuffer, std::uint64_t address,
           SurfaceFormat dataFormat, TextureChannelType channelType,
           TileMode tileMode, std::uint32_t width, std::uint32_t height,
           std::uint32_t depth, std::uint32_t pitch, shader::AccessOp access,
           VkImageLayout layout, bool isColor = true) {
    if (address == 0) {
      assert(isColor == false);
      assert(access == shader::AccessOp::None);

      DetachedImageKey key{
          .dataFormat = dataFormat,
          .channelType = channelType,
          .width = width,
          .height = height,
          .depth = depth,
      };

      auto [it, inserted] =
          datachedImages.emplace(key, Ref<CacheImageOverlay>());

      if (inserted) {
        auto format = surfaceFormatToVkFormat(dataFormat, channelType);
        auto detachedOverlay = new CacheImageOverlay();
        detachedOverlay->image =
            vk::Image2D::Allocate(getDeviceLocalMemory(), width, height, format,
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        VkImageViewCreateInfo createInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = detachedOverlay->image.getHandle(),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange =
                {
                    .aspectMask = static_cast<VkImageAspectFlags>(
                        VK_IMAGE_ASPECT_DEPTH_BIT |
                        VK_IMAGE_ASPECT_STENCIL_BIT),
                    .levelCount = 1,
                    .layerCount = 1,
                },
        };

        vkCreateImageView(vk::g_vkDevice, &createInfo, vk::g_vkAllocator,
                          &detachedOverlay->view);

        it->second = detachedOverlay;
      }

      vk::ImageRef(it->second->image)
          .transitionLayout(readCommandBuffer, layout);
      return it->second;
    }

    auto &line = getLine(address, pitch * height * depth);
    return line.getImage(writeTag, readTaskSet, writeTaskSet, readCommandBuffer,
                         address, dataFormat, channelType, tileMode, width,
                         height, depth, pitch, access, layout, isColor);
  }

  Ref<CacheBufferOverlay>
  getBuffer(std::uint64_t writeTag, TaskSet &readTaskSet, TaskSet &writeTaskSet,
            std::uint64_t address, std::uint32_t elementCount,
            std::uint32_t stride, std::uint32_t elementSize,
            shader::AccessOp access) {
    auto &line = getLine(address, stride != 0 ? stride * elementCount
                                              : elementSize * elementCount);
    return line.getBuffer(writeTag, readTaskSet, writeTaskSet, address,
                          elementCount, stride, elementSize, access);
  }

private:
  CacheLine &getLine(std::uint64_t address, std::size_t size) {
    std::lock_guard lock(mtx);
    auto it = cacheLines.lower_bound(address);

    if (it == cacheLines.end() ||
        address >= it->second.areaAddress + it->second.areaSize ||
        it->second.areaAddress >= address + size) {
      auto area = memoryAreaTable.queryArea(address / kPageSize);
      area.beginAddress *= kPageSize;
      area.endAddress *= kPageSize;

      assert(address >= area.beginAddress && address + size < area.endAddress);
      it = cacheLines.emplace_hint(
          it, std::piecewise_construct, std::tuple{area.beginAddress},
          std::tuple{area.beginAddress, area.endAddress});
    }

    return it->second;
  }

} g_cache;

static VkShaderEXT getPrimTypeRectGeomShader() {
  static VkShaderEXT shader = VK_NULL_HANDLE;
  if (shader != VK_NULL_HANDLE) {
    return shader;
  }

  auto layout = getGraphicsLayout().first;
  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .flags = 0,
      .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
      .nextStage = 0,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = sizeof(spirv_rect_list_geom),
      .pCode = spirv_rect_list_geom,
      .pName = "main",
      .setLayoutCount = 1,
      .pSetLayouts = &layout,
  };

  Verify() << _vkCreateShadersEXT(vk::g_vkDevice, 1, &createInfo,
                                  vk::g_vkAllocator, &shader);
  return shader;
}

struct RenderState {
  DrawContext &ctxt;
  QueueRegisters &regs;
  std::uint64_t tag = g_cache.createTag();
  std::vector<Ref<CacheImageOverlay>> usedImages;
  std::vector<Ref<CacheBufferOverlay>> usedBuffers;

  void loadShaderBindings(TaskSet &readTaskSet, TaskSet &storeTaskSet,
                          VkDescriptorSet descSet, const shader::Shader &shader,
                          VkCommandBuffer cmdBuffer) {
    for (auto &uniform : shader.uniforms) {
      switch (uniform.kind) {
      case shader::Shader::UniformKind::Buffer: {
        auto &vbuffer = *reinterpret_cast<const GnmVBuffer *>(uniform.buffer);

        auto bufferRef = g_cache.getBuffer(
            tag, readTaskSet, storeTaskSet, vbuffer.getAddress(),
            vbuffer.getNumRecords(), vbuffer.getStride(),
            vbuffer.getElementSize(), uniform.accessOp);

        VkDescriptorBufferInfo bufferInfo{
            .buffer = bufferRef->buffer.getHandle(),
            .offset = vbuffer.getAddress() - bufferRef->bufferAddress,
            .range = vbuffer.getSize(),
        };

        VkWriteDescriptorSet writeDescSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descSet,
            .dstBinding = uniform.binding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &bufferInfo,
        };

        usedBuffers.push_back(std::move(bufferRef));

        vkUpdateDescriptorSets(vk::g_vkDevice, 1, &writeDescSet, 0, nullptr);
        break;
      }

      case shader::Shader::UniformKind::Image: {
        auto &tbuffer = *reinterpret_cast<const GnmTBuffer *>(uniform.buffer);
        auto dataFormat = tbuffer.dfmt;
        auto channelType = tbuffer.nfmt;

        // assert(tbuffer->width == tbuffer->pitch);
        std::size_t width = tbuffer.width + 1;
        std::size_t height = tbuffer.height + 1;
        std::size_t depth = tbuffer.depth + 1;
        std::size_t pitch = tbuffer.pitch + 1;
        auto tileMode = (TileMode)tbuffer.tiling_idx;

        auto image = g_cache.getImage(
            tag, readTaskSet, storeTaskSet, cmdBuffer, tbuffer.getAddress(),
            dataFormat, channelType, tileMode, width, height, depth, pitch,
            shader::AccessOp::Load, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        VkDescriptorImageInfo imageInfo{
            .imageView = image->view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        usedImages.push_back(std::move(image));

        VkWriteDescriptorSet writeDescSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descSet,
            .dstBinding = uniform.binding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .pImageInfo = &imageInfo,
        };

        vkUpdateDescriptorSets(vk::g_vkDevice, 1, &writeDescSet, 0, nullptr);
        break;
      }

      case shader::Shader::UniformKind::Sampler: {
        // TODO: load S# sampler
        auto sampler = g_cache.getSampler();

        VkDescriptorImageInfo imageInfo{
            .sampler = sampler,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        };

        VkWriteDescriptorSet writeDescSet{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descSet,
            .dstBinding = uniform.binding,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &imageInfo,
        };

        vkUpdateDescriptorSets(vk::g_vkDevice, 1, &writeDescSet, 0, nullptr);
        break;
      }
      }
    }
  }

  void eliminateFastClear() {
    // TODO
    // util::unreachable();
  }

  void resolve() {
    // TODO: when texture cache will be implemented it MSAA should be done by
    // GPU
    util::unreachable();
    auto srcBuffer = regs.colorBuffers[0];
    auto dstBuffer = regs.colorBuffers[1];

    const auto src = g_hostMemory.getPointer(srcBuffer.base);
    auto dst = g_hostMemory.getPointer(dstBuffer.base);

    if (src == nullptr || dst == nullptr) {
      return;
    }

    std::memcpy(dst, src, regs.screenScissorH * regs.screenScissorW * 4);
  }

  void draw(std::uint32_t count, std::uint64_t indeciesAddress,
            std::uint32_t indexCount) {
    if (regs.cbColorFormat == CbColorFormat::Disable) {
      return;
    }

    if (regs.cbColorFormat == CbColorFormat::EliminateFastClear) {
      eliminateFastClear();
      return;
    }

    if (regs.cbColorFormat == CbColorFormat::Resolve) {
      resolve();
      return;
    }

    if (regs.pgmVsAddress == 0 || regs.pgmPsAddress == 0) {
      return;
    }

    if (regs.cbRenderTargetMask == 0 || regs.colorBuffers[0].base == 0) {
      return;
    }

    regs.depthClearEnable = true;

    TaskSet loadTaskSet, storeTaskSet;
    auto [desriptorSetLayout, pipelineLayout] = getGraphicsLayout();
    auto &vertexShader = g_cache.getShader(
        loadTaskSet, desriptorSetLayout, shader::Stage::Vertex,
        regs.pgmVsAddress, regs.userVsData, regs.vsUserSpgrs);

    auto &fragmentShader = g_cache.getShader(
        loadTaskSet, desriptorSetLayout, shader::Stage::Fragment,
        regs.pgmPsAddress, regs.userPsData, regs.psUserSpgrs);

    auto sem = vk::Semaphore::Create();
    loadTaskSet.wait();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkCommandBuffer transferCommandBuffers[2];

    {
      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandPool = ctxt.commandPool;
      allocInfo.commandBufferCount = std::size(transferCommandBuffers);

      Verify() << vkAllocateCommandBuffers(vk::g_vkDevice, &allocInfo,
                                           transferCommandBuffers);
    }

    VkCommandBuffer readCommandBuffer = transferCommandBuffers[0];
    Verify() << vkBeginCommandBuffer(readCommandBuffer, &beginInfo);

    auto primType = static_cast<PrimitiveType>(regs.vgtPrimitiveType);

    std::vector<VkRenderingAttachmentInfo> colorAttachments;

    std::vector<VkBool32> colorBlendEnable;
    std::vector<VkColorBlendEquationEXT> colorBlendEquation;
    std::vector<VkColorComponentFlags> colorWriteMask;
    for (auto targetMask = regs.cbRenderTargetMask;
         auto &colorBuffer : regs.colorBuffers) {
      if (targetMask == 0 || colorBuffer.base == 0) {
        break;
      }

      auto mask = targetMask & 0xf;

      if (mask == 0) {
        targetMask >>= 4;
        continue;
      }
      targetMask >>= 4;

      shader::AccessOp access =
          shader::AccessOp::Load | shader::AccessOp::Store;

      auto dataFormat = (SurfaceFormat)colorBuffer.format;
      auto channelType = kTextureChannelTypeSrgb; // TODO

      auto colorImage = g_cache.getImage(
          tag, loadTaskSet, storeTaskSet, readCommandBuffer, colorBuffer.base,
          dataFormat, channelType, (TileMode)colorBuffer.tileModeIndex,
          regs.screenScissorW, regs.screenScissorH, 1, regs.screenScissorW,
          access, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

      colorAttachments.push_back({
          .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
          .imageView = colorImage->view,
          .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
          .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
          .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      });

      usedImages.push_back(std::move(colorImage));

      colorBlendEnable.push_back(regs.blendEnable ? VK_TRUE : VK_FALSE);
      colorBlendEquation.push_back(VkColorBlendEquationEXT{
          .srcColorBlendFactor =
              blendMultiplierToVkBlendFactor(regs.blendColorSrc),
          .dstColorBlendFactor =
              blendMultiplierToVkBlendFactor(regs.blendColorDst),
          .colorBlendOp = blendFuncToVkBlendOp(regs.blendColorFn),
          .srcAlphaBlendFactor =
              regs.blendSeparateAlpha
                  ? blendMultiplierToVkBlendFactor(regs.blendAlphaSrc)
                  : blendMultiplierToVkBlendFactor(regs.blendColorSrc),
          .dstAlphaBlendFactor =
              regs.blendSeparateAlpha
                  ? blendMultiplierToVkBlendFactor(regs.blendAlphaDst)
                  : blendMultiplierToVkBlendFactor(regs.blendColorDst),
          .alphaBlendOp = blendFuncToVkBlendOp(regs.blendAlphaFn),
      });

      colorWriteMask.push_back(((mask & 1) ? VK_COLOR_COMPONENT_R_BIT : 0) |
                               ((mask & 2) ? VK_COLOR_COMPONENT_G_BIT : 0) |
                               ((mask & 4) ? VK_COLOR_COMPONENT_B_BIT : 0) |
                               ((mask & 8) ? VK_COLOR_COMPONENT_A_BIT : 0));
    }

    auto descSet = g_cache.getGraphicsDescriptorSet();

    loadShaderBindings(loadTaskSet, storeTaskSet, descSet, vertexShader.info,
                       readCommandBuffer);

    loadShaderBindings(loadTaskSet, storeTaskSet, descSet, fragmentShader.info,
                       readCommandBuffer);

    shader::AccessOp depthAccess = shader::AccessOp::None;

    if (!regs.depthClearEnable && regs.zReadBase != 0) {
      depthAccess |= shader::AccessOp::Load;
    }

    if (regs.depthWriteEnable && regs.zWriteBase != 0) {
      depthAccess |= shader::AccessOp::Store;
    }

    if (regs.zReadBase != regs.zWriteBase) {
      __builtin_unreachable();
    }

    auto depthImage = g_cache.getImage(
        tag, loadTaskSet, storeTaskSet, readCommandBuffer, regs.zReadBase,
        kSurfaceFormat24_8, kTextureChannelTypeUNorm,
        kTileModeDisplay_LinearAligned, regs.screenScissorW,
        regs.screenScissorH, 1, regs.screenScissorW, depthAccess,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, false);

    VkRenderingAttachmentInfo depthAttachment{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = depthImage->view,
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .loadOp = !regs.depthClearEnable && regs.zReadBase
                      ? VK_ATTACHMENT_LOAD_OP_LOAD
                      : VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = regs.depthWriteEnable && regs.zWriteBase
                       ? VK_ATTACHMENT_STORE_OP_STORE
                       : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = {.depthStencil = {.depth = regs.depthClear}}};

    VkCommandBuffer drawCommandBuffer;
    {
      VkCommandBufferAllocateInfo allocInfo{};
      allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
      allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      allocInfo.commandPool = ctxt.commandPool;
      allocInfo.commandBufferCount = 1;

      Verify() << vkAllocateCommandBuffers(vk::g_vkDevice, &allocInfo,
                                           &drawCommandBuffer);
    }

    Verify() << vkBeginCommandBuffer(drawCommandBuffer, &beginInfo);

    VkRenderingInfo renderInfo{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea =
            {
                .offset = {.x = static_cast<int32_t>(regs.screenScissorX),
                           .y = static_cast<int32_t>(regs.screenScissorY)},
                .extent =
                    {
                        .width = regs.screenScissorW,
                        .height = regs.screenScissorH,
                    },
            },
        .layerCount = 1,
        .colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size()),
        .pColorAttachments = colorAttachments.data(),
        .pDepthAttachment = &depthAttachment,
        .pStencilAttachment = &depthAttachment,
    };

    vkCmdBeginRendering(drawCommandBuffer, &renderInfo);

    // std::printf("viewport: %ux%u, %ux%u\n", regs.screenScissorX,
    //             regs.screenScissorY, regs.screenScissorW,
    //             regs.screenScissorH);
    VkViewport viewport{};
    viewport.x = regs.screenScissorX;
    viewport.y = (float)regs.screenScissorH - regs.screenScissorY;
    viewport.width = regs.screenScissorW;
    viewport.height = -(float)regs.screenScissorH;
    viewport.minDepth = -1.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(drawCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent.width = regs.screenScissorW;
    scissor.extent.height = regs.screenScissorH;
    scissor.offset.x = regs.screenScissorX;
    scissor.offset.y = regs.screenScissorY;
    vkCmdSetScissor(drawCommandBuffer, 0, 1, &scissor);

    _vkCmdSetColorBlendEnableEXT(drawCommandBuffer, 0, colorBlendEnable.size(),
                                 colorBlendEnable.data());
    _vkCmdSetColorBlendEquationEXT(drawCommandBuffer, 0,
                                   colorBlendEquation.size(),
                                   colorBlendEquation.data());

    _vkCmdSetDepthClampEnableEXT(drawCommandBuffer, VK_TRUE);
    vkCmdSetDepthCompareOp(drawCommandBuffer, (VkCompareOp)regs.zFunc);
    vkCmdSetDepthTestEnable(drawCommandBuffer,
                            regs.depthEnable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthWriteEnable(drawCommandBuffer,
                             regs.depthWriteEnable ? VK_TRUE : VK_FALSE);
    vkCmdSetDepthBounds(drawCommandBuffer, -1.f, 1.f);
    vkCmdSetDepthBoundsTestEnable(drawCommandBuffer,
                                  regs.depthBoundsEnable ? VK_TRUE : VK_FALSE);
    vkCmdSetStencilOp(drawCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                      VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
                      VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);

    // VkDeviceSize strides = 0;
    // vkCmdBindVertexBuffers2EXT(drawCommandBuffer, 0, 0, nullptr, nullptr,
    //                            nullptr, &strides);
    vkCmdSetRasterizerDiscardEnable(drawCommandBuffer, VK_FALSE);
    vkCmdSetDepthBiasEnable(drawCommandBuffer, VK_TRUE);
    vkCmdSetDepthBias(drawCommandBuffer, 0, 1, 1);
    vkCmdSetPrimitiveRestartEnable(drawCommandBuffer, VK_FALSE);

    _vkCmdSetLogicOpEnableEXT(drawCommandBuffer, VK_FALSE);
    _vkCmdSetLogicOpEXT(drawCommandBuffer, VK_LOGIC_OP_AND);
    _vkCmdSetPolygonModeEXT(drawCommandBuffer, VK_POLYGON_MODE_FILL);
    _vkCmdSetRasterizationSamplesEXT(drawCommandBuffer, VK_SAMPLE_COUNT_1_BIT);
    VkSampleMask sampleMask = ~0;
    _vkCmdSetSampleMaskEXT(drawCommandBuffer, VK_SAMPLE_COUNT_1_BIT,
                           &sampleMask);
    _vkCmdSetTessellationDomainOriginEXT(
        drawCommandBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT);
    _vkCmdSetAlphaToCoverageEnableEXT(drawCommandBuffer, VK_FALSE);
    _vkCmdSetVertexInputEXT(drawCommandBuffer, 0, nullptr, 0, nullptr);
    _vkCmdSetColorWriteMaskEXT(drawCommandBuffer, 0, colorWriteMask.size(),
                               colorWriteMask.data());

    vkCmdSetStencilCompareMask(drawCommandBuffer,
                               VK_STENCIL_FACE_FRONT_AND_BACK, 0);
    vkCmdSetStencilWriteMask(drawCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                             0);
    vkCmdSetStencilReference(drawCommandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
                             0);

    vkCmdSetCullMode(
        drawCommandBuffer,
        (regs.cullBack ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE) |
            (regs.cullFront ? VK_CULL_MODE_FRONT_BIT : VK_CULL_MODE_NONE));
    vkCmdSetFrontFace(drawCommandBuffer, regs.face
                                             ? VK_FRONT_FACE_CLOCKWISE
                                             : VK_FRONT_FACE_COUNTER_CLOCKWISE);

    vkCmdSetPrimitiveTopology(drawCommandBuffer, getVkPrimitiveType(primType));
    vkCmdSetStencilTestEnable(drawCommandBuffer, VK_FALSE);

    vkCmdBindDescriptorSets(drawCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &descSet, 0, nullptr);

    vk::Buffer indexBufferStorage;
    BufferRef indexBuffer;
    auto needConversion = isPrimRequiresConversion(primType);
    VkIndexType vkIndexType = (regs.indexType & 0x1f) == 0
                                  ? VK_INDEX_TYPE_UINT16
                                  : VK_INDEX_TYPE_UINT32;

    if (needConversion) {
      auto indecies = g_hostMemory.getPointer(indeciesAddress);
      if (indecies == nullptr) {
        indexCount = count;
      }

      unsigned origIndexSize = vkIndexType == VK_INDEX_TYPE_UINT16 ? 16 : 32;
      auto converterFn = getPrimConverterFn(primType, &indexCount);

      if (indecies == nullptr) {
        if (indexCount < 0x10000) {
          vkIndexType = VK_INDEX_TYPE_UINT16;
        } else if (indecies) {
          vkIndexType = VK_INDEX_TYPE_UINT32;
        }
      }

      unsigned indexSize = vkIndexType == VK_INDEX_TYPE_UINT16 ? 16 : 32;
      auto indexBufferSize = indexSize * indexCount;

      indexBufferStorage = vk::Buffer::Allocate(
          getHostVisibleMemory(), indexBufferSize,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

      void *data = indexBufferStorage.getData();

      if (indecies == nullptr) {
        if (indexSize == 16) {
          for (std::uint32_t i = 0; i < indexCount; ++i) {
            auto [dstIndex, srcIndex] = converterFn(i);
            ((std::uint16_t *)data)[dstIndex] = srcIndex;
          }
        } else {
          for (std::uint32_t i = 0; i < indexCount; ++i) {
            auto [dstIndex, srcIndex] = converterFn(i);
            ((std::uint32_t *)data)[dstIndex] = srcIndex;
          }
        }
      } else {
        if (indexSize == 16) {
          for (std::uint32_t i = 0; i < indexCount; ++i) {
            auto [dstIndex, srcIndex] = converterFn(i);
            std::uint32_t origIndex =
                origIndexSize == 16 ? ((std::uint16_t *)indecies)[srcIndex]
                                    : ((std::uint32_t *)indecies)[srcIndex];
            ((std::uint16_t *)data)[dstIndex] = origIndex;
          }

        } else {
          for (std::uint32_t i = 0; i < indexCount; ++i) {
            auto [dstIndex, srcIndex] = converterFn(i);
            std::uint32_t origIndex =
                origIndexSize == 16 ? ((std::uint16_t *)indecies)[srcIndex]
                                    : ((std::uint32_t *)indecies)[srcIndex];
            ((std::uint32_t *)data)[dstIndex] = origIndex;
          }
        }
      }

      indexBuffer = {indexBufferStorage.getHandle(), 0, indexBufferSize};
    } else if (indeciesAddress != 0) {
      unsigned indexSize = vkIndexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;

      auto bufferRef =
          g_cache.getBuffer(tag, loadTaskSet, storeTaskSet, indeciesAddress,
                            indexCount, 0, indexSize, shader::AccessOp::Load);
      indexBuffer = {
          .buffer = bufferRef->buffer.getHandle(),
          .offset = indeciesAddress - bufferRef->bufferAddress,
          .size = static_cast<std::uint64_t>(indexCount) * indexSize,
      };

      usedBuffers.push_back(std::move(bufferRef));
    }

    VkShaderStageFlagBits stages[]{VK_SHADER_STAGE_VERTEX_BIT,
                                   VK_SHADER_STAGE_FRAGMENT_BIT};
    _vkCmdBindShadersEXT(drawCommandBuffer, 1, stages + 0,
                         &vertexShader.shader);
    _vkCmdBindShadersEXT(drawCommandBuffer, 1, stages + 1,
                         &fragmentShader.shader);

    if (primType == kPrimitiveTypeRectList) {
      VkShaderStageFlagBits stage = VK_SHADER_STAGE_GEOMETRY_BIT;
      auto shader = getPrimTypeRectGeomShader();
      _vkCmdBindShadersEXT(drawCommandBuffer, 1, &stage, &shader);
    }

    if (indexBuffer.buffer == nullptr) {
      vkCmdDraw(drawCommandBuffer, count, 1, 0, 0);
    } else {
      vkCmdBindIndexBuffer(drawCommandBuffer, indexBuffer.buffer,
                           indexBuffer.offset, vkIndexType);
      vkCmdDrawIndexed(drawCommandBuffer, indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRendering(drawCommandBuffer);
    vkEndCommandBuffer(drawCommandBuffer);

    loadTaskSet.wait();

    vkEndCommandBuffer(readCommandBuffer);

    submitToQueue(ctxt.queue, readCommandBuffer, sem, 1);
    submitToQueue(ctxt.queue, drawCommandBuffer, sem, 1, 2,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);

    VkCommandBuffer writeCommandBuffer = transferCommandBuffers[1];
    Verify() << vkBeginCommandBuffer(writeCommandBuffer, &beginInfo);
    sem.wait(2, UINTMAX_MAX);

    g_cache.releaseGraphicsDescriptorSet(descSet);

    vkEndCommandBuffer(writeCommandBuffer);
    submitToQueue(ctxt.queue, writeCommandBuffer, sem, 2, 3,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);

    sem.wait(3, UINTMAX_MAX);
    storeTaskSet.enqueue(g_scheduler);
    storeTaskSet.wait();
  }

  void dispatch(std::size_t dimX, std::size_t dimY, std::size_t dimZ) {
    auto descSet = g_cache.getComputeDescriptorSet();
    TaskSet storeTaskSet;

    auto computeTask = getComputeQueueScheduler().enqueue(
        [&, this](VkCommandBuffer commandBuffer) {
          TaskSet loadTaskSet;
          auto [desriptorSetLayout, pipelineLayout] = getComputeLayout();

          auto &computeShader = g_cache.getShader(
              loadTaskSet, desriptorSetLayout, shader::Stage::Compute,
              regs.pgmComputeAddress, regs.userComputeData,
              regs.computeUserSpgrs, regs.computeNumThreadX,
              regs.computeNumThreadY, regs.computeNumThreadZ);
          loadTaskSet.wait();

          VkShaderStageFlagBits stages[]{VK_SHADER_STAGE_COMPUTE_BIT};
          _vkCmdBindShadersEXT(commandBuffer, 1, stages + 0,
                               &computeShader.shader);

          loadShaderBindings(loadTaskSet, storeTaskSet, descSet,
                             computeShader.info, commandBuffer);
          loadTaskSet.wait();

          vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                                  pipelineLayout, 0, 1, &descSet, 0, nullptr);

          vkCmdDispatch(commandBuffer, dimX, dimY, dimZ);
        });

    computeTask->wait();
    g_cache.releaseComputeDescriptorSet(descSet);
    storeTaskSet.enqueue(g_scheduler);
    storeTaskSet.wait();
  }
};

static void draw(DrawContext &ctxt, QueueRegisters &regs, std::uint32_t count,
                 std::uint64_t indeciesAddress, std::uint32_t indexCount) {
  RenderState{.ctxt = ctxt, .regs = regs}.draw(count, indeciesAddress,
                                               indexCount);
}

static void dispatch(DrawContext &ctxt, QueueRegisters &regs, std::size_t dimX,
                     std::size_t dimY, std::size_t dimZ) {

  RenderState{.ctxt = ctxt, .regs = regs}.dispatch(dimX, dimY, dimZ);
}

enum class EventWriteSource : std::uint8_t {
  Immediate32 = 0x1,
  Immediate64 = 0x2,
  GlobalClockCounter = 0x3,
  GpuCoreClockCounter = 0x4,
};

struct EopData {
  std::uint32_t eventType;
  std::uint32_t eventIndex;
  std::uint64_t address;
  std::uint64_t value;
  std::uint8_t dstSel;
  std::uint8_t intSel;
  EventWriteSource eventSource;
};

static std::uint64_t globalClock() {
  // TODO
  return 0x0;
}

static std::uint64_t gpuCoreClock() {
  // TODO
  return 0x0;
}

static void writeEop(EopData data) {
  switch (data.eventSource) {
  case EventWriteSource::Immediate32: {
    *g_hostMemory.getPointer<std::uint32_t>(data.address) = data.value;
    break;
  }
  case EventWriteSource::Immediate64: {
    *g_hostMemory.getPointer<std::uint64_t>(data.address) = data.value;
    break;
  }
  case EventWriteSource::GlobalClockCounter: {
    *g_hostMemory.getPointer<std::uint64_t>(data.address) = globalClock();
    break;
  }
  case EventWriteSource::GpuCoreClockCounter: {
    *g_hostMemory.getPointer<std::uint64_t>(data.address) = gpuCoreClock();
    break;
  }
  }
}

static void drawIndexAuto(amdgpu::device::DrawContext &ctxt,
                          QueueRegisters &regs, std::uint32_t count) {
  draw(ctxt, regs, count, 0, 0);
}

static void drawIndex2(amdgpu::device::DrawContext &ctxt, QueueRegisters &regs,
                       std::uint32_t maxSize, std::uint64_t address,
                       std::uint32_t count) {
  draw(ctxt, regs, count, address, maxSize);
}

static void handleCommandBuffer(DrawContext &ctxt, QueueRegisters &regs,
                                std::uint32_t *cmds, std::uint32_t count) {
  bool log = false;
  for (std::uint32_t cmdOffset = 0; cmdOffset < count; ++cmdOffset) {
    auto cmd = cmds[cmdOffset];
    auto type = getBits(cmd, 31, 30);

    if (type == 0) {
      std::printf("!packet type 0!\n");
      auto baseIndex = getBits(cmd, 15, 0);
      auto count = getBits(cmd, 29, 16);
      std::printf("-- %04x: %08x: baseIndex=%x, count=%d\n", cmdOffset, cmd,
                  baseIndex, count);
      cmdOffset += count;
    } else if (type == 1) {
      std::printf("Unexpected packet type 1!\n");
    } else if (type == 2) {
      std::printf("!packet type 2!\n");
      continue;
    } else if (type == 3) {
      auto predicate = getBit(cmd, 0);
      auto shaderType = getBit(cmd, 1);
      auto op = getBits(cmd, 15, 8);
      auto len = getBits(cmd, 29, 16) + 1;

      if (log) {
        std::printf("-- %04x: %08x: %s len=%d, shaderType = %cX\n", cmdOffset,
                    cmd, opcodeToString(op).c_str(), len,
                    'G' ^ (shaderType << 1));

        for (std::uint32_t offset = 0; offset < len; ++offset) {
          std::printf("   %04x: %08x\n", cmdOffset + 1 + offset,
                      cmds[cmdOffset + 1 + offset]);
        }
      }

      switch (op) {
      case kOpcodeLOAD_CONST_RAM: {
        std::uint64_t addressLo = cmds[cmdOffset + 1];
        std::uint64_t addressHi = cmds[cmdOffset + 2];
        std::uint32_t numDw = getBits(cmds[cmdOffset + 3], 14, 0);
        std::uint32_t offset = getBits(cmds[cmdOffset + 4], 15, 0);
        if (log) {
          std::printf("   ` address=%lx, numDw = %x, offset=%x\n",
                      addressLo | (addressHi << 32), numDw, offset);
        }
        break;
      }

      case kOpcodeSET_UCONFIG_REG: {
        std::uint32_t baseRegOffset = 0xc000 + cmds[cmdOffset + 1];

        for (std::uint32_t regOffset = 0; regOffset < len - 1; ++regOffset) {
          if (log) {
            std::printf("   %04x: %04x: %s = 0x%08x\n",
                        cmdOffset + 2 + regOffset,
                        (baseRegOffset + regOffset) << 2,
                        registerToString(baseRegOffset + regOffset).c_str(),
                        cmds[cmdOffset + 2 + regOffset]);
          }

          regs.setRegister(baseRegOffset + regOffset,
                           cmds[cmdOffset + 2 + regOffset]);
        }
        break;
      }

      case kOpcodeSET_CONTEXT_REG: {
        std::uint32_t baseRegOffset = 0xa000 + cmds[cmdOffset + 1];

        for (std::uint32_t regOffset = 0; regOffset < len - 1; ++regOffset) {
          if (log) {
            std::printf("   %04x: %04x: %s = 0x%08x\n",
                        cmdOffset + 2 + regOffset,
                        (baseRegOffset + regOffset) << 2,
                        registerToString(baseRegOffset + regOffset).c_str(),
                        cmds[cmdOffset + 2 + regOffset]);
          }
          regs.setRegister(baseRegOffset + regOffset,
                           cmds[cmdOffset + 2 + regOffset]);
        }
        break;
      }

      case kOpcodeSET_SH_REG: {
        std::uint32_t baseRegOffset = 0x2c00 + cmds[cmdOffset + 1];

        for (std::uint32_t regOffset = 0; regOffset < len - 1; ++regOffset) {
          if (log) {
            std::printf("   %04x: %04x: %s = 0x%08x\n",
                        cmdOffset + 2 + regOffset,
                        (baseRegOffset + regOffset) << 2,
                        registerToString(baseRegOffset + regOffset).c_str(),
                        cmds[cmdOffset + 2 + regOffset]);
          }

          regs.setRegister(baseRegOffset + regOffset,
                           cmds[cmdOffset + 2 + regOffset]);
        }
        break;
      }

      case kOpcodeWRITE_DATA: {
        auto control = cmds[cmdOffset + 1];
        auto destAddrLo = cmds[cmdOffset + 2];
        auto destAddrHi = cmds[cmdOffset + 3];
        auto data = cmds + cmdOffset + 4;
        auto size = len - 3;

        // 0 - Micro Engine - ME
        // 1 - Prefetch parser - PFP
        // 2 - Constant engine - CE
        // 3 - Dispatch engine - DE
        auto engineSel = getBits(control, 31, 30);

        // wait for confirmation that write complete
        auto wrConfirm = getBit(control, 20);

        // do not increment address
        auto wrOneAddr = getBit(control, 16);

        // 0 - mem-mapped register
        // 1 - memory sync
        // 2 - tc/l2
        // 3 - gds
        // 4 - reserved
        // 5 - memory async
        auto dstSel = getBits(control, 11, 8);

        auto memMappedRegisterAddress = getBits(destAddrLo, 15, 0);
        auto memory32bit = getBits(destAddrLo, 31, 2);
        auto memory64bit = getBits(destAddrLo, 31, 3);
        auto gdsOffset = getBits(destAddrLo, 15, 0);

        if (log) {
          std::printf("   %04x: control=%x [engineSel=%d, "
                      "wrConfirm=%d,wrOneAddr=%d, dstSel=%d]\n",
                      cmdOffset + 1, control, engineSel, wrConfirm, wrOneAddr,
                      dstSel);

          std::printf("   %04x: destAddrLo=%x "
                      "[memory32bit=%x,memory64bit=%x,gdsOffset=%x]\n",
                      cmdOffset + 2, destAddrLo, memory32bit, memory64bit,
                      gdsOffset);

          std::printf("   %04x: destAddrHi=%x\n", cmdOffset + 3, destAddrHi);

          for (std::uint32_t offset = 4; offset < len; ++offset) {
            std::printf("   %04x: %08x\n", cmdOffset + offset,
                        cmds[cmdOffset + offset]);
          }
        }
        auto address =
            destAddrLo | (static_cast<std::uint64_t>(destAddrHi) << 32);
        auto dest = g_hostMemory.getPointer<std::uint32_t>(address);
        if (log) {
          std::printf("   address=%lx\n", address);
        }
        for (unsigned i = 0; i < size; ++i) {
          dest[i] = data[i];
        }

        break;
      }

      case kOpcodeINDEX_TYPE: {
        regs.indexType = cmds[cmdOffset + 1];
        break;
      }

      case kOpcodeDRAW_INDEX_AUTO: {
        drawIndexAuto(ctxt, regs, cmds[cmdOffset + 1]);
        break;
      }

      case kOpcodeDRAW_INDEX_2: {
        auto maxSize = cmds[cmdOffset + 1];
        auto address = cmds[cmdOffset + 2] |
                       (static_cast<std::uint64_t>(cmds[cmdOffset + 3]) << 32);
        auto count = cmds[cmdOffset + 4];
        drawIndex2(ctxt, regs, maxSize, address, count);
        break;
      }

      case kOpcodeDISPATCH_DIRECT: {
        auto dimX = cmds[cmdOffset + 1];
        auto dimY = cmds[cmdOffset + 2];
        auto dimZ = cmds[cmdOffset + 3];
        if (log) {
          std::printf("   %04x: DIM X=%u\n", cmdOffset + 1, dimX);
          std::printf("   %04x: DIM Y=%u\n", cmdOffset + 2, dimY);
          std::printf("   %04x: DIM Z=%u\n", cmdOffset + 3, dimZ);
        }
        dispatch(ctxt, regs, dimX, dimY, dimZ);
      }

      case kOpcodeEVENT_WRITE_EOP: {
        EopData eopData{};
        eopData.eventType = getBits(cmds[cmdOffset + 1], 6, 0);
        eopData.eventIndex = getBits(cmds[cmdOffset + 1], 12, 8);
        eopData.address =
            cmds[cmdOffset + 2] |
            (static_cast<std::uint64_t>(getBits(cmds[cmdOffset + 3], 16, 0))
             << 32);
        eopData.value = cmds[cmdOffset + 4] |
                        (static_cast<std::uint64_t>(cmds[cmdOffset + 5]) << 32);
        eopData.dstSel = 0;
        eopData.intSel = getBits(cmds[cmdOffset + 3], 26, 24);
        eopData.eventSource =
            static_cast<EventWriteSource>(getBits(cmds[cmdOffset + 3], 32, 29));
        writeEop(eopData);
        break;
      }

      case kOpcodeEVENT_WRITE_EOS: {
        std::uint32_t eventType = getBits(cmds[cmdOffset + 1], 6, 0);
        std::uint32_t eventIndex = getBits(cmds[cmdOffset + 1], 12, 8);
        std::uint64_t address =
            cmds[cmdOffset + 2] |
            (static_cast<std::uint64_t>(getBits(cmds[cmdOffset + 3], 16, 0))
             << 32);
        std::uint32_t command = getBits(cmds[cmdOffset + 3], 32, 16);

        if (log) {
          std::printf("address = %#lx\n", address);
          std::printf("command = %#x\n", command);
        }
        if (command == 0x4000) { // store 32bit data
          *g_hostMemory.getPointer<std::uint32_t>(address) =
              cmds[cmdOffset + 4];
        } else {
          util::unreachable();
        }

        break;
      }

      case kOpcodeWAIT_REG_MEM: {
        auto function = cmds[cmdOffset + 1] & 7;
        auto pollAddressLo = cmds[cmdOffset + 2];
        auto pollAddressHi = cmds[cmdOffset + 3];
        auto reference = cmds[cmdOffset + 4];
        auto mask = cmds[cmdOffset + 5];
        auto pollInterval = cmds[cmdOffset + 5];

        auto pollAddress =
            pollAddressLo | (static_cast<std::uint64_t>(pollAddressHi) << 32);
        auto pointer =
            g_hostMemory.getPointer<volatile std::uint32_t>(pollAddress);

        reference &= mask;

        auto compare = [&](std::uint32_t value, std::uint32_t reference,
                           int function) {
          switch (function) {
          case 0:
            return true;
          case 1:
            return value < reference;
          case 2:
            return value <= reference;
          case 3:
            return value == reference;
          case 4:
            return value != reference;
          case 5:
            return value >= reference;
          case 6:
            return value > reference;
          }

          util::unreachable();
        };

        if (log) {
          std::printf("   polling address %lx, reference = %x, function = %u\n",
                      pollAddress, reference, function);
          std::fflush(stdout);
        }
        while (true) {
          auto value = *pointer & mask;

          if (compare(value, reference, function)) {
            break;
          }
        }
        break;
      }

      case kOpcodeNOP:
        if (log) {
          for (std::uint32_t offset = 0; offset < len; ++offset) {
            std::printf("   %04x: %08x\n", cmdOffset + 1 + offset,
                        cmds[cmdOffset + 1 + offset]);
          }
        }
        break;
      default:
        if (log) {
          for (std::uint32_t offset = 0; offset < len; ++offset) {
            std::printf("   %04x: %08x\n", cmdOffset + 1 + offset,
                        cmds[cmdOffset + 1 + offset]);
          }
          break;
        }
      }

      cmdOffset += len;
    }
  }
}

void amdgpu::device::AmdgpuDevice::handleProtectMemory(std::uint64_t address,
                                                       std::uint64_t size,
                                                       std::uint32_t prot) {
  auto beginPage = address / kPageSize;
  auto endPage = (address + size + kPageSize - 1) / kPageSize;

  ::mprotect(g_hostMemory.getPointer(address), size, prot >> 4);

  if (prot >> 4) {
    memoryAreaTable.map(beginPage, endPage);
    const char *protStr;
    switch (prot >> 4) {
    case PROT_READ:
      protStr = "R";
      break;

    case PROT_WRITE:
      protStr = "W";
      break;

    case PROT_WRITE | PROT_READ:
      protStr = "W";
      break;

    default:
      protStr = "unknown";
      break;
    }
    std::printf("Allocated area at %zx, size %lx, prot %s\n", address, size,
                protStr);
  } else {
    memoryAreaTable.unmap(beginPage, endPage);
    std::printf("Unmapped area at %zx, size %lx\n", address, size);
  }

  std::size_t index = 0;
  for (auto area : memoryAreaTable) {
    if (index >= std::size(g_bridge->memoryAreas)) {
      util::unreachable("too many memory areas");
    }

    g_bridge->memoryAreas[index++] = {
        .address = area.beginAddress * kPageSize,
        .size = (area.endAddress - area.beginAddress) * kPageSize,
        .prot = (PROT_READ | PROT_WRITE) << 4 // TODO
    };
  }

  g_bridge->memoryAreaCount = index;
}

static std::unordered_map<std::uint64_t, QueueRegisters> queueRegs;

void amdgpu::device::AmdgpuDevice::handleCommandBuffer(std::uint64_t queueId,
                                                       std::uint64_t address,
                                                       std::uint64_t size) {
  auto count = size / sizeof(std::uint32_t);

  // std::printf("address = %lx, count = %lx\n", address, count);

  ::handleCommandBuffer(dc, queueRegs[queueId],
                        g_hostMemory.getPointer<std::uint32_t>(address), count);
}

bool amdgpu::device::AmdgpuDevice::handleFlip(
    std::uint32_t bufferIndex, std::uint64_t arg, VkCommandBuffer cmd,
    VkImage targetImage, VkExtent2D targetExtent,
    std::vector<VkBuffer> &usedBuffers, std::vector<VkImage> &usedImages) {
  // std::printf("requested flip %d\n", bufferIndex);

  if (bufferIndex == ~static_cast<std::uint32_t>(0)) {
    g_bridge->flipBuffer = bufferIndex;
    g_bridge->flipArg = arg;
    g_bridge->flipCount = g_bridge->flipCount + 1;

    // black surface, ignore for now
    return false;
  }

  // std::fprintf(stderr, "device local memory: ");
  // getDeviceLocalMemory().dump();
  // std::fprintf(stderr, "host visible memory: ");
  // getHostVisibleMemory().dump();

  auto buffer = g_bridge->buffers[bufferIndex];

  if (buffer.pitch == 0 || buffer.height == 0 || buffer.address == 0) {
    std::printf("Attempt to flip unallocated buffer\n");
    return false;
  }

  std::printf("flip: address=%lx, buffer=%ux%u, target=%ux%u\n", buffer.address,
              buffer.width, buffer.height, targetExtent.width,
              targetExtent.height);

  TaskSet readTask;
  TaskSet writeTask;
  Ref<CacheImageOverlay> imageRef;

  auto loadTask =
      getGraphicsQueueScheduler().enqueue([&](VkCommandBuffer commandBuffer) {
        imageRef = g_cache.getImage(
            0, readTask, writeTask, commandBuffer, buffer.address,
            kSurfaceFormat8_8_8_8, kTextureChannelTypeSrgb,
            buffer.tilingMode == 1 ? kTileModeDisplay_2dThin
                                   : kTileModeDisplay_LinearAligned,
            buffer.width, buffer.height, 1, buffer.pitch,
            shader::AccessOp::Load, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
      });

  loadTask->wait();
  readTask.wait();

  writeTask.enqueue(g_scheduler);
  writeTask.wait();

  transitionImageLayout(cmd, targetImage, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkImageBlit region{
      .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
      .srcOffsets = {{},
                     {static_cast<int32_t>(buffer.width),
                      static_cast<int32_t>(buffer.height), 1}},
      .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
      .dstOffsets = {{},
                     {static_cast<int32_t>(targetExtent.width),
                      static_cast<int32_t>(targetExtent.height), 1}},
  };

  vkCmdBlitImage(cmd, imageRef->image.getHandle(),
                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, targetImage,
                 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region,
                 VK_FILTER_LINEAR);

  transitionImageLayout(cmd, targetImage, VK_IMAGE_ASPECT_COLOR_BIT,
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  g_bridge->flipBuffer = bufferIndex;
  g_bridge->flipArg = arg;
  g_bridge->flipCount = g_bridge->flipCount + 1;
  auto bufferInUse =
      g_hostMemory.getPointer<std::uint64_t>(g_bridge->bufferInUseAddress);
  if (bufferInUse != nullptr) {
    bufferInUse[bufferIndex] = 0;
  }
  return true;
}

AmdgpuDevice::AmdgpuDevice(amdgpu::device::DrawContext dc,
                           amdgpu::bridge::BridgeHeader *bridge)
    : dc(dc) {
  g_bridge = bridge;
}

AmdgpuDevice::~AmdgpuDevice() {
  g_cache.clear();

  auto [gSetLayout, gPipelineLayout] = getGraphicsLayout();
  auto [cSetLayout, cPipelineLayout] = getComputeLayout();

  vkDestroyDescriptorSetLayout(vk::g_vkDevice, gSetLayout, vk::g_vkAllocator);
  vkDestroyDescriptorSetLayout(vk::g_vkDevice, cSetLayout, vk::g_vkAllocator);

  vkDestroyPipelineLayout(vk::g_vkDevice, gPipelineLayout, vk::g_vkAllocator);
  vkDestroyPipelineLayout(vk::g_vkDevice, cPipelineLayout, vk::g_vkAllocator);
}
