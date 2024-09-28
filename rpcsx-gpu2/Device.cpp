#include "Device.hpp"
#include "Renderer.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/constants.hpp"
#include "gnm/pm4.hpp"
#include "rx/bits.hpp"
#include "rx/die.hpp"
#include "rx/mem.hpp"
#include "shader/spv.hpp"
#include "shaders/rdna-semantic-spirv.hpp"
#include "vk.hpp"
#include <fcntl.h>
#include <sys/mman.h>

using namespace amdgpu;

Device::Device() {
  if (!shader::spv::validate(g_rdna_semantic_spirv)) {
    shader::spv::dump(g_rdna_semantic_spirv, true);
    rx::die("builtin semantic validation failed");
  }

  if (auto sem = shader::spv::deserialize(
          shaderSemanticContext, g_rdna_semantic_spirv,
          shaderSemanticContext.getUnknownLocation())) {
    auto shaderSemantic = *sem;
    shader::gcn::canonicalizeSemantic(shaderSemanticContext, shaderSemantic);
    shader::gcn::collectSemanticModuleInfo(gcnSemanticModuleInfo,
                                           shaderSemantic);
    gcnSemantic = shader::gcn::collectSemanticInfo(gcnSemanticModuleInfo);
  } else {
    rx::die("failed to deserialize builtin semantics\n");
  }

  for (int index = 0; auto &cache : caches) {
    cache.vmId = index++;
  }

  for (auto &pipe : graphicsPipes) {
    pipe.device = this;
  }

  // for (auto &pipe : computePipes) {
  //   pipe.device = this;
  // }
}

Device::~Device() {
  for (auto fd : dmemFd) {
    if (fd >= 0) {
      ::close(fd);
    }
  }

  for (auto &[pid, info] : processInfo) {
    if (info.vmFd >= 0) {
      ::close(info.vmFd);
    }
  }
}

void Device::mapProcess(std::int64_t pid, int vmId, const char *shmName) {
  auto &process = processInfo[pid];
  process.vmId = vmId;

  auto memory = amdgpu::RemoteMemory{vmId};

  std::string pidVmName = shmName;
  pidVmName += '-';
  pidVmName += std::to_string(pid);
  int memoryFd = ::shm_open(pidVmName.c_str(), O_RDWR, S_IRUSR | S_IWUSR);
  process.vmFd = memoryFd;

  if (memoryFd < 0) {
    std::printf("failed to process %x shared memory\n", (int)pid);
    std::abort();
  }

  for (auto [startAddress, endAddress, slot] : process.vmTable) {
    auto gpuProt = slot.prot >> 4;
    if (gpuProt == 0) {
      continue;
    }

    auto devOffset = slot.offset + startAddress - slot.baseAddress;
    int mapFd = memoryFd;

    if (slot.memoryType >= 0) {
      mapFd = dmemFd[slot.memoryType];
    }

    auto mmapResult =
        ::mmap(memory.getPointer(startAddress), endAddress - startAddress,
               gpuProt, MAP_FIXED | MAP_SHARED, mapFd, devOffset);

    if (mmapResult == MAP_FAILED) {
      std::printf("failed to map process %x memory, address %lx-%lx, type %x\n",
                  (int)pid, startAddress, endAddress, slot.memoryType);
      std::abort();
    }

    handleProtectChange(vmId, startAddress, endAddress - startAddress,
                        slot.prot);
  }
}

void Device::unmapProcess(std::int64_t pid) {
  auto &process = processInfo[pid];
  auto startAddress = static_cast<std::uint64_t>(process.vmId) << 40;
  auto size = static_cast<std::uint64_t>(1) << 40;
  rx::mem::reserve(reinterpret_cast<void *>(startAddress), size);

  ::close(process.vmFd);
  process.vmFd = -1;
  process.vmId = -1;
}

void Device::protectMemory(int pid, std::uint64_t address, std::uint64_t size,
                           int prot) {
  auto &process = processInfo[pid];

  auto vmSlotIt = process.vmTable.queryArea(address);
  if (vmSlotIt == process.vmTable.end()) {
    std::abort();
  }

  auto vmSlot = (*vmSlotIt).payload;

  process.vmTable.map(address, address + size,
                      VmMapSlot{
                          .memoryType = vmSlot.memoryType,
                          .prot = static_cast<int>(prot),
                          .offset = vmSlot.offset,
                          .baseAddress = vmSlot.baseAddress,
                      });

  if (process.vmId >= 0) {
    auto memory = amdgpu::RemoteMemory{process.vmId};
    rx::mem::protect(memory.getPointer(address), size, prot >> 4);
    handleProtectChange(process.vmId, address, size, prot);
  }
}

