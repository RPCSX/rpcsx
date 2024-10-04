#include "Cache.hpp"
#include "Device.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/vulkan.hpp"
#include "rx/MemoryTable.hpp"
#include "rx/die.hpp"
#include "shader/Evaluator.hpp"
#include "shader/GcnConverter.hpp"
#include "shader/dialect.hpp"
#include "shader/glsl.hpp"
#include "shader/spv.hpp"
#include "vk.hpp"
#include <cstddef>
#include <cstring>
#include <memory>
#include <print>
#include <utility>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;
using namespace shader;

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

void Cache::ShaderResources::loadResources(
    gcn::Resources &res, std::span<const std::uint32_t> userSgprs) {
  this->userSgprs = userSgprs;
  for (auto &pointer : res.pointers) {
    auto pointerBase = eval(pointer.base).zExtScalar();
    auto pointerOffset = eval(pointer.offset).zExtScalar();

    if (!pointerBase || !pointerOffset) {
      res.dump();
      rx::die("failed to evaluate pointer");
    }

    bufferMemoryTable.map(*pointerBase,
                          *pointerBase + *pointerOffset + pointer.size,
                          Access::Read);
    resourceSlotToAddress.push_back(
        {slotOffset + pointer.resourceSlot, *pointerBase});
  }

  for (auto &bufferRes : res.buffers) {
    auto word0 = eval(bufferRes.words[0]).zExtScalar();
    auto word1 = eval(bufferRes.words[1]).zExtScalar();
    auto word2 = eval(bufferRes.words[2]).zExtScalar();
    auto word3 = eval(bufferRes.words[3]).zExtScalar();

    if (!word0 || !word1 || !word2 || !word3) {
      res.dump();
      rx::die("failed to evaluate V#");
    }

    gnm::VBuffer buffer{};
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer), &*word0,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 1, &*word1,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 2, &*word2,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 3, &*word3,
                sizeof(std::uint32_t));

    bufferMemoryTable.map(buffer.address(), buffer.address() + buffer.size(),
                          bufferRes.access);
    resourceSlotToAddress.push_back(
        {slotOffset + bufferRes.resourceSlot, buffer.address()});
  }

  for (auto &texture : res.textures) {
    auto word0 = eval(texture.words[0]).zExtScalar();
    auto word1 = eval(texture.words[1]).zExtScalar();
    auto word2 = eval(texture.words[2]).zExtScalar();
    auto word3 = eval(texture.words[3]).zExtScalar();

    if (!word0 || !word1 || !word2 || !word3) {
      res.dump();
      rx::die("failed to evaluate 128 bit T#");
    }

    gnm::TBuffer buffer{};
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer), &*word0,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 1, &*word1,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 2, &*word2,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 3, &*word3,
                sizeof(std::uint32_t));

    if (texture.words[4] != nullptr) {
      auto word4 = eval(texture.words[4]).zExtScalar();
      auto word5 = eval(texture.words[5]).zExtScalar();
      auto word6 = eval(texture.words[6]).zExtScalar();
      auto word7 = eval(texture.words[7]).zExtScalar();

      if (!word4 || !word5 || !word6 || !word7) {
        res.dump();
        rx::die("failed to evaluate 256 bit T#");
      }

      std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 4, &*word4,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 5, &*word5,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 6, &*word6,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&buffer) + 7, &*word7,
                  sizeof(std::uint32_t));
    }

    std::vector<amdgpu::Cache::ImageView> *resources = nullptr;

    switch (buffer.type) {
    case gnm::TextureType::Array1D:
    case gnm::TextureType::Dim1D:
      resources = &imageResources[0];
      break;
    case gnm::TextureType::Dim2D:
    case gnm::TextureType::Array2D:
    case gnm::TextureType::Msaa2D:
    case gnm::TextureType::MsaaArray2D:
    case gnm::TextureType::Cube:
      resources = &imageResources[1];
      break;
    case gnm::TextureType::Dim3D:
      resources = &imageResources[2];
      break;
    }

    rx::dieIf(resources == nullptr,
              "ShaderResources: unexpected texture type %u",
              static_cast<unsigned>(buffer.type));

    slotResources[slotOffset + texture.resourceSlot] = resources->size();
    resources->push_back(cacheTag->getImageView(
        amdgpu::ImageKey::createFrom(buffer), texture.access));
  }

  for (auto &sampler : res.samplers) {
    auto word0 = eval(sampler.words[0]).zExtScalar();
    auto word1 = eval(sampler.words[1]).zExtScalar();
    auto word2 = eval(sampler.words[2]).zExtScalar();
    auto word3 = eval(sampler.words[3]).zExtScalar();

    if (!word0 || !word1 || !word2 || !word3) {
      res.dump();
      rx::die("failed to evaluate S#");
    }

    gnm::SSampler sSampler{};
    std::memcpy(reinterpret_cast<std::uint32_t *>(&sSampler), &*word0,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&sSampler) + 1, &*word1,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&sSampler) + 2, &*word2,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&sSampler) + 3, &*word3,
                sizeof(std::uint32_t));

    if (sampler.unorm) {
      sSampler.force_unorm_coords = true;
    }

    slotResources[slotOffset + sampler.resourceSlot] = samplerResources.size();
    samplerResources.push_back(
        cacheTag->getSampler(amdgpu::SamplerKey::createFrom(sSampler)));
  }

  slotOffset += res.slots;
}

