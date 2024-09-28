#include "Cache.hpp"
#include "Device.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/vulkan.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/die.hpp"
#include "shader/GcnConverter.hpp"
#include "shader/dialect.hpp"
#include "shader/glsl.hpp"
#include "shader/spv.hpp"
#include "vk.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;

static bool isPrimRequiresConversion(gnm::PrimitiveType primType) {
  switch (primType) {
  case gnm::PrimitiveType::PointList:
  case gnm::PrimitiveType::LineList:
  case gnm::PrimitiveType::LineStrip:
  case gnm::PrimitiveType::TriList:
  case gnm::PrimitiveType::TriFan:
  case gnm::PrimitiveType::TriStrip:
  case gnm::PrimitiveType::Patch:
  case gnm::PrimitiveType::LineListAdjacency:
  case gnm::PrimitiveType::LineStripAdjacency:
  case gnm::PrimitiveType::TriListAdjacency:
  case gnm::PrimitiveType::TriStripAdjacency:
    return false;

  case gnm::PrimitiveType::LineLoop: // FIXME
    rx::die("unimplemented line loop primitive");
    return false;

  case gnm::PrimitiveType::RectList:
    return false;

  case gnm::PrimitiveType::QuadList:
  case gnm::PrimitiveType::QuadStrip:
  case gnm::PrimitiveType::Polygon:
    return true;

  default:
    rx::die("unknown primitive type: %u", (unsigned)primType);
  }
}

static std::pair<std::uint64_t, std::uint64_t>
quadListPrimConverter(std::uint64_t index) {
  static constexpr int indicies[] = {0, 1, 2, 2, 3, 0};
  return {index, index / 6 + indicies[index % 6]};
}

static std::pair<std::uint64_t, std::uint64_t>
quadStripPrimConverter(std::uint64_t index) {
  static constexpr int indicies[] = {0, 1, 3, 0, 3, 2};
  return {index, (index / 6) * 4 + indicies[index % 6]};
}

using ConverterFn =
    std::pair<std::uint64_t, std::uint64_t>(std::uint64_t index);

static ConverterFn *getPrimConverterFn(gnm::PrimitiveType primType,
                                       std::uint32_t *count) {
  switch (primType) {
  case gnm::PrimitiveType::QuadList:
    *count = *count / 4 * 6;
    return quadListPrimConverter;

  case gnm::PrimitiveType::QuadStrip:
    *count = *count / 4 * 6;
    return quadStripPrimConverter;

  default:
    rx::die("getPrimConverterFn: unexpected primType %u",
            static_cast<unsigned>(primType));
  }
}

static VkShaderStageFlagBits shaderStageToVk(shader::gcn::Stage stage) {
  switch (stage) {
  case shader::gcn::Stage::Ps:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case shader::gcn::Stage::VsVs:
    return VK_SHADER_STAGE_VERTEX_BIT;
  // case shader::gcn::Stage::VsEs:
  // case shader::gcn::Stage::VsLs:
  case shader::gcn::Stage::Cs:
    return VK_SHADER_STAGE_COMPUTE_BIT;
    // case shader::gcn::Stage::Gs:
    // case shader::gcn::Stage::GsVs:
    // case shader::gcn::Stage::Hs:
    // case shader::gcn::Stage::DsVs:
    // case shader::gcn::Stage::DsEs:

  default:
    rx::die("unsupported shader stage %u", int(stage));
  }
}

static void fillStageBindings(VkDescriptorSetLayoutBinding *bindings,
                              VkShaderStageFlagBits stage, int setIndex,
                              std::uint32_t setCount) {

  auto createDescriptorBinding = [&](VkDescriptorType type, uint32_t count,
                                     int dim = 0) {
    auto binding = Cache::getDescriptorBinding(type, dim);
    rx::dieIf(binding < 0, "unexpected descriptor type %#x\n", int(type));
    bindings[binding] = VkDescriptorSetLayoutBinding{
        .binding = static_cast<std::uint32_t>(binding),
        .descriptorType = type,
        .descriptorCount = count * setCount,
        .stageFlags = VkShaderStageFlags(
            stage | (binding > 0 && stage != VK_SHADER_STAGE_COMPUTE_BIT
                         ? VK_SHADER_STAGE_ALL_GRAPHICS
                         : 0)),
        .pImmutableSamplers = nullptr,
    };
  };

  createDescriptorBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1);
  if (setIndex == 0) {
    createDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLER, 16);
    createDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16, 1);
    createDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16, 2);
    createDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16, 3);
    createDescriptorBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 16);
  }
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

struct Cache::Entry {
  virtual ~Entry() = default;

