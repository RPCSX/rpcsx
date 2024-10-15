#pragma once

#include "Pipe.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/constants.hpp"
#include "rx/AddressRange.hpp"
#include "shader/Access.hpp"
#include "shader/Evaluator.hpp"
#include "shader/GcnConverter.hpp"
#include <algorithm>
#include <map>
#include <memory>
#include <print>
#include <rx/ConcurrentBitPool.hpp>
#include <rx/MemoryTable.hpp>
#include <shader/gcn.hpp>
#include <utility>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
using Access = shader::Access;

struct ShaderKey {
  std::uint64_t address;
  shader::gcn::Stage stage;
  shader::gcn::Environment env;
};

enum class ImageKind : std::uint8_t { Color, Depth, Stencil };

struct ImageKey {
  std::uint64_t readAddress;
  std::uint64_t writeAddress;
  gnm::TextureType type;
  gnm::DataFormat dfmt;
  gnm::NumericFormat nfmt;
  TileMode tileMode = {};
  VkExtent3D extent = {1, 1, 1};
  std::uint32_t pitch = 1;
  unsigned baseMipLevel = 0;
  unsigned mipCount = 1;
  unsigned baseArrayLayer = 0;
  unsigned arrayLayerCount = 1;
  ImageKind kind = ImageKind::Color;
  bool pow2pad = false;

  static ImageKey createFrom(const gnm::TBuffer &tbuffer);
};

struct SamplerKey {
  VkFilter magFilter;
  VkFilter minFilter;
  VkSamplerMipmapMode mipmapMode;
  VkSamplerAddressMode addressModeU;
  VkSamplerAddressMode addressModeV;
  VkSamplerAddressMode addressModeW;
  float mipLodBias;
  float maxAnisotropy;
  VkCompareOp compareOp;
  float minLod;
  float maxLod;
  VkBorderColor borderColor;
  bool anisotropyEnable;
  bool compareEnable;
  bool unnormalizedCoordinates;

  static SamplerKey createFrom(const gnm::SSampler &sampler);

  auto operator<=>(const SamplerKey &other) const = default;
};

struct Cache {
  enum class EntryType {
    HostVisibleBuffer,
    DeviceLocalBuffer,
    IndexBuffer,
    Image,
    Shader,

    Count
  };

  static constexpr std::array kGraphicsStages = {
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
  };

  static constexpr std::array kDescriptorBindings = {
      VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      VK_DESCRIPTOR_TYPE_SAMPLER,
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 1 * 1000),
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 2 * 1000),
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 3 * 1000),
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
  };

  static constexpr int getStageIndex(VkShaderStageFlagBits stage) {
    if (stage == VK_SHADER_STAGE_COMPUTE_BIT) {
      return 0;
    }

    auto it = std::find(kGraphicsStages.begin(), kGraphicsStages.end(), stage);

    if (it == kGraphicsStages.end()) {
      return -1;
    }

    return it - kGraphicsStages.begin();
  }

  static constexpr int getDescriptorBinding(VkDescriptorType type,
                                            int dim = 0) {
    auto it = std::find(kDescriptorBindings.begin(), kDescriptorBindings.end(),
                        type + dim * 1000);

    if (it == kDescriptorBindings.end()) {
      return -1;
    }

    return it - kDescriptorBindings.begin();
  }

  enum class TagId : std::uint64_t {};
  struct Entry;

  struct Shader {
    VkShaderEXT handle = VK_NULL_HANDLE;
    shader::gcn::ShaderInfo *info;
    VkShaderStageFlagBits stage;
  };

  struct Sampler {
    VkSampler handle = VK_NULL_HANDLE;
  };

  struct Buffer {
    VkBuffer handle = VK_NULL_HANDLE;
    std::uint64_t offset;
    std::uint64_t deviceAddress;
    TagId tagId;
    std::byte *data;
  };

  struct IndexBuffer {
    VkBuffer handle = VK_NULL_HANDLE;
    std::uint64_t offset;
    std::uint32_t indexCount;
    gnm::PrimitiveType primType;
    gnm::IndexType indexType;
  };

  struct Image {
    VkImage handle = VK_NULL_HANDLE;
    Entry *entry;
    VkFormat format;
    VkImageSubresourceRange subresource;
  };

  struct ImageView {
    VkImageView handle = VK_NULL_HANDLE;
    VkImage imageHandle;
    VkFormat format;
    VkImageSubresourceRange subresource;
  };

  struct Tag;

