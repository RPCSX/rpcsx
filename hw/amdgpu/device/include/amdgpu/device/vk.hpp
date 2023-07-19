#pragma once

#include "tiler.hpp"
#include "util/VerifyVulkan.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <span>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace amdgpu::device::vk {
extern VkDevice g_vkDevice;
extern VkAllocationCallbacks *g_vkAllocator;
std::uint32_t findPhysicalMemoryTypeIndex(std::uint32_t typeBits,
                                          VkMemoryPropertyFlags properties);

class DeviceMemory {
  VkDeviceMemory mDeviceMemory = VK_NULL_HANDLE;
  VkDeviceSize mSize = 0;
  unsigned mMemoryTypeIndex = 0;

public:
  DeviceMemory(DeviceMemory &) = delete;
  DeviceMemory(DeviceMemory &&other) { *this = std::move(other); }
  DeviceMemory() = default;

  ~DeviceMemory() {
    if (mDeviceMemory != nullptr) {
      vkFreeMemory(g_vkDevice, mDeviceMemory, g_vkAllocator);
    }
  }

  DeviceMemory &operator=(DeviceMemory &&other) {
    std::swap(mDeviceMemory, other.mDeviceMemory);
    std::swap(mSize, other.mSize);
    std::swap(mMemoryTypeIndex, other.mMemoryTypeIndex);
    return *this;
  }

  VkDeviceMemory getHandle() const { return mDeviceMemory; }
  VkDeviceSize getSize() const { return mSize; }
  unsigned getMemoryTypeIndex() const { return mMemoryTypeIndex; }

  static DeviceMemory AllocateFromType(std::size_t size,
                                       unsigned memoryTypeIndex) {
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;

    DeviceMemory result;
    Verify() << vkAllocateMemory(g_vkDevice, &allocInfo, g_vkAllocator,
                                 &result.mDeviceMemory);
    result.mSize = size;
    result.mMemoryTypeIndex = memoryTypeIndex;
    return result;
  }

  static DeviceMemory Allocate(std::size_t size, unsigned memoryTypeBits,
                               VkMemoryPropertyFlags properties) {
    return AllocateFromType(
        size, findPhysicalMemoryTypeIndex(memoryTypeBits, properties));
  }

  static DeviceMemory Allocate(VkMemoryRequirements requirements,
                               VkMemoryPropertyFlags properties) {
    return AllocateFromType(
        requirements.size,
        findPhysicalMemoryTypeIndex(requirements.memoryTypeBits, properties));
  }

  static DeviceMemory CreateExternalFd(int fd, std::size_t size,
                                       unsigned memoryTypeIndex) {
    VkImportMemoryFdInfoKHR importMemoryInfo{
        VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR,
        nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT,
        fd,
    };

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &importMemoryInfo,
        .allocationSize = size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    DeviceMemory result;
    Verify() << vkAllocateMemory(g_vkDevice, &allocInfo, g_vkAllocator,
                                 &result.mDeviceMemory);
    result.mSize = size;
    result.mMemoryTypeIndex = memoryTypeIndex;
    return result;
  }
  static DeviceMemory
  CreateExternalHostMemory(void *hostPointer, std::size_t size,
                           VkMemoryPropertyFlags properties) {
    VkMemoryHostPointerPropertiesEXT hostPointerProperties = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};

