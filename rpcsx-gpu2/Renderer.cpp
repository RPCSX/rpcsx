#include "Renderer.hpp"
#include "Device.hpp"
#include "gnm/descriptors.hpp"
#include "gnm/gnm.hpp"
#include "rx/MemoryTable.hpp"

#include <amdgpu/tiler.hpp>
#include <gnm/constants.hpp>
#include <gnm/vulkan.hpp>
#include <print>
#include <shader/Evaluator.hpp>
#include <shader/dialect.hpp>
#include <shader/gcn.hpp>
#include <shaders/fill_red.frag.h>
#include <shaders/flip.frag.h>
#include <shaders/flip.vert.h>
#include <shaders/rect_list.geom.h>

#include <bit>
#include <vulkan/vulkan_core.h>

using namespace shader;

namespace gnm {
VkRect2D toVkRect2D(amdgpu::PaScRect rect) {
  return {
      .offset =
          {
              .x = rect.left,
              .y = rect.top,
          },
      .extent =
          {
              .width = static_cast<uint32_t>(rect.right - rect.left),
              .height = static_cast<uint32_t>(rect.bottom - rect.top),
          },
  };
}

amdgpu::PaScRect intersection(amdgpu::PaScRect rect, amdgpu::PaScRect scissor) {
  amdgpu::PaScRect result{
      .left = std::max(rect.left, scissor.left),
      .top = std::max(rect.top, scissor.top),
      .right = std::min(rect.right, scissor.right),
      .bottom = std::min(rect.bottom, scissor.bottom),
  };

  result.top = std::min(result.top, result.bottom);
  result.bottom = std::max(result.top, result.bottom);
  result.left = std::min(result.left, result.right);
  result.right = std::max(result.left, result.right);
  return result;
}
} // namespace gnm

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

static VkShaderEXT getPrimTypeRectGeomShader(amdgpu::Cache &cache) {
  static VkShaderEXT shader = VK_NULL_HANDLE;
  if (shader != VK_NULL_HANDLE) {
    return shader;
  }

  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .stage = VK_SHADER_STAGE_GEOMETRY_BIT,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = sizeof(spirv_rect_list_geom),
      .pCode = spirv_rect_list_geom,
      .pName = "main",
      .setLayoutCount =
          static_cast<uint32_t>(cache.getGraphicsDescriptorSetLayouts().size()),
      .pSetLayouts = cache.getGraphicsDescriptorSetLayouts().data()};

  VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &createInfo,
                                 vk::context->allocator, &shader));
  return shader;
}

static VkShaderEXT getFillRedFragShader(amdgpu::Cache &cache) {
  static VkShaderEXT shader = VK_NULL_HANDLE;
  if (shader != VK_NULL_HANDLE) {
    return shader;
  }

  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = sizeof(spirv_fill_red_frag),
      .pCode = spirv_fill_red_frag,
      .pName = "main",
      .setLayoutCount =
          static_cast<uint32_t>(cache.getGraphicsDescriptorSetLayouts().size()),
      .pSetLayouts = cache.getGraphicsDescriptorSetLayouts().data()};

  VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &createInfo,
                                 vk::context->allocator, &shader));
  return shader;
}

static VkShaderEXT getFlipVertexShader(amdgpu::Cache &cache) {
  static VkShaderEXT shader = VK_NULL_HANDLE;
  if (shader != VK_NULL_HANDLE) {
    return shader;
  }

  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = sizeof(spirv_flip_vert),
      .pCode = spirv_flip_vert,
      .pName = "main",
      .setLayoutCount =
          static_cast<uint32_t>(cache.getGraphicsDescriptorSetLayouts().size()),
      .pSetLayouts = cache.getGraphicsDescriptorSetLayouts().data()};

  VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &createInfo,
                                 vk::context->allocator, &shader));
  return shader;
}

static VkShaderEXT getFlipFragmentShader(amdgpu::Cache &cache) {
  static VkShaderEXT shader = VK_NULL_HANDLE;
  if (shader != VK_NULL_HANDLE) {
    return shader;
  }

  VkShaderCreateInfoEXT createInfo{
      .sType = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .codeType = VK_SHADER_CODE_TYPE_SPIRV_EXT,
      .codeSize = sizeof(spirv_flip_frag),
      .pCode = spirv_flip_frag,
      .pName = "main",
      .setLayoutCount =
          static_cast<uint32_t>(cache.getGraphicsDescriptorSetLayouts().size()),
      .pSetLayouts = cache.getGraphicsDescriptorSetLayouts().data()};

  VK_VERIFY(vk::CreateShadersEXT(vk::context->device, 1, &createInfo,
                                 vk::context->allocator, &shader));
  return shader;
}