  Cache::TagId tagId;
  std::uint64_t baseAddress;
  Access acquiredAccess = Access::None;

  virtual void flush(Cache::Tag &tag, Scheduler &scheduler,
                     std::uint64_t beginAddress, std::uint64_t endAddress) {}
};

struct CachedShader : Cache::Entry {
  std::uint64_t magic;
  VkShaderEXT handle;
  shader::gcn::ShaderInfo info;
  std::vector<std::pair<std::uint64_t, std::vector<std::byte>>> usedMemory;

  ~CachedShader() {
    vk::DestroyShaderEXT(vk::context->device, handle, vk::context->allocator);
  }
};

struct CachedBuffer : Cache::Entry {
  vk::Buffer buffer;
  std::size_t size;

  void flush(Cache::Tag &tag, Scheduler &scheduler, std::uint64_t beginAddress,
             std::uint64_t endAddress) override {
    if ((acquiredAccess & Access::Write) == Access::None) {
      return;
    }

    // std::printf("writing buffer to memory %lx\n", baseAddress);
    std::memcpy(RemoteMemory{tag.getVmId()}.getPointer(baseAddress),
                buffer.getData(), size);
  }
};

struct CachedIndexBuffer : Cache::Entry {
  vk::Buffer buffer;
  std::size_t size;
  gnm::IndexType indexType;
  gnm::PrimitiveType primType;
};

constexpr VkImageAspectFlags toAspect(ImageKind kind) {
  switch (kind) {
  case ImageKind::Color:
    return VK_IMAGE_ASPECT_COLOR_BIT;
  case ImageKind::Depth:
    return VK_IMAGE_ASPECT_DEPTH_BIT;
  case ImageKind::Stencil:
    return VK_IMAGE_ASPECT_STENCIL_BIT;
  }

  return VK_IMAGE_ASPECT_NONE;
}

struct CachedImage : Cache::Entry {
  vk::Image image;
  ImageKind kind;
  SurfaceInfo info;
  TileMode acquiredTileMode;
  gnm::DataFormat acquiredDfmt{};

