#pragma once

#include "rx/MemoryTable.hpp"
#include "rx/die.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define VK_VERIFY(...)                                                         \
  if (VkResult _ = (__VA_ARGS__); _ != VK_SUCCESS) {                           \
    ::vk::verifyFailed(_, #__VA_ARGS__);                                       \
  }

namespace vk {
void verifyFailed(VkResult result, const char *message);

struct Context {
  VkInstance instance = VK_NULL_HANDLE;
  VkPhysicalDeviceMemoryProperties physicalMemoryProperties;
  VkAllocationCallbacks *allocator = nullptr;
  VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  std::vector<std::string> deviceExtensions;
  std::vector<std::pair<VkQueue, unsigned>> computeQueues;
  std::vector<std::pair<VkQueue, unsigned>> graphicsQueues;
  VkQueue presentQueue = VK_NULL_HANDLE;
  unsigned presentQueueFamily{};

  VkSwapchainKHR swapchain = VK_NULL_HANDLE;
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  VkExtent2D swapchainExtent{};
  std::vector<VkImage> swapchainImages;
  VkFormat swapchainColorFormat = VK_FORMAT_B8G8R8A8_UNORM;
  VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  std::vector<VkImageView> swapchainImageViews;
  std::vector<VkFence> inFlightFences;
  VkSemaphore presentCompleteSemaphore = VK_NULL_HANDLE;
  VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE;
  VkPhysicalDeviceDescriptorBufferPropertiesEXT descriptorBufferProps;

  bool supportsBarycentric = false;
  bool supportsInt8 = false;
  bool supportsInt64Atomics = false;

  Context() = default;
  Context(const Context &) = delete;
  Context(Context &&other) { other.swap(*this); }
  Context &operator=(Context &&other) {
    other.swap(*this);
    return *this;
  }

  ~Context() {
    for (auto imageView : swapchainImageViews) {
      vkDestroyImageView(device, imageView, allocator);
    }

    if (swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device, swapchain, allocator);
    }

    for (auto fence : inFlightFences) {
      vkDestroyFence(device, fence, allocator);
    }

    if (presentCompleteSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, presentCompleteSemaphore, allocator);
    }

    if (renderCompleteSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device, renderCompleteSemaphore, allocator);
    }

    if (device != VK_NULL_HANDLE) {
      vkDestroyDevice(device, allocator);
    }

    if (surface != VK_NULL_HANDLE) {
      vkDestroySurfaceKHR(instance, surface, allocator);
    }

    if (instance != VK_NULL_HANDLE) {
      vkDestroyInstance(instance, allocator);
    }
  }

  void swap(Context &other) {
    std::swap(instance, other.instance);
    std::swap(physicalMemoryProperties, other.physicalMemoryProperties);
    std::swap(allocator, other.allocator);
    std::swap(device, other.device);
    std::swap(physicalDevice, other.physicalDevice);
    std::swap(deviceExtensions, other.deviceExtensions);
    std::swap(computeQueues, other.computeQueues);
    std::swap(graphicsQueues, other.graphicsQueues);
    std::swap(presentQueue, other.presentQueue);
    std::swap(presentQueueFamily, other.presentQueueFamily);

    std::swap(swapchain, other.swapchain);
    std::swap(swapchainExtent, other.swapchainExtent);
    std::swap(swapchainImages, other.swapchainImages);
    std::swap(swapchainColorFormat, other.swapchainColorFormat);
    std::swap(swapchainColorSpace, other.swapchainColorSpace);
    std::swap(swapchainImageViews, other.swapchainImageViews);
  }

  bool hasDeviceExtension(std::string_view ext);
  void createSwapchain();
  void recreateSwapchain();
  void createDevice(VkSurfaceKHR surface, int gpuIndex,
                    std::vector<const char *> requiredExtensions,
                    std::vector<const char *> optionalExtensions);

  static Context create(std::vector<const char *> requiredLayers,
                        std::vector<const char *> optionalLayers,
                        std::vector<const char *> requiredExtensions,
                        std::vector<const char *> optionalExtensions);

  std::uint32_t findPhysicalMemoryTypeIndex(std::uint32_t typeBits,
                                            VkMemoryPropertyFlags properties);
};