static VkPrimitiveTopology toVkPrimitiveType(gnm::PrimitiveType type) {
  switch (type) {
  case gnm::PrimitiveType::PointList:
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
  case gnm::PrimitiveType::LineList:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  case gnm::PrimitiveType::LineStrip:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
  case gnm::PrimitiveType::TriList:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  case gnm::PrimitiveType::TriFan:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
  case gnm::PrimitiveType::TriStrip:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
  case gnm::PrimitiveType::Patch:
    return VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
  case gnm::PrimitiveType::LineListAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY;
  case gnm::PrimitiveType::LineStripAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY;
  case gnm::PrimitiveType::TriListAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
  case gnm::PrimitiveType::TriStripAdjacency:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY;
  case gnm::PrimitiveType::LineLoop:
    return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP; // FIXME

  case gnm::PrimitiveType::RectList:
  case gnm::PrimitiveType::QuadList:
  case gnm::PrimitiveType::QuadStrip:
  case gnm::PrimitiveType::Polygon:
    return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  default:
    rx::die("toVkPrimitiveType: unexpected primitive type %u",
            static_cast<unsigned>(type));
  }
}

struct ShaderResources : eval::Evaluator {
  amdgpu::Cache::Tag *cacheTag;
  shader::eval::Evaluator evaluator;
  std::map<std::uint32_t, std::uint32_t> slotResources;
  std::span<const std::uint32_t> userSgprs;

  std::uint32_t slotOffset = 0;
  rx::MemoryTableWithPayload<Access> bufferMemoryTable;
  std::vector<std::pair<std::uint32_t, std::uint64_t>> resourceSlotToAddress;
  std::vector<amdgpu::Cache::Sampler> samplerResources;
  std::vector<amdgpu::Cache::ImageView> imageResources[3];

  using Evaluator::eval;

  ShaderResources() = default;

