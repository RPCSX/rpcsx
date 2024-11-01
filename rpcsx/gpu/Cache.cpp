#include "Cache.hpp"
#include "Device.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/vulkan.hpp"
#include "rx/Config.hpp"
#include "rx/hexdump.hpp"
#include "rx/mem.hpp"
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
#include <rx/AddressRange.hpp>
#include <rx/MemoryTable.hpp>
#include <rx/die.hpp>
#include <rx/format.hpp>
#include <utility>
#include <vulkan/vulkan_core.h>

using namespace amdgpu;
using namespace shader;

static bool testHostInvalidations(Device *device, int vmId,
                                  std::uint64_t address, std::uint64_t size) {
  auto firstPage = address / rx::mem::pageSize;
  auto lastPage = (address + size + rx::mem::pageSize - 1) / rx::mem::pageSize;

  for (auto page = firstPage; page < lastPage; ++page) {
    auto prevValue =
        device->cachePages[vmId][page].load(std::memory_order::relaxed);

    if (~prevValue & kPageInvalidated) {
      continue;
    }

    return true;
  }

  return false;
}

static bool handleHostInvalidations(Device *device, int vmId,
                                    std::uint64_t address, std::uint64_t size) {
  auto firstPage = address / rx::mem::pageSize;
  auto lastPage = (address + size + rx::mem::pageSize - 1) / rx::mem::pageSize;

  bool hasInvalidations = false;

  for (auto page = firstPage; page < lastPage; ++page) {
    auto prevValue =
        device->cachePages[vmId][page].load(std::memory_order::relaxed);

    if (~prevValue & kPageInvalidated) {
      continue;
    }

    while (!device->cachePages[vmId][page].compare_exchange_weak(
        prevValue, prevValue & ~kPageInvalidated, std::memory_order::relaxed)) {
    }

    hasInvalidations = true;
  }

  return hasInvalidations;
}