  void flush(Cache::Tag &tag, Scheduler &scheduler, std::uint64_t beginAddress,
             std::uint64_t endAddress) override {
    if ((acquiredAccess & Access::Write) == Access::None) {
      return;
    }

    // std::printf("writing image to buffer to %lx\n", baseAddress);

    VkImageSubresourceRange subresourceRange{
        .aspectMask = toAspect(kind),
        .baseMipLevel = 0,
        .levelCount = image.getMipLevels(),
        .baseArrayLayer = 0,
        .layerCount = image.getArrayLayers(),
    };

    auto transferBuffer = vk::Buffer::Allocate(
        vk::getDeviceLocalMemory(), info.totalSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    auto tiledBuffer =
        tag.getBuffer(baseAddress, info.totalSize, Access::Write);
    auto &tiler = tag.getDevice()->tiler;

    transitionImageLayout(
        scheduler.getCommandBuffer(), image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

    for (unsigned mipLevel = 0; mipLevel < image.getMipLevels(); ++mipLevel) {
      VkBufferImageCopy region = {
          .bufferRowLength =
              mipLevel > 0 ? 0 : std::max(info.pitch >> mipLevel, 1u),
          .imageSubresource =
              {
                  .aspectMask = toAspect(kind),
                  .mipLevel = mipLevel,
                  .baseArrayLayer = 0,
                  .layerCount = image.getArrayLayers(),
              },
          .imageExtent =
              {
                  .width = std::max(image.getWidth() >> mipLevel, 1u),
                  .height = std::max(image.getHeight() >> mipLevel, 1u),
                  .depth = std::max(image.getDepth() >> mipLevel, 1u),
              },
      };

      vkCmdCopyImageToBuffer(scheduler.getCommandBuffer(), image.getHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             transferBuffer.getHandle(), 1, &region);

      tiler.tile(scheduler, info, acquiredTileMode, acquiredDfmt,
                 transferBuffer.getAddress(), tiledBuffer.deviceAddress,
                 mipLevel, 0, image.getArrayLayers());
    }

    transitionImageLayout(scheduler.getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresourceRange);
    // scheduler.afterSubmit([transferBuffer = std::move(transferBuffer)] {});
    scheduler.submit();
    scheduler.wait();
  }
};

struct CachedImageView : Cache::Entry {
  vk::ImageView view;
};

ImageKey ImageKey::createFrom(const gnm::TBuffer &buffer) {
  return {
      .readAddress = buffer.address(),
      .writeAddress = buffer.address(),
      .type = buffer.type,
      .dfmt = buffer.dfmt,
      .nfmt = buffer.nfmt,
      .tileMode = getDefaultTileModes()[buffer.tiling_idx],
      .offset = {},
      .extent =
          {
              .width = buffer.width + 1u,
              .height = buffer.height + 1u,
              .depth = buffer.depth + 1u,
          },
      .pitch = buffer.pitch + 1u,
      .baseMipLevel = static_cast<std::uint32_t>(buffer.base_level),
      .mipCount = buffer.last_level - buffer.base_level + 1u,
      .baseArrayLayer = static_cast<std::uint32_t>(buffer.base_array),
      .arrayLayerCount = buffer.last_array - buffer.base_array + 1u,
      .kind = ImageKind::Color,
      .pow2pad = buffer.pow2pad != 0,
  };
}

ImageViewKey ImageViewKey::createFrom(const gnm::TBuffer &buffer) {
  ImageViewKey result{};
  static_cast<ImageKey &>(result) = ImageKey::createFrom(buffer);
  result.R = buffer.dst_sel_x;
  result.G = buffer.dst_sel_y;
  result.B = buffer.dst_sel_z;
  result.A = buffer.dst_sel_w;
  return result;
}

SamplerKey SamplerKey::createFrom(const gnm::SSampler &sampler) {
  float lodBias = ((std::int16_t(sampler.lod_bias) << 2) >> 2) / float(256.f);
  // FIXME: lodBias can be scaled by gnm::TBuffer

  return {
      .magFilter = toVkFilter(sampler.xy_mag_filter),
      .minFilter = toVkFilter(sampler.xy_min_filter),
      .mipmapMode = toVkSamplerMipmapMode(sampler.mip_filter),
      .addressModeU = toVkSamplerAddressMode(sampler.clamp_x),
      .addressModeV = toVkSamplerAddressMode(sampler.clamp_y),
      .addressModeW = toVkSamplerAddressMode(sampler.clamp_z),
      .mipLodBias = lodBias,
      .maxAnisotropy = 0, // max_aniso_ratio
      .compareOp = toVkCompareOp(sampler.depth_compare_func),
      .minLod = static_cast<float>(sampler.min_lod),
      .maxLod = static_cast<float>(sampler.max_lod),
      .borderColor = toVkBorderColor(sampler.border_color_type),
      .anisotropyEnable = false,
      .compareEnable = sampler.depth_compare_func != gnm::CompareFunc::Never,
      .unnormalizedCoordinates = sampler.force_unorm_coords != 0,
  };
}

Cache::Shader Cache::Tag::getShader(const ShaderKey &key,
                                    const ShaderKey *dependedKey) {
  auto stage = shaderStageToVk(key.stage);
  if (auto result = findShader(key, dependedKey)) {
    auto cachedShader = static_cast<CachedShader *>(result.get());
    mAcquiredResources.push_back(result);
    return {cachedShader->handle, &cachedShader->info, stage};
  }

  auto vmId = mParent->mVmIm;

  std::optional<shader::gcn::ConvertedShader> converted;

  {
    shader::gcn::Context context;
    auto deserialized = shader::gcn::deserialize(
        context, key.env, mParent->mDevice->gcnSemantic, key.address,
        [vmId](std::uint64_t address) -> std::uint32_t {
          return *RemoteMemory{vmId}.getPointer<std::uint32_t>(address);
        });

    // deserialized.print(std::cerr, context.ns);

    converted = shader::gcn::convertToSpv(
        context, deserialized, mParent->mDevice->gcnSemanticModuleInfo,
        key.stage, key.env);
    if (!converted) {
      return {};
    }

    converted->info.resources.dump();
    if (!shader::spv::validate(converted->spv)) {
      shader::spv::dump(converted->spv, true);
      return {};
    }

    std::fprintf(stderr, "%s", shader::glsl::decompile(converted->spv).c_str());
    // if (auto opt = shader::spv::optimize(converted->spv)) {
    //   converted->spv = std::move(*opt);
    //   std::fprintf(stderr, "opt: %s",
    //              shader::glsl::decompile(converted->spv).c_str());
    // } else {
    //   std::printf("optimization failed\n");
    // }
  }

  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .flags = 0,
      .stage = stage,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = converted->spv.size() * sizeof(converted->spv[0]),
      .pCode = converted->spv.data(),
      .pName = "main",
      .setLayoutCount = static_cast<uint32_t>(
          stage == VK_SHADER_STAGE_COMPUTE_BIT ? 1
                                               : Cache::kGraphicsStages.size()),
      .pSetLayouts = (stage == VK_SHADER_STAGE_COMPUTE_BIT
                          ? &mParent->mComputeDescriptorSetLayout
                          : mParent->mGraphicsDescriptorSetLayouts.data())};

  VkShaderEXT handle;
  VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &createInfo,
                                 vk::context->allocator, &handle));

  auto result = std::make_shared<CachedShader>();
  result->baseAddress = key.address;
  result->tagId = getReadId();
  result->handle = handle;
  result->info = std::move(converted->info);
  readMemory(&result->magic, key.address, sizeof(result->magic));

  for (auto entry : converted->info.memoryMap) {
    auto address = entry.beginAddress;
    auto size = entry.endAddress - entry.beginAddress;
    auto &inserted = result->usedMemory.emplace_back();
    inserted.first = address;
    inserted.second.resize(size);
    readMemory(inserted.second.data(), address, size);
  }

  mParent->mShaders.map(key.address, key.address + 8, result);
  mAcquiredResources.push_back(result);
  return {handle, &result->info, stage};
}