  void loadResources(shader::gcn::Resources &res,
                     std::span<const std::uint32_t> userSgprs) {
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
          amdgpu::ImageViewKey::createFrom(buffer), texture.access));
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

      slotResources[slotOffset + sampler.resourceSlot] =
          samplerResources.size();
      samplerResources.push_back(
          cacheTag->getSampler(amdgpu::SamplerKey::createFrom(sSampler)));
    }

    slotOffset += res.slots;
  }

  void buildMemoryTable(MemoryTable &memoryTable) {
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

  std::uint32_t getResourceSlot(std::uint32_t id) {
    if (auto it = slotResources.find(id); it != slotResources.end()) {
      return it->second;
    }
    return -1;
  }

  template <typename T> T readPointer(std::uint64_t address) {
    T result{};
    cacheTag->readMemory(&result, address, sizeof(result));
    return result;
  }

  eval::Value eval(ir::InstructionId instId,
                   std::span<const ir::Operand> operands) override {
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
};

void amdgpu::draw(GraphicsPipe &pipe, int vmId, std::uint32_t firstVertex,
                  std::uint32_t vertexCount, std::uint32_t firstInstance,
                  std::uint32_t instanceCount, std::uint64_t indiciesAddress,
                  std::uint32_t indexCount) {
  if (pipe.uConfig.vgtPrimitiveType == gnm::PrimitiveType::None) {
    return;
  }

  if (pipe.context.cbColorControl.mode == gnm::CbMode::Disable) {
    return;
  }

  if (pipe.context.cbColorControl.mode != gnm::CbMode::Normal) {
    std::println("unimplemented context.cbColorControl.mode = {}",
                 static_cast<int>(pipe.context.cbColorControl.mode));
    return;
  }

  if (pipe.context.cbTargetMask.raw == 0) {
    return;
  }

  auto cacheTag = pipe.device->getCacheTag(vmId, pipe.scheduler);
  auto targetMask = pipe.context.cbTargetMask.raw;

  VkRenderingAttachmentInfo colorAttachments[8]{};
  VkBool32 colorBlendEnable[8]{};
  VkColorBlendEquationEXT colorBlendEquation[8]{};
  VkColorComponentFlags colorWriteMask[8]{};
  VkViewport viewPorts[8]{};
  VkRect2D viewPortScissors[8]{};
  unsigned renderTargets = 0;

  VkRenderingAttachmentInfo depthAttachment{};
  VkRenderingAttachmentInfo stencilAttachment{};

  auto depthAccess = Access::None;
  auto stencilAccess = Access::None;

  if (pipe.context.dbDepthControl.depthEnable) {
    if (!pipe.context.dbRenderControl.depthClearEnable) {
      depthAccess |= Access::Read;
    }
    if (!pipe.context.dbDepthView.zReadOnly) {
      depthAccess |= Access::Write;
    }
  }

  if (pipe.context.dbDepthControl.stencilEnable) {
    if (!pipe.context.dbRenderControl.stencilClearEnable) {
      stencilAccess |= Access::Read;
    }
    if (!pipe.context.dbDepthView.stencilReadOnly) {
      stencilAccess |= Access::Write;
    }
  }

  if (depthAccess != Access::None) {
    auto viewPortScissor = pipe.context.paScScreenScissor;
    auto viewPortRect = gnm::toVkRect2D(viewPortScissor);

    auto imageView = cacheTag.getImageView(
        {{
            .readAddress = pipe.context.dbZReadBase,
            .writeAddress = pipe.context.dbZWriteBase,
            .dfmt = gnm::getDataFormat(pipe.context.dbZInfo.format),
            .nfmt = gnm::getNumericFormat(pipe.context.dbZInfo.format),
            .extent =
                {
                    .width = viewPortRect.extent.width,
                    .height = viewPortRect.extent.height,
                    .depth = 1,
                },
            .pitch = viewPortRect.extent.width,
            .kind = ImageKind::Depth,
        }},
        depthAccess);

    depthAttachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = imageView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    if ((depthAccess & Access::Read) == Access::None) {
      depthAttachment.clearValue.depthStencil.depth = pipe.context.dbDepthClear;
      depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    }

    if ((depthAccess & Access::Write) == Access::None) {
      depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_NONE;
    }
  }

  for (auto &cbColor : pipe.context.cbColor) {
    if (targetMask == 0) {
      break;
    }

    if (cbColor.info.dfmt == gnm::kDataFormatInvalid) {
      continue;
    }

    auto viewPortScissor = pipe.context.paScScreenScissor;
    // viewPortScissor = gnm::intersection(
    //     viewPortScissor, pipe.context.paScVportScissor[renderTargets]);
    // viewPortScissor =
    //     gnm::intersection(viewPortScissor, pipe.context.paScWindowScissor);
    // viewPortScissor =
    //     gnm::intersection(viewPortScissor, pipe.context.paScGenericScissor);

    auto viewPortRect = gnm::toVkRect2D(viewPortScissor);

    viewPorts[renderTargets].x = viewPortRect.offset.x;
    viewPorts[renderTargets].y = viewPortRect.offset.y;
    viewPorts[renderTargets].width = viewPortRect.extent.width;
    viewPorts[renderTargets].height = viewPortRect.extent.height;
    viewPorts[renderTargets].minDepth =
        pipe.context.paScVportZ[renderTargets].min;
    viewPorts[renderTargets].maxDepth =
        pipe.context.paScVportZ[renderTargets].max;

    auto vkViewPortScissor = gnm::toVkRect2D(viewPortScissor);
    viewPortScissors[renderTargets] = vkViewPortScissor;

    ImageViewKey renderTargetInfo{};
    renderTargetInfo.type = gnm::TextureType::Dim2D;
    renderTargetInfo.pitch = vkViewPortScissor.extent.width;
    renderTargetInfo.readAddress = static_cast<std::uint64_t>(cbColor.base)
                                   << 8;
    renderTargetInfo.writeAddress = renderTargetInfo.readAddress;
    renderTargetInfo.extent.width = vkViewPortScissor.extent.width;
    renderTargetInfo.extent.height = vkViewPortScissor.extent.height;
    renderTargetInfo.extent.depth = 1;
    renderTargetInfo.dfmt = cbColor.info.dfmt;
    renderTargetInfo.nfmt = cbColor.info.nfmt;
    renderTargetInfo.mipCount = 1;
    renderTargetInfo.arrayLayerCount = 1;

    renderTargetInfo.tileMode =
        cbColor.info.linearGeneral
            ? TileMode{.raw = 0}
            : getDefaultTileModes()[cbColor.attrib.tileModeIndex];
    // std::printf("draw to %lx\n", renderTargetInfo.address);

    auto access = Access::None;

    if (!cbColor.info.fastClear) {
      access |= Access::Read;
    }
    if (targetMask & 0xf) {
      access |= Access::Write;
    }

    auto imageView = cacheTag.getImageView(renderTargetInfo, access);

    colorAttachments[renderTargets] = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = imageView.handle,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        .loadOp = cbColor.info.fastClear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                         : VK_ATTACHMENT_LOAD_OP_LOAD,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,

        .clearValue =
            {
                .color =
                    {
                        .uint32 =
                            {
                                cbColor.clearWord0,
                                cbColor.clearWord1,
                                cbColor.clearWord2,
                            },
                    },
            },
    };

    auto &blendControl = pipe.context.cbBlendControl[renderTargets];

    colorBlendEnable[renderTargets] = blendControl.enable;
    colorBlendEquation[renderTargets] = VkColorBlendEquationEXT{
        .srcColorBlendFactor = gnm::toVkBlendFactor(blendControl.colorSrcBlend),
        .dstColorBlendFactor = gnm::toVkBlendFactor(blendControl.colorDstBlend),
        .colorBlendOp = gnm::toVkBlendOp(blendControl.colorCombFcn),
        .srcAlphaBlendFactor =
            blendControl.separateAlphaBlend
                ? gnm::toVkBlendFactor(blendControl.alphaSrcBlend)
                : gnm::toVkBlendFactor(blendControl.colorSrcBlend),
        .dstAlphaBlendFactor =
            blendControl.separateAlphaBlend
                ? gnm::toVkBlendFactor(blendControl.alphaDstBlend)
                : gnm::toVkBlendFactor(blendControl.colorDstBlend),
        .alphaBlendOp = blendControl.separateAlphaBlend
                            ? gnm::toVkBlendOp(blendControl.alphaCombFcn)
                            : gnm::toVkBlendOp(blendControl.colorCombFcn),
    };

    colorWriteMask[renderTargets] =
        ((targetMask & 1) ? VK_COLOR_COMPONENT_R_BIT : 0) |
        ((targetMask & 2) ? VK_COLOR_COMPONENT_G_BIT : 0) |
        ((targetMask & 4) ? VK_COLOR_COMPONENT_B_BIT : 0) |
        ((targetMask & 8) ? VK_COLOR_COMPONENT_A_BIT : 0);

    renderTargets++;
    targetMask >>= 4;
  }

  if (renderTargets == 0) {
    return;
  }

  //   if (pipe.context.cbTargetMask == 0) {
  //     return;
  //   }

  //   auto cache = pipe.device->getCache(vmId);

  if (indiciesAddress == 0) {
    indexCount = vertexCount;
  }

  auto indexBuffer = cacheTag.getIndexBuffer(indiciesAddress, indexCount,
                                             pipe.uConfig.vgtPrimitiveType,
                                             pipe.uConfig.vgtIndexType);

  auto stages = Cache::kGraphicsStages;
  VkShaderEXT shaders[stages.size()]{};

  auto pipelineLayout = cacheTag.getGraphicsPipelineLayout();
  auto descriptorSets = cacheTag.createGraphicsDescriptorSets();

  std::vector<std::uint32_t *> descriptorBuffers;
  auto &memoryTableBuffer = cacheTag.getCache()->getMemoryTableBuffer();
  std::uint64_t memoryTableAddress = memoryTableBuffer.getAddress();
  auto memoryTable = std::bit_cast<MemoryTable *>(memoryTableBuffer.getData());

  std::uint64_t gdsAddress = cacheTag.getCache()->getGdsBuffer().getAddress();
  ShaderResources shaderResources;
  shaderResources.cacheTag = &cacheTag;

  struct MemoryTableConfigSlot {
    std::uint32_t bufferIndex;
    std::uint32_t configIndex;
    std::uint32_t resourceSlot;
  };
  std::vector<MemoryTableConfigSlot> memoryTableConfigSlots;

  auto addShader = [&](const SpiShaderPgm &pgm, shader::gcn::Stage stage) {
    shader::gcn::Environment env{
        .vgprCount = pgm.rsrc1.getVGprCount(),
        .sgprCount = pgm.rsrc1.getSGprCount(),
        .userSgprs = std::span(pgm.userData.data(), pgm.rsrc2.userSgpr),
        // .supportsBarycentric = vk::context->supportsBarycentric,
        .supportsInt8 = vk::context->supportsInt8,
        .supportsInt64Atomics = vk::context->supportsInt64Atomics,
    };

    auto shader = cacheTag.getShader({
        .address = pgm.address << 8,
        .stage = stage,
        .env = env,
    });

    std::uint32_t slotOffset = shaderResources.slotOffset;

    shaderResources.loadResources(
        shader.info->resources,
        std::span(pgm.userData.data(), pgm.rsrc2.userSgpr));

    const auto &configSlots = shader.info->configSlots;

    auto configSize = configSlots.size() * sizeof(std::uint32_t);
    auto configBuffer = cacheTag.getInternalBuffer(configSize);

    auto configPtr = reinterpret_cast<std::uint32_t *>(configBuffer.data);

    shader::gcn::PsVGprInput
        psVgprInput[static_cast<std::size_t>(shader::gcn::PsVGprInput::Count)];
    std::size_t psVgprInputs = 0;

    if (stage == shader::gcn::Stage::Ps) {
      SpiPsInput spiInputAddr = pipe.context.spiPsInputAddr;

      if (spiInputAddr.perspSampleEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::IPerspSample;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JPerspSample;
      }
      if (spiInputAddr.perspCenterEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::IPerspCenter;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JPerspCenter;
      }
      if (spiInputAddr.perspCentroidEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::IPerspCentroid;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JPerspCentroid;
      }
      if (spiInputAddr.perspPullModelEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::IW;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JW;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::_1W;
      }
      if (spiInputAddr.linearSampleEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::ILinearSample;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JLinearSample;
      }
      if (spiInputAddr.linearCenterEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::ILinearCenter;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JLinearCenter;
      }
      if (spiInputAddr.linearCentroidEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::ILinearCentroid;
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::JLinearCentroid;
      }
      if (spiInputAddr.posXFloatEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::X;
      }
      if (spiInputAddr.posYFloatEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::Y;
      }
      if (spiInputAddr.posZFloatEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::Z;
      }
      if (spiInputAddr.posWFloatEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::W;
      }
      if (spiInputAddr.frontFaceEna) {
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::FrontFace;
      }
      if (spiInputAddr.ancillaryEna) {
        rx::die("unimplemented ancillary fs input");
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::Ancillary;
      }
      if (spiInputAddr.sampleCoverageEna) {
        rx::die("unimplemented sample coverage fs input");
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::SampleCoverage;
      }
      if (spiInputAddr.posFixedPtEna) {
        rx::die("unimplemented pos fixed fs input");
        psVgprInput[psVgprInputs++] = shader::gcn::PsVGprInput::PosFixed;
      }
    }

    for (std::size_t index = 0; const auto &slot : configSlots) {
      switch (slot.type) {
      case shader::gcn::ConfigType::Imm:
        cacheTag.readMemory(&configPtr[index], slot.data,
                            sizeof(std::uint32_t));
        break;
      case shader::gcn::ConfigType::UserSgpr:
        configPtr[index] = pgm.userData[slot.data];
        break;
      case shader::gcn::ConfigType::ViewPortOffsetX:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].xOffset /
                (viewPorts[0].width / 2.f) -
            1);
        break;
      case shader::gcn::ConfigType::ViewPortOffsetY:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].yOffset /
                (viewPorts[slot.data].height / 2.f) -
            1);
        break;
      case shader::gcn::ConfigType::ViewPortOffsetZ:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].zOffset);
        break;
      case shader::gcn::ConfigType::ViewPortScaleX:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].xScale /
            (viewPorts[slot.data].width / 2.f));
        break;
      case shader::gcn::ConfigType::ViewPortScaleY:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].yScale /
            (viewPorts[slot.data].height / 2.f));
        break;
      case shader::gcn::ConfigType::ViewPortScaleZ:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.paClVports[slot.data].zScale);
        break;
      case shader::gcn::ConfigType::PsInputVGpr:
        if (slot.data > psVgprInputs) {
          configPtr[index] = ~0;
        } else {
          configPtr[index] =
              std::bit_cast<std::uint32_t>(psVgprInput[slot.data]);
        }
        break;
      case shader::gcn::ConfigType::VsPrimType:
        if (indexBuffer.handle == VK_NULL_HANDLE &&
            pipe.uConfig.vgtPrimitiveType != indexBuffer.primType) {
          configPtr[index] =
              static_cast<std::uint32_t>(pipe.uConfig.vgtPrimitiveType.value);
        } else {
          configPtr[index] = 0;
        }
        break;

      case shader::gcn::ConfigType::ResourceSlot:
        memoryTableConfigSlots.push_back({
            .bufferIndex = static_cast<std::uint32_t>(descriptorBuffers.size()),
            .configIndex = static_cast<std::uint32_t>(index),
            .resourceSlot = static_cast<std::uint32_t>(slotOffset + slot.data),
        });
        break;

      case shader::gcn::ConfigType::MemoryTable:
        if (slot.data == 0) {
          configPtr[index] = static_cast<std::uint32_t>(memoryTableAddress);
        } else {
          configPtr[index] =
              static_cast<std::uint32_t>(memoryTableAddress >> 32);
        }
        break;
      case shader::gcn::ConfigType::Gds:
        if (slot.data == 0) {
          configPtr[index] = static_cast<std::uint32_t>(gdsAddress);
        } else {
          configPtr[index] = static_cast<std::uint32_t>(gdsAddress >> 32);
        }
        break;

      case shader::gcn::ConfigType::CbCompSwap:
        configPtr[index] = std::bit_cast<std::uint32_t>(
            pipe.context.cbColor[slot.data].info.compSwap);
        break;
      }

      ++index;
    }

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
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pBufferInfo = &bufferInfo,
    };

    vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);

    shaders[stageIndex] = shader.handle
                              ? shader.handle
                              : getFillRedFragShader(*cacheTag.getCache());
    descriptorBuffers.push_back(configPtr);
  };

  if (pipe.context.vgtShaderStagesEn.vsEn == amdgpu::VsStage::VsReal) {
    addShader(pipe.sh.spiShaderPgmVs, shader::gcn::Stage::VsVs);
  }

  if (true) {
    addShader(pipe.sh.spiShaderPgmPs, shader::gcn::Stage::Ps);
  } else {
    shaders[Cache::getStageIndex(VK_SHADER_STAGE_FRAGMENT_BIT)] =
        getFillRedFragShader(*cacheTag.getCache());
  }

  if (pipe.uConfig.vgtPrimitiveType == gnm::PrimitiveType::RectList) {
    shaders[Cache::getStageIndex(VK_SHADER_STAGE_GEOMETRY_BIT)] =
        getPrimTypeRectGeomShader(*cacheTag.getCache());
  }

  if (indiciesAddress == 0) {
    vertexCount = indexBuffer.indexCount;
  }

  auto commandBuffer = pipe.scheduler.getCommandBuffer();

  VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = gnm::toVkRect2D(pipe.context.paScScreenScissor),
      .layerCount = 1,
      .colorAttachmentCount = renderTargets,
      .pColorAttachments = colorAttachments,
      .pDepthAttachment = &depthAttachment,
      // .pStencilAttachment = &stencilAttachment,
  };

  vkCmdBeginRendering(commandBuffer, &renderInfo);
  vkCmdSetRasterizerDiscardEnable(commandBuffer, VK_FALSE);

  vkCmdSetViewportWithCount(commandBuffer, renderTargets, viewPorts);
  vkCmdSetScissorWithCount(commandBuffer, renderTargets, viewPortScissors);

  vk::CmdSetColorBlendEnableEXT(commandBuffer, 0, renderTargets,
                                colorBlendEnable);
  vk::CmdSetColorBlendEquationEXT(commandBuffer, 0, renderTargets,
                                  colorBlendEquation);

  vk::CmdSetDepthClampEnableEXT(commandBuffer, VK_FALSE);
  vkCmdSetDepthCompareOp(commandBuffer,
                         gnm::toVkCompareOp(pipe.context.dbDepthControl.zFunc));
  vkCmdSetDepthTestEnable(commandBuffer, pipe.context.dbDepthControl.depthEnable
                                             ? VK_TRUE
                                             : VK_FALSE);
  vkCmdSetDepthWriteEnable(
      commandBuffer,
      pipe.context.dbDepthControl.depthWriteEnable ? VK_TRUE : VK_FALSE);
  vkCmdSetDepthBounds(commandBuffer, pipe.context.dbDepthBoundsMin,
                      pipe.context.dbDepthBoundsMax);
  vkCmdSetDepthBoundsTestEnable(
      commandBuffer,
      pipe.context.dbDepthControl.depthBoundsEnable ? VK_TRUE : VK_FALSE);
  //   vkCmdSetStencilOp(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK,
  //                     VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP,
  //                     VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS);

  vkCmdSetDepthBiasEnable(commandBuffer, VK_FALSE);
  vkCmdSetDepthBias(commandBuffer, 0, 1, 1);
  vkCmdSetPrimitiveRestartEnable(commandBuffer, VK_FALSE);

  vk::CmdSetAlphaToOneEnableEXT(commandBuffer, VK_FALSE);

  vk::CmdSetLogicOpEnableEXT(commandBuffer, VK_FALSE);
  vk::CmdSetLogicOpEXT(commandBuffer, VK_LOGIC_OP_AND);
  vk::CmdSetPolygonModeEXT(commandBuffer, VK_POLYGON_MODE_FILL);
  vk::CmdSetRasterizationSamplesEXT(commandBuffer, VK_SAMPLE_COUNT_1_BIT);
  VkSampleMask sampleMask = ~0;
  vk::CmdSetSampleMaskEXT(commandBuffer, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
  vk::CmdSetTessellationDomainOriginEXT(
      commandBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT);
  vk::CmdSetAlphaToCoverageEnableEXT(commandBuffer, VK_FALSE);
  vk::CmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  vk::CmdSetColorWriteMaskEXT(commandBuffer, 0, renderTargets, colorWriteMask);

  vkCmdSetStencilCompareMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
  vkCmdSetStencilWriteMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
  vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);

  VkCullModeFlags cullMode = VK_CULL_MODE_NONE;
  if (pipe.context.paSuScModeCntl.cullBack) {
    cullMode |= VK_CULL_MODE_BACK_BIT;
  }
  if (pipe.context.paSuScModeCntl.cullFront) {
    cullMode |= VK_CULL_MODE_FRONT_BIT;
  }

  vkCmdSetCullMode(commandBuffer, cullMode);
  vkCmdSetFrontFace(commandBuffer,
                    gnm::toVkFrontFace(pipe.context.paSuScModeCntl.face));

  vkCmdSetPrimitiveTopology(commandBuffer,
                            toVkPrimitiveType(pipe.uConfig.vgtPrimitiveType));
  vkCmdSetStencilTestEnable(commandBuffer, VK_FALSE);

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, descriptorSets.size(),
                          descriptorSets.data(), 0, nullptr);

  vk::CmdBindShadersEXT(commandBuffer, stages.size(), stages.data(), shaders);

  shaderResources.buildMemoryTable(*memoryTable);

  for (auto &sampler : shaderResources.samplerResources) {
    uint32_t index = &sampler - shaderResources.samplerResources.data();

    VkDescriptorImageInfo samplerInfo{.sampler = sampler.handle};

    VkWriteDescriptorSet writeDescSet{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets[0],
        .dstBinding = Cache::getDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLER),
        .dstArrayElement = index,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &samplerInfo,
    };

    vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
  }

  for (auto &imageResources : shaderResources.imageResources) {
    auto dim = (&imageResources - shaderResources.imageResources) + 1;
    for (auto &image : imageResources) {
      uint32_t index = &image - imageResources.data();

      VkDescriptorImageInfo imageInfo{
          .imageView = image.handle,
          .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
      };

      VkWriteDescriptorSet writeDescSet{
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSets[0],
          .dstBinding = static_cast<uint32_t>(Cache::getDescriptorBinding(
              VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, dim)),
          .dstArrayElement = index,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &imageInfo,
      };

      vkUpdateDescriptorSets(vk::context->device, 1, &writeDescSet, 0, nullptr);
    }
  }

  for (auto &mtConfig : memoryTableConfigSlots) {
    auto config = descriptorBuffers[mtConfig.bufferIndex];
    config[mtConfig.configIndex] =
        shaderResources.getResourceSlot(mtConfig.resourceSlot);
  }

  if (indexBuffer.handle != VK_NULL_HANDLE) {
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, indexBuffer.offset,
                         gnm::toVkIndexType(indexBuffer.indexType));
    vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, firstVertex,
                     firstInstance);
  } else {
    vkCmdDraw(commandBuffer, vertexCount, instanceCount, firstVertex,
              firstInstance);
  }

  vkCmdEndRendering(commandBuffer);
  pipe.scheduler.submit();
  pipe.scheduler.then([=, cacheTag = std::move(cacheTag),
                       shaderResources = std::move(shaderResources)] {});
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