void Device::onCommandBuffer(std::int64_t pid, int cmdHeader,
                             std::uint64_t address, std::uint64_t size) {
  auto &process = processInfo[pid];
  if (process.vmId < 0) {
    return;
  }

  auto memory = RemoteMemory{process.vmId};

  auto op = rx::getBits(cmdHeader, 15, 8);

  if (op == gnm::IT_INDIRECT_BUFFER_CNST) {
    graphicsPipes[0].setCeQueue(Queue::createFromRange(
        process.vmId, memory.getPointer<std::uint32_t>(address),
        size / sizeof(std::uint32_t)));
  } else if (op == gnm::IT_INDIRECT_BUFFER) {
    graphicsPipes[0].setDeQueue(
        Queue::createFromRange(process.vmId,
                               memory.getPointer<std::uint32_t>(address),
                               size / sizeof(std::uint32_t)),
        1);
  } else {
    rx::die("unimplemented command buffer %x", cmdHeader);
  }
}

bool Device::processPipes() {
  bool allProcessed = true;

  // for (auto &pipe : computePipes) {
  //   if (!pipe.processAllRings()) {
  //     allProcessed = false;
  //   }
  // }

  for (auto &pipe : graphicsPipes) {
    if (!pipe.processAllRings()) {
      allProcessed = false;
    }
  }

  return allProcessed;
}

static void
transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image,
                      VkImageLayout oldLayout, VkImageLayout newLayout,
                      const VkImageSubresourceRange &subresourceRange) {
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = subresourceRange;

  auto layoutToStageAccess = [](VkImageLayout layout)
      -> std::pair<VkPipelineStageFlags, VkAccessFlags> {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    case VK_IMAGE_LAYOUT_GENERAL:
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
      std::abort();
    }
  };

  auto [sourceStage, sourceAccess] = layoutToStageAccess(oldLayout);
  auto [destinationStage, destinationAccess] = layoutToStageAccess(newLayout);

  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