std::shared_ptr<Cache::Entry>
Cache::Tag::findShader(const ShaderKey &key, const ShaderKey *dependedKey) {
  auto data = RemoteMemory{mParent->mVmIm}.getPointer(key.address);

  auto cacheIt = mParent->mShaders.queryArea(key.address);

  if (cacheIt == mParent->mShaders.end() ||
      cacheIt->get()->baseAddress != key.address) {
    return {};
  }

  std::uint64_t magic;
  readMemory(&magic, key.address, sizeof(magic));

  auto cachedShader = static_cast<CachedShader *>(cacheIt->get());
  if (cachedShader->magic != magic) {
    return {};
  }

  for (auto [index, sgpr] : cachedShader->info.requiredSgprs) {
    if (index >= key.env.userSgprs.size() || key.env.userSgprs[index] != sgpr) {
      return {};
    }
  }

  for (auto &usedMemory : cachedShader->usedMemory) {
    if (compareMemory(usedMemory.second.data(), usedMemory.first,
                      usedMemory.second.size())) {
      return {};
    }
  }

  return cacheIt.get();
}

Cache::Sampler Cache::Tag::getSampler(const SamplerKey &key) {
  auto [it, inserted] = getCache()->mSamplers.emplace(key, VK_NULL_HANDLE);

  if (inserted) {
    VkSamplerCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = key.magFilter,
        .minFilter = key.minFilter,
        .mipmapMode = key.mipmapMode,
        .addressModeU = key.addressModeU,
        .addressModeV = key.addressModeV,
        .addressModeW = key.addressModeW,
        .mipLodBias = key.mipLodBias,
        .anisotropyEnable = key.anisotropyEnable,
        .maxAnisotropy = key.maxAnisotropy,
        .compareEnable = key.compareEnable,
        .compareOp = key.compareOp,
        .minLod = key.minLod,
        .maxLod = key.maxLod,
        .borderColor = key.borderColor,
        .unnormalizedCoordinates = key.unnormalizedCoordinates,
    };

    VK_VERIFY(vkCreateSampler(vk::context->device, &info,
                              vk::context->allocator, &it->second));
  }

  return {it->second};
}