    auto vkGetMemoryHostPointerPropertiesEXT =
        (PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetDeviceProcAddr(
            g_vkDevice, "vkGetMemoryHostPointerPropertiesEXT");

    Verify() << vkGetMemoryHostPointerPropertiesEXT(
        g_vkDevice, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        hostPointer, &hostPointerProperties);

    auto memoryTypeBits = hostPointerProperties.memoryTypeBits;

    VkImportMemoryHostPointerInfoEXT importMemoryInfo = {
        VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
        nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        hostPointer,
    };

    auto memoryTypeIndex =
        findPhysicalMemoryTypeIndex(memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &importMemoryInfo,
        .allocationSize = size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    DeviceMemory result;
    Verify() << vkAllocateMemory(g_vkDevice, &allocInfo, g_vkAllocator,
                                 &result.mDeviceMemory);
    result.mSize = size;
    result.mMemoryTypeIndex = memoryTypeIndex;
    return result;
  }

  void *map(VkDeviceSize offset, VkDeviceSize size) {
    void *result = 0;
    Verify() << vkMapMemory(g_vkDevice, mDeviceMemory, offset, size, 0,
                            &result);

    return result;
  }

  void unmap() { vkUnmapMemory(g_vkDevice, mDeviceMemory); }
};

struct DeviceMemoryRef {
  VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
  VkDeviceSize offset = 0;
  VkDeviceSize size = 0;
  void *data = nullptr;
};

class MemoryResource {
  DeviceMemory mMemory;
  VkMemoryPropertyFlags mProperties = 0;
  std::size_t mSize = 0;
  std::size_t mAllocationOffset = 0;
  char *mData = nullptr;

public:
  MemoryResource(const MemoryResource &) = delete;

  MemoryResource() = default;
  MemoryResource(MemoryResource &&other) = default;
  MemoryResource &operator=(MemoryResource &&other) = default;

  ~MemoryResource() {
    if (mMemory.getHandle() != nullptr && mData != nullptr) {
      vkUnmapMemory(g_vkDevice, mMemory.getHandle());
    }
  }

  void clear() { mAllocationOffset = 0; }

  static MemoryResource CreateFromFd(int fd, std::size_t size) {
    auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    MemoryResource result;
    result.mMemory = DeviceMemory::CreateExternalFd(
        fd, size, findPhysicalMemoryTypeIndex(~0, properties));
    result.mProperties = properties;
    result.mSize = size;

    return result;
  }

  static MemoryResource CreateFromHost(void *data, std::size_t size) {
    auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    MemoryResource result;
    result.mMemory =
        DeviceMemory::CreateExternalHostMemory(data, size, properties);
    result.mProperties = properties;
    result.mSize = size;

    return result;
  }

  static MemoryResource CreateHostVisible(std::size_t size) {
    auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    MemoryResource result;
    result.mMemory = DeviceMemory::Allocate(size, ~0, properties);
    result.mProperties = properties;
    result.mSize = size;

    void *data = nullptr;
    Verify() << vkMapMemory(g_vkDevice, result.mMemory.getHandle(), 0, size, 0,
                            &data);
    result.mData = reinterpret_cast<char *>(data);

    return result;
  }

  static MemoryResource CreateDeviceLocal(std::size_t size) {
    auto properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    MemoryResource result;
    result.mMemory = DeviceMemory::Allocate(size, ~0, properties);
    result.mProperties = properties;
    result.mSize = size;
    return result;
  }

  DeviceMemoryRef allocate(VkMemoryRequirements requirements) {
    if ((requirements.memoryTypeBits & (1 << mMemory.getMemoryTypeIndex())) ==
        0) {
      util::unreachable();
    }

    auto offset = (mAllocationOffset + requirements.alignment - 1) &
                  ~(requirements.alignment - 1);
    mAllocationOffset = offset + requirements.size;
    if (mAllocationOffset > mSize) {
      util::unreachable("out of memory resource");
    }

    return {mMemory.getHandle(), offset, requirements.size, mData};
  }

  DeviceMemoryRef getFromOffset(std::uint64_t offset, std::size_t size) {
    return {mMemory.getHandle(), offset, size, nullptr};
  }

  std::size_t getSize() const { return mSize; }

  explicit operator bool() const { return mMemory.getHandle() != nullptr; }
};

struct Semaphore {
  VkSemaphore mSemaphore = VK_NULL_HANDLE;

public:
  Semaphore(const Semaphore &) = delete;

  Semaphore() = default;
  Semaphore(Semaphore &&other) { *this = std::move(other); }

