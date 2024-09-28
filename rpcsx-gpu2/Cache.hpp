#pragma once

#include "Pipe.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/constants.hpp"
#include "rx/die.hpp"
#include "shader/Access.hpp"
#include "shader/GcnConverter.hpp"
#include <algorithm>
#include <memory>
#include <mutex>
#include <rx/MemoryTable.hpp>
#include <shader/gcn.hpp>
#include <vulkan/vulkan_core.h>

namespace amdgpu {
using Access = shader::Access;

struct ShaderKey {
  std::uint64_t address;
  shader::gcn::Stage stage;
  shader::gcn::Environment env;
};

enum class ImageKind {
  Color,
  Depth,
  Stencil
};

struct ImageKey {
  std::uint64_t readAddress;
  std::uint64_t writeAddress;
  gnm::TextureType type;
  gnm::DataFormat dfmt;
  gnm::NumericFormat nfmt;
  TileMode tileMode = {};
  VkOffset3D offset = {};
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

struct ImageViewKey : ImageKey {
  gnm::Swizzle R = gnm::Swizzle::R;
  gnm::Swizzle G = gnm::Swizzle::G;
  gnm::Swizzle B = gnm::Swizzle::B;
  gnm::Swizzle A = gnm::Swizzle::A;

  static ImageViewKey createFrom(const gnm::TBuffer &tbuffer);
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
  static constexpr std::array kGraphicsStages = {
      VK_SHADER_STAGE_VERTEX_BIT,
      VK_SHADER_STAGE_GEOMETRY_BIT,
      VK_SHADER_STAGE_FRAGMENT_BIT,
      VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
      VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
  };

  static constexpr std::array kDescriptorBindings = {
      VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      VK_DESCRIPTOR_TYPE_SAMPLER,
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 1 * 1000),
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 2 * 1000),
      VkDescriptorType(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE + 3 * 1000),
      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
  };

  static constexpr int getStageIndex(VkShaderStageFlagBits stage) {
    auto it = std::find(kGraphicsStages.begin(), kGraphicsStages.end(), stage);

    if (it == kGraphicsStages.end()) {
      return -1;
    }

    return it - kGraphicsStages.begin();
  }

  static constexpr int getDescriptorBinding(VkDescriptorType type, int dim = 0) {
    auto it =
        std::find(kDescriptorBindings.begin(), kDescriptorBindings.end(), type + dim * 1000);

    if (it == kDescriptorBindings.end()) {
      return -1;
    }

    return it - kDescriptorBindings.begin();
  }

  enum class TagId : std::uint64_t {};
  struct Entry;

  int vmId = -1;

  struct Shader {
    VkShaderEXT handle;
    shader::gcn::ShaderInfo *info;
    VkShaderStageFlagBits stage;
  };

  struct Sampler {
    VkSampler handle;
  };

  struct Buffer {
    VkBuffer handle;
    std::uint64_t offset;
    std::uint64_t deviceAddress;
    TagId tagId;
    std::byte *data;
  };

  struct IndexBuffer {
    VkBuffer handle;
    std::uint64_t offset;
    std::uint32_t indexCount;
    gnm::PrimitiveType primType;
    gnm::IndexType indexType;
  };

  struct Image {
    VkImage handle;
  };

  struct ImageView {
    VkImageView handle;
    VkImage imageHandle;
  };

  class Tag {
    Cache *mParent = nullptr;
    Scheduler *mScheduler = nullptr;
    TagId mTagId{};

    std::vector<std::shared_ptr<Entry>> mAcquiredResources;
    std::vector<std::array<VkDescriptorSet, kGraphicsStages.size()>>
        mGraphicsDescriptorSets;

    std::vector<VkDescriptorSet> mComputeDescriptorSets;

  public:
    Tag() = default;
    Tag(Cache *parent, Scheduler &scheduler, TagId id)
        : mParent(parent), mScheduler(&scheduler), mTagId(id) {}
    Tag(const Tag &) = delete;
    Tag(Tag &&other) { other.swap(*this); }
    Tag &operator=(Tag &&other) {
      other.swap(*this);
      return *this;
    }

    void submitAndWait() {
      mScheduler->submit();
      mScheduler->wait();
    }

    ~Tag() { release(); }

    TagId getReadId() const { return TagId{std::uint64_t(mTagId) - 1}; }
    TagId getWriteId() const { return mTagId; }

    void swap(Tag &other) {
      std::swap(mParent, other.mParent);
      std::swap(mScheduler, other.mScheduler);
      std::swap(mTagId, other.mTagId);
      std::swap(mAcquiredResources, other.mAcquiredResources);
      std::swap(mGraphicsDescriptorSets, other.mGraphicsDescriptorSets);
      std::swap(mComputeDescriptorSets, other.mComputeDescriptorSets);
    }

    Cache *getCache() const { return mParent; }
    Device *getDevice() const { return mParent->mDevice; }
    int getVmId() const { return mParent->mVmIm; }

    Shader getShader(const ShaderKey &key,
                     const ShaderKey *dependedKey = nullptr);
    Sampler getSampler(const SamplerKey &key);
    Buffer getBuffer(std::uint64_t address, std::uint64_t size, Access access);
    Buffer getInternalBuffer(std::uint64_t size);
    IndexBuffer getIndexBuffer(std::uint64_t address, std::uint32_t indexCount,
                               gnm::PrimitiveType primType,
                               gnm::IndexType indexType);
    Image getImage(const ImageKey &key, Access access);
    ImageView getImageView(const ImageViewKey &key, Access access);
    void readMemory(void *target, std::uint64_t address, std::uint64_t size);
    void writeMemory(const void *source, std::uint64_t address,
                     std::uint64_t size);
    int compareMemory(const void *source, std::uint64_t address,
                      std::uint64_t size);
    void release();