extern Context *context;

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
      vkFreeMemory(context->device, mDeviceMemory, context->allocator);
    }
    mDeviceMemory = nullptr;
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
    VkMemoryAllocateFlagsInfo flags{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
    };

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &flags,
        .allocationSize = size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    DeviceMemory result;
    VK_VERIFY(vkAllocateMemory(context->device, &allocInfo, context->allocator,
                               &result.mDeviceMemory));
    result.mSize = size;
    result.mMemoryTypeIndex = memoryTypeIndex;
    return result;
  }

  static DeviceMemory Allocate(std::size_t size, unsigned memoryTypeBits,
                               VkMemoryPropertyFlags properties) {
    return AllocateFromType(
        size, context->findPhysicalMemoryTypeIndex(memoryTypeBits, properties));
  }

  static DeviceMemory Allocate(VkMemoryRequirements requirements,
                               VkMemoryPropertyFlags properties) {
    return AllocateFromType(requirements.size,
                            context->findPhysicalMemoryTypeIndex(
                                requirements.memoryTypeBits, properties));
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
    VK_VERIFY(vkAllocateMemory(context->device, &allocInfo, context->allocator,
                               &result.mDeviceMemory));
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
            context->device, "vkGetMemoryHostPointerPropertiesEXT");

    VK_VERIFY(vkGetMemoryHostPointerPropertiesEXT(
        context->device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        hostPointer, &hostPointerProperties));

    auto memoryTypeBits = hostPointerProperties.memoryTypeBits;

    VkImportMemoryHostPointerInfoEXT importMemoryInfo = {
        VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT,
        nullptr,
        VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,
        hostPointer,
    };

    auto memoryTypeIndex =
        context->findPhysicalMemoryTypeIndex(memoryTypeBits, properties);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &importMemoryInfo,
        .allocationSize = size,
        .memoryTypeIndex = memoryTypeIndex,
    };

    DeviceMemory result;
    VK_VERIFY(vkAllocateMemory(context->device, &allocInfo, context->allocator,
                               &result.mDeviceMemory));
    result.mSize = size;
    result.mMemoryTypeIndex = memoryTypeIndex;
    return result;
  }

  void *map(VkDeviceSize offset, VkDeviceSize size) {
    void *result = 0;
    VK_VERIFY(
        vkMapMemory(context->device, mDeviceMemory, offset, size, 0, &result));
    return result;
  }

  void unmap() { vkUnmapMemory(context->device, mDeviceMemory); }
};

struct DeviceMemoryRef {
  VkDeviceMemory deviceMemory = VK_NULL_HANDLE;
  VkDeviceSize offset = 0;
  VkDeviceSize size = 0;
  void *data = nullptr;
  void *allocator = nullptr;

  void (*release)(DeviceMemoryRef &memoryRef) = nullptr;
};

class MemoryResource {
  DeviceMemory mMemory;
  char *mData = nullptr;
  rx::MemoryAreaTable<> table;
  // const char *debugName = "<unknown>";

  std::mutex mMtx;

public:
  MemoryResource() = default;
  ~MemoryResource() { clear(); }

  void clear() {
    if (mMemory.getHandle() != nullptr && mData != nullptr) {
      vkUnmapMemory(context->device, mMemory.getHandle());
    }
  }

  void free() {
    clear();
    mMemory = {};
  }

  void initFromHost(void *data, std::size_t size) {
    assert(mMemory.getHandle() == nullptr);
    auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    mMemory = DeviceMemory::CreateExternalHostMemory(data, size, properties);
    table.map(0, size);
    // debugName = "direct";
  }

  void initHostVisible(std::size_t size) {
    assert(mMemory.getHandle() == nullptr);
    auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto memory = DeviceMemory::Allocate(size, ~0, properties);

    void *data = nullptr;
    VK_VERIFY(
        vkMapMemory(context->device, memory.getHandle(), 0, size, 0, &data));

    mMemory = std::move(memory);
    table.map(0, size);
    mData = reinterpret_cast<char *>(data);
    // debugName = "host";
  }

  void initDeviceLocal(std::size_t size) {
    assert(mMemory.getHandle() == nullptr);
    auto properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    mMemory = DeviceMemory::Allocate(size, ~0, properties);
    table.map(0, size);
    // debugName = "local";
  }