void amdgpu::flip(Cache::Tag &cacheTag, VkCommandBuffer commandBuffer,
                  VkExtent2D targetExtent, std::uint64_t address,
                  VkImageView target, VkExtent2D imageExtent,
                  CbCompSwap compSwap, TileMode tileMode, gnm::DataFormat dfmt,
                  gnm::NumericFormat nfmt) {
  auto pipelineLayout = cacheTag.getGraphicsPipelineLayout();
  auto descriptorSets = cacheTag.createGraphicsDescriptorSets();

  ImageViewKey framebuffer{};
  framebuffer.type = gnm::TextureType::Dim2D;
  framebuffer.pitch = imageExtent.width;
  framebuffer.readAddress = address;
  framebuffer.extent.width = imageExtent.width;
  framebuffer.extent.height = imageExtent.height;
  framebuffer.extent.depth = 1;
  framebuffer.dfmt = dfmt;
  framebuffer.nfmt = nfmt;
  framebuffer.mipCount = 1;
  framebuffer.arrayLayerCount = 1;
  framebuffer.tileMode = tileMode;

  switch (compSwap) {
  case CbCompSwap::Std:
    framebuffer.R = gnm::Swizzle::R;
    framebuffer.G = gnm::Swizzle::G;
    framebuffer.B = gnm::Swizzle::B;
    framebuffer.A = gnm::Swizzle::A;
    break;
  case CbCompSwap::Alt:
    framebuffer.R = gnm::Swizzle::B;
    framebuffer.G = gnm::Swizzle::G;
    framebuffer.B = gnm::Swizzle::R;
    framebuffer.A = gnm::Swizzle::A;
    break;
  case CbCompSwap::StdRev:
    framebuffer.R = gnm::Swizzle::A;
    framebuffer.G = gnm::Swizzle::B;
    framebuffer.B = gnm::Swizzle::G;
    framebuffer.A = gnm::Swizzle::R;
    break;
  case CbCompSwap::AltRev:
    framebuffer.R = gnm::Swizzle::A;
    framebuffer.G = gnm::Swizzle::R;
    framebuffer.B = gnm::Swizzle::G;
    framebuffer.A = gnm::Swizzle::B;
    break;
  }

  SamplerKey framebufferSampler = {
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
  };

  auto imageView = cacheTag.getImageView(framebuffer, Access::Read);
  auto sampler = cacheTag.getSampler(framebufferSampler);

  cacheTag.submitAndWait();

  VkDescriptorImageInfo imageInfo{
      .sampler = sampler.handle,
      .imageView = imageView.handle,
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  };

  VkWriteDescriptorSet writeDescSet[]{
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSets[0],
          .dstBinding =
              Cache::getDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2),
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
          .pImageInfo = &imageInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSets[0],
          .dstBinding = Cache::getDescriptorBinding(VK_DESCRIPTOR_TYPE_SAMPLER),
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
          .pImageInfo = &imageInfo,
      }};

  vkUpdateDescriptorSets(vk::context->device, std::size(writeDescSet),
                         writeDescSet, 0, nullptr);

  VkRenderingAttachmentInfo colorAttachments[1]{{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = target,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = {},
  }};
  VkBool32 colorBlendEnable[1]{VK_FALSE};
  VkColorBlendEquationEXT colorBlendEquation[1]{};
  VkColorComponentFlags colorWriteMask[1]{
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT};
  VkViewport viewPorts[1]{
      {
          .width = float(targetExtent.width),
          .height = float(targetExtent.height),
      },
  };

  VkRect2D viewPortScissors[1]{{
      {},
      targetExtent,
  }};

  VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea =
          {
              .offset = {},
              .extent = targetExtent,
          },
      .layerCount = 1,
      .colorAttachmentCount = 1,
      .pColorAttachments = colorAttachments,
  };

  vkCmdBeginRendering(commandBuffer, &renderInfo);
  vkCmdSetRasterizerDiscardEnable(commandBuffer, VK_FALSE);

  vkCmdSetViewportWithCount(commandBuffer, 1, viewPorts);
  vkCmdSetScissorWithCount(commandBuffer, 1, viewPortScissors);

  vk::CmdSetColorBlendEnableEXT(commandBuffer, 0, 1, colorBlendEnable);
  vk::CmdSetColorBlendEquationEXT(commandBuffer, 0, 1, colorBlendEquation);

  vk::CmdSetDepthClampEnableEXT(commandBuffer, VK_FALSE);
  vkCmdSetDepthTestEnable(commandBuffer, VK_FALSE);
  vkCmdSetDepthWriteEnable(commandBuffer, VK_FALSE);
  vkCmdSetDepthBounds(commandBuffer, 0.0f, 1.0f);
  vkCmdSetDepthBoundsTestEnable(commandBuffer, VK_FALSE);

  vkCmdSetDepthBiasEnable(commandBuffer, VK_FALSE);
  vkCmdSetDepthBias(commandBuffer, 0, 1, 1);
  vkCmdSetPrimitiveRestartEnable(commandBuffer, VK_FALSE);

  vk::CmdSetAlphaToOneEnableEXT(commandBuffer, VK_FALSE);

  vk::CmdSetLogicOpEnableEXT(commandBuffer, VK_FALSE);
  vk::CmdSetLogicOpEXT(commandBuffer, VK_LOGIC_OP_AND);
  vk::CmdSetPolygonModeEXT(commandBuffer, VK_POLYGON_MODE_FILL);
  vk::CmdSetRasterizationSamplesEXT(commandBuffer, VK_SAMPLE_COUNT_1_BIT);
  VkSampleMask sampleMask = ~0;
  vk::CmdSetSampleMaskEXT(commandBuffer, VK_SAMPLE_COUNT_1_BIT, &sampleMask);
  vk::CmdSetTessellationDomainOriginEXT(
      commandBuffer, VK_TESSELLATION_DOMAIN_ORIGIN_LOWER_LEFT);
  vk::CmdSetAlphaToCoverageEnableEXT(commandBuffer, VK_FALSE);
  vk::CmdSetVertexInputEXT(commandBuffer, 0, nullptr, 0, nullptr);
  vk::CmdSetColorWriteMaskEXT(commandBuffer, 0, 1, colorWriteMask);

  vkCmdSetStencilCompareMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
  vkCmdSetStencilWriteMask(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);
  vkCmdSetStencilReference(commandBuffer, VK_STENCIL_FACE_FRONT_AND_BACK, 0);

  vkCmdSetCullMode(commandBuffer, VK_CULL_MODE_NONE);
  vkCmdSetFrontFace(commandBuffer, VK_FRONT_FACE_CLOCKWISE);

  vkCmdSetPrimitiveTopology(commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
  vkCmdSetStencilTestEnable(commandBuffer, VK_FALSE);

  auto stages = Cache::kGraphicsStages;
  VkShaderEXT shaders[stages.size()]{};

  shaders[Cache::getStageIndex(VK_SHADER_STAGE_VERTEX_BIT)] =
      getFlipVertexShader(*cacheTag.getCache());

  shaders[Cache::getStageIndex(VK_SHADER_STAGE_FRAGMENT_BIT)] =
      getFlipFragmentShader(*cacheTag.getCache());

  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelineLayout, 0, descriptorSets.size(),
                          descriptorSets.data(), 0, nullptr);

  vk::CmdBindShadersEXT(commandBuffer, stages.size(), stages.data(), shaders);

  vkCmdDraw(commandBuffer, 6, 1, 0, 0);

  vkCmdEndRendering(commandBuffer);

  // {
  //   VkImageMemoryBarrier barrier{
  //       .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
  //       .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
  //       .dstAccessMask = VK_ACCESS_NONE,
  //       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  //       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
  //       .image = imageView.imageHandle,
  //       .subresourceRange =
  //           {
  //               .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
  //               .levelCount = 1,
  //               .layerCount = 1,
  //           },
  //   };

  //   vkCmdPipelineBarrier(commandBuffer,
  //   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
  //                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr,
  //                        0, nullptr, 1, &barrier);
  // }
}