static void markHostInvalidated(Device *device, int vmId, std::uint64_t address,
                                std::uint64_t size) {
  auto firstPage = address / rx::mem::pageSize;
  auto lastPage = (address + size + rx::mem::pageSize - 1) / rx::mem::pageSize;

  for (auto page = firstPage; page < lastPage; ++page) {
    std::uint8_t prevValue = 0;

    while (!device->cachePages[vmId][page].compare_exchange_weak(
        prevValue, prevValue | kPageInvalidated, std::memory_order::relaxed)) {
    }
  }
}

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

    bufferMemoryTable.map(*pointerBase + *pointerOffset,
                          *pointerBase + *pointerOffset + pointer.size,
                          Access::Read);
    resourceSlotToAddress.emplace_back(slotOffset + pointer.resourceSlot,
                                       *pointerBase + *pointerOffset);
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

    if (auto it = bufferMemoryTable.queryArea(buffer.address());
        it != bufferMemoryTable.end() &&
        it.beginAddress() == buffer.address() && it.size() == buffer.size()) {
      it.get() |= bufferRes.access;
    } else {
      bufferMemoryTable.map(buffer.address(), buffer.address() + buffer.size(),
                            bufferRes.access);
    }
    resourceSlotToAddress.emplace_back(slotOffset + bufferRes.resourceSlot,
                                       buffer.address());
  }

  for (auto &imageBuffer : res.imageBuffers) {
    auto word0 = eval(imageBuffer.words[0]).zExtScalar();
    auto word1 = eval(imageBuffer.words[1]).zExtScalar();
    auto word2 = eval(imageBuffer.words[2]).zExtScalar();
    auto word3 = eval(imageBuffer.words[3]).zExtScalar();

    if (!word0 || !word1 || !word2 || !word3) {
      res.dump();
      rx::die("failed to evaluate V#");
    }

    gnm::TBuffer tbuffer{};
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer), &*word0,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 1, &*word1,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 2, &*word2,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 3, &*word3,
                sizeof(std::uint32_t));

    if (imageBuffer.words[4] != nullptr) {
      auto word4 = eval(imageBuffer.words[4]).zExtScalar();
      auto word5 = eval(imageBuffer.words[5]).zExtScalar();
      auto word6 = eval(imageBuffer.words[6]).zExtScalar();
      auto word7 = eval(imageBuffer.words[7]).zExtScalar();

      if (!word4 || !word5 || !word6 || !word7) {
        res.dump();
        rx::die("failed to evaluate 256 bit T#");
      }

      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 4, &*word4,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 5, &*word5,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 6, &*word6,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 7, &*word7,
                  sizeof(std::uint32_t));
    }

    auto info = computeSurfaceInfo(
        getDefaultTileModes()[tbuffer.tiling_idx], tbuffer.type, tbuffer.dfmt,
        tbuffer.width + 1, tbuffer.height + 1, tbuffer.depth + 1,
        tbuffer.pitch + 1, 0, tbuffer.last_array + 1, 0, tbuffer.last_level + 1,
        tbuffer.pow2pad != 0);

    if (auto it = imageMemoryTable.queryArea(tbuffer.address());
        it != imageMemoryTable.end() &&
        it.beginAddress() == tbuffer.address() &&
        it.size() == info.totalTiledSize) {
      it.get().second |= imageBuffer.access;
    } else {
      imageMemoryTable.map(
          tbuffer.address(), tbuffer.address() + info.totalTiledSize,
          {ImageBufferKey::createFrom(tbuffer), imageBuffer.access});
    }
    resourceSlotToAddress.emplace_back(slotOffset + imageBuffer.resourceSlot,
                                       tbuffer.address());
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

    gnm::TBuffer tbuffer{};
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer), &*word0,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 1, &*word1,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 2, &*word2,
                sizeof(std::uint32_t));
    std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 3, &*word3,
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

      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 4, &*word4,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 5, &*word5,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 6, &*word6,
                  sizeof(std::uint32_t));
      std::memcpy(reinterpret_cast<std::uint32_t *>(&tbuffer) + 7, &*word7,
                  sizeof(std::uint32_t));
    }

    std::vector<amdgpu::Cache::ImageView> *resources = nullptr;

    switch (tbuffer.type) {
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
              static_cast<unsigned>(tbuffer.type));

    slotResources[slotOffset + texture.resourceSlot] = resources->size();
    resources->push_back(cacheTag->getImageView(
        amdgpu::ImageViewKey::createFrom(tbuffer), texture.access));
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
    auto range = rx::AddressRange::fromBeginEnd(p.beginAddress, p.endAddress);
    auto buffer = cacheTag->getBuffer(range, p.payload);

    auto memoryTableSlot = memoryTable.count;
    memoryTable.slots[memoryTable.count++] = {
        .address = p.beginAddress,
        .size = range.size(),
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
void Cache::ShaderResources::buildImageMemoryTable(MemoryTable &memoryTable) {
  memoryTable.count = 0;

  for (auto p : imageMemoryTable) {
    auto range = rx::AddressRange::fromBeginEnd(p.beginAddress, p.endAddress);
    auto buffer = cacheTag->getImageBuffer(p.payload.first, p.payload.second);

    auto memoryTableSlot = memoryTable.count;
    memoryTable.slots[memoryTable.count++] = {
        .address = p.beginAddress,
        .size = range.size(),
        .flags = static_cast<uint8_t>(p.payload.second),
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

  if (instId == ir::amdgpu::IMAGE_BUFFER) {
    rx::die("resource depends on image buffer value");
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
    cacheTag->readMemory(
        &result, rx::AddressRange::fromBeginSize(address, sizeof(result)));
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

  auto layoutToStageAccess =
      [](VkImageLayout layout,
         bool isSrc) -> std::pair<VkPipelineStageFlags, VkAccessFlags> {
    switch (layout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    case VK_IMAGE_LAYOUT_GENERAL:
      return {isSrc ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
                    : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
              0};

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

  auto [sourceStage, sourceAccess] = layoutToStageAccess(oldLayout, true);
  auto [destinationStage, destinationAccess] =
      layoutToStageAccess(newLayout, false);

  barrier.srcAccessMask = sourceAccess;
  barrier.dstAccessMask = destinationAccess;

  vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);
}

struct Cache::Entry {
  virtual ~Entry() = default;

  Cache::TagStorage *acquiredTag = nullptr;
  TagId tagId{};
  bool hasDelayedFlush = false;
  rx::AddressRange addressRange;
  EntryType type;
  std::atomic<Access> acquiredAccess = Access::None;

  [[nodiscard]] bool isInUse() const {
    return acquiredAccess.load(std::memory_order::relaxed) != Access::None;
  }

  void acquire(Cache::Tag *tag, Access access) {
    auto expAccess = Access::None;

    while (true) {
      if (acquiredAccess.compare_exchange_strong(expAccess, access)) {
        break;
      }

      if (acquiredTag == tag->mStorage) {
        acquiredAccess.store(expAccess | access, std::memory_order::relaxed);
        break;
      }

      acquiredAccess.wait(expAccess, std::memory_order::relaxed);
    }

    acquiredTag = tag->mStorage;
  }

  bool release(Cache::Tag *tag) {
    if (acquiredTag != tag->mStorage) {
      return false;
    }

    auto access = acquiredAccess.load(std::memory_order::relaxed);
    bool hasSubmits = false;
    if ((access & Access::Write) == Access::Write) {
      tagId = tag->getWriteId();
      hasSubmits = release(tag, access);
    }

    acquiredTag = nullptr;

    acquiredAccess.store(Access::None, std::memory_order::release);
    acquiredAccess.notify_one();

    return hasSubmits;
  }

  virtual bool release(Cache::Tag *tag, Access access) { return false; }
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

  void update(Cache::Tag &tag, std::span<const rx::AddressRange> ranges,
              CachedBuffer *from) {
    std::vector<VkBufferCopy> regions;
    regions.reserve(ranges.size());

    for (auto range : ranges) {
      auto selfRange = addressRange.intersection(range);
      auto fromRange = from->addressRange.intersection(range);

      assert(selfRange.size() == fromRange.size());

      regions.push_back(
          {.srcOffset =
               fromRange.beginAddress() - from->addressRange.beginAddress(),
           .dstOffset = selfRange.beginAddress() - addressRange.beginAddress(),
           .size = selfRange.size()});
    }

    vkCmdCopyBuffer(tag.getScheduler().getCommandBuffer(),
                    from->buffer.getHandle(), buffer.getHandle(),
                    regions.size(), regions.data());
  }
};

struct CachedHostVisibleBuffer : CachedBuffer {
  using CachedBuffer::update;

  bool expensive() {
    return !rx::g_config.disableGpuCache &&
           addressRange.size() >= rx::mem::pageSize;
  }

  bool flush(void *target, rx::AddressRange range) {
    if (!hasDelayedFlush) {
      return false;
    }

    hasDelayedFlush = false;

    auto data =
        buffer.getData() + range.beginAddress() - addressRange.beginAddress();
    std::memcpy(target, data, range.size());

    return false;
  }

  void update(rx::AddressRange range, void *from) {
    auto data =
        buffer.getData() + range.beginAddress() - addressRange.beginAddress();
    std::memcpy(data, from, range.size());
  }

  bool release(Cache::Tag *tag, Access) override {
    if (addressRange.beginAddress() == 0) {
      return false;
    }

    auto locked = expensive();
    tag->getCache()->trackWrite(addressRange, tagId, locked);
    hasDelayedFlush = true;

    if (locked) {
      return false;
    }

    auto address =
        RemoteMemory{tag->getVmId()}.getPointer(addressRange.beginAddress());
    return flush(address, addressRange);
  }
};

struct CachedIndexBuffer : Cache::Entry {
  vk::Buffer buffer;
  std::uint64_t offset;
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

struct CachedImageBuffer : Cache::Entry {
  vk::Buffer buffer;
  GpuTiler *tiler;
  TileMode tileMode{};
  gnm::DataFormat dfmt{};
  std::uint32_t pitch{};
  SurfaceInfo info;
  unsigned mipLevels = 1;
  unsigned arrayLayers = 1;
  unsigned width = 1;
  unsigned height = 1;
  unsigned depth = 1;

  bool expensive() {
    return false;
  }

  [[nodiscard]] bool isLinear() const {
    return tileMode.arrayMode() == kArrayModeLinearGeneral ||
           tileMode.arrayMode() == kArrayModeLinearAligned;
  }

  [[nodiscard]] VkImageSubresourceRange
  getSubresource(rx::AddressRange range) const {
    auto offset = range.beginAddress() - addressRange.beginAddress();
    auto size = range.size();
    std::uint32_t firstMip = -1;
    std::uint32_t lastMip = 0;

    for (std::uint32_t mipLevel = 0; mipLevel < mipLevels; ++mipLevel) {
      auto &mipInfo = info.getSubresourceInfo(mipLevel);
      if (mipInfo.tiledOffset > offset + size) {
        break;
      }

      if (mipInfo.tiledOffset + mipInfo.tiledSize * arrayLayers < offset) {
        continue;
      }

      firstMip = std::min(firstMip, mipLevel);
      lastMip = std::max(lastMip, mipLevel);
    }

    assert(firstMip <= lastMip);

    return {
        .aspectMask = 0,
        .baseMipLevel = firstMip,
        .levelCount = lastMip - firstMip + 1,
        .baseArrayLayer = 0,
        .layerCount = arrayLayers,
    };
  }

  [[nodiscard]] std::size_t getTiledSize() const { return info.totalTiledSize; }
  [[nodiscard]] std::size_t getLinerSize() const {
    return info.totalLinearSize;
  }

  void update(Cache::Tag *tag, rx::AddressRange range,
              Cache::Buffer tiledBuffer) {
    auto subresource = getSubresource(range);
    auto &sched = tag->getScheduler();

    if (!isLinear()) {
      auto linearAddress = buffer.getAddress();

      for (unsigned mipLevel = subresource.baseMipLevel;
           mipLevel < subresource.baseMipLevel + subresource.levelCount;
           ++mipLevel) {
        tiler->detile(sched, info, tileMode, tiledBuffer.deviceAddress,
                      info.totalTiledSize, linearAddress, info.totalLinearSize,
                      mipLevel, 0, info.arrayLayerCount);
      }
      return;
    }

    std::vector<VkBufferCopy> regions;
    regions.reserve(subresource.levelCount);

    for (unsigned mipLevel = subresource.baseMipLevel;
         mipLevel < subresource.baseMipLevel + subresource.levelCount;
         ++mipLevel) {
      auto &mipInfo = info.getSubresourceInfo(mipLevel);
      regions.push_back({
          .srcOffset = mipInfo.tiledOffset + tiledBuffer.offset,
          .dstOffset = mipInfo.linearOffset,
          .size = mipInfo.linearSize,
      });
    }

    vkCmdCopyBuffer(sched.getCommandBuffer(), tiledBuffer.handle,
                    buffer.getHandle(), regions.size(), regions.data());
  }

  void write(Scheduler &scheduler, Cache::Buffer tiledBuffer,
             const VkImageSubresourceRange &subresourceRange) {
    if (!isLinear()) {
      for (unsigned mipLevel = 0; mipLevel < subresourceRange.levelCount;
           ++mipLevel) {
        tiler->tile(scheduler, info, tileMode, buffer.getAddress(),
                    info.totalLinearSize, tiledBuffer.deviceAddress,
                    info.totalTiledSize, mipLevel, 0,
                    subresourceRange.levelCount);
      }

      return;
    }

    std::vector<VkBufferCopy> regions;
    regions.reserve(subresourceRange.levelCount);

    for (unsigned mipLevelOffset = 0;
         mipLevelOffset < subresourceRange.levelCount; ++mipLevelOffset) {
      auto mipLevel = mipLevelOffset + subresourceRange.baseMipLevel;
      auto &mipInfo = info.getSubresourceInfo(mipLevel);

      regions.push_back({
          .srcOffset = mipInfo.linearOffset,
          .dstOffset = mipInfo.tiledOffset + tiledBuffer.offset,
          .size = mipInfo.linearSize,
      });
    }

    vkCmdCopyBuffer(scheduler.getCommandBuffer(), buffer.getHandle(),
                    tiledBuffer.handle, regions.size(), regions.data());
  }

  bool flush(Cache::Tag &tag, Scheduler &scheduler, rx::AddressRange range) {
    if (!hasDelayedFlush) {
      return false;
    }

    hasDelayedFlush = false;

    auto subresourceRange = getSubresource(range);
    auto beginOffset =
        info.getSubresourceInfo(subresourceRange.baseMipLevel).tiledOffset;
    auto lastLevelInfo = info.getSubresourceInfo(
        subresourceRange.baseMipLevel + subresourceRange.levelCount - 1);
    auto totalTiledSubresourceSize =
        lastLevelInfo.tiledOffset +
        lastLevelInfo.tiledSize * subresourceRange.layerCount;

    auto targetRange = rx::AddressRange::fromBeginSize(
        range.beginAddress() + beginOffset, totalTiledSubresourceSize);

    auto tiledBuffer = tag.getBuffer(targetRange, Access::Write);

    write(scheduler, tiledBuffer, subresourceRange);
    return true;
  }

  bool release(Cache::Tag *tag, Access) override {
    hasDelayedFlush = true;
    auto locked = expensive();

    for (auto &subresource : std::span(info.subresources, mipLevels)) {
      auto subresourceRange = rx::AddressRange::fromBeginSize(
          subresource.tiledOffset + addressRange.beginAddress(),
          subresource.tiledSize);

      tag->getCache()->trackWrite(subresourceRange, tagId, locked);
    }

    if (locked) {
      return false;
    }

    return flush(*tag, tag->getScheduler(), addressRange);
  }
};

struct CachedImage : Cache::Entry {
  vk::Image image;
  ImageKind kind;
  ImageBufferKey imageBufferKey;
  SurfaceInfo info;

  bool expensive() {
    return false;
    if (rx::g_config.disableGpuCache) {
      return false;
    }

    return info.totalTiledSize >= rx::mem::pageSize;
  }

  [[nodiscard]] VkImageSubresourceRange
  getSubresource(rx::AddressRange range) const {
    auto offset = range.beginAddress() - addressRange.beginAddress();
    auto size = range.size();
    std::uint32_t firstMip = -1;
    std::uint32_t lastMip = 0;

    for (std::uint32_t mipLevel = 0; mipLevel < image.getMipLevels();
         ++mipLevel) {
      auto &mipInfo = info.getSubresourceInfo(mipLevel);
      if (mipInfo.tiledOffset > offset + size) {
        break;
      }

      if (mipInfo.tiledOffset + mipInfo.tiledSize * image.getArrayLayers() <
          offset) {
        continue;
      }

      firstMip = std::min(firstMip, mipLevel);
      lastMip = std::max(lastMip, mipLevel);
    }

    assert(firstMip <= lastMip);

    return {
        .aspectMask = toAspect(kind),
        .baseMipLevel = firstMip,
        .levelCount = lastMip - firstMip + 1,
        .baseArrayLayer = 0,
        .layerCount = image.getArrayLayers(),
    };
  }

  [[nodiscard]] std::size_t getTiledSize() const { return info.totalTiledSize; }
  [[nodiscard]] std::size_t getLinerSize() const {
    return info.totalLinearSize;
  }

  void update(Cache::Tag *tag, rx::AddressRange range,
              Cache::ImageBuffer imageBuffer) {
    auto subresource = getSubresource(range);

    std::vector<VkBufferImageCopy> regions;
    regions.reserve(subresource.levelCount);

    auto &sched = tag->getScheduler();

    for (unsigned mipLevel = subresource.baseMipLevel;
         mipLevel < subresource.baseMipLevel + subresource.levelCount;
         ++mipLevel) {
      auto &mipInfo = info.getSubresourceInfo(mipLevel);
      regions.push_back({
          .bufferOffset = mipInfo.tiledOffset + imageBuffer.offset,
          .bufferRowLength =
              mipLevel > 0 ? 0 : std::max(imageBufferKey.pitch >> mipLevel, 1u),
          .imageSubresource =
              {
                  .aspectMask = toAspect(kind),
                  .mipLevel = mipLevel,
                  .baseArrayLayer = subresource.baseArrayLayer,
                  .layerCount = subresource.layerCount,
              },
          .imageExtent =
              {
                  .width = std::max(image.getWidth() >> mipLevel, 1u),
                  .height = std::max(image.getHeight() >> mipLevel, 1u),
                  .depth = std::max(image.getDepth() >> mipLevel, 1u),
              },
      });
    }

    transitionImageLayout(sched.getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresource);

    vkCmdCopyBufferToImage(
        sched.getCommandBuffer(), imageBuffer.handle, image.getHandle(),
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, regions.size(), regions.data());

    transitionImageLayout(sched.getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresource);
  }

  void write(Scheduler &scheduler, Cache::ImageBuffer imageBuffer,
             rx::AddressRange range) {
    auto subresourceRange = getSubresource(range);
    std::vector<VkBufferImageCopy> regions;
    regions.reserve(subresourceRange.levelCount);

    for (unsigned mipLevelOffset = 0;
         mipLevelOffset < subresourceRange.levelCount; ++mipLevelOffset) {
      auto mipLevel = mipLevelOffset + subresourceRange.baseMipLevel;
      auto &regionInfo = info.getSubresourceInfo(mipLevel);

      regions.push_back({
          .bufferOffset = imageBuffer.offset + regionInfo.linearOffset,
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

    transitionImageLayout(
        scheduler.getCommandBuffer(), image, VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);

    vkCmdCopyImageToBuffer(scheduler.getCommandBuffer(), image.getHandle(),
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           imageBuffer.handle, regions.size(), regions.data());

    transitionImageLayout(scheduler.getCommandBuffer(), image,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_GENERAL, subresourceRange);
  }

  bool flush(Cache::Tag &tag, Scheduler &scheduler, rx::AddressRange range) {
    if (!hasDelayedFlush) {
      return false;
    }

    hasDelayedFlush = false;

    auto imageBuffer = tag.getImageBuffer(imageBufferKey, Access::Write);
    write(scheduler, imageBuffer, range);
    return true;
  }

  bool release(Cache::Tag *tag, Access) override {
    hasDelayedFlush = true;
    auto locked = expensive();
    tag->getCache()->trackWrite(addressRange, tagId, locked);

    if (locked) {
      return true;
    }

    return flush(*tag, tag->getScheduler(), addressRange);
  }
};

struct CachedImageView : Cache::Entry {
  vk::ImageView view;
};

ImageViewKey ImageViewKey::createFrom(const gnm::TBuffer &tbuffer) {
  return {
      .readAddress = tbuffer.address(),
      .writeAddress = tbuffer.address(),
      .type = tbuffer.type,
      .dfmt = tbuffer.dfmt,
      .nfmt = tbuffer.nfmt,
      .tileMode = getDefaultTileModes()[tbuffer.tiling_idx],
      .extent =
          {
              .width = tbuffer.width + 1u,
              .height = tbuffer.height + 1u,
              .depth = tbuffer.depth + 1u,
          },
      .pitch = tbuffer.pitch + 1u,
      .baseMipLevel = static_cast<std::uint32_t>(tbuffer.base_level),
      .mipCount = tbuffer.last_level - tbuffer.base_level + 1u,
      .baseArrayLayer = static_cast<std::uint32_t>(tbuffer.base_array),
      .arrayLayerCount = tbuffer.last_array - tbuffer.base_array + 1u,
      .kind = ImageKind::Color,
      .pow2pad = tbuffer.pow2pad != 0,
      .r = tbuffer.dst_sel_x,
      .g = tbuffer.dst_sel_y,
      .b = tbuffer.dst_sel_z,
      .a = tbuffer.dst_sel_w,
  };
}

ImageKey ImageKey::createFrom(const gnm::TBuffer &tbuffer) {
  return {
      .readAddress = tbuffer.address(),
      .writeAddress = tbuffer.address(),
      .type = tbuffer.type,
      .dfmt = tbuffer.dfmt,
      .nfmt = tbuffer.nfmt,
      .tileMode = getDefaultTileModes()[tbuffer.tiling_idx],
      .extent =
          {
              .width = tbuffer.width + 1u,
              .height = tbuffer.height + 1u,
              .depth = tbuffer.depth + 1u,
          },
      .pitch = tbuffer.pitch + 1u,
      .baseMipLevel = static_cast<std::uint32_t>(tbuffer.base_level),
      .mipCount = tbuffer.last_level - tbuffer.base_level + 1u,
      .baseArrayLayer = static_cast<std::uint32_t>(tbuffer.base_array),
      .arrayLayerCount = tbuffer.last_array - tbuffer.base_array + 1u,
      .kind = ImageKind::Color,
      .pow2pad = tbuffer.pow2pad != 0,
  };
}

ImageKey ImageKey::createFrom(const ImageViewKey &imageView) {
  return {
      .readAddress = imageView.readAddress,
      .writeAddress = imageView.writeAddress,
      .type = imageView.type,
      .dfmt = imageView.dfmt,
      .nfmt = imageView.nfmt,
      .tileMode = imageView.tileMode,
      .extent = imageView.extent,
      .pitch = imageView.pitch,
      .baseMipLevel = imageView.baseMipLevel,
      .mipCount = imageView.mipCount,
      .baseArrayLayer = imageView.baseArrayLayer,
      .arrayLayerCount = imageView.arrayLayerCount,
      .kind = imageView.kind,
      .pow2pad = imageView.pow2pad,
  };
}

ImageBufferKey ImageBufferKey::createFrom(const gnm::TBuffer &tbuffer) {
  return {
      .address = tbuffer.address(),
      .type = tbuffer.type,
      .dfmt = tbuffer.dfmt,
      .tileMode = getDefaultTileModes()[tbuffer.tiling_idx],
      .extent =
          {
              .width = tbuffer.width + 1u,
              .height = tbuffer.height + 1u,
              .depth = tbuffer.depth + 1u,
          },
      .pitch = tbuffer.pitch + 1u,
      .baseMipLevel = static_cast<std::uint32_t>(tbuffer.base_level),
      .mipCount = tbuffer.last_level - tbuffer.base_level + 1u,
      .baseArrayLayer = static_cast<std::uint32_t>(tbuffer.base_array),
      .arrayLayerCount = tbuffer.last_array - tbuffer.base_array + 1u,
      .pow2pad = tbuffer.pow2pad != 0,
  };
}

ImageBufferKey ImageBufferKey::createFrom(const ImageKey &imageKey) {
  return {
      .address = imageKey.readAddress,
      .type = imageKey.type,
      .dfmt = imageKey.dfmt,
      .tileMode = imageKey.tileMode,
      .extent = imageKey.extent,
      .pitch = imageKey.pitch,
      .baseMipLevel = imageKey.baseMipLevel,
      .mipCount = imageKey.mipCount,
      .baseArrayLayer = imageKey.baseArrayLayer,
      .arrayLayerCount = imageKey.arrayLayerCount,
      .pow2pad = imageKey.pow2pad,
  };
}

SamplerKey SamplerKey::createFrom(const gnm::SSampler &sampler) {
  float lodBias = sampler.lod_bias / 256.f;
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
      .minLod = sampler.min_lod / 256.f,
      .maxLod = sampler.max_lod / 256.f,
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
    mStorage->mAcquiredViewResources.push_back(result);
    return {
        .handle = cachedShader->handle,
        .info = &cachedShader->info,
        .stage = stage,
    };
  }

  auto vmId = mParent->mVmId;

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

    std::print(stderr, "{}", shader::glsl::decompile(converted->spv));
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

  auto magicRange =
      rx::AddressRange::fromBeginSize(key.address, sizeof(std::uint64_t));
  auto result = std::make_shared<CachedShader>();
  result->addressRange = magicRange;
  result->tagId = getReadId();
  result->handle = handle;
  result->info = std::move(converted->info);
  readMemory(&result->magic, rx::AddressRange::fromBeginSize(
                                 key.address, sizeof(result->magic)));

  for (auto entry : result->info.memoryMap) {
    auto entryRange =
        rx::AddressRange::fromBeginEnd(entry.beginAddress, entry.endAddress);
    auto &inserted = result->usedMemory.emplace_back();
    inserted.first = entryRange.beginAddress();
    inserted.second.resize(entryRange.size());
    readMemory(inserted.second.data(), entryRange);
  }

  auto &info = result->info;

  mParent->trackUpdate(EntryType::Shader, result->addressRange, result,
                       getReadId(), true);
  mStorage->mAcquiredViewResources.push_back(std::move(result));

  return {.handle = handle, .info = &info, .stage = stage};
}

std::shared_ptr<Cache::Entry>
Cache::Tag::findShader(const ShaderKey &key, const ShaderKey *dependedKey) {
  auto magicRange =
      rx::AddressRange::fromBeginSize(key.address, sizeof(std::uint64_t));

  auto result = mParent->getInSyncEntry(EntryType::Shader, magicRange);
  if (result == nullptr) {
    return {};
  }

  std::uint64_t magic;
  readMemory(&magic, magicRange);

  auto cachedShader = static_cast<CachedShader *>(result.get());
  if (cachedShader->magic != magic) {
    return {};
  }

  for (auto [index, sgpr] : cachedShader->info.requiredSgprs) {
    if (index >= key.env.userSgprs.size() || key.env.userSgprs[index] != sgpr) {
      return {};
    }
  }

  for (auto &usedMemory : cachedShader->usedMemory) {
    auto usedRange = rx::AddressRange::fromBeginSize(usedMemory.first,
                                                     usedMemory.second.size());
    if (compareMemory(usedMemory.second.data(), usedRange) != 0) {
      return {};
    }
  }

  return result;
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

Cache::Buffer Cache::Tag::getBuffer(rx::AddressRange range, Access access) {
  auto &table = mParent->getTable(EntryType::HostVisibleBuffer);
  auto it = table.queryArea(range.beginAddress());

  if (it == table.end() || !it.range().contains(range)) {
    auto flushRange = mParent->flushImages(*this, range);
    flushRange = flushRange.merge(mParent->flushImageBuffers(*this, range));
    if (flushRange) {
      mScheduler->submit();
      mScheduler->wait();
    }

    mParent->flushBuffers(range);

    it = table.map(range.beginAddress(), range.endAddress(), nullptr, false,
                   true);
  }

  if (it.get() == nullptr) {
    auto cached = std::make_shared<CachedHostVisibleBuffer>();
    cached->addressRange = range;
    cached->buffer = vk::Buffer::Allocate(
        vk::getHostVisibleMemory(), range.size(),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

    it.get() = std::move(cached);
  }

  mStorage->mAcquiredMemoryResources.push_back(it.get());

  auto cached = static_cast<CachedHostVisibleBuffer *>(it->get());
  cached->acquire(this, access);
  auto addressRange = it.get()->addressRange;

  if ((access & Access::Read) != Access::None) {
    if (!cached->expensive() ||
        handleHostInvalidations(getDevice(), mParent->mVmId,
                                addressRange.beginAddress(),
                                addressRange.size()) ||
        !mParent->isInSync(addressRange, cached->tagId)) {
      auto flushedRange = mParent->flushImages(*this, range);
      flushedRange =
          flushedRange.merge(mParent->flushImageBuffers(*this, range));

      if (flushedRange) {
        getScheduler().submit();
        getScheduler().wait();
      }

      mParent->trackUpdate(
          EntryType::HostVisibleBuffer, addressRange, it.get(), getReadId(),
          (access & Access::Write) == Access::None && cached->expensive());
      amdgpu::RemoteMemory memory{mParent->mVmId};
      cached->update(addressRange,
                     memory.getPointer(addressRange.beginAddress()));
    }
  }

  auto offset = range.beginAddress() - addressRange.beginAddress();
  return {
      .handle = cached->buffer.getHandle(),
      .offset = offset,
      .deviceAddress = cached->buffer.getAddress() + offset,
      .tagId = cached->tagId,
      .data = cached->buffer.getData() + offset,
  };
}

Cache::Buffer Cache::Tag::getInternalHostVisibleBuffer(std::uint64_t size) {
  auto buffer = vk::Buffer::Allocate(vk::getHostVisibleMemory(), size,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                                         VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                         VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

  auto cached = std::make_shared<CachedHostVisibleBuffer>();
  cached->addressRange = rx::AddressRange::fromBeginSize(0, size);
  cached->buffer = std::move(buffer);
  cached->tagId = getReadId();

  mStorage->mAcquiredMemoryResources.push_back(cached);

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

  auto cached = std::make_shared<CachedHostVisibleBuffer>();
  cached->addressRange = rx::AddressRange::fromBeginSize(0, size);
  cached->buffer = std::move(buffer);
  cached->tagId = getReadId();

  mStorage->mAcquiredMemoryResources.push_back(cached);

  return {
      .handle = cached->buffer.getHandle(),
      .offset = 0,
      .deviceAddress = cached->buffer.getAddress(),
      .tagId = getReadId(),
      .data = cached->buffer.getData(),
  };
}

void Cache::Tag::buildDescriptors(VkDescriptorSet descriptorSet) {
  auto &res = mStorage->shaderResources;
  auto memoryTableBuffer = getMemoryTable();
  auto imageMemoryTableBuffer = getImageMemoryTable();
  auto memoryTable = std::bit_cast<MemoryTable *>(memoryTableBuffer.data);
  auto imageMemoryTable =
      std::bit_cast<MemoryTable *>(imageMemoryTableBuffer.data);

  res.buildMemoryTable(*memoryTable);
  res.buildImageMemoryTable(*imageMemoryTable);

  for (auto &sampler : res.samplerResources) {
    uint32_t index = &sampler - res.samplerResources.data();

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

  for (auto &imageResources : res.imageResources) {
    auto dim = (&imageResources - res.imageResources) + 1;
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
                                              std::uint32_t indexOffset,
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
        .offset = indexOffset,
        .indexCount = indexCount,
        .primType = primType,
        .indexType = indexType,
    };
  }

  auto range = rx::AddressRange::fromBeginSize(
      address + static_cast<std::uint64_t>(indexOffset) * origIndexSize, size);

  auto indexBuffer = getBuffer(range, Access::Read);

  if (!isPrimRequiresConversion(primType)) {
    return {
        .handle = indexBuffer.handle,
        .offset = indexBuffer.offset,
        .indexCount = indexCount,
        .primType = primType,
        .indexType = indexType,
    };
  }

  auto &indexBufferTable = mParent->getTable(EntryType::IndexBuffer);
  auto it = indexBufferTable.queryArea(address);
  if (it != indexBufferTable.end() &&
      range.contains(it.range().contains(range))) {

    auto &resource = it.get();
    auto indexBuffer = static_cast<CachedIndexBuffer *>(resource.get());
    if (resource->tagId == indexBuffer->tagId &&
        indexBuffer->addressRange.size() == size) {
      mStorage->mAcquiredViewResources.push_back(resource);

      return {
          .handle = indexBuffer->buffer.getHandle(),
          .offset = indexBuffer->offset,
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
  cached->addressRange = range;
  cached->buffer = std::move(convertedIndexBuffer);
  cached->offset = 0;
  cached->tagId = indexBuffer.tagId;
  cached->primType = primType;
  cached->indexType = indexType;

  auto handle = cached->buffer.getHandle();

  mParent->trackUpdate(EntryType::IndexBuffer, cached->addressRange, cached,
                       getReadId(), true);
  mStorage->mAcquiredViewResources.push_back(std::move(cached));

  return {
      .handle = handle,
      .offset = 0,
      .indexCount = indexCount,
      .primType = primType,
      .indexType = indexType,
  };
}

static bool isImageCompatible(CachedImage *cached, const ImageKey &key) {
  // FIXME: relax it
  return cached->image.getFormat() == gnm::toVkFormat(key.dfmt, key.nfmt) &&
         cached->image.getWidth() == key.extent.width &&
         cached->image.getHeight() == key.extent.height &&
         cached->image.getDepth() == key.extent.depth &&
         cached->imageBufferKey.pitch == key.pitch &&
         cached->imageBufferKey.tileMode.raw == key.tileMode.raw &&
         cached->kind == key.kind;
}

static bool isImageBufferCompatible(CachedImageBuffer *cached,
                                    const ImageBufferKey &key) {
  // FIXME: relax it
  return cached->dfmt == key.dfmt && cached->width == key.extent.width &&
         cached->height == key.extent.height &&
         cached->depth == key.extent.depth && cached->pitch == key.pitch &&
         cached->tileMode.raw == key.tileMode.raw;
}

Cache::ImageBuffer Cache::Tag::getImageBuffer(const ImageBufferKey &key,
                                              Access access) {
  auto surfaceInfo = computeSurfaceInfo(
      key.tileMode, key.type, key.dfmt, key.extent.width, key.extent.height,
      key.extent.depth, key.pitch, key.baseArrayLayer, key.arrayLayerCount,
      key.baseMipLevel, key.mipCount, key.pow2pad);

  auto range =
      rx::AddressRange::fromBeginSize(key.address, surfaceInfo.totalTiledSize);

  auto &table = mParent->getTable(EntryType::ImageBuffer);

  std::vector<std::shared_ptr<CachedImageBuffer>> flushed;
  for (auto it = table.lowerBound(range.beginAddress()); it != table.end();
       ++it) {
    if (!range.intersects(it.range())) {
      break;
    }

    auto imgBuffer = std::static_pointer_cast<CachedImageBuffer>(it.get());

    if (range == it.range()) {
      if (isImageBufferCompatible(imgBuffer.get(), key)) {
        break;
      }

      if (imgBuffer->flush(*this, getScheduler(), imgBuffer->addressRange)) {
        flushed.push_back(std::move(imgBuffer));
      }

      it.get() = nullptr;
      break;
    }

    if (imgBuffer->flush(*this, getScheduler(), imgBuffer->addressRange)) {
      flushed.push_back(std::move(imgBuffer));
    }
  }

  if (!flushed.empty()) {
    getScheduler().submit();
    getScheduler().wait();
    flushed.clear();
  }

  auto it =
      table.map(range.beginAddress(), range.endAddress(), nullptr, false, true);

  if (it.get() == nullptr) {
    auto cached = std::make_shared<CachedImageBuffer>();
    cached->buffer = vk::Buffer::Allocate(
        vk::getDeviceLocalMemory(), surfaceInfo.totalLinearSize,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    cached->tiler = &getDevice()->tiler;
    cached->info = surfaceInfo;
    cached->addressRange = range;
    cached->tileMode = key.tileMode;
    cached->dfmt = key.dfmt;
    cached->pitch = key.pitch;
    cached->arrayLayers = key.arrayLayerCount;
    cached->mipLevels = key.mipCount;
    cached->width = key.extent.width;
    cached->height = key.extent.height;
    cached->depth = key.extent.depth;

    it.get() = std::move(cached);
  }

  mStorage->mAcquiredImageBufferResources.push_back(it.get());

  auto cached = std::static_pointer_cast<CachedImageBuffer>(it.get());
  cached->acquire(this, access);

  if ((access & Access::Read) != Access::None) {
    if (!cached->expensive() ||
        testHostInvalidations(getDevice(), mParent->mVmId, range.beginAddress(),
                              range.size()) ||
        !mParent->isInSync(cached->addressRange, cached->tagId)) {

      auto tiledBuffer = getBuffer(range, Access::Read);
      if (tiledBuffer.tagId != cached->tagId) {
        mParent->trackUpdate(
            EntryType::ImageBuffer, range, it.get(), tiledBuffer.tagId,
            (access & Access::Write) == Access::None && cached->expensive());

        cached->update(this, cached->addressRange, tiledBuffer);
      }
    }
  }

  std::uint64_t offset =
      cached->addressRange.beginAddress() - range.beginAddress();

  Cache::ImageBuffer result{
      .handle = cached->buffer.getHandle(),
      .offset = offset,
      .deviceAddress = cached->buffer.getAddress() + offset,
      .tagId = cached->tagId,
  };

  return result;
}

Cache::Image Cache::Tag::getImage(const ImageKey &key, Access access) {
  auto surfaceInfo = computeSurfaceInfo(
      key.tileMode, key.type, key.dfmt, key.extent.width, key.extent.height,
      key.extent.depth, key.pitch, key.baseArrayLayer, key.arrayLayerCount,
      key.baseMipLevel, key.mipCount, key.pow2pad);
  auto storeRange = rx::AddressRange::fromBeginSize(key.writeAddress,
                                                    surfaceInfo.totalTiledSize);
  auto updateRange = rx::AddressRange::fromBeginSize(
      key.readAddress, surfaceInfo.totalTiledSize);

  if ((access & Access::Write) != Access::Write) {
    storeRange = updateRange;
  }

  auto &table = mParent->getTable(EntryType::Image);

  std::vector<std::shared_ptr<CachedImage>> flushed;

  for (auto it = table.lowerBound(storeRange.beginAddress()); it != table.end();
       ++it) {
    if (!storeRange.intersects(it.range())) {
      break;
    }

    auto img = std::static_pointer_cast<CachedImage>(it.get());

    if (storeRange == it.range()) {
      if (isImageCompatible(img.get(), key)) {
        break;
      }

      if (img->flush(*this, getScheduler(), img->addressRange)) {
        flushed.push_back(std::move(img));
      }

      it.get() = nullptr;
      break;
    }

    if (img->flush(*this, getScheduler(), img->addressRange)) {
      flushed.push_back(std::move(img));
    }
  }

  if (!flushed.empty()) {
    getScheduler().submit();
    getScheduler().wait();
    flushed.clear();
  }

  auto it = table.map(storeRange.beginAddress(), storeRange.endAddress(),
                      nullptr, false, true);

  if (it.get() == nullptr) {
    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VkFormat format;
    if (key.kind == ImageKind::Color) {
      usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
      bool isCompressed =
          key.dfmt == gnm::kDataFormatBc1 || key.dfmt == gnm::kDataFormatBc2 ||
          key.dfmt == gnm::kDataFormatBc3 || key.dfmt == gnm::kDataFormatBc4 ||
          key.dfmt == gnm::kDataFormatBc5 || key.dfmt == gnm::kDataFormatBc6 ||
          key.dfmt == gnm::kDataFormatBc7 ||
          key.dfmt == gnm::kDataFormatGB_GR ||
          key.dfmt == gnm::kDataFormatBG_RG ||
          key.dfmt == gnm::kDataFormat5_6_5;

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
          rx::die("unexpected stencil format %u, %u",
                  static_cast<int>(key.dfmt), static_cast<int>(key.nfmt));
        }
      } else {
        rx::die("image kind %u %u, %u", static_cast<int>(key.kind),
                static_cast<int>(key.dfmt), static_cast<int>(key.nfmt));
      }

      usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    auto image = vk::Image::Allocate(vk::getDeviceLocalMemory(),
                                     gnm::toVkImageType(key.type), key.extent,
                                     key.mipCount, key.arrayLayerCount, format,
                                     VK_SAMPLE_COUNT_1_BIT, usage);

    auto cached = std::make_shared<CachedImage>();
    cached->image = std::move(image);
    cached->info = surfaceInfo;
    cached->addressRange = storeRange;
    cached->kind = key.kind;
    cached->imageBufferKey = ImageBufferKey::createFrom(key);

    transitionImageLayout(mScheduler->getCommandBuffer(), cached->image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                          cached->getSubresource(storeRange));
    it.get() = std::move(cached);
  }

  mStorage->mAcquiredImageResources.push_back(it.get());

  auto cached = std::static_pointer_cast<CachedImage>(it.get());
  cached->acquire(this, access);

  if ((access & Access::Read) != Access::None) {
    if (!cached->expensive() ||
        testHostInvalidations(getDevice(), mParent->mVmId,
                              updateRange.beginAddress(), updateRange.size()) ||
        !mParent->isInSync(cached->addressRange, cached->tagId)) {

      auto imageBufferKey = cached->imageBufferKey;
      imageBufferKey.address = key.readAddress;
      auto imageBuffer = getImageBuffer(imageBufferKey, Access::Read);
      if (imageBuffer.tagId != cached->tagId) {
        mParent->trackUpdate(
            EntryType::Image, storeRange, it.get(), imageBuffer.tagId,
            (access & Access::Write) == Access::None && cached->expensive());

        cached->update(this, cached->addressRange, imageBuffer);
      }
    }
  }

  auto entry = cached.get();
  auto handle = cached->image.getHandle();

  return {
      .handle = handle,
      .entry = entry,
      .format = entry->image.getFormat(),
      .subresource = entry->getSubresource(storeRange),
  };
}

Cache::ImageView Cache::Tag::getImageView(const ImageViewKey &key,
                                          Access access) {
  auto surfaceInfo = computeSurfaceInfo(
      key.tileMode, key.type, key.dfmt, key.extent.width, key.extent.height,
      key.extent.depth, key.pitch, key.baseArrayLayer, key.arrayLayerCount,
      key.baseMipLevel, key.mipCount, key.pow2pad);

  auto storeRange = rx::AddressRange::fromBeginSize(key.writeAddress,
                                                    surfaceInfo.totalTiledSize);
  auto image = getImage(ImageKey::createFrom(key), access);
  auto result = vk::ImageView(gnm::toVkImageViewType(key.type), image.handle,
                              image.format,
                              {
                                  .r = gnm::toVkComponentSwizzle(key.r),
                                  .g = gnm::toVkComponentSwizzle(key.g),
                                  .b = gnm::toVkComponentSwizzle(key.b),
                                  .a = gnm::toVkComponentSwizzle(key.a),
                              },
                              {
                                  .aspectMask = toAspect(key.kind),
                                  .baseMipLevel = key.baseMipLevel,
                                  .levelCount = key.mipCount,
                                  .baseArrayLayer = key.baseArrayLayer,
                                  .layerCount = key.arrayLayerCount,
                              });
  auto cached = std::make_shared<CachedImageView>();
  cached->addressRange = storeRange;
  cached->view = std::move(result);

  auto handle = cached->view.getHandle();
  mStorage->mAcquiredViewResources.push_back(std::move(cached));

  return {
      .handle = handle,
      .imageHandle = image.handle,
      .subresource = image.subresource,
  };
}

void Cache::Tag::readMemory(void *target, rx::AddressRange range) {
  mParent->flush(*this, range);
  auto memoryPtr =
      RemoteMemory{mParent->mVmId}.getPointer(range.beginAddress());
  std::memcpy(target, memoryPtr, range.size());
}

void Cache::Tag::writeMemory(const void *source, rx::AddressRange range) {
  mParent->flush(*this, range);
  auto memoryPtr =
      RemoteMemory{mParent->mVmId}.getPointer(range.beginAddress());
  std::memcpy(memoryPtr, source, range.size());
}

int Cache::Tag::compareMemory(const void *source, rx::AddressRange range) {
  mParent->flush(*this, range);
  auto memoryPtr =
      RemoteMemory{mParent->mVmId}.getPointer(range.beginAddress());
  return std::memcmp(memoryPtr, source, range.size());
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
  if (mStorage == nullptr) {
    return;
  }

  unlock();

  if (mAcquiredMemoryTable + 1 != 0) {
    getCache()->mMemoryTablePool.release(mAcquiredMemoryTable);
    mAcquiredMemoryTable = -1;
  }

  if (mAcquiredImageMemoryTable + 1 != 0) {
    getCache()->mMemoryTablePool.release(mAcquiredImageMemoryTable);
    mAcquiredImageMemoryTable = -1;
  }

  std::vector<std::shared_ptr<Entry>> tmpResources;
  bool hasSubmits = false;

  while (!mStorage->mAcquiredImageResources.empty()) {
    auto resource = std::move(mStorage->mAcquiredImageResources.back());
    mStorage->mAcquiredImageResources.pop_back();
    if (resource->release(this)) {
      hasSubmits = true;
    }

    tmpResources.push_back(std::move(resource));
  }

  if (hasSubmits) {
    hasSubmits = false;
    mScheduler->submit();
    mScheduler->wait();
  }

  while (!mStorage->mAcquiredImageBufferResources.empty()) {
    auto resource = std::move(mStorage->mAcquiredImageBufferResources.back());
    mStorage->mAcquiredImageBufferResources.pop_back();
    if (resource->release(this)) {
      hasSubmits = true;
    }

    tmpResources.push_back(std::move(resource));
  }

  if (hasSubmits) {
    hasSubmits = false;
    mScheduler->submit();
    mScheduler->wait();
  }

  while (!mStorage->mAcquiredMemoryResources.empty()) {
    auto resource = std::move(mStorage->mAcquiredMemoryResources.back());
    mStorage->mAcquiredMemoryResources.pop_back();
    resource->release(this);
    tmpResources.push_back(std::move(resource));
  }

  mStorage->clear();
  auto storageIndex = mStorage - mParent->mTagStorages;
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

  return getShader(gcn::Stage::Ps, pgm, context, 0, {}, viewPorts,
                   {psVgprInput, psVgprInputs});
}

Cache::Shader Cache::GraphicsTag::getVertexShader(
    gcn::Stage stage, const SpiShaderPgm &pgm,
    const Registers::Context &context, std::uint32_t indexOffset,
    gnm::PrimitiveType vsPrimType, std::span<const VkViewport> viewPorts) {
  return getShader(stage, pgm, context, indexOffset, vsPrimType, viewPorts, {});
}

Cache::Shader Cache::GraphicsTag::getShader(
    gcn::Stage stage, const SpiShaderPgm &pgm,
    const Registers::Context &context, std::uint32_t indexOffset,
    gnm::PrimitiveType vsPrimType, std::span<const VkViewport> viewPorts,
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
  std::uint64_t imageMemoryTableAddress = getImageMemoryTable().deviceAddress;

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
      readMemory(&configPtr[index], rx::AddressRange::fromBeginSize(
                                        slot.data, sizeof(std::uint32_t)));
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

    case gcn::ConfigType::VsIndexOffset:
      configPtr[index] = static_cast<std::uint32_t>(indexOffset);
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
    case gcn::ConfigType::ImageMemoryTable:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(imageMemoryTableAddress);
      } else {
        configPtr[index] =
            static_cast<std::uint32_t>(imageMemoryTableAddress >> 32);
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
  std::uint64_t imageMemoryTableAddress = getImageMemoryTable().deviceAddress;

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
      readMemory(&configPtr[index], rx::AddressRange::fromBeginSize(
                                        slot.data, sizeof(std::uint32_t)));
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
    case gcn::ConfigType::ImageMemoryTable:
      if (slot.data == 0) {
        configPtr[index] = static_cast<std::uint32_t>(imageMemoryTableAddress);
      } else {
        configPtr[index] =
            static_cast<std::uint32_t>(imageMemoryTableAddress >> 32);
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

Cache::Cache(Device *device, int vmId) : mDevice(device), mVmId(vmId) {
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
            .descriptorCount = 4 * kDescriptorSetCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
            .descriptorCount = 3 * 32 * kDescriptorSetCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_SAMPLER,
            .descriptorCount = 32 * kDescriptorSetCount,
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 32 * kDescriptorSetCount,
        },
    };

    VkDescriptorPoolCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets =
            static_cast<uint32_t>(std::size(mGraphicsDescriptorSets) *
                                      mGraphicsDescriptorSetLayouts.size() +
                                  std::size(mComputeDescriptorSets)) *
            2,
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
      VK_VERIFY(vkAllocateDescriptorSets(vk::context->device, &info,
                                         graphicsSet.data()));
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
      VK_VERIFY(
          vkAllocateDescriptorSets(vk::context->device, &info, &computeSet));
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

void Cache::invalidate(Tag &tag, rx::AddressRange range) {
  flush(tag, range);
  markHostInvalidated(mDevice, mVmId, range.beginAddress(), range.size());
}
void Cache::flush(Tag &tag, rx::AddressRange range) {
  auto flushedRange = flushImages(tag, range);
  flushedRange = flushedRange.merge(flushImageBuffers(tag, range));

  if (flushedRange) {
    tag.getScheduler().submit();
    tag.getScheduler().wait();
  }

  flushBuffers(range);
}

void Cache::trackUpdate(EntryType type, rx::AddressRange range,
                        std::shared_ptr<Entry> entry, TagId tagId,
                        bool watchChanges) {
  if (auto it = mSyncTable.map(range.beginAddress(), range.endAddress(), {},
                               false, true);
      it.get() < tagId) {
    it.get() = tagId;
  }

  entry->tagId = tagId;
  auto &table = getTable(type);
  table.map(range.beginAddress(), range.endAddress(), std::move(entry));

  if (watchChanges) {
    mDevice->watchWrites(mVmId, range.beginAddress(), range.size());
  }
}

void Cache::trackWrite(rx::AddressRange range, TagId tagId, bool lockMemory) {
  if (auto it = mSyncTable.map(range.beginAddress(), range.endAddress(), {},
                               false, true);
      it.get() < tagId) {
    it.get() = tagId;
  }

  if (!lockMemory) {
    return;
  }

  mDevice->lockReadWrite(mVmId, range.beginAddress(), range.size(), true);
}

rx::AddressRange Cache::flushImages(Tag &tag, rx::AddressRange range) {
  auto &table = getTable(EntryType::Image);
  rx::AddressRange result;
  auto beginIt = table.lowerBound(range.beginAddress());

  while (beginIt != table.end()) {
    auto cached = beginIt->get();
    if (!cached->addressRange.intersects(range)) {
      break;
    }

    if (static_cast<CachedImage *>(cached)->flush(tag, tag.getScheduler(),
                                                  range)) {
      result = result.merge(cached->addressRange);
    }
    ++beginIt;
  }

  return result;
}

rx::AddressRange Cache::flushImageBuffers(Tag &tag, rx::AddressRange range) {
  auto &table = getTable(EntryType::ImageBuffer);
  rx::AddressRange result;
  auto beginIt = table.lowerBound(range.beginAddress());

  while (beginIt != table.end()) {
    auto cached = beginIt->get();
    if (!cached->addressRange.intersects(range)) {
      break;
    }

    if (static_cast<CachedImageBuffer *>(cached)->flush(tag, tag.getScheduler(),
                                                        range)) {
      result = result.merge(cached->addressRange);
    }

    ++beginIt;
  }

  return result;
}

rx::AddressRange Cache::flushBuffers(rx::AddressRange range) {
  auto &table = getTable(EntryType::HostVisibleBuffer);
  auto beginIt = table.lowerBound(range.beginAddress());

  rx::AddressRange result;
  while (beginIt != table.end()) {
    auto cached = beginIt->get();
    if (!cached->addressRange.intersects(range)) {
      break;
    }

    auto address =
        RemoteMemory{mVmId}.getPointer(cached->addressRange.beginAddress());
    if (static_cast<CachedHostVisibleBuffer *>(cached)->flush(
            address, cached->addressRange)) {
      result = result.merge(cached->addressRange);
    }

    ++beginIt;
  }

  return result;
}

std::shared_ptr<Cache::Entry> Cache::getInSyncEntry(EntryType type,
                                                    rx::AddressRange range) {
  auto &table = getTable(type);
  auto it = table.queryArea(range.beginAddress());
  if (it == table.end() || !it.range().contains(range)) {
    return {};
  }

  auto syncIt = mSyncTable.queryArea(range.beginAddress());

  if (syncIt.endAddress() < range.endAddress()) {
    return {};
  }

  if (syncIt.get() != it.get()->tagId) {
    return {};
  }

  return it.get();
}