  DeviceMemoryRef allocate(VkMemoryRequirements requirements) {
    if ((requirements.memoryTypeBits & (1 << mMemory.getMemoryTypeIndex())) ==
        0) {
      std::abort();
    }

    std::lock_guard lock(mMtx);

    for (auto elem : table) {
      auto offset = (elem.beginAddress + requirements.alignment - 1) &
                    ~(requirements.alignment - 1);

      if (offset >= elem.endAddress) {
        continue;
      }

      auto blockSize = elem.endAddress - offset;

      if (blockSize < requirements.size) {
        continue;
      }

      // if (debugName == std::string_view{"local"}) {
      // std::printf("memory: allocation %s memory %lx-%lx\n", debugName,
      // offset,
      //             offset + requirements.size);
      // }

      table.unmap(offset, offset + requirements.size);
      return {mMemory.getHandle(),
              offset,
              requirements.size,
              mData,
              this,
              [](DeviceMemoryRef &memoryRef) {
                auto self =
                    reinterpret_cast<MemoryResource *>(memoryRef.allocator);
                self->deallocate(memoryRef);
              }};
    }

    std::abort();
  }

  void deallocate(DeviceMemoryRef memory) {
    std::lock_guard lock(mMtx);
    table.map(memory.offset, memory.offset + memory.size);
  }

  void dump() {
    std::lock_guard lock(mMtx);

    for (auto elem : table) {
      std::fprintf(stderr, "%zu - %zu\n", elem.beginAddress, elem.endAddress);
    }
  }

  DeviceMemoryRef getFromOffset(std::uint64_t offset, std::size_t size) {
    return {mMemory.getHandle(), offset, size, nullptr, nullptr, nullptr};
  }

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
      vkDestroySemaphore(context->device, mSemaphore, nullptr);
    }
  }

  static Semaphore Create(std::uint64_t initialValue = 0) {
    VkSemaphoreTypeCreateInfo typeCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
        VK_SEMAPHORE_TYPE_TIMELINE, initialValue};

    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                        &typeCreateInfo, 0};

    Semaphore result;
    VK_VERIFY(vkCreateSemaphore(context->device, &createInfo, nullptr,
                                &result.mSemaphore));
    return result;
  }

  VkResult wait(std::uint64_t value, uint64_t timeout) const {
    VkSemaphoreWaitInfo waitInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                    nullptr,
                                    VK_SEMAPHORE_WAIT_ANY_BIT,
                                    1,
                                    &mSemaphore,
                                    &value};

    return vkWaitSemaphores(context->device, &waitInfo, timeout);
  }

  void signal(std::uint64_t value) {
    VkSemaphoreSignalInfo signalInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO,
                                        nullptr, mSemaphore, value};

    VK_VERIFY(vkSignalSemaphore(context->device, &signalInfo));
  }

  [[gnu::used]] std::uint64_t getCounterValue() const {
    std::uint64_t result = 0;
    VK_VERIFY(vkGetSemaphoreCounterValue(context->device, mSemaphore, &result));
    return result;
  }

  VkSemaphore getHandle() const { return mSemaphore; }

  bool operator==(std::nullptr_t) const { return mSemaphore == nullptr; }
  bool operator!=(std::nullptr_t) const { return mSemaphore != nullptr; }
};

struct BinSemaphore {
  VkSemaphore mSemaphore = VK_NULL_HANDLE;

public:
  BinSemaphore(const BinSemaphore &) = delete;

  BinSemaphore() = default;
  BinSemaphore(BinSemaphore &&other) { *this = std::move(other); }

  BinSemaphore &operator=(BinSemaphore &&other) {
    std::swap(mSemaphore, other.mSemaphore);
    return *this;
  }

  ~BinSemaphore() {
    if (mSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(context->device, mSemaphore, nullptr);
    }
  }

  static BinSemaphore Create() {
    VkSemaphoreTypeCreateInfo typeCreateInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO, nullptr,
        VK_SEMAPHORE_TYPE_BINARY, 0};

    VkSemaphoreCreateInfo createInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
                                        &typeCreateInfo, 0};

    BinSemaphore result;
    VK_VERIFY(vkCreateSemaphore(context->device, &createInfo, nullptr,
                                &result.mSemaphore));
    return result;
  }

  VkSemaphore getHandle() const { return mSemaphore; }

  bool operator==(std::nullptr_t) const { return mSemaphore == nullptr; }
};

struct Fence {
  VkFence mFence = VK_NULL_HANDLE;

public:
  Fence(const Fence &) = delete;