void Cache::ShaderResources::buildMemoryTable(MemoryTable &memoryTable) {
  memoryTable.count = 0;

  for (auto p : bufferMemoryTable) {
    auto size = p.endAddress - p.beginAddress;
    auto buffer = cacheTag->getBuffer(p.beginAddress, size, p.payload);

    auto memoryTableSlot = memoryTable.count;
    memoryTable.slots[memoryTable.count++] = {
        .address = p.beginAddress,
        .size = size,
        .flags = static_cast<uint8_t>(p.payload),
        .deviceAddress = buffer.deviceAddress,
    };

    for (auto [slot, address] : resourceSlotToAddress) {
      if (address >= p.beginAddress && address < p.endAddress) {
        slotResources[slot] = memoryTableSlot;
      }
    }
  }
}

std::uint32_t Cache::ShaderResources::getResourceSlot(std::uint32_t id) {
  if (auto it = slotResources.find(id); it != slotResources.end()) {
    return it->second;
  }
  return -1;
}

eval::Value
Cache::ShaderResources::eval(ir::InstructionId instId,
                             std::span<const ir::Operand> operands) {
  if (instId == ir::amdgpu::POINTER) {
    auto type = operands[0].getAsValue();
    auto loadSize = *operands[1].getAsInt32();
    auto base = eval(operands[2]).zExtScalar();
    auto offset = eval(operands[3]).zExtScalar();

    if (!base || !offset) {
      rx::die("failed to evaluate pointer dependency");
    }

    eval::Value result;
    auto address = *base + *offset;

    switch (loadSize) {
    case 1:
      result = readPointer<std::uint8_t>(address);
      break;
    case 2:
      result = readPointer<std::uint16_t>(address);
      break;
    case 4:
      result = readPointer<std::uint32_t>(address);
      break;
    case 8:
      result = readPointer<std::uint64_t>(address);
      break;
    case 12:
      result = readPointer<u32vec3>(address);
      break;
    case 16:
      result = readPointer<u32vec4>(address);
      break;
    case 32:
      result = readPointer<std::array<std::uint32_t, 8>>(address);
      break;
    default:
      rx::die("unexpected pointer load size");
    }

    return result;
  }

  if (instId == ir::amdgpu::VBUFFER) {
    rx::die("resource depends on buffer value");
  }

  if (instId == ir::amdgpu::TBUFFER) {
    rx::die("resource depends on texture value");
  }

  if (instId == ir::amdgpu::SAMPLER) {
    rx::die("resource depends on sampler value");
  }

  if (instId == ir::amdgpu::USER_SGPR) {
    auto index = static_cast<std::uint32_t>(*operands[1].getAsInt32());
    rx::dieIf(index >= userSgprs.size(), "out of user sgprs");
    return userSgprs[index];
  }

  if (instId == ir::amdgpu::IMM) {
    auto address = static_cast<std::uint64_t>(*operands[1].getAsInt64());

    std::uint32_t result;
    cacheTag->readMemory(&result, address, sizeof(result));
    return result;
  }

  return Evaluator::eval(instId, operands);
}

static VkShaderStageFlagBits shaderStageToVk(gcn::Stage stage) {
  switch (stage) {
  case gcn::Stage::Ps:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case gcn::Stage::VsVs:
    return VK_SHADER_STAGE_VERTEX_BIT;
  // case gcn::Stage::VsEs:
  // case gcn::Stage::VsLs:
  case gcn::Stage::Cs:
    return VK_SHADER_STAGE_COMPUTE_BIT;
    // case gcn::Stage::Gs:
    // case gcn::Stage::GsVs:
    // case gcn::Stage::Hs:
    // case gcn::Stage::DsVs:
    // case gcn::Stage::DsEs:

  default:
    rx::die("unsupported shader stage %u", int(stage));
  }
}