  Semaphore &operator=(Semaphore &&other) {
    std::swap(mSemaphore, other.mSemaphore);
    return *this;
  }

  ~Semaphore() {
    if (mSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(g_vkDevice, mSemaphore, nullptr);
    }
  }

  static Semaphore Create(std::uint64_t initialValue = 0) {
    VkSemaphoreTypeCreateInfo typeCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
        VK_SEMAPHORE_TYPE_TIMELINE, initialValue};

    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                        &typeCreateInfo, 0};

    Semaphore result;
    Verify() << vkCreateSemaphore(g_vkDevice, &createInfo, nullptr,
                                  &result.mSemaphore);
    return result;
  }

  VkResult wait(std::uint64_t value, uint64_t timeout) const {
    VkSemaphoreWaitInfo waitInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                    nullptr,
                                    VK_SEMAPHORE_WAIT_ANY_BIT,
                                    1,
                                    &mSemaphore,
                                    &value};

    return vkWaitSemaphores(g_vkDevice, &waitInfo, timeout);
  }

  void signal(std::uint64_t value) {
    VkSemaphoreSignalInfo signalInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                                        nullptr, mSemaphore, value};

    Verify() << vkSignalSemaphore(g_vkDevice, &signalInfo);
  }

  std::uint64_t getCounterValue() const {
    std::uint64_t result = 0;
    Verify() << vkGetSemaphoreCounterValue(g_vkDevice, mSemaphore, &result);
    return result;
  }

  VkSemaphore getHandle() const { return mSemaphore; }

  bool operator==(std::nullptr_t) const { return mSemaphore == nullptr; }
  bool operator!=(std::nullptr_t) const { return mSemaphore != nullptr; }
};

struct CommandBuffer {
  VkCommandBuffer mCmdBuffer = VK_NULL_HANDLE;

public:
  CommandBuffer(const CommandBuffer &) = delete;

  CommandBuffer() = default;
  CommandBuffer(CommandBuffer &&other) { *this = std::move(other); }

  CommandBuffer &operator=(CommandBuffer &&other) {
    std::swap(mCmdBuffer, other.mCmdBuffer);
    return *this;
  }

  CommandBuffer(VkCommandPool commandPool,
                VkCommandBufferLevel level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                VkCommandBufferUsageFlagBits flags = {}) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(g_vkDevice, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
  }

  void end() { vkEndCommandBuffer(mCmdBuffer); }

  bool operator==(std::nullptr_t) const { return mCmdBuffer == nullptr; }
  bool operator!=(std::nullptr_t) const { return mCmdBuffer != nullptr; }
};

class Buffer {
  VkBuffer mBuffer = VK_NULL_HANDLE;
  DeviceMemoryRef mMemory;

public:
  Buffer(const Buffer &) = delete;

  Buffer() = default;
  Buffer(Buffer &&other) { *this = std::move(other); }
  ~Buffer() {
    if (mBuffer != nullptr) {
      vkDestroyBuffer(g_vkDevice, mBuffer, g_vkAllocator);
    }
  }

  Buffer &operator=(Buffer &&other) {
    std::swap(mBuffer, other.mBuffer);
    std::swap(mMemory, other.mMemory);
    return *this;
  }

  Buffer(std::size_t size, VkBufferUsageFlags usage,
         VkBufferCreateFlags flags = 0,
         VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
         std::span<const std::uint32_t> queueFamilyIndices = {}) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.flags = flags;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;
    bufferInfo.queueFamilyIndexCount = queueFamilyIndices.size();
    bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();