  Fence() = default;
  Fence(Fence &&other) { *this = std::move(other); }

  Fence &operator=(Fence &&other) {
    std::swap(mFence, other.mFence);
    return *this;
  }

  ~Fence() {
    if (mFence != VK_NULL_HANDLE) {
      vkDestroyFence(context->device, mFence, nullptr);
    }
  }

  static Fence Create() {
    VkFenceCreateInfo fenceCreateInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                         nullptr, 0};
    Fence result;
    VK_VERIFY(vkCreateFence(context->device, &fenceCreateInfo, nullptr,
                            &result.mFence));
    return result;
  }

  void wait() const {
    VK_VERIFY(vkWaitForFences(context->device, 1, &mFence, 1, UINT64_MAX));
  }

  bool isComplete() const {
    return vkGetFenceStatus(context->device, mFence) == VK_SUCCESS;
  }

  void reset() { vkResetFences(context->device, 1, &mFence); }

  VkFence getHandle() const { return mFence; }

  bool operator==(std::nullptr_t) const { return mFence == nullptr; }
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
                VkCommandBufferUsageFlags flags = {}) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = level;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(context->device, &allocInfo, &mCmdBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = flags;

    vkBeginCommandBuffer(mCmdBuffer, &beginInfo);
  }

  VkCommandBuffer getHandle() const { return mCmdBuffer; }

  operator VkCommandBuffer() const { return mCmdBuffer; }

  void end() { vkEndCommandBuffer(mCmdBuffer); }

  bool operator==(std::nullptr_t) const { return mCmdBuffer == nullptr; }
};

class CommandPool {
  VkCommandPool mHandle = VK_NULL_HANDLE;

public:
  CommandPool(const CommandPool &) = delete;

  CommandPool() = default;
  CommandPool(CommandPool &&other) { *this = std::move(other); }
  ~CommandPool() {
    if (mHandle != nullptr) {
      vkDestroyCommandPool(context->device, mHandle, context->allocator);
    }
  }

  CommandPool &operator=(CommandPool &&other) {
    std::swap(mHandle, other.mHandle);
    return *this;
  }

  static CommandPool Create(uint32_t queueFamilyIndex,
                            VkCommandPoolCreateFlags flags = 0) {
    VkCommandPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = queueFamilyIndex,
    };

    CommandPool result;
    VK_VERIFY(vkCreateCommandPool(context->device, &info, context->allocator,
                                  &result.mHandle));
    return result;
  }

  CommandBuffer createOneTimeSubmitBuffer() {
    return createPrimaryBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  }

  CommandBuffer createPrimaryBuffer(VkCommandBufferUsageFlags flags) {
    return CommandBuffer(mHandle, VK_COMMAND_BUFFER_LEVEL_PRIMARY, flags);
  }

  operator VkCommandPool() const { return mHandle; }
  VkCommandPool getHandle() const { return mHandle; }

  bool operator==(std::nullptr_t) const { return mHandle == nullptr; }
};

class CommandPoolRef {
  VkCommandPool mHandle = VK_NULL_HANDLE;

public:
  CommandPoolRef() = default;
  CommandPoolRef(VkCommandPool handle) : mHandle(handle) {}
  CommandPoolRef(const CommandPool &pool) : mHandle(pool.getHandle()) {}

  CommandBuffer createOneTimeSubmitBuffer() {
    return createPrimaryBuffer(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
  }

  CommandBuffer createPrimaryBuffer(VkCommandBufferUsageFlags flags) {
    return CommandBuffer(mHandle, VK_COMMAND_BUFFER_LEVEL_PRIMARY, flags);
  }

  VkCommandPool getHandle() const { return mHandle; }
  operator VkCommandPool() const { return mHandle; }

  bool operator==(std::nullptr_t) const { return mHandle == nullptr; }
};

class Buffer {
  VkBuffer mBuffer = VK_NULL_HANDLE;
  VkDeviceAddress mAddress{};
  DeviceMemoryRef mMemory;

public:
  Buffer(const Buffer &) = delete;

  Buffer() = default;
  Buffer(Buffer &&other) { *this = std::move(other); }
  ~Buffer() {
    if (mBuffer != nullptr) {
      vkDestroyBuffer(context->device, mBuffer, context->allocator);

      if (mMemory.release != nullptr) {
        mMemory.release(mMemory);
      }
    }
  }