private:
  struct MemoryTableSlot {
    std::uint64_t address;
    union {
      struct {
        std::uint64_t size : 40;
        std::uint64_t flags : 4;
      };
      std::uint64_t sizeAndFlags;
    };
    std::uint64_t deviceAddress;
  };

  struct MemoryTable {
    std::uint32_t count;
    std::uint32_t pad;
    MemoryTableSlot slots[];
  };

  struct ShaderResources : shader::eval::Evaluator {
    std::map<std::uint32_t, std::uint32_t> slotResources;
    std::span<const std::uint32_t> userSgprs;
    Tag *cacheTag = nullptr;

    std::uint32_t slotOffset = 0;
    rx::MemoryTableWithPayload<Access> bufferMemoryTable;
    std::vector<std::pair<std::uint32_t, std::uint64_t>> resourceSlotToAddress;
    std::vector<Cache::Sampler> samplerResources;
    std::vector<Cache::ImageView> imageResources[3];

    using Evaluator::eval;

    void clear() {
      slotResources.clear();
      userSgprs = {};
      cacheTag = nullptr;
      slotOffset = 0;
      bufferMemoryTable.clear();
      resourceSlotToAddress.clear();
      samplerResources.clear();
      for (auto &res : imageResources) {
        res.clear();
      }

      Evaluator::invalidate();
    }

    void loadResources(shader::gcn::Resources &res,
                       std::span<const std::uint32_t> userSgprs);
    void buildMemoryTable(MemoryTable &memoryTable);
    std::uint32_t getResourceSlot(std::uint32_t id);

    template <typename T> T readPointer(std::uint64_t address) {
      T result{};
      cacheTag->readMemory(
          &result, rx::AddressRange::fromBeginSize(address, sizeof(result)));
      return result;
    }

    shader::eval::Value
    eval(shader::ir::InstructionId instId,
         std::span<const shader::ir::Operand> operands) override;
  };

  struct TagStorage {
    struct MemoryTableConfigSlot {
      std::uint32_t bufferIndex;
      std::uint32_t configIndex;
      std::uint32_t resourceSlot;
    };

    std::vector<std::shared_ptr<Entry>> mAcquiredImageResources;
    std::vector<std::shared_ptr<Entry>> mAcquiredMemoryResources;
    std::vector<std::shared_ptr<Entry>> mAcquiredViewResources;
    std::vector<MemoryTableConfigSlot> memoryTableConfigSlots;
    std::vector<std::uint32_t *> descriptorBuffers;
    ShaderResources shaderResources;

    TagStorage() = default;
    TagStorage(const TagStorage &) = delete;

    void clear() {
      mAcquiredImageResources.clear();
      mAcquiredMemoryResources.clear();
      memoryTableConfigSlots.clear();
      descriptorBuffers.clear();
      shaderResources.clear();
    }
  };

  struct TagData {
    TagStorage *mStorage = nullptr;
    Scheduler *mScheduler = nullptr;
    Cache *mParent = nullptr;
    std::unique_lock<std::mutex> mResourcesLock;
    TagId mTagId{};
    std::uint32_t mAcquiredMemoryTable = -1;
  };