static void fillStageBindings(VkDescriptorSetLayoutBinding *bindings,
                              VkShaderStageFlagBits stage, int setIndex) {

  auto createDescriptorBinding = [&](VkDescriptorType type, uint32_t count,
                                     int dim = 0) {
    auto binding = Cache::getDescriptorBinding(type, dim);
    rx::dieIf(binding < 0, "unexpected descriptor type %#x\n", int(type));
    bindings[binding] = VkDescriptorSetLayoutBinding{
        .binding = static_cast<std::uint32_t>(binding),
        .descriptorType = type,
        .descriptorCount = count,
        .stageFlags = VkShaderStageFlags(
            stage | (binding > 0 && stage != VK_SHADER_STAGE_COMPUTE_BIT
                         ? VK_SHADER_STAGE_ALL_GRAPHICS
                         : 0)),
        .pImmutableSamplers = nullptr,
    };
  };

  createDescriptorBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1);
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
  gcn::ShaderInfo info;
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

    transitionImageLayout(
        scheduler.getCommandBuffer(), image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

    bool isLinear = acquiredTileMode.arrayMode() == kArrayModeLinearGeneral ||
                    acquiredTileMode.arrayMode() == kArrayModeLinearAligned;

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(image.getMipLevels());

    auto tiledBuffer =
        tag.getBuffer(baseAddress, info.totalSize, Access::Write);

    if (isLinear) {
      for (unsigned mipLevel = 0; mipLevel < image.getMipLevels(); ++mipLevel) {
        auto &regionInfo = info.getSubresourceInfo(mipLevel);

        regions.push_back({
            .bufferOffset = regionInfo.offset,
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
        });
      }

      vkCmdCopyImageToBuffer(scheduler.getCommandBuffer(), image.getHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             tiledBuffer.handle, regions.size(),
                             regions.data());
    } else {
      auto tiledSize = info.totalSize;
      std::uint64_t linearOffset = 0;
      for (unsigned mipLevel = 0; mipLevel < image.getMipLevels(); ++mipLevel) {
        auto &regionInfo = info.getSubresourceInfo(mipLevel);
        regions.push_back({
            .bufferOffset = linearOffset,
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
        });

        linearOffset += regionInfo.linearSize * image.getArrayLayers();
      }

      auto linearSize = linearOffset;
      auto transferBuffer = vk::Buffer::Allocate(
          vk::getDeviceLocalMemory(), linearOffset,
          VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

      vkCmdCopyImageToBuffer(scheduler.getCommandBuffer(), image.getHandle(),
                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                             transferBuffer.getHandle(), regions.size(),
                             regions.data());

      auto &tiler = tag.getDevice()->tiler;

      linearOffset = 0;
      for (unsigned mipLevel = 0; mipLevel < image.getMipLevels(); ++mipLevel) {
        auto &regionInfo = info.getSubresourceInfo(mipLevel);
        tiler.tile(scheduler, info, acquiredTileMode, acquiredDfmt,
                   transferBuffer.getAddress() + linearOffset,
                   linearSize - linearOffset, tiledBuffer.deviceAddress,
                   tiledSize, mipLevel, 0, image.getArrayLayers());
        linearOffset += regionInfo.linearSize * image.getArrayLayers();
      }

      scheduler.afterSubmit([transferBuffer = std::move(transferBuffer)] {});
    }

    transitionImageLayout(scheduler.getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresourceRange);

    scheduler.submit();
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
    mStorage->mAcquiredResources.push_back(result);
    return {cachedShader->handle, &cachedShader->info, stage};
  }

  auto vmId = mParent->mVmIm;

  std::optional<gcn::ConvertedShader> converted;

  {
    auto env = key.env;
    env.supportsBarycentric = vk::context->supportsBarycentric;
    env.supportsInt8 = vk::context->supportsInt8;
    env.supportsInt64Atomics = vk::context->supportsInt64Atomics;
    env.supportsNonSemanticInfo = vk::context->supportsNonSemanticInfo;

    gcn::Context context;
    auto deserialized = gcn::deserialize(
        context, env, mParent->mDevice->gcnSemantic, key.address,
        [vmId](std::uint64_t address) -> std::uint32_t {
          return *RemoteMemory{vmId}.getPointer<std::uint32_t>(address);
        });

    // deserialized.print(std::cerr, context.ns);

    converted = gcn::convertToSpv(context, deserialized,
                                  mParent->mDevice->gcnSemanticModuleInfo,
                                  key.stage, env);
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
  mStorage->mAcquiredResources.push_back(result);
  return {handle, &result->info, stage};
}

std::shared_ptr<Cache::Entry>
Cache::Tag::findShader(const ShaderKey &key, const ShaderKey *dependedKey) {
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
          VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
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

  mStorage->mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

Cache::Buffer Cache::Tag::getInternalHostVisibleBuffer(std::uint64_t size) {
  auto buffer = vk::Buffer::Allocate(vk::getHostVisibleMemory(), size,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  auto cached = std::make_shared<CachedBuffer>();
  cached->baseAddress = 0;
  cached->acquiredAccess = Access::None;
  cached->buffer = std::move(buffer);
  cached->size = size;
  cached->tagId = getReadId();

  mStorage->mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

Cache::Buffer Cache::Tag::getInternalDeviceLocalBuffer(std::uint64_t size) {
  auto buffer = vk::Buffer::Allocate(vk::getDeviceLocalMemory(), size,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  auto cached = std::make_shared<CachedBuffer>();
  cached->baseAddress = 0;
  cached->acquiredAccess = Access::None;
  cached->buffer = std::move(buffer);
  cached->size = size;
  cached->tagId = getReadId();

  mStorage->mAcquiredResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

void Cache::Tag::buildDescriptors(VkDescriptorSet descriptorSet) {
  auto memoryTableBuffer = getMemoryTable();
  auto memoryTable = std::bit_cast<MemoryTable *>(memoryTableBuffer.data);
  mStorage->shaderResources.buildMemoryTable(*memoryTable);

  for (auto &sampler : mStorage->shaderResources.samplerResources) {
    uint32_t index =
        &sampler - mStorage->shaderResources.samplerResources.data();

    VkDescriptorImageInfo samplerInfo{.sampler = sampler.handle};

    VkWriteDescriptorSet writeDescSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSet,
        .dstBinding = Cache::getDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLER),
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &samplerInfo,
    };

    vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
  }

  for (auto &imageResources : mStorage->shaderResources.imageResources) {
    auto dim = (&imageResources - mStorage->shaderResources.imageResources) + 1;
    auto binding = static_cast<uint32_t>(
        Cache::getDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, dim));

    for (auto &image : imageResources) {
      uint32_t index = &image - imageResources.data();

      VkDescriptorImageInfo imageInfo{
          .imageView = image.handle,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };

      VkWriteDescriptorSet writeDescSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = binding,
          .dstArrayElement = index,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &imageInfo,
      };

      vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
    }
  }

  for (auto &mtConfig : mStorage->memoryTableConfigSlots) {
    auto config = mStorage->descriptorBuffers[mtConfig.bufferIndex];
    config[mtConfig.configIndex] =
        mStorage->shaderResources.getResourceSlot(mtConfig.resourceSlot);
  }
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
      mStorage->mAcquiredResources.push_back(resource);

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

  auto handle = cached->buffer.getHandle();

  mParent->mIndexBuffers.map(address, address + size, cached);
  mStorage->mAcquiredResources.push_back(std::move(cached));

  return {
      .handle = handle,
      .offset = 0,
      .indexCount = indexCount,
      .primType = primType,
      .indexType = indexType,
  };
}

Cache::Image Cache::Tag::getImage(const ImageKey &key, Access access) {
  auto surfaceInfo = computeSurfaceInfo(
      key.tileMode, key.type, key.dfmt, key.offset.x + key.extent.width,
      key.offset.y + key.extent.height, key.offset.z + key.extent.depth,
      key.pitch, key.baseArrayLayer, key.arrayLayerCount, key.baseMipLevel,
      key.mipCount, key.pow2pad);

  VkImageUsageFlags usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

  VkFormat format;
  if (key.kind == ImageKind::Color) {
    usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    bool isCompressed =
        key.dfmt == gnm::kDataFormatBc1 || key.dfmt == gnm::kDataFormatBc2 ||
        key.dfmt == gnm::kDataFormatBc3 || key.dfmt == gnm::kDataFormatBc4 ||
        key.dfmt == gnm::kDataFormatBc5 || key.dfmt == gnm::kDataFormatBc6 ||
        key.dfmt == gnm::kDataFormatBc7 || key.dfmt == gnm::kDataFormatGB_GR ||
        key.dfmt == gnm::kDataFormatBG_RG;

    if (!isCompressed) {
      usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    format = gnm::toVkFormat(key.dfmt, key.nfmt);
  } else {
    if (key.kind == ImageKind::Depth) {
      if (key.dfmt == gnm::kDataFormat32 &&
          key.nfmt == gnm::kNumericFormatFloat) {
        format = VK_FORMAT_D32_SFLOAT;
      } else if (key.dfmt == gnm::kDataFormat16 &&
                 key.nfmt == gnm::kNumericFormatUNorm) {
        format = VK_FORMAT_D16_UNORM;
      } else {
        rx::die("unexpected depth format %u, %u", static_cast<int>(key.dfmt),
                static_cast<int>(key.nfmt));
      }
    } else if (key.kind == ImageKind::Stencil) {
      if (key.dfmt == gnm::kDataFormat8 &&
          key.nfmt == gnm::kNumericFormatUInt) {
        format = VK_FORMAT_S8_UINT;
      } else {
        rx::die("unexpected stencil format %u, %u", static_cast<int>(key.dfmt),
                static_cast<int>(key.nfmt));
      }
    } else {
      rx::die("image kind %u %u, %u", static_cast<int>(key.kind),
              static_cast<int>(key.dfmt), static_cast<int>(key.nfmt));
    }

    usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }

  auto image = vk::Image::Allocate(
      vk::getDeviceLocalMemory(), gnm::toVkImageType(key.type), key.extent,
      key.mipCount, key.arrayLayerCount, format, VK_SAMPLE_COUNT_1_BIT, usage);

  VkImageSubresourceRange subresourceRange{
      .aspectMask = toAspect(key.kind),
      .baseMipLevel = key.baseMipLevel,
      .levelCount = key.mipCount,
      .baseArrayLayer = key.baseArrayLayer,
      .layerCount = key.arrayLayerCount,
  };

  if ((access & Access::Read) != Access::None) {
    bool isLinear = key.tileMode.arrayMode() == kArrayModeLinearGeneral ||
                    key.tileMode.arrayMode() == kArrayModeLinearAligned;

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(key.mipCount);

    VkBuffer sourceBuffer;

    auto tiledBuffer =
        getBuffer(key.readAddress, surfaceInfo.totalSize, Access::Read);

    if (isLinear) {
      sourceBuffer = tiledBuffer.handle;
      for (unsigned mipLevel = key.baseMipLevel;
           mipLevel < key.baseMipLevel + key.mipCount; ++mipLevel) {
        auto &info = surfaceInfo.getSubresourceInfo(mipLevel);
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
      }
    } else {
      auto &tiler = mParent->mDevice->tiler;

      std::uint64_t linearOffset = 0;
      for (unsigned mipLevel = key.baseMipLevel;
           mipLevel < key.baseMipLevel + key.mipCount; ++mipLevel) {
        auto &info = surfaceInfo.getSubresourceInfo(mipLevel);

        regions.push_back({
            .bufferOffset = linearOffset,
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

        linearOffset += info.linearSize * key.arrayLayerCount;
      }

      auto detiledSize = linearOffset;

      auto detiledBuffer =
          vk::Buffer::Allocate(vk::getDeviceLocalMemory(), detiledSize,
                               VK_BUFFER_USAGE_2_TRANSFER_DST_BIT_KHR |
                                   VK_BUFFER_USAGE_2_TRANSFER_SRC_BIT_KHR);

      sourceBuffer = detiledBuffer.getHandle();
      std::uint64_t dstAddress = detiledBuffer.getAddress();

      mScheduler->afterSubmit([detiledBuffer = std::move(detiledBuffer)] {});

      for (unsigned mipLevel = key.baseMipLevel;
           mipLevel < key.baseMipLevel + key.mipCount; ++mipLevel) {
        auto &info = surfaceInfo.getSubresourceInfo(mipLevel);

        tiler.detile(*mScheduler, surfaceInfo, key.tileMode, key.dfmt,
                     tiledBuffer.deviceAddress, surfaceInfo.totalSize,
                     dstAddress, detiledSize, mipLevel, 0, key.arrayLayerCount);

        detiledSize -= info.linearSize * key.arrayLayerCount;
        dstAddress += info.linearSize * key.arrayLayerCount;
      }
    }

    transitionImageLayout(
        mScheduler->getCommandBuffer(), image, VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

    vkCmdCopyBufferToImage(
        mScheduler->getCommandBuffer(), sourceBuffer, image.getHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());

    transitionImageLayout(mScheduler->getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresourceRange);
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
  mStorage->mAcquiredResources.push_back(cached);

  return {
      .handle = cached->image.getHandle(),
      .format = format,
      .subresource = subresourceRange,
  };
}

Cache::ImageView Cache::Tag::getImageView(const ImageKey &key, Access access) {
  auto image = getImage(key, access);
  auto result = vk::ImageView(gnm::toVkImageViewType(key.type), image.handle,
                              image.format, {},
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

  mStorage->mAcquiredResources.push_back(cached);

  return {
      .handle = cached->view.getHandle(),
      .imageHandle = image.handle,
      .subresource = image.subresource,
  };
}

void Cache::Tag::readMemory(void *target, std::uint64_t address,
                            std::uint64_t size) {
  // mParent->flush(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  std::memcpy(target, memoryPtr, size);
}

void Cache::Tag::writeMemory(const void *source, std::uint64_t address,
                             std::uint64_t size) {
  // mParent->invalidate(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  std::memcpy(memoryPtr, source, size);
}

int Cache::Tag::compareMemory(const void *source, std::uint64_t address,
                              std::uint64_t size) {
  // mParent->flush(*mScheduler, address, size);
  auto memoryPtr = RemoteMemory{mParent->mVmIm}.getPointer(address);
  return std::memcmp(memoryPtr, source, size);
}

void Cache::GraphicsTag::release() {
  if (mAcquiredGraphicsDescriptorSet + 1 != 0) {
    getCache()->mGraphicsDescriptorSetPool.release(
        mAcquiredGraphicsDescriptorSet);
    mAcquiredGraphicsDescriptorSet = -1;
  }

  Tag::release();
}

void Cache::ComputeTag::release() {
  if (mAcquiredComputeDescriptorSet + 1 != 0) {
    getCache()->mComputeDescriptorSetPool.release(
        mAcquiredComputeDescriptorSet);
    mAcquiredComputeDescriptorSet = -1;
  }

  Tag::release();
}

void Cache::Tag::release() {
  if (mAcquiredMemoryTable + 1 != 0) {
    getCache()->mMemoryTablePool.release(mAcquiredMemoryTable);
    mAcquiredMemoryTable = -1;
  }

  if (mStorage == nullptr) {
    return;
  }

  std::vector<std::shared_ptr<Entry>> tmpResources;
  while (!mStorage->mAcquiredResources.empty()) {
    auto resource = std::move(mStorage->mAcquiredResources.back());
    mStorage->mAcquiredResources.pop_back();
    resource->flush(*this, *mScheduler, 0, ~static_cast<std::uint64_t>(0));
    tmpResources.push_back(std::move(resource));
  }

  if (!tmpResources.empty()) {
    mScheduler->submit();
    mScheduler->wait();
  }

  mStorage->clear();
  auto storageIndex = mStorage - mParent->mTagStorages;
  // std::println("release tag storage {}", storageIndex);
  mStorage = nullptr;
  mParent->mTagStoragePool.release(storageIndex);
}

Cache::Shader
Cache::GraphicsTag::getPixelShader(const SpiShaderPgm &pgm,
                                   const Registers::Context &context,
                                   std::span<const VkViewport> viewPorts) {
  gcn::PsVGprInput
      psVgprInput[static_cast<std::size_t>(gcn::PsVGprInput::Count)];
  std::size_t psVgprInputs = 0;

  SpiPsInput spiInputAddr = context.spiPsInputAddr;

  if (spiInputAddr.perspSampleEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::IPerspSample;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JPerspSample;
  }
  if (spiInputAddr.perspCenterEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::IPerspCenter;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JPerspCenter;
  }
  if (spiInputAddr.perspCentroidEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::IPerspCentroid;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JPerspCentroid;
  }
  if (spiInputAddr.perspPullModelEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::IW;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JW;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::_1W;
  }
  if (spiInputAddr.linearSampleEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::ILinearSample;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JLinearSample;
  }
  if (spiInputAddr.linearCenterEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::ILinearCenter;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JLinearCenter;
  }
  if (spiInputAddr.linearCentroidEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::ILinearCentroid;
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::JLinearCentroid;
  }
  if (spiInputAddr.posXFloatEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::X;
  }
  if (spiInputAddr.posYFloatEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::Y;
  }
  if (spiInputAddr.posZFloatEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::Z;
  }
  if (spiInputAddr.posWFloatEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::W;
  }
  if (spiInputAddr.frontFaceEna) {
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::FrontFace;
  }
  if (spiInputAddr.ancillaryEna) {
    rx::die("unimplemented ancillary fs input");
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::Ancillary;
  }
  if (spiInputAddr.sampleCoverageEna) {
    rx::die("unimplemented sample coverage fs input");
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::SampleCoverage;
  }
  if (spiInputAddr.posFixedPtEna) {
    rx::die("unimplemented pos fixed fs input");
    psVgprInput[psVgprInputs++] = gcn::PsVGprInput::PosFixed;
  }

  return getShader(gcn::Stage::Ps, pgm, context, {}, viewPorts,
                   {psVgprInput, psVgprInputs});
}

Cache::Shader
Cache::GraphicsTag::getVertexShader(gcn::Stage stage, const SpiShaderPgm &pgm,
                                    const Registers::Context &context,
                                    gnm::PrimitiveType vsPrimType,
                                    std::span<const VkViewport> viewPorts) {
  return getShader(stage, pgm, context, vsPrimType, viewPorts, {});
}

Cache::Shader
Cache::GraphicsTag::getShader(gcn::Stage stage, const SpiShaderPgm &pgm,
                              const Registers::Context &context,
                              gnm::PrimitiveType vsPrimType,
                              std::span<const VkViewport> viewPorts,
                              std::span<const gcn::PsVGprInput> psVgprInput) {
  auto descriptorSets = getDescriptorSets();
  gcn::Environment env{
      .vgprCount = pgm.rsrc1.getVGprCount(),
      .sgprCount = pgm.rsrc1.getSGprCount(),
      .userSgprs = std::span(pgm.userData.data(), pgm.rsrc2.userSgpr),
  };

  auto shader = Tag::getShader({
      .address = pgm.address << 8,
      .stage = stage,
      .env = env,
  });

  if (!shader.handle) {
    return shader;
  }

  std::uint64_t memoryTableAddress = getMemoryTable().deviceAddress;

  std::uint64_t gdsAddress = mParent->getGdsBuffer().getAddress();
  mStorage->shaderResources.cacheTag = this;

  std::uint32_t slotOffset = mStorage->shaderResources.slotOffset;

  mStorage->shaderResources.loadResources(
      shader.info->resources,
      std::span(pgm.userData.data(), pgm.rsrc2.userSgpr));

  const auto &configSlots = shader.info->configSlots;

  auto configSize = configSlots.size() * sizeof(std::uint32_t);
  auto configBuffer = getInternalHostVisibleBuffer(configSize);

  auto configPtr = reinterpret_cast<std::uint32_t *>(configBuffer.data);

  for (std::size_t index = 0; const auto &slot : configSlots) {
    switch (slot.type) {
    case gcn::ConfigType::Imm:
      readMemory(&configPtr[index], slot.data, sizeof(std::uint32_t));
      break;
    case gcn::ConfigType::UserSgpr:
      configPtr[index] = pgm.userData[slot.data];
      break;
    case gcn::ConfigType::ViewPortOffsetX:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].xOffset /
                                           (viewPorts[slot.data].width / 2.f) -
                                       1);
      break;
    case gcn::ConfigType::ViewPortOffsetY:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].yOffset /
                                           (viewPorts[slot.data].height / 2.f) -
                                       1);
      break;
    case gcn::ConfigType::ViewPortOffsetZ:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].zOffset);
      break;
    case gcn::ConfigType::ViewPortScaleX:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].xScale /
                                       (viewPorts[slot.data].width / 2.f));
      break;
    case gcn::ConfigType::ViewPortScaleY:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].yScale /
                                       (viewPorts[slot.data].height / 2.f));
      break;
    case gcn::ConfigType::ViewPortScaleZ:
      configPtr[index] =
          std::bit_cast<std::uint32_t>(context.paClVports[slot.data].zScale);
      break;
    case gcn::ConfigType::PsInputVGpr:
      if (slot.data > psVgprInput.size()) {
        configPtr[index] = ~0;
      } else {
        configPtr[index] = std::bit_cast<std::uint32_t>(psVgprInput[slot.data]);
      }
      break;
    case gcn::ConfigType::VsPrimType:
      configPtr[index] = static_cast<std::uint32_t>(vsPrimType);
      break;

    case gcn::ConfigType::ResourceSlot:
      mStorage->memoryTableConfigSlots.push_back({
          .bufferIndex =
              static_cast<std::uint32_t>(mStorage->descriptorBuffers.size()),
          .configIndex = static_cast<std::uint32_t>(index),
          .resourceSlot = static_cast<std::uint32_t>(slotOffset + slot.data),
      });
      break;

    case gcn::ConfigType::MemoryTable:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(memoryTableAddress);
      } else {
        configPtr[index] = static_cast<std::uint32_t>(memoryTableAddress >> 32);
      }
      break;
    case gcn::ConfigType::Gds:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(gdsAddress);
      } else {
        configPtr[index] = static_cast<std::uint32_t>(gdsAddress >> 32);
      }
      break;

    case gcn::ConfigType::CbCompSwap:
      configPtr[index] = std::bit_cast<std::uint32_t>(
          context.cbColor[slot.data].info.compSwap);
      break;

    default:
      rx::die("unexpected resource slot in graphics shader %u, stage %u",
              int(slot.type), int(stage));
    }

    ++index;
  }

  mStorage->descriptorBuffers.push_back(configPtr);

  VkDescriptorBufferInfo bufferInfo{
      .buffer = configBuffer.handle,
      .offset = configBuffer.offset,
      .range = configSize,
  };

  auto stageIndex = Cache::getStageIndex(shader.stage);

  VkWriteDescriptorSet writeDescSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets[stageIndex],
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .pBufferInfo = &bufferInfo,
  };

  vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
  return shader;
}