bool Device::flip(std::int64_t pid, int bufferIndex, std::uint64_t arg,
                  VkCommandBuffer commandBuffer, VkImage swapchainImage,
                  VkImageView swapchainImageView, VkFence fence) {
  auto &pipe = graphicsPipes[0];
  auto &scheduler = pipe.scheduler;
  auto &process = processInfo[pid];
  if (process.vmId < 0) {
    return false;
  }

  auto &buffer = process.buffers[bufferIndex];
  auto &bufferAttr = process.bufferAttributes[buffer.attrId];

  gnm::DataFormat dfmt;
  gnm::NumericFormat nfmt;
  CbCompSwap compSwap;
  switch (bufferAttr.pixelFormat) {
  case 0x80000000:
    // bgra
    dfmt = gnm::kDataFormat8_8_8_8;
    nfmt = gnm::kNumericFormatSNormNoZero;
    compSwap = CbCompSwap::Alt;
    break;

  case 0x80002200:
    // rgba
    dfmt = gnm::kDataFormat8_8_8_8;
    nfmt = gnm::kNumericFormatSNormNoZero;
    compSwap = CbCompSwap::Std;
    break;

  case 0x88060000:
    // bgra
    dfmt = gnm::kDataFormat2_10_10_10;
    nfmt = gnm::kNumericFormatSNormNoZero;
    compSwap = CbCompSwap::Alt;
    break;

  default:
    rx::die("unimplemented color buffer format %x", bufferAttr.pixelFormat);
  }

  // std::printf("displaying buffer %lx\n", buffer.address);
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(commandBuffer, &beginInfo);

  auto cacheTag = getCacheTag(process.vmId, scheduler);

  if (false) {
    transitionImageLayout(commandBuffer, swapchainImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .levelCount = 1,
                              .layerCount = 1,
                          });

    amdgpu::flip(
        cacheTag, commandBuffer, vk::context->swapchainExtent, buffer.address,
        swapchainImageView, {bufferAttr.width, bufferAttr.height}, compSwap,
        getDefaultTileModes()[bufferAttr.tilingMode == 1 ? 10 : 8], dfmt, nfmt);

    transitionImageLayout(commandBuffer, swapchainImage,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .levelCount = 1,
                              .layerCount = 1,
                          });
  } else {
    ImageKey frameKey{
        .readAddress = buffer.address,
        .type = gnm::TextureType::Dim2D,
        .dfmt = dfmt,
        .nfmt = nfmt,
        .tileMode = getDefaultTileModes()[bufferAttr.tilingMode == 1 ? 10 : 8],
        .extent =
            {
                .width = bufferAttr.width,
                .height = bufferAttr.height,
                .depth = 1,
            },
        .pitch = bufferAttr.width,
        .mipCount = 1,
        .arrayLayerCount = 1,
    };

    auto image = cacheTag.getImage(frameKey, Access::Read);

    scheduler.submit();
    scheduler.wait();

    transitionImageLayout(commandBuffer, swapchainImage,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .levelCount = 1,
                              .layerCount = 1,
                          });

    VkImageBlit region{
        .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .srcOffsets = {{},
                       {static_cast<int32_t>(bufferAttr.width),
                        static_cast<int32_t>(bufferAttr.height), 1}},
        .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .mipLevel = 0,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
        .dstOffsets =
            {{},
             {static_cast<int32_t>(vk::context->swapchainExtent.width),
              static_cast<int32_t>(vk::context->swapchainExtent.height), 1}},
    };

    vkCmdBlitImage(commandBuffer, image.handle, VK_IMAGE_LAYOUT_GENERAL,
                   swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                   &region, VK_FILTER_LINEAR);

    transitionImageLayout(commandBuffer, swapchainImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          {
                              .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .levelCount = 1,
                              .layerCount = 1,
                          });
  }

  auto submitCompleteTask = scheduler.createExternalSubmit();

  {
    vkEndCommandBuffer(commandBuffer);

    VkSemaphoreSubmitInfo signalSemSubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk::context->renderCompleteSemaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = scheduler.getSemaphoreHandle(),
            .value = submitCompleteTask,
            .stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
        },
    };

    VkSemaphoreSubmitInfo waitSemSubmitInfos[] = {
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = vk::context->presentCompleteSemaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = scheduler.getSemaphoreHandle(),
            .value = submitCompleteTask - 1,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        },
    };

    VkCommandBufferSubmitInfo cmdBufferSubmitInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = commandBuffer,
    };

    VkSubmitInfo2 submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount = 1,
        .pWaitSemaphoreInfos = waitSemSubmitInfos,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &cmdBufferSubmitInfo,
        .signalSemaphoreInfoCount = 2,
        .pSignalSemaphoreInfos = signalSemSubmitInfos,
    };

    vkQueueSubmit2(vk::context->presentQueue, 1, &submitInfo, fence);
    vkQueueWaitIdle(vk::context->presentQueue);
  }

  scheduler.then([=, this, cacheTag = std::move(cacheTag)] {
    bridge->flipBuffer[process.vmId] = bufferIndex;
    bridge->flipArg[process.vmId] = arg;
    bridge->flipCount[process.vmId] = bridge->flipCount[process.vmId] + 1;

    auto mem = RemoteMemory{process.vmId};
    auto bufferInUse =
        mem.getPointer<std::uint64_t>(bridge->bufferInUseAddress[process.vmId]);
    if (bufferInUse != nullptr) {
      bufferInUse[bufferIndex] = 0;
    }
  });

  return true;
}

void Device::mapMemory(std::int64_t pid, std::uint64_t address,
                       std::uint64_t size, int memoryType, int dmemIndex,
                       int prot, std::int64_t offset) {
  auto &process = processInfo[pid];

  process.vmTable.map(address, address + size,
                      VmMapSlot{
                          .memoryType = memoryType >= 0 ? dmemIndex : -1,
                          .prot = prot,
                          .offset = offset,
                          .baseAddress = address,
                      });

  if (process.vmId < 0) {
    return;
  }

  auto memory = amdgpu::RemoteMemory{process.vmId};

  int mapFd = process.vmFd;

  if (memoryType >= 0) {
    mapFd = dmemFd[dmemIndex];
  }

  auto mmapResult = ::mmap(memory.getPointer(address), size, prot >> 4,
                           MAP_FIXED | MAP_SHARED, mapFd, offset);

  if (mmapResult == MAP_FAILED) {
    rx::die("failed to map process %x memory, address %lx-%lx, type %x",
            (int)pid, address, address + size, memoryType);
  }

  handleProtectChange(process.vmId, address, size, prot);
}

void Device::registerBuffer(std::int64_t pid, bridge::CmdBuffer buffer) {
  auto &process = processInfo[pid];

  if (buffer.attrId >= 10 || buffer.index >= 10) {
    rx::die("out of buffers %u, %u", buffer.attrId, buffer.index);
  }

  process.buffers[buffer.index] = buffer;
}

void Device::registerBufferAttribute(std::int64_t pid,
                                     bridge::CmdBufferAttribute attr) {
  auto &process = processInfo[pid];
  if (attr.attrId >= 10) {
    rx::die("out of buffer attributes %u", attr.attrId);
  }

  process.bufferAttributes[attr.attrId] = attr;
}

void Device::handleProtectChange(int vmId, std::uint64_t address,
                                 std::uint64_t size, int prot) {}