public:
  struct Tag : protected TagData {
    Tag(const Tag &) = delete;
    Tag() noexcept = default;
    Tag(Tag &&other) noexcept { swap(other); }
    Tag &operator=(Tag &&other) noexcept {
      swap(other);
      return *this;
    }
    ~Tag() { release(); }

    void swap(Tag &other) noexcept {
      std::swap(static_cast<TagData &>(*this), static_cast<TagData &>(other));
    }

    Shader getShader(const ShaderKey &key,
                     const ShaderKey *dependedKey = nullptr);

    TagId getReadId() const { return TagId{std::uint64_t(mTagId) - 1}; }
    TagId getWriteId() const { return mTagId; }

    Cache *getCache() const { return mParent; }
    Device *getDevice() const { return mParent->mDevice; }
    Scheduler &getScheduler() const { return *mScheduler; }
    int getVmId() const { return mParent->mVmId; }

    Buffer getInternalHostVisibleBuffer(std::uint64_t size);
    Buffer getInternalDeviceLocalBuffer(std::uint64_t size);

    void unlock() { mResourcesLock.unlock(); }

    void buildDescriptors(VkDescriptorSet descriptorSet);

    Sampler getSampler(const SamplerKey &key);
    Buffer getBuffer(rx::AddressRange range, Access access);
    IndexBuffer getIndexBuffer(std::uint64_t address, std::uint32_t offset,
                               std::uint32_t indexCount,
                               gnm::PrimitiveType primType,
                               gnm::IndexType indexType);
    Image getImage(const ImageKey &key, Access access);
    ImageView getImageView(const ImageKey &key, Access access);
    void readMemory(void *target, rx::AddressRange range);
    void writeMemory(const void *source, rx::AddressRange range);
    int compareMemory(const void *source, rx::AddressRange range);
    void release();

    [[nodiscard]] VkPipelineLayout getGraphicsPipelineLayout() const {
      return getCache()->getGraphicsPipelineLayout();
    }

    [[nodiscard]] VkPipelineLayout getComputePipelineLayout() const {
      return getCache()->getComputePipelineLayout();
    }

    Buffer getMemoryTable() {
      if (mAcquiredMemoryTable + 1 == 0) {
        mAcquiredMemoryTable = mParent->mMemoryTablePool.acquire();
      }

      auto &buffer = mParent->mMemoryTableBuffer;
      auto offset = mAcquiredMemoryTable * kMemoryTableSize;

      Buffer result{
          .offset = offset,
          .deviceAddress = buffer.getAddress() + offset,
          .tagId = getReadId(),
          .data = buffer.getData() + offset,
      };

      return result;
    }

    std::shared_ptr<Entry> findShader(const ShaderKey &key,
                                      const ShaderKey *dependedKey = nullptr);
    friend Cache;
  };

  struct GraphicsTag : Tag {
    GraphicsTag() = default;
    GraphicsTag(GraphicsTag &&other) noexcept { swap(other); }
    GraphicsTag &operator=(GraphicsTag &&other) noexcept {
      swap(other);
      return *this;
    }
    ~GraphicsTag() { release(); }

    std::array<VkDescriptorSet, kGraphicsStages.size()> getDescriptorSets() {
      if (mAcquiredGraphicsDescriptorSet + 1 == 0) {
        mAcquiredGraphicsDescriptorSet =
            mParent->mGraphicsDescriptorSetPool.acquire();
      }

      return mParent->mGraphicsDescriptorSets[mAcquiredGraphicsDescriptorSet];
    }

    Shader getShader(shader::gcn::Stage stage, const SpiShaderPgm &pgm,
                     const Registers::Context &context,
                     std::uint32_t indexOffset, gnm::PrimitiveType vsPrimType,
                     std::span<const VkViewport> viewPorts,
                     std::span<const shader::gcn::PsVGprInput> psVgprInput);

    Shader getPixelShader(const SpiShaderPgm &pgm,
                          const Registers::Context &context,
                          std::span<const VkViewport> viewPorts);

    Shader getVertexShader(shader::gcn::Stage stage, const SpiShaderPgm &pgm,
                           const Registers::Context &context,
                           std::uint32_t indexOffset,
                           gnm::PrimitiveType vsPrimType,
                           std::span<const VkViewport> viewPorts);
    void release();

    void swap(GraphicsTag &other) noexcept {
      Tag::swap(other);
      std::swap(mAcquiredGraphicsDescriptorSet,
                other.mAcquiredGraphicsDescriptorSet);
    }

  private:
    std::uint32_t mAcquiredGraphicsDescriptorSet = -1;
  };

  struct ComputeTag : Tag {
    ComputeTag() = default;
    ComputeTag(ComputeTag &&other) noexcept { swap(other); }
    ComputeTag &operator=(ComputeTag &&other) noexcept {
      swap(other);
      return *this;
    }
    ~ComputeTag() { release(); }

    Shader getShader(const Registers::ComputeConfig &pgm);

    VkDescriptorSet getDescriptorSet() {
      if (mAcquiredComputeDescriptorSet + 1 == 0) {
        mAcquiredComputeDescriptorSet =
            mParent->mComputeDescriptorSetPool.acquire();
      }

      return mParent->mComputeDescriptorSets[mAcquiredComputeDescriptorSet];
    }

    void release();

    void swap(ComputeTag &other) noexcept {
      Tag::swap(other);
      std::swap(mAcquiredComputeDescriptorSet,
                other.mAcquiredComputeDescriptorSet);
    }

  private:
    std::uint32_t mAcquiredComputeDescriptorSet = -1;
  };

private:
  template <typename T> T createTagImpl(Scheduler &scheduler) {
    T result;

    auto id = mNextTagId.load(std::memory_order::acquire);
    while (!mNextTagId.compare_exchange_weak(
        id, TagId{static_cast<std::uint64_t>(id) + 2},
        std::memory_order::release, std::memory_order::relaxed)) {
    }

    auto storageIndex = mTagStoragePool.acquire();

    // std::println("acquire tag storage {}", storageIndex);
    result.mStorage = mTagStorages + storageIndex;
    result.mTagId = id;
    result.mParent = this;
    result.mScheduler = &scheduler;

    std::unique_lock<std::mutex> lock(mResourcesMtx);
    result.mResourcesLock = std::move(lock);

    return result;
  }