    Verify() << vkCreateBuffer(g_vkDevice, &bufferInfo, g_vkAllocator,
                               &mBuffer);
  }

  void *getData() const {
    return reinterpret_cast<char *>(mMemory.data) + mMemory.offset;
  }

  static Buffer
  CreateExternal(std::size_t size, VkBufferUsageFlags usage,
                 VkBufferCreateFlags flags = 0,
                 VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                 std::span<const std::uint32_t> queueFamilyIndices = {}) {
    VkExternalMemoryBufferCreateInfo info{
        VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = &info;
    bufferInfo.flags = flags;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = sharingMode;
    bufferInfo.queueFamilyIndexCount = queueFamilyIndices.size();
    bufferInfo.pQueueFamilyIndices = queueFamilyIndices.data();

    Buffer result;

    Verify() << vkCreateBuffer(g_vkDevice, &bufferInfo, g_vkAllocator,
                               &result.mBuffer);

    return result;
  }

  static Buffer
  Allocate(MemoryResource &pool, std::size_t size, VkBufferUsageFlags usage,
           VkBufferCreateFlags flags = 0,
           VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
           std::span<const std::uint32_t> queueFamilyIndices = {}) {
    Buffer result(size, usage, flags, sharingMode, queueFamilyIndices);
    result.allocateAndBind(pool);

    return result;
  }

  VkBuffer getHandle() const { return mBuffer; }
  [[nodiscard]] VkBuffer release() { return std::exchange(mBuffer, nullptr); }

  VkMemoryRequirements getMemoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(g_vkDevice, mBuffer, &requirements);
    return requirements;
  }

  void allocateAndBind(MemoryResource &pool) {
    auto memory = pool.allocate(getMemoryRequirements());
    bindMemory(memory);
  }

  void bindMemory(DeviceMemoryRef memory) {
    Verify() << vkBindBufferMemory(g_vkDevice, mBuffer, memory.deviceMemory,
                                   memory.offset);
    mMemory = memory;
  }

  void copyTo(VkCommandBuffer cmdBuffer, VkBuffer dstBuffer,
              std::span<const VkBufferCopy> regions) {
    vkCmdCopyBuffer(cmdBuffer, mBuffer, dstBuffer, regions.size(),
                    regions.data());

    VkDependencyInfo depInfo = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
  }

  void readFromImage(const void *address, std::uint32_t pixelSize,
                     TileMode tileMode, uint32_t width, uint32_t height,
                     uint32_t depth, uint32_t pitch) {
    if (address == nullptr || tileMode == 0 || getData() == nullptr) {
      return;
    }

    if (tileMode == kTileModeDisplay_LinearAligned) {
      // std::fprintf(stderr, "Unsupported tile mode %x\n", tileMode);
      if (pitch == width) {
        auto imageSize = width * height * depth * pixelSize;
        std::memcpy(getData(), address, imageSize);
        return;
      }

      auto src = reinterpret_cast<const char *>(address);
      auto dst = reinterpret_cast<char *>(getData());

      for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(dst + y * width * pixelSize, src + y * pitch * pixelSize,
                    width * pixelSize);
      }

      return;
    }

    auto src = reinterpret_cast<const char *>(address);
    auto dst = reinterpret_cast<char *>(getData());

    for (uint32_t y = 0; y < height; ++y) {
      auto linearOffset =
          computeLinearElementByteOffset(0, y, 0, 0, pitch, 1, pixelSize, 1);

      for (std::uint32_t x = 0; x + 1 < width; x += 2) {
        auto tiledOffset = computeTiledElementByteOffset(
            tileMode, pixelSize * 8, x, y, 0, kMacroTileMode_1x2_16, 0, 0, 0, 0,
            width, height, 1, pitch, 1);

        std::memcpy(dst + linearOffset, src + tiledOffset, pixelSize * 2);
        linearOffset += pixelSize * 2;
      }
    }
  }

  void writeAsImageTo(void *address, std::uint32_t pixelSize, TileMode tileMode,
                      uint32_t width, uint32_t height, uint32_t depth,
                      uint32_t pitch) {
    if (address == nullptr || tileMode == 0) {
      return;
    }

    if (tileMode == kTileModeDisplay_LinearAligned) {
      // std::fprintf(stderr, "Unsupported tile mode %x\n", tileMode);
      if (pitch == width) {
        auto bufferSize = width * height * depth * pixelSize;
        std::memcpy(address, getData(), bufferSize);
        return;
      }

      auto src = reinterpret_cast<const char *>(getData());
      auto dst = reinterpret_cast<char *>(address);

      for (std::uint32_t y = 0; y < height; ++y) {
        std::memcpy(dst + y * pitch * pixelSize, src + y * width * pixelSize,
                    width * pixelSize);
      }
      return;
    }

    auto src = reinterpret_cast<const char *>(getData());
    auto dst = reinterpret_cast<char *>(address);

    for (uint32_t y = 0; y < height; ++y) {
      for (uint32_t x = 0; x < width; ++x) {
        auto tiledOffset = computeTiledElementByteOffset(
            tileMode, pixelSize * 8, x, y, 0, kMacroTileMode_1x2_16, 0, 0, 0, 0,
            width, height, 1, pitch, 1);

        auto linearOffset =
            computeLinearElementByteOffset(x, y, 0, 0, pitch, 1, pixelSize, 1);

        std::memcpy(dst + tiledOffset, src + linearOffset, pixelSize);
      }
    }
  }

  // const DeviceMemoryRef &getMemory() const { return mMemory; }
  bool operator==(std::nullptr_t) const { return mBuffer == nullptr; }
  bool operator!=(std::nullptr_t) const { return mBuffer != nullptr; }
};