Cache::Buffer Cache::Tag::getBuffer(std::uint64_t address, std::uint64_t size,
                                    Access access) {
  auto buffer = vk::Buffer::Allocate(
      vk::getHostVisibleMemory(), size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  if ((access & Access::Read) != Access::None) {
    readMemory(buffer.getData(), address, size);
  }

  auto cached = std::make_shared<CachedBuffer>();
  cached->baseAddress = address;
  cached->acquiredAccess = access;
  cached->buffer = std::move(buffer);
  cached->size = size;
  cached->tagId =
      (access & Access::Write) != Access::Write ? getWriteId() : getReadId();

  mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

Cache::Buffer Cache::Tag::getInternalBuffer(std::uint64_t size) {
  auto buffer = vk::Buffer::Allocate(
      vk::getHostVisibleMemory(), size,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  auto cached = std::make_shared<CachedBuffer>();
  cached->baseAddress = 0;
  cached->acquiredAccess = Access::None;
  cached->buffer = std::move(buffer);
  cached->size = size;
  cached->tagId = getReadId();

  mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

Cache::IndexBuffer Cache::Tag::getIndexBuffer(std::uint64_t address,
                                              std::uint32_t indexCount,
                                              gnm::PrimitiveType primType,
                                              gnm::IndexType indexType) {
  unsigned origIndexSize = indexType == gnm::IndexType::Int16 ? 2 : 4;
  std::uint32_t size = indexCount * origIndexSize;

  if (address == 0) {
    if (isPrimRequiresConversion(primType)) {
      getPrimConverterFn(primType, &indexCount);
      primType = gnm::PrimitiveType::TriList;
    }

    return {
        .handle = VK_NULL_HANDLE,
        .offset = 0,
        .indexCount = indexCount,
        .primType = primType,
        .indexType = indexType,
    };
  }

  auto indexBuffer = getBuffer(address, size, Access::Read);

  if (!isPrimRequiresConversion(primType)) {
    return {
        .handle = indexBuffer.handle,
        .offset = indexBuffer.offset,
        .indexCount = indexCount,
        .primType = primType,
        .indexType = indexType,
    };
  }

  auto it = mParent->mIndexBuffers.queryArea(address);
  if (it != mParent->mIndexBuffers.end() && it.beginAddress() == address &&
      it.endAddress() == address + size) {

    auto &resource = it.get();
    auto indexBuffer = static_cast<CachedIndexBuffer *>(resource.get());
    if (indexBuffer->size == size && resource->tagId == indexBuffer->tagId) {
      mAcquiredResources.push_back(resource);
      return {
          .handle = indexBuffer->buffer.getHandle(),
          .offset = 0,
          .indexCount = indexCount,
          .primType = indexBuffer->primType,
          .indexType = indexBuffer->indexType,
      };
    }
  }

  auto converterFn = getPrimConverterFn(primType, &indexCount);
  primType = gnm::PrimitiveType::TriList;

  if (indexCount >= 0x10000) {
    indexType = gnm::IndexType::Int32;
  }

  unsigned indexSize = indexType == gnm::IndexType::Int16 ? 2 : 4;
  auto indexBufferSize = indexSize * indexCount;

  auto convertedIndexBuffer = vk::Buffer::Allocate(
      vk::getHostVisibleMemory(), indexBufferSize,
      VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

  void *data = convertedIndexBuffer.getData();

  auto indicies = indexBuffer.data + indexBuffer.offset;

  if (indexSize == 2) {
    for (std::uint32_t i = 0; i < indexCount; ++i) {
      auto [dstIndex, srcIndex] = converterFn(i);
      std::uint32_t origIndex = origIndexSize == 2
                                    ? ((std::uint16_t *)indicies)[srcIndex]
                                    : ((std::uint32_t *)indicies)[srcIndex];
      ((std::uint16_t *)data)[dstIndex] = origIndex;
    }

  } else {
    for (std::uint32_t i = 0; i < indexCount; ++i) {
      auto [dstIndex, srcIndex] = converterFn(i);
      std::uint32_t origIndex = origIndexSize == 2
                                    ? ((std::uint16_t *)indicies)[srcIndex]
                                    : ((std::uint32_t *)indicies)[srcIndex];
      ((std::uint32_t *)data)[dstIndex] = origIndex;
    }
  }

  auto cached = std::make_shared<CachedIndexBuffer>();
  cached->baseAddress = address;
  cached->acquiredAccess = Access::Read;
  cached->buffer = std::move(convertedIndexBuffer);
  cached->size = size;
  cached->tagId = indexBuffer.tagId;
  cached->primType = primType;
  cached->indexType = indexType;

  mParent->mIndexBuffers.map(address, address + size, cached);
  mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .indexCount = indexCount,
      .primType = cached->primType,
      .indexType = cached->indexType,
  };
}

Cache::Image Cache::Tag::getImage(const ImageKey &key, Access access) {
  auto surfaceInfo = computeSurfaceInfo(
      key.tileMode, key.type, key.dfmt, key.offset.x + key.extent.width,
      key.offset.y + key.extent.height, key.offset.z + key.extent.depth,
      key.pitch, key.baseArrayLayer, key.arrayLayerCount, key.baseMipLevel,
      key.mipCount, key.pow2pad);

  VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
      VK_IMAGE_USAGE_SAMPLED_BIT // | VK_IMAGE_USAGE_STORAGE_BIT
      ;

  bool isCompressed =
      key.dfmt == gnm::kDataFormatBc1 || key.dfmt == gnm::kDataFormatBc2 ||
      key.dfmt == gnm::kDataFormatBc3 || key.dfmt == gnm::kDataFormatBc4 ||
      key.dfmt == gnm::kDataFormatBc5 || key.dfmt == gnm::kDataFormatBc6 ||
      key.dfmt == gnm::kDataFormatBc7 || key.dfmt == gnm::kDataFormatGB_GR ||
      key.dfmt == gnm::kDataFormatBG_RG;

  if (!isCompressed) {
    usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  auto image = vk::Image::Allocate(
      vk::getDeviceLocalMemory(), gnm::toVkImageType(key.type), key.extent,
      key.mipCount, key.arrayLayerCount, gnm::toVkFormat(key.dfmt, key.nfmt),
      VK_SAMPLE_COUNT_1_BIT, usage);

  if ((access & Access::Read) != Access::None) {
    auto tiledBuffer =
        getBuffer(key.readAddress, surfaceInfo.totalSize, Access::Read);

    auto &tiler = mParent->mDevice->tiler;
    auto detiledBuffer =
        vk::Buffer::Allocate(vk::getDeviceLocalMemory(), surfaceInfo.totalSize,
                             VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR |
                                 VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR);
    VkImageSubresourceRange subresourceRange{
        .aspectMask = toAspect(key.kind),
        .baseMipLevel = key.baseMipLevel,
        .levelCount = key.mipCount,
        .baseArrayLayer = key.baseArrayLayer,
        .layerCount = key.arrayLayerCount,
    };

    bool isLinear = key.tileMode.arrayMode() == kArrayModeLinearGeneral ||
                    key.tileMode.arrayMode() == kArrayModeLinearAligned;

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(key.mipCount);
    std::vector<VkBufferCopy> bufferRegions;

    std::uint64_t dstAddress = 0;
    std::uint64_t srcAddress = 0;

    if (isLinear) {
      regions.reserve(key.mipCount);
    } else {
      dstAddress = detiledBuffer.getAddress();
      srcAddress = tiledBuffer.deviceAddress;
    }

    for (unsigned mipLevel = key.baseMipLevel;
         mipLevel < key.baseMipLevel + key.mipCount; ++mipLevel) {
      auto &info = surfaceInfo.getSubresourceInfo(mipLevel);
      if (isLinear) {
        bufferRegions.push_back({
            .srcOffset = info.offset,
            .dstOffset = dstAddress,
            .size = info.linearSize * key.arrayLayerCount,
        });
      } else {
        tiler.detile(*mScheduler, surfaceInfo, key.tileMode, key.dfmt,
                     srcAddress, dstAddress, mipLevel, 0, key.arrayLayerCount);
      }

      regions.push_back({
          .bufferOffset = info.offset,
          .bufferRowLength =
              mipLevel > 0 ? 0 : std::max(key.pitch >> mipLevel, 1u),
          .imageSubresource =
              {
                  .aspectMask = toAspect(key.kind),
                  .mipLevel = mipLevel,
                  .baseArrayLayer = key.baseArrayLayer,
                  .layerCount = key.arrayLayerCount,
              },
          .imageExtent =
              {
                  .width = std::max(key.extent.width >> mipLevel, 1u),
                  .height = std::max(key.extent.height >> mipLevel, 1u),
                  .depth = std::max(key.extent.depth >> mipLevel, 1u),
              },
      });

      dstAddress += info.linearSize * key.arrayLayerCount;
      srcAddress += info.tiledSize * key.arrayLayerCount;
    }

    if (!bufferRegions.empty()) {
      vkCmdCopyBuffer(mScheduler->getCommandBuffer(), tiledBuffer.handle,
                      detiledBuffer.getHandle(), bufferRegions.size(),
                      bufferRegions.data());
    }

    transitionImageLayout(
        mScheduler->getCommandBuffer(), image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    vkCmdCopyBufferToImage(mScheduler->getCommandBuffer(),
                           detiledBuffer.getHandle(), image.getHandle(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(),
                           regions.data());

    transitionImageLayout(mScheduler->getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresourceRange);

    mScheduler->afterSubmit([detiledBuffer = std::move(detiledBuffer)] {});
  }

  auto cached = std::make_shared<CachedImage>();
  cached->image = std::move(image);
  cached->info = std::move(surfaceInfo);
  cached->baseAddress = (access & Access::Write) != Access::None
                            ? key.writeAddress
                            : key.readAddress;
  cached->kind = key.kind;
  cached->acquiredAccess = access;
  cached->acquiredTileMode = key.tileMode;
  cached->acquiredDfmt = key.dfmt;
  mAcquiredResources.push_back(cached);

  return {.handle = cached->image.getHandle()};
}

Cache::ImageView Cache::Tag::getImageView(const ImageViewKey &key,
                                          Access access) {
  auto image = getImage(key, access);
  auto result = vk::ImageView(gnm::toVkImageViewType(key.type), image.handle,
                              gnm::toVkFormat(key.dfmt, key.nfmt),
                              {
                                  .r = gnm::toVkComponentSwizzle(key.R),
                                  .g = gnm::toVkComponentSwizzle(key.G),
                                  .b = gnm::toVkComponentSwizzle(key.B),
                                  .a = gnm::toVkComponentSwizzle(key.A),
                              },
                              {
                                  .aspectMask = toAspect(key.kind),
                                  .baseMipLevel = key.baseMipLevel,
                                  .levelCount = key.mipCount,
                                  .baseArrayLayer = key.baseArrayLayer,
                                  .layerCount = key.arrayLayerCount,
                              });
  auto cached = std::make_shared<CachedImageView>();
  cached->baseAddress = (access & Access::Write) != Access::None
                            ? key.writeAddress
                            : key.readAddress;
  cached->acquiredAccess = access;
  cached->view = std::move(result);

  mAcquiredResources.push_back(cached);

  return {
      .handle = cached->view.getHandle(),
      .imageHandle = image.handle,
  };
}

void Cache::Tag::readMemory(void *target, std::uint64_t address,
                            std::uint64_t size) {
  mParent->flush(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  std::memcpy(target, memoryPtr, size);
}

void Cache::Tag::writeMemory(const void *source, std::uint64_t address,
                             std::uint64_t size) {
  mParent->flush(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  std::memcpy(memoryPtr, source, size);
}

int Cache::Tag::compareMemory(const void *source, std::uint64_t address,
                              std::uint64_t size) {
  mParent->flush(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  return std::memcmp(memoryPtr, source, size);
}

void Cache::Tag::release() {
  for (auto ds : mGraphicsDescriptorSets) {
    getCache()->destroyGraphicsDescriptorSets(ds);
  }

  for (auto ds : mComputeDescriptorSets) {
    getCache()->destroyComputeDescriptorSet(ds);
  }

  mGraphicsDescriptorSets.clear();
  mComputeDescriptorSets.clear();

  if (mAcquiredResources.empty()) {
    return;
  }

  while (!mAcquiredResources.empty()) {
    auto resource = std::move(mAcquiredResources.back());
    mAcquiredResources.pop_back();
    resource->flush(*this, *mScheduler, 0, ~static_cast<std::uint64_t>(0));
  }

  mScheduler->submit();
  mScheduler->then([mAcquiredResources = std::move(mAcquiredResources)] {});
}

Cache::Tag Cache::createTag(Scheduler &scheduler) {
  auto tag = Tag{this, scheduler, mNextTagId};
  mNextTagId = static_cast<TagId>(static_cast<std::uint64_t>(mNextTagId) + 2);
  return tag;
}

Cache::Cache(Device *device, int vmId) : mDevice(device), mVmIm(vmId) {
  mMemoryTableBuffer =
      vk::Buffer::Allocate(vk::getHostVisibleMemory(), 0x10000);
  mGdsBuffer = vk::Buffer::Allocate(vk::getHostVisibleMemory(), 0x40000);

  {
    VkDescriptorSetLayoutBinding bindings[kGraphicsStages.size()]
                                         [kDescriptorBindings.size()];

    for (std::size_t index = 0; auto stage : kGraphicsStages) {
      fillStageBindings(bindings[index], stage, index, 128);
      ++index;
    }

    for (std::size_t index = 0; auto &layout : mGraphicsDescriptorSetLayouts) {
      VkDescriptorSetLayoutCreateInfo descLayoutInfo{
          .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
          .bindingCount = static_cast<uint32_t>(
              index == 0 ? kDescriptorBindings.size() : 1),
          .pBindings = bindings[index],
      };

      ++index;

      VK_VERIFY(vkCreateDescriptorSetLayout(vk::context->device,
                                            &descLayoutInfo,
                                            vk::context->allocator, &layout));
    }
  }

  {
    VkDescriptorSetLayoutBinding bindings[kDescriptorBindings.size()];

    fillStageBindings(bindings, VK_SHADER_STAGE_COMPUTE_BIT, 0, 128);

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = kDescriptorBindings.size(),
        .pBindings = bindings,
    };

    VK_VERIFY(vkCreateDescriptorSetLayout(vk::context->device, &layoutInfo,
                                          vk::context->allocator,
                                          &mComputeDescriptorSetLayout));
  }

  {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount =
            static_cast<uint32_t>(mGraphicsDescriptorSetLayouts.size()),
        .pSetLayouts = mGraphicsDescriptorSetLayouts.data(),
    };

    VK_VERIFY(vkCreatePipelineLayout(vk::context->device, &pipelineLayoutInfo,
                                     vk::context->allocator,
                                     &mGraphicsPipelineLayout));
  }

  {
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &mComputeDescriptorSetLayout,
    };

    VK_VERIFY(vkCreatePipelineLayout(vk::context->device, &pipelineLayoutInfo,
                                     vk::context->allocator,
                                     &mComputePipelineLayout));
  }
}
Cache::~Cache() {}

void Cache::addFrameBuffer(Scheduler &scheduler, int index,
                           std::uint64_t address, std::uint32_t width,
                           std::uint32_t height, int format,
                           TileMode tileMode) {}

void Cache::removeFrameBuffer(Scheduler &scheduler, int index) {}

VkImage Cache::getFrameBuffer(Scheduler &scheduler, int index) { return {}; }

static void
flushCacheImpl(Scheduler &scheduler, Cache::Tag &tag,
               rx::MemoryTableWithPayload<std::shared_ptr<Cache::Entry>> &table,
               std::uint64_t beginAddress, std::uint64_t endAddress) {
  auto beginIt = table.lowerBound(beginAddress);
  auto endIt = table.lowerBound(endAddress);

  while (beginIt != endIt) {
    auto cached = beginIt->get();
    cached->flush(tag, scheduler, beginAddress, endAddress);
    ++beginIt;
  }
}

static void invalidateCacheImpl(
    Scheduler &scheduler,
    rx::MemoryTableWithPayload<std::shared_ptr<Cache::Entry>> &table,
    std::uint64_t beginAddress, std::uint64_t endAddress) {
  table.unmap(beginAddress, endAddress);
}

void Cache::invalidate(Scheduler &scheduler, std::uint64_t address,
                       std::uint64_t size) {
  auto beginAddress = address;
  auto endAddress = address + size;

  rx::dieIf(beginAddress >= endAddress,
            "wrong flush range: address %lx, size %lx", address, size);

  invalidateCacheImpl(scheduler, mBuffers, beginAddress, endAddress);
  invalidateCacheImpl(scheduler, mImages, beginAddress, endAddress);

  invalidateCacheImpl(scheduler, mSyncTable, beginAddress, endAddress);
}

void Cache::flush(Scheduler &scheduler, std::uint64_t address,
                  std::uint64_t size) {
  auto beginAddress = address;
  auto endAddress = address + size;

  rx::dieIf(beginAddress >= endAddress,
            "wrong flush range: address %lx, size %lx", address, size);

  auto tag = createTag(scheduler);
  flushCacheImpl(scheduler, tag, mBuffers, beginAddress, endAddress);
  flushCacheImpl(scheduler, tag, mIndexBuffers, beginAddress, endAddress);
  flushCacheImpl(scheduler, tag, mImages, beginAddress, endAddress);
  // flushCacheImpl(scheduler, tag, mShaders, beginAddress, endAddress);

  flushCacheImpl(scheduler, tag, mSyncTable, beginAddress, endAddress);
}

std::array<VkDescriptorSet, Cache::kGraphicsStages.size()>
Cache::createGraphicsDescriptorSets() {
  std::lock_guard lock(mDescriptorMtx);

  if (!mGraphicsDescriptorSets.empty()) {
    auto result = mGraphicsDescriptorSets.back();
    mGraphicsDescriptorSets.pop_back();
    return result;
  }

  constexpr auto maxSets = Cache::kGraphicsStages.size() * 128;

  if (mGraphicsDescriptorPool == nullptr) {
    VkDescriptorPoolSize poolSizes[]{
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 3 * 16 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 16 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 16 * (maxSets / 4),
        },
    };

    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes,
    };

    VK_VERIFY(vkCreateDescriptorPool(vk::context->device, &info,
                                     vk::context->allocator,
                                     &mGraphicsDescriptorPool));
  }

  VkDescriptorSetAllocateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = mGraphicsDescriptorPool,
      .descriptorSetCount =
          static_cast<uint32_t>(mGraphicsDescriptorSetLayouts.size()),
      .pSetLayouts = mGraphicsDescriptorSetLayouts.data(),
  };

  std::array<VkDescriptorSet, Cache::kGraphicsStages.size()> result;
  VK_VERIFY(
      vkAllocateDescriptorSets(vk::context->device, &info, result.data()));
  return result;
}

VkDescriptorSet Cache::createComputeDescriptorSet() {
  std::lock_guard lock(mDescriptorMtx);

  if (!mComputeDescriptorSets.empty()) {
    auto result = mComputeDescriptorSets.back();
    mComputeDescriptorSets.pop_back();
    return result;
  }

  constexpr auto maxSets = 128;

  if (mComputeDescriptorPool == nullptr) {
    VkDescriptorPoolSize poolSizes[]{
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 3 * 16 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 16 * (maxSets / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 16 * (maxSets / 4),
        },
    };

    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = maxSets,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes,
    };

    VK_VERIFY(vkCreateDescriptorPool(vk::context->device, &info,
                                     vk::context->allocator,
                                     &mComputeDescriptorPool));
  }

  VkDescriptorSetAllocateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = mComputeDescriptorPool,
      .descriptorSetCount = 1,
      .pSetLayouts = &mComputeDescriptorSetLayout,
  };

  VkDescriptorSet result;
  VK_VERIFY(vkAllocateDescriptorSets(vk::context->device, &info, &result));
  return result;
}