public:
  Cache(Device *device, int vmId);
  ~Cache();

  Tag createTag(Scheduler &scheduler) { return createTagImpl<Tag>(scheduler); }
  GraphicsTag createGraphicsTag(Scheduler &scheduler) {
    return createTagImpl<GraphicsTag>(scheduler);
  }
  ComputeTag createComputeTag(Scheduler &scheduler) {
    return createTagImpl<ComputeTag>(scheduler);
  }

  vk::Buffer &getGdsBuffer() { return mGdsBuffer; }

  void addFrameBuffer(Scheduler &scheduler, int index, std::uint64_t address,
                      std::uint32_t width, std::uint32_t height, int format,
                      TileMode tileMode);
  void removeFrameBuffer(Scheduler &scheduler, int index);
  VkImage getFrameBuffer(Scheduler &scheduler, int index);

  void flush(Tag &tag, rx::AddressRange range);
  void flush(Scheduler &sched, rx::AddressRange range) {
    auto tag = createTag(sched);
    flush(tag, range);
  }

  void invalidate(Tag &tag, rx::AddressRange range);
  void invalidate(Scheduler &sched, rx::AddressRange range) {
    auto tag = createTag(sched);
    invalidate(tag, range);
  }

  [[nodiscard]] VkPipelineLayout getGraphicsPipelineLayout() const {
    return mGraphicsPipelineLayout;
  }

  [[nodiscard]] VkPipelineLayout getComputePipelineLayout() const {
    return mComputePipelineLayout;
  }

  [[nodiscard]] auto &getGraphicsDescriptorSetLayouts() const {
    return mGraphicsDescriptorSetLayouts;
  }

  void trackUpdate(EntryType type, rx::AddressRange range,
                   std::shared_ptr<Entry> entry, TagId tagId,
                   bool watchChanges);

  void trackWrite(rx::AddressRange range, TagId tagId, bool lockMemory);

  [[nodiscard]] bool isInSync(rx::AddressRange range, TagId expTagId) {
    auto syncIt = mSyncTable.queryArea(range.beginAddress());
    return syncIt != mSyncTable.end() && syncIt.range().contains(range) &&
           syncIt.get() == expTagId;
  }

  auto &getTable(EntryType type) { return mTables[static_cast<int>(type)]; }
  rx::AddressRange flushImages(Tag &tag, rx::AddressRange range);
  rx::AddressRange flushBuffers(rx::AddressRange range);

private:
  std::shared_ptr<Entry> getInSyncEntry(EntryType type, rx::AddressRange range);

  Device *mDevice;
  int mVmId;
  std::atomic<TagId> mNextTagId{TagId{2}};
  vk::Buffer mGdsBuffer;

  static constexpr auto kMemoryTableSize = 0x10000;
  static constexpr auto kMemoryTableCount = 64;
  static constexpr auto kDescriptorSetCount = 128;
  static constexpr auto kTagStorageCount = 128;

  rx::ConcurrentBitPool<kMemoryTableCount> mMemoryTablePool;
  vk::Buffer mMemoryTableBuffer;

  std::array<VkDescriptorSetLayout, kGraphicsStages.size()>
      mGraphicsDescriptorSetLayouts{};
  VkDescriptorSetLayout mComputeDescriptorSetLayout{};
  VkPipelineLayout mGraphicsPipelineLayout{};
  VkPipelineLayout mComputePipelineLayout{};
  VkDescriptorPool mDescriptorPool{};

  rx::ConcurrentBitPool<kDescriptorSetCount> mGraphicsDescriptorSetPool;
  rx::ConcurrentBitPool<kDescriptorSetCount> mComputeDescriptorSetPool;
  rx::ConcurrentBitPool<kTagStorageCount> mTagStoragePool;
  std::array<VkDescriptorSet, kGraphicsStages.size()>
      mGraphicsDescriptorSets[kDescriptorSetCount];
  VkDescriptorSet mComputeDescriptorSets[kDescriptorSetCount];
  TagStorage mTagStorages[kTagStorageCount];
  std::map<SamplerKey, VkSampler> mSamplers;

  std::shared_ptr<Entry> mFrameBuffers[10];
  std::mutex mResourcesMtx;

  rx::MemoryTableWithPayload<std::shared_ptr<Entry>>
      mTables[static_cast<std::size_t>(EntryType::Count)];
  rx::MemoryTableWithPayload<TagId> mSyncTable;
};
} // namespace amdgpu