Cache::Shader
Cache::ComputeTag::getShader(const Registers::ComputeConfig &pgm) {
  auto descriptorSet = getDescriptorSet();
  gcn::Environment env{
      .vgprCount = pgm.rsrc1.getVGprCount(),
      .sgprCount = pgm.rsrc1.getSGprCount(),
      .numThreadX = static_cast<std::uint8_t>(pgm.numThreadX),
      .numThreadY = static_cast<std::uint8_t>(pgm.numThreadY),
      .numThreadZ = static_cast<std::uint8_t>(pgm.numThreadZ),
      .userSgprs = std::span(pgm.userData.data(), pgm.rsrc2.userSgpr),
  };

  auto shader = Tag::getShader({
      .address = pgm.address << 8,
      .stage = gcn::Stage::Cs,
      .env = env,
  });

  if (!shader.handle) {
    return shader;
  }

  std::uint64_t memoryTableAddress = getMemoryTable().deviceAddress;

  std::uint64_t gdsAddress = mParent->getGdsBuffer().getAddress();
  mStorage->shaderResources.cacheTag = this;

  std::uint32_t slotOffset = mStorage->shaderResources.slotOffset;

  mStorage->shaderResources.loadResources(
      shader.info->resources,
      std::span(pgm.userData.data(), pgm.rsrc2.userSgpr));

  const auto &configSlots = shader.info->configSlots;

  auto configSize = configSlots.size() * sizeof(std::uint32_t);
  auto configBuffer = getInternalHostVisibleBuffer(configSize);

  auto configPtr = reinterpret_cast<std::uint32_t *>(configBuffer.data);

  std::uint32_t sgprInput[static_cast<std::size_t>(gcn::CsSGprInput::Count)];
  std::uint32_t sgprInputCount = 0;

  if (pgm.rsrc2.tgIdXEn) {
    sgprInput[sgprInputCount++] =
        static_cast<std::uint32_t>(gcn::CsSGprInput::ThreadGroupIdX);
  }

  if (pgm.rsrc2.tgIdYEn) {
    sgprInput[sgprInputCount++] =
        static_cast<std::uint32_t>(gcn::CsSGprInput::ThreadGroupIdY);
  }

  if (pgm.rsrc2.tgIdZEn) {
    sgprInput[sgprInputCount++] =
        static_cast<std::uint32_t>(gcn::CsSGprInput::ThreadGroupIdZ);
  }

  if (pgm.rsrc2.tgSizeEn) {
    sgprInput[sgprInputCount++] =
        static_cast<std::uint32_t>(gcn::CsSGprInput::ThreadGroupSize);
  }

  if (pgm.rsrc2.scratchEn) {
    sgprInput[sgprInputCount++] =
        static_cast<std::uint32_t>(gcn::CsSGprInput::Scratch);
  }

  for (std::size_t index = 0; const auto &slot : configSlots) {
    switch (slot.type) {
    case gcn::ConfigType::Imm:
      readMemory(&configPtr[index], slot.data, sizeof(std::uint32_t));
      break;
    case gcn::ConfigType::UserSgpr:
      configPtr[index] = pgm.userData[slot.data];
      break;
    case gcn::ConfigType::ResourceSlot:
      mStorage->memoryTableConfigSlots.push_back({
          .bufferIndex =
              static_cast<std::uint32_t>(mStorage->descriptorBuffers.size()),
          .configIndex = static_cast<std::uint32_t>(index),
          .resourceSlot = static_cast<std::uint32_t>(slotOffset + slot.data),
      });
      break;

    case gcn::ConfigType::MemoryTable:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(memoryTableAddress);
      } else {
        configPtr[index] = static_cast<std::uint32_t>(memoryTableAddress >> 32);
      }
      break;
    case gcn::ConfigType::Gds:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(gdsAddress);
      } else {
        configPtr[index] = static_cast<std::uint32_t>(gdsAddress >> 32);
      }
      break;

    case gcn::ConfigType::CsTgIdCompCnt:
      configPtr[index] = pgm.rsrc2.tidIgCompCount;
      break;

    case gcn::ConfigType::CsInputSGpr:
      if (slot.data < sgprInputCount) {
        configPtr[index] = sgprInput[slot.data];
      } else {
        configPtr[index] = -1;
      }
      break;

    default:
      rx::die("unexpected resource slot in compute shader %u", int(slot.type));
    }

    ++index;
  }

  mStorage->descriptorBuffers.push_back(configPtr);

  VkDescriptorBufferInfo bufferInfo{
      .buffer = configBuffer.handle,
      .offset = configBuffer.offset,
      .range = configSize,
  };

  VkWriteDescriptorSet writeDescSet{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSet,
      .dstBinding = 0,
      .descriptorCount = 1,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .pBufferInfo = &bufferInfo,
  };

  vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
  return shader;
}