  Buffer &operator=(Buffer &&other) {
    std::swap(mBuffer, other.mBuffer);
    std::swap(mAddress, other.mAddress);
    std::swap(mMemory, other.mMemory);
    return *this;
  }

  Buffer(std::size_t size, VkBufferUsageFlags usage,
         VkBufferCreateFlags flags = 0,
         VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
         std::span<const std::uint32_t> queueFamilyIndices = {}) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .flags = flags,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = sharingMode,
        .queueFamilyIndexCount =
            static_cast<std::uint32_t>(queueFamilyIndices.size()),
        .pQueueFamilyIndices = queueFamilyIndices.data(),
    };

    VK_VERIFY(vkCreateBuffer(context->device, &bufferInfo, context->allocator,
                             &mBuffer));
  }

  operator VkBuffer() const { return mBuffer; }

  std::byte *getData() const {
    rx::dieIf(mMemory.data == nullptr,
              "unexpected Buffer::getData call with device local memory");
    return reinterpret_cast<std::byte *>(mMemory.data) + mMemory.offset;
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

    VK_VERIFY(vkCreateBuffer(context->device, &bufferInfo, context->allocator,
                             &result.mBuffer));

    return result;
  }

  static Buffer
  Allocate(MemoryResource &pool, std::size_t size, VkBufferUsageFlags usage = 0,
           VkBufferCreateFlags flags = 0,
           VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
           std::span<const std::uint32_t> queueFamilyIndices = {}) {
    Buffer result(size, usage, flags, sharingMode, queueFamilyIndices);
    result.allocateAndBind(pool);
    return result;
  }

  VkDeviceAddress getAddress() const { return mAddress; }
  VkBuffer getHandle() const { return mBuffer; }
  [[nodiscard]] VkBuffer release() { return std::exchange(mBuffer, nullptr); }

  VkMemoryRequirements getMemoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(context->device, mBuffer, &requirements);
    return requirements;
  }

  void allocateAndBind(MemoryResource &pool) {
    auto memory = pool.allocate(getMemoryRequirements());
    bindMemory(memory);
  }

  void bindMemory(DeviceMemoryRef memory) {
    VK_VERIFY(vkBindBufferMemory(context->device, mBuffer, memory.deviceMemory,
                                 memory.offset));
    mMemory = memory;

    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = getHandle(),
    };

    mAddress = vkGetBufferDeviceAddress(vk::context->device, &addressInfo);
  }

  void copyTo(VkCommandBuffer cmdBuffer, VkBuffer dstBuffer,
              std::span<const VkBufferCopy> regions) {
    vkCmdCopyBuffer(cmdBuffer, mBuffer, dstBuffer, regions.size(),
                    regions.data());

    VkDependencyInfo depInfo = {.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    vkCmdPipelineBarrier2(cmdBuffer, &depInfo);
  }

  const DeviceMemoryRef &getMemory() const { return mMemory; }
  bool operator==(std::nullptr_t) const { return mBuffer == nullptr; }
  bool operator!=(std::nullptr_t) const { return mBuffer != nullptr; }
};

class Image {
  VkImage mImage = VK_NULL_HANDLE;
  VkImageType mImageType{};
  VkFormat mFormat = {};
  VkImageAspectFlags mAspects = {};
  VkExtent3D mExtent{};
  unsigned mMipLevels = 0;
  unsigned mArrayLayers = 0;
  VkSampleCountFlagBits mSamples = {};
  DeviceMemoryRef mMemory;

public:
  Image(const Image &) = delete;

  Image() = default;
  Image(Image &&other) { *this = std::move(other); }

  ~Image() {
    if (mImage != VK_NULL_HANDLE) {
      vkDestroyImage(context->device, mImage, context->allocator);

      if (mMemory.release != nullptr) {
        mMemory.release(mMemory);
      }
    }
  }

  Image &operator=(Image &&other) {
    std::swap(mImage, other.mImage);
    std::swap(mImageType, other.mImageType);
    std::swap(mFormat, other.mFormat);
    std::swap(mAspects, other.mAspects);
    std::swap(mExtent, other.mExtent);
    std::swap(mMipLevels, other.mMipLevels);
    std::swap(mArrayLayers, other.mArrayLayers);
    std::swap(mSamples, other.mSamples);
    std::swap(mMemory, other.mMemory);
    return *this;
  }