class Image2D;

class ImageRef {
  VkImage mImage = VK_NULL_HANDLE;
  VkFormat mFormat = {};
  VkImageAspectFlags mAspects = {};
  VkImageLayout *mLayout = {};
  unsigned mWidth = 0;
  unsigned mHeight = 0;
  unsigned mDepth = 0;

public:
  ImageRef() = default;
  ImageRef(Image2D &);

  static ImageRef Create(VkImage image, VkFormat format,
                         VkImageAspectFlags aspects, VkImageLayout *layout,
                         unsigned width, unsigned height, unsigned depth) {
    ImageRef result;
    result.mImage = image;
    result.mFormat = format;
    result.mAspects = aspects;
    result.mLayout = layout;
    result.mWidth = width;
    result.mHeight = height;
    result.mDepth = depth;
    return result;
  }

  VkImage getHandle() const { return mImage; }

  VkMemoryRequirements getMemoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(g_vkDevice, mImage, &requirements);
    return requirements;
  }

  void readFromBuffer(VkCommandBuffer cmdBuffer, const Buffer &buffer,
                      VkImageAspectFlags destAspect) {
    transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = destAspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {mWidth, mHeight, 1};

    vkCmdCopyBufferToImage(cmdBuffer, buffer.getHandle(), mImage,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

  void writeToBuffer(VkCommandBuffer cmdBuffer, const Buffer &buffer,
                     VkImageAspectFlags sourceAspect) {
    transitionLayout(cmdBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = sourceAspect;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {mWidth, mHeight, 1};

    vkCmdCopyImageToBuffer(cmdBuffer, mImage,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           buffer.getHandle(), 1, &region);
  }

  [[nodiscard]] Buffer writeToBuffer(VkCommandBuffer cmdBuffer,
                                     MemoryResource &pool,
                                     VkImageAspectFlags sourceAspect) {
    auto transferBuffer = Buffer::Allocate(
        pool, getMemoryRequirements().size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    writeToBuffer(cmdBuffer, transferBuffer, sourceAspect);
    return transferBuffer;
  }

  [[nodiscard]] Buffer read(VkCommandBuffer cmdBuffer, MemoryResource &pool,
                            const void *address, TileMode tileMode,
                            VkImageAspectFlags destAspect, std::uint32_t bpp,
                            std::size_t width = 0, std::size_t height = 0,
                            std::size_t pitch = 0) {
    if (width == 0) {
      width = mWidth;
    }
    if (height == 0) {
      height = mHeight;
    }
    if (pitch == 0) {
      pitch = width;
    }
    auto memSize = getMemoryRequirements().size;
    auto transferBuffer = Buffer::Allocate(
        pool, memSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    transferBuffer.readFromImage(address, bpp, tileMode, width, height, 1,
                                 pitch);

    readFromBuffer(cmdBuffer, transferBuffer, destAspect);

    return transferBuffer;
  }

  void transitionLayout(VkCommandBuffer cmdBuffer, VkImageLayout newLayout) {
    if (*mLayout == newLayout) {
      return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = *mLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = mImage;
    barrier.subresourceRange.aspectMask = mAspects;
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
        return {VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_ACCESS_SHADER_READ_BIT};

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

    auto [sourceStage, sourceAccess] = layoutToStageAccess(*mLayout);
    auto [destinationStage, destinationAccess] = layoutToStageAccess(newLayout);

    barrier.srcAccessMask = sourceAccess;
    barrier.dstAccessMask = destinationAccess;

    vkCmdPipelineBarrier(cmdBuffer, sourceStage, destinationStage, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    *mLayout = newLayout;
  }
};

class Image2D {
  VkImage mImage = VK_NULL_HANDLE;
  VkFormat mFormat = {};
  VkImageAspectFlags mAspects = {};
  VkImageLayout mLayout = {};
  unsigned mWidth = 0;
  unsigned mHeight = 0;

public:
  Image2D(const Image2D &) = delete;

  Image2D() = default;
  Image2D(Image2D &&other) { *this = std::move(other); }

  ~Image2D() {
    if (mImage != nullptr) {
      vkDestroyImage(g_vkDevice, mImage, g_vkAllocator);
    }
  }

  Image2D &operator=(Image2D &&other) {
    std::swap(mImage, other.mImage);
    std::swap(mFormat, other.mFormat);
    std::swap(mAspects, other.mAspects);
    std::swap(mLayout, other.mLayout);
    std::swap(mWidth, other.mWidth);
    std::swap(mHeight, other.mHeight);
    return *this;
  }

  Image2D(uint32_t width, uint32_t height, VkFormat format,
          VkImageUsageFlags usage,
          VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
          VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
          VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
          uint32_t mipLevels = 1, uint32_t arrayLevels = 1,
          VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = arrayLevels;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = samples;
    imageInfo.sharingMode = sharingMode;

    mFormat = format;

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      mAspects |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
      mAspects |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    mLayout = initialLayout;
    mWidth = width;
    mHeight = height;

    Verify() << vkCreateImage(g_vkDevice, &imageInfo, nullptr, &mImage);
  }

  static Image2D
  Allocate(MemoryResource &pool, uint32_t width, uint32_t height,
           VkFormat format, VkImageUsageFlags usage,
           VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
           VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT,
           VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
           uint32_t mipLevels = 1, uint32_t arrayLevels = 1,
           VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED) {

    Image2D result(width, height, format, usage, tiling, samples, sharingMode,
                   mipLevels, arrayLevels, initialLayout);

    result.allocateAndBind(pool);
    return result;
  }

  VkImage getHandle() const { return mImage; }
  [[nodiscard]] VkImage release() { return std::exchange(mImage, nullptr); }

  VkMemoryRequirements getMemoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(g_vkDevice, mImage, &requirements);
    return requirements;
  }

  void allocateAndBind(MemoryResource &pool) {
    auto memory = pool.allocate(getMemoryRequirements());
    bindMemory(memory);
  }

  void bindMemory(DeviceMemoryRef memory) {
    Verify() << vkBindImageMemory(g_vkDevice, mImage, memory.deviceMemory,
                                  memory.offset);
  }

  friend ImageRef;
};

inline ImageRef::ImageRef(Image2D &image) {
  mImage = image.mImage;
  mFormat = image.mFormat;
  mAspects = image.mAspects;
  mLayout = &image.mLayout;
  mWidth = image.mWidth;
  mHeight = image.mHeight;
  mDepth = 1;
}
} // namespace amdgpu::device::vk