    VkPipelineLayout getGraphicsPipelineLayout() const {
      return getCache()->getGraphicsPipelineLayout();
    }

    VkPipelineLayout getComputePipelineLayout() const {
      return getCache()->getComputePipelineLayout();
    }

    std::array<VkDescriptorSet, kGraphicsStages.size()>
    createGraphicsDescriptorSets() {
      auto result = getCache()->createGraphicsDescriptorSets();
      mGraphicsDescriptorSets.push_back(result);
      return result;
    }

    VkDescriptorSet createComputeDescriptorSet() {
      auto result = getCache()->createComputeDescriptorSet();
      mComputeDescriptorSets.push_back(result);
      return result;
    }

    std::shared_ptr<Entry> findShader(const ShaderKey &key,
                                      const ShaderKey *dependedKey = nullptr);
  };

  Cache(Device *device, int vmId);
  ~Cache();
  Tag createTag(Scheduler &scheduler);

  vk::Buffer &getMemoryTableBuffer() { return mMemoryTableBuffer; }
  vk::Buffer &getGdsBuffer() { return mGdsBuffer; }

  void addFrameBuffer(Scheduler &scheduler, int index, std::uint64_t address,
                      std::uint32_t width, std::uint32_t height, int format,
                      TileMode tileMode);
  void removeFrameBuffer(Scheduler &scheduler, int index);
  VkImage getFrameBuffer(Scheduler &scheduler, int index);
  void invalidate(Scheduler &scheduler, std::uint64_t address,
                  std::uint64_t size);

  void invalidate(Scheduler &scheduler) {
    invalidate(scheduler, 0, ~static_cast<std::uint64_t>(0));
  }

  void flush(Scheduler &scheduler, std::uint64_t address, std::uint64_t size);
  void flush(Scheduler &scheduler) {
    flush(scheduler, 0, ~static_cast<std::uint64_t>(0));
  }

  const std::array<VkDescriptorSetLayout, kGraphicsStages.size()> &
  getGraphicsDescriptorSetLayouts() const {
    return mGraphicsDescriptorSetLayouts;
  }

  VkDescriptorSetLayout
  getGraphicsDescriptorSetLayout(VkShaderStageFlagBits stage) const {
    int index = getStageIndex(stage);
    rx::dieIf(index < 0, "getGraphicsDescriptorSetLayout: unexpected stage");
    return mGraphicsDescriptorSetLayouts[index];
  }

  VkDescriptorSetLayout getComputeDescriptorSetLayout() const {
    return mComputeDescriptorSetLayout;
  }
  VkPipelineLayout getGraphicsPipelineLayout() const {
    return mGraphicsPipelineLayout;
  }

  VkPipelineLayout getComputePipelineLayout() const {
    return mComputePipelineLayout;
  }

  std::array<VkDescriptorSet, kGraphicsStages.size()>
  createGraphicsDescriptorSets();
  VkDescriptorSet createComputeDescriptorSet();

  void destroyGraphicsDescriptorSets(
      const std::array<VkDescriptorSet, kGraphicsStages.size()> &set) {
    std::lock_guard lock(mDescriptorMtx);
    mGraphicsDescriptorSets.push_back(set);
  }

  void destroyComputeDescriptorSet(VkDescriptorSet set) {
    std::lock_guard lock(mDescriptorMtx);
    mComputeDescriptorSets.push_back(set);
  }

private:
  TagId getSyncTag(std::uint64_t address, std::uint64_t size, TagId currentTag);

  Device *mDevice;
  int mVmIm;
  TagId mNextTagId{2};
  vk::Buffer mMemoryTableBuffer;
  vk::Buffer mGdsBuffer;

  std::mutex mDescriptorMtx;
  std::array<VkDescriptorSetLayout, kGraphicsStages.size()>
      mGraphicsDescriptorSetLayouts{};
  VkDescriptorSetLayout mComputeDescriptorSetLayout{};
  VkPipelineLayout mGraphicsPipelineLayout{};
  VkPipelineLayout mComputePipelineLayout{};
  VkDescriptorPool mGraphicsDescriptorPool{};
  VkDescriptorPool mComputeDescriptorPool{};
  std::vector<std::array<VkDescriptorSet, kGraphicsStages.size()>>
      mGraphicsDescriptorSets;
  std::vector<VkDescriptorSet> mComputeDescriptorSets;
  std::map<SamplerKey, VkSampler> mSamplers;

  std::shared_ptr<Entry> mFrameBuffers[10];

  rx::MemoryTableWithPayload<std::shared_ptr<Entry>> mBuffers;
  rx::MemoryTableWithPayload<std::shared_ptr<Entry>> mIndexBuffers;
  rx::MemoryTableWithPayload<std::shared_ptr<Entry>> mImages;
  rx::MemoryTableWithPayload<std::shared_ptr<Entry>> mShaders;

  rx::MemoryTableWithPayload<std::shared_ptr<Entry>> mSyncTable;
};
} // namespace amdgpu