  Image(VkImageType type, VkExtent3D extent, uint32_t mipLevels,
        uint32_t arrayLayers, VkSampleCountFlagBits samples, VkFormat format,
        VkImageUsageFlags usage, VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
        VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED) {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = type,
        .format = format,
        .extent = extent,
        .mipLevels = mipLevels,
        .arrayLayers = arrayLayers,
        .samples = samples,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = sharingMode,
        .initialLayout = initialLayout,
    };

    mImageType = type;
    mFormat = format;
    mExtent = extent;
    mMipLevels = mipLevels;
    mArrayLayers = arrayLayers;
    mSamples = samples;

    if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
      mAspects |= VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
      mAspects |= VK_IMAGE_ASPECT_COLOR_BIT;
    }

    VK_VERIFY(vkCreateImage(context->device, &imageInfo, nullptr, &mImage));
  }

  operator VkImage() const { return mImage; }

  static Image
  Allocate(MemoryResource &pool, VkImageType type, VkExtent3D extent,
           uint32_t mipLevels, uint32_t arrayLayers, VkFormat format,
           VkSampleCountFlagBits samples, VkImageUsageFlags usage,
           VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
           VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE,
           VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED) {

    Image result(type, extent, mipLevels, arrayLayers, samples, format, usage,
                 tiling, sharingMode, initialLayout);

    result.allocateAndBind(pool);
    return result;
  }

  VkExtent3D getExtent() const { return mExtent; }
  VkImageType getImageType() const { return mImageType; }
  VkFormat getFormat() const { return mFormat; }
  VkImageAspectFlags getAspects() { return mAspects; }
  std::uint32_t getWidth() const { return getExtent().width; }
  std::uint32_t getHeight() const { return getExtent().height; }
  std::uint32_t getDepth() const { return getExtent().depth; }
  std::uint32_t getArrayLayers() const { return mArrayLayers; }
  std::uint32_t getMipLevels() const { return mMipLevels; }
  VkSampleCountFlagBits getSamples() const { return mSamples; }

  VkImage getHandle() const { return mImage; }
  [[nodiscard]] VkImage release() { return std::exchange(mImage, nullptr); }

  VkMemoryRequirements getMemoryRequirements() const {
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(context->device, mImage, &requirements);
    return requirements;
  }

  void allocateAndBind(MemoryResource &pool) {
    auto memory = pool.allocate(getMemoryRequirements());
    bindMemory(memory);
  }

  void bindMemory(DeviceMemoryRef memory) {
    VK_VERIFY(vkBindImageMemory(context->device, mImage, memory.deviceMemory,
                                memory.offset));
    mMemory = memory;
  }

  const DeviceMemoryRef &getMemory() const { return mMemory; }
};

struct ImageView {
  VkImageView mHandle = VK_NULL_HANDLE;
  VkImageViewType mType{};
  VkFormat mFormat{};
  VkImageSubresourceRange mSubresourceRange;

public:
  ImageView(const ImageView &) = delete;

  ImageView() = default;
  ImageView(ImageView &&other) { *this = std::move(other); }

  ~ImageView() {
    if (mHandle != nullptr) {
      vkDestroyImageView(context->device, mHandle, context->allocator);
    }
  }

  ImageView &operator=(ImageView &&other) {
    std::swap(mHandle, other.mHandle);
    std::swap(mType, other.mType);
    std::swap(mFormat, other.mFormat);
    std::swap(mSubresourceRange, other.mSubresourceRange);
    return *this;
  }

  ImageView(VkImageViewType type, VkImage image, VkFormat format,
            VkComponentMapping components,
            VkImageSubresourceRange subresourceRange) {
    VkImageViewCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .flags = 0,
        .image = image,
        .viewType = type,
        .format = format,
        .components = components,
        .subresourceRange = subresourceRange,
    };

    VK_VERIFY(vkCreateImageView(context->device, &imageInfo, context->allocator,
                                &mHandle));
  }

  VkImageView getHandle() const { return mHandle; }

  [[nodiscard]] VkImageView release() {
    return std::exchange(mHandle, nullptr);
  }
};

vk::MemoryResource &getHostVisibleMemory();
vk::MemoryResource &getDeviceLocalMemory();