Cache::Cache(Device *device, int vmId) : mDevice(device), mVmIm(vmId) {
  mMemoryTableBuffer = vk::Buffer::Allocate(
      vk::getHostVisibleMemory(), kMemoryTableSize * kMemoryTableCount);

  mGdsBuffer = vk::Buffer::Allocate(vk::getHostVisibleMemory(), 0x40000);

  {
    VkDescriptorSetLayoutBinding bindings[kGraphicsStages.size()]
                                         [kDescriptorBindings.size()];

    for (std::size_t index = 0; auto stage : kGraphicsStages) {
      fillStageBindings(bindings[index], stage, index);
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

    fillStageBindings(bindings, VK_SHADER_STAGE_COMPUTE_BIT, 0);

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

  {
    VkDescriptorPoolSize descriptorPoolSizes[]{
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 4 * (kDescriptorSetCount * 2 / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 3 * 32 * (kDescriptorSetCount * 2 / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 32 * (kDescriptorSetCount * 2 / 4),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 32 * (kDescriptorSetCount * 2 / 4),
        },
    };

    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = kDescriptorSetCount * 2,
        .poolSizeCount = static_cast<uint32_t>(std::size(descriptorPoolSizes)),
        .pPoolSizes = descriptorPoolSizes,
    };

    VK_VERIFY(vkCreateDescriptorPool(vk::context->device, &info,
                                     vk::context->allocator, &mDescriptorPool));
  }

  {
    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPool,
        .descriptorSetCount =
            static_cast<uint32_t>(mGraphicsDescriptorSetLayouts.size()),
        .pSetLayouts = mGraphicsDescriptorSetLayouts.data(),
    };

    for (auto &graphicsSet : mGraphicsDescriptorSets) {
      vkAllocateDescriptorSets(vk::context->device, &info, graphicsSet.data());
    }
  }

  {
    VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = mDescriptorPool,
        .descriptorSetCount = 1,
        .pSetLayouts = &mComputeDescriptorSetLayout,
    };

    for (auto &computeSet : mComputeDescriptorSets) {
      vkAllocateDescriptorSets(vk::context->device, &info, &computeSet);
    }
  }
}

Cache::~Cache() {
  for (auto &samp : mSamplers) {
    vkDestroySampler(vk::context->device, samp.second, vk::context->allocator);
  }

  vkDestroyDescriptorPool(vk::context->device, mDescriptorPool,
                          vk::context->allocator);

  vkDestroyPipelineLayout(vk::context->device, mGraphicsPipelineLayout,
                          vk::context->allocator);
  vkDestroyPipelineLayout(vk::context->device, mComputePipelineLayout,
                          vk::context->allocator);

  for (auto &layout : mGraphicsDescriptorSetLayouts) {
    vkDestroyDescriptorSetLayout(vk::context->device, layout,
                                 vk::context->allocator);
  }
  vkDestroyDescriptorSetLayout(vk::context->device, mComputeDescriptorSetLayout,
                               vk::context->allocator);
}

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
  scheduler.submit();
  scheduler.wait();
}