VkResult CreateShadersEXT(VkDevice device, uint32_t createInfoCount,
                          const VkShaderCreateInfoEXT *pCreateInfos,
                          const VkAllocationCallbacks *pAllocator,
                          VkShaderEXT *pShaders);

void DestroyShaderEXT(VkDevice device, VkShaderEXT shader,
                      const VkAllocationCallbacks *pAllocator);

void CmdBindShadersEXT(VkCommandBuffer commandBuffer, uint32_t stageCount,
                       const VkShaderStageFlagBits *pStages,
                       const VkShaderEXT *pShaders);
void CmdSetColorBlendEnableEXT(VkCommandBuffer commandBuffer,
                               uint32_t firstAttachment,
                               uint32_t attachmentCount,
                               const VkBool32 *pColorBlendEnables);
void CmdSetColorBlendEquationEXT(
    VkCommandBuffer commandBuffer, uint32_t firstAttachment,
    uint32_t attachmentCount,
    const VkColorBlendEquationEXT *pColorBlendEquations);

void CmdSetDepthClampEnableEXT(VkCommandBuffer commandBuffer,
                               VkBool32 depthClampEnable);
void CmdSetLogicOpEXT(VkCommandBuffer commandBuffer, VkLogicOp logicOp);
void CmdSetPolygonModeEXT(VkCommandBuffer commandBuffer,
                          VkPolygonMode polygonMode);
void CmdSetAlphaToOneEnableEXT(VkCommandBuffer commandBuffer,
                               VkBool32 alphaToOneEnable);
void CmdSetLogicOpEnableEXT(VkCommandBuffer commandBuffer,
                            VkBool32 logicOpEnable);
void CmdSetRasterizationSamplesEXT(VkCommandBuffer commandBuffer,
                                   VkSampleCountFlagBits rasterizationSamples);
void CmdSetSampleMaskEXT(VkCommandBuffer commandBuffer,
                         VkSampleCountFlagBits samples,
                         const VkSampleMask *pSampleMask);
void CmdSetTessellationDomainOriginEXT(VkCommandBuffer commandBuffer,
                                       VkTessellationDomainOrigin domainOrigin);
void CmdSetAlphaToCoverageEnableEXT(VkCommandBuffer commandBuffer,
                                    VkBool32 alphaToCoverageEnable);
void CmdSetVertexInputEXT(
    VkCommandBuffer commandBuffer, uint32_t vertexBindingDescriptionCount,
    const VkVertexInputBindingDescription2EXT *pVertexBindingDescriptions,
    uint32_t vertexAttributeDescriptionCount,
    const VkVertexInputAttributeDescription2EXT *pVertexAttributeDescriptions);
void CmdSetColorWriteMaskEXT(VkCommandBuffer commandBuffer,
                             uint32_t firstAttachment, uint32_t attachmentCount,
                             const VkColorComponentFlags *pColorWriteMasks);

void GetDescriptorSetLayoutSizeEXT(VkDevice device,
                                   VkDescriptorSetLayout layout,
                                   VkDeviceSize *pLayoutSizeInBytes);

void GetDescriptorSetLayoutBindingOffsetEXT(VkDevice device,
                                            VkDescriptorSetLayout layout,
                                            uint32_t binding,
                                            VkDeviceSize *pOffset);
void GetDescriptorEXT(VkDevice device,
                      const VkDescriptorGetInfoEXT *pDescriptorInfo,
                      size_t dataSize, void *pDescriptor);
void CmdBindDescriptorBuffersEXT(
    VkCommandBuffer commandBuffer, uint32_t bufferCount,
    const VkDescriptorBufferBindingInfoEXT *pBindingInfos);

void CmdSetDescriptorBufferOffsetsEXT(VkCommandBuffer commandBuffer,
                                      VkPipelineBindPoint pipelineBindPoint,
                                      VkPipelineLayout layout,
                                      uint32_t firstSet, uint32_t setCount,
                                      const uint32_t *pBufferIndices,
                                      const VkDeviceSize *pOffsets);

void CmdBindDescriptorBufferEmbeddedSamplersEXT(
    VkCommandBuffer commandBuffer, VkPipelineBindPoint pipelineBindPoint,
    VkPipelineLayout layout, uint32_t set);

VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT messenger,
                                   const VkAllocationCallbacks *pAllocator);
} // namespace vk
