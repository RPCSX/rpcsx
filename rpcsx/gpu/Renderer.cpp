#include "Renderer.hpp"
#include "Device.hpp"
#include "gnm/gnm.hpp"

#include <amdgpu/tiler.hpp>
#include <gnm/constants.hpp>
#include <gnm/vulkan.hpp>
#include <print>
#include <rx/format.hpp>
#include <shader/Evaluator.hpp>
#include <shader/dialect.hpp>
#include <shader/gcn.hpp>
#include <shaders/fill_red.frag.h>
#include <shaders/rect_list.geom.h>

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

amdgpu::PaScRect intersection(amdgpu::PaScRect lhs, amdgpu::PaScRect rhs) {
  if (lhs.left > lhs.right) {
    lhs.left = rhs.left;
  }

  if (lhs.top > lhs.bottom) {
    lhs.top = rhs.top;
  }

  if (rhs.left > rhs.right) {
    rhs.left = lhs.left;
  }

  if (rhs.top > rhs.bottom) {
    rhs.top = lhs.top;
  }

  return {
      .left = std::max(lhs.left, rhs.left),
      .top = std::max(lhs.top, rhs.top),
      .right = std::min(lhs.right, rhs.right),
      .bottom = std::min(lhs.bottom, rhs.bottom),
  };
}

amdgpu::PaScRect extend(amdgpu::PaScRect lhs, amdgpu::PaScRect rhs) {
  return {
      .left = std::max(lhs.left, rhs.left),
      .top = std::max(lhs.top, rhs.top),
      .right = std::max(lhs.right, rhs.right),
      .bottom = std::max(lhs.bottom, rhs.bottom),
  };
}
} // namespace gnm

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

void amdgpu::draw(GraphicsPipe &pipe, int vmId, std::uint32_t firstVertex,
                  std::uint32_t vertexCount, std::uint32_t firstInstance,
                  std::uint32_t instanceCount, std::uint64_t indiciesAddress,
                  std::uint32_t indexOffset, std::uint32_t indexCount) {
  if (pipe.context.cbColorControl.mode == gnm::CbMode::Disable) {
    return;
  }

  if (pipe.context.cbColorControl.mode != gnm::CbMode::Normal &&
      pipe.context.cbColorControl.mode != gnm::CbMode::EliminateFastClear) {
    std::println("unimplemented context.cbColorControl.mode = {}",
                 static_cast<gnm::CbMode>(pipe.context.cbColorControl.mode));
    return;
  }

  auto cacheTag = pipe.device->getGraphicsTag(vmId, pipe.scheduler);
  auto targetMask = pipe.context.cbTargetMask.raw;

  VkRenderingAttachmentInfo colorAttachments[8]{};
  VkBool32 colorBlendEnable[8]{};
  VkColorBlendEquationEXT colorBlendEquation[8]{};
  VkColorComponentFlags colorWriteMask[8]{};
  VkViewport viewPorts[8]{};
  VkRect2D viewPortScissors[8]{};
  unsigned renderTargets = 0;

  VkRenderingAttachmentInfo depthAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
  };
  VkRenderingAttachmentInfo stencilAttachment{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
  };

  auto depthAccess = Access::None;
  auto stencilAccess = Access::None;

  if (pipe.context.dbDepthControl.depthEnable && pipe.context.dbZInfo.format != gnm::kZFormatInvalid) {
    if (!pipe.context.dbRenderControl.depthClearEnable) {
      depthAccess |= Access::Read;
    }
    if (!pipe.context.dbDepthView.zReadOnly &&
        pipe.context.dbDepthControl.depthWriteEnable) {
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

  // FIXME
  stencilAccess = Access::None;

  amdgpu::PaScRect drawRect{};

  for (auto &cbColor : pipe.context.cbColor) {
    if (targetMask == 0) {
      break;
    }

    if (cbColor.info.dfmt == gnm::kDataFormatInvalid) {
      continue;
    }

    auto viewPortScissor = pipe.context.paScScreenScissor;
    viewPortScissor = gnm::intersection(
        viewPortScissor, pipe.context.paScVportScissor[renderTargets]);
    viewPortScissor =
        gnm::intersection(viewPortScissor, pipe.context.paScWindowScissor);
    viewPortScissor =
        gnm::intersection(viewPortScissor, pipe.context.paScGenericScissor);

    auto viewPortRect = gnm::toVkRect2D(viewPortScissor);

    drawRect = gnm::extend(drawRect, viewPortScissor);

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
    renderTargetInfo.extent.width =
        vkViewPortScissor.offset.x + vkViewPortScissor.extent.width;
    renderTargetInfo.extent.height =
        vkViewPortScissor.offset.y + vkViewPortScissor.extent.height;
    renderTargetInfo.extent.depth = 1;
    renderTargetInfo.dfmt = cbColor.info.dfmt;
    renderTargetInfo.nfmt =
        gnm::toNumericFormat(cbColor.info.nfmt, cbColor.info.dfmt);
    renderTargetInfo.mipCount = 1;
    renderTargetInfo.arrayLayerCount = 1;

    renderTargetInfo.tileMode =
        cbColor.info.linearGeneral
            ? TileMode{.raw = 0}
            : getDefaultTileModes()[cbColor.attrib.tileModeIndex];

    auto access = Access::None;

    if (!cbColor.info.fastClear) {
      access |= Access::Read;
    }
    if (targetMask & 0xf) {
      access |= Access::Write;
    }

    if (pipe.uConfig.vgtPrimitiveType == gnm::PrimitiveType::None) {
      if (cbColor.info.fastClear) {
        auto image =
            cacheTag.getImage(ImageKey::createFrom(renderTargetInfo), access);
        VkClearColorValue clearValue = {
            .uint32 =
                {
                    cbColor.clearWord0,
                    cbColor.clearWord1,
                    cbColor.clearWord2,
                },
        };

        vkCmdClearColorImage(cacheTag.getScheduler().getCommandBuffer(),
                             image.handle, VK_IMAGE_LAYOUT_GENERAL, &clearValue,
                             1, &image.subresource);
      }

      continue;
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
    if ((depthAccess & Access::Write) != Access::None) {
      auto screenRect = gnm::toVkRect2D(pipe.context.paScScreenScissor);

      auto image = cacheTag.getImage(
          {
              .readAddress =
                  static_cast<std::uint64_t>(pipe.context.dbZReadBase) << 8,
              .writeAddress =
                  static_cast<std::uint64_t>(pipe.context.dbZWriteBase) << 8,
              .type = gnm::TextureType::Dim2D,
              .dfmt = gnm::getDataFormat(pipe.context.dbZInfo.format),
              .nfmt = gnm::getNumericFormat(pipe.context.dbZInfo.format),
              .extent =
                  {
                      .width = screenRect.extent.width,
                      .height = screenRect.extent.height,
                      .depth = 1,
                  },
              .pitch = screenRect.extent.width,
              .mipCount = 1,
              .arrayLayerCount = 1,
              .kind = ImageKind::Depth,
          },
          Access::Write);

      VkClearDepthStencilValue depthStencil = {
          .depth = pipe.context.dbDepthClear,
      };

      vkCmdClearDepthStencilImage(cacheTag.getScheduler().getCommandBuffer(),
                                  image.handle, VK_IMAGE_LAYOUT_GENERAL,
                                  &depthStencil, 1, &image.subresource);
      pipe.scheduler.submit();
      pipe.scheduler.wait();
    }

    return;
  }

  if (pipe.uConfig.vgtPrimitiveType == gnm::PrimitiveType::None) {
    pipe.scheduler.submit();
    pipe.scheduler.wait();
    return;
  }

  if (depthAccess != Access::None) {
    auto imageView = cacheTag.getImageView(
        {
            .readAddress = static_cast<std::uint64_t>(pipe.context.dbZReadBase)
                           << 8,
            .writeAddress =
                static_cast<std::uint64_t>(pipe.context.dbZWriteBase) << 8,
            .type = gnm::TextureType::Dim2D,
            .dfmt = gnm::getDataFormat(pipe.context.dbZInfo.format),
            .nfmt = gnm::getNumericFormat(pipe.context.dbZInfo.format),
            .extent =
                {
                    .width = pipe.context.dbDepthSize.getPitch(),
                    .height = pipe.context.dbDepthSize.getHeight(),
                    .depth = 1,
                },
            .pitch = pipe.context.dbDepthSize.getPitch(),
            .mipCount = 1,
            .arrayLayerCount = 1,
            .kind = ImageKind::Depth,
        },
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

  if (indiciesAddress == 0) {
    indexCount = vertexCount;
  }

  auto indexBuffer = cacheTag.getIndexBuffer(
      indiciesAddress, indexOffset, indexCount, pipe.uConfig.vgtPrimitiveType,
      pipe.uConfig.vgtIndexType);

  auto stages = Cache::kGraphicsStages;
  VkShaderEXT shaders[stages.size()]{};

  auto pipelineLayout = cacheTag.getGraphicsPipelineLayout();
  auto descriptorSets = cacheTag.getDescriptorSets();
  Cache::Shader vertexShader;

  if (pipe.context.vgtShaderStagesEn.vsEn == amdgpu::VsStage::VsReal) {
    gnm::PrimitiveType vsPrimType = {};
    if (indexBuffer.handle == VK_NULL_HANDLE &&
        pipe.uConfig.vgtPrimitiveType != indexBuffer.primType) {
      vsPrimType = pipe.uConfig.vgtPrimitiveType.value;
    }

    auto indexOffset =
        indexBuffer.handle == VK_NULL_HANDLE ? indexBuffer.offset : 0;

    vertexShader = cacheTag.getVertexShader(
        gcn::Stage::VsVs, pipe.sh.spiShaderPgmVs, pipe.context, indexOffset,
        vsPrimType, viewPorts);
  }

  auto pixelShader =
      cacheTag.getPixelShader(pipe.sh.spiShaderPgmPs, pipe.context, viewPorts);

  if (pixelShader.handle == nullptr) {
    shaders[Cache::getStageIndex(VK_SHADER_STAGE_FRAGMENT_BIT)] =
        getFillRedFragShader(*cacheTag.getCache());
  }

  shaders[Cache::getStageIndex(VK_SHADER_STAGE_VERTEX_BIT)] =
      vertexShader.handle;
  shaders[Cache::getStageIndex(VK_SHADER_STAGE_FRAGMENT_BIT)] =
      pixelShader.handle;

  if (pipe.uConfig.vgtPrimitiveType == gnm::PrimitiveType::RectList) {
    shaders[Cache::getStageIndex(VK_SHADER_STAGE_GEOMETRY_BIT)] =
        getPrimTypeRectGeomShader(*cacheTag.getCache());
  }

  if (indiciesAddress == 0) {
    vertexCount = indexBuffer.indexCount;
  }

  VkRenderingInfo renderInfo{
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .renderArea = gnm::toVkRect2D(drawRect),
      .layerCount = 1,
      .colorAttachmentCount = renderTargets,
      .pColorAttachments = colorAttachments,
      .pDepthAttachment =
          depthAccess != Access::None ? &depthAttachment : nullptr,
      .pStencilAttachment =
          stencilAccess != Access::None ? &stencilAttachment : nullptr,
  };

  cacheTag.buildDescriptors(descriptorSets[0]);

  pipe.scheduler.submit();
  pipe.scheduler.afterSubmit([cacheTag = std::move(cacheTag)] {});

  auto commandBuffer = pipe.scheduler.getCommandBuffer();

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

  if (pipe.uConfig.vgtPrimitiveType != gnm::PrimitiveType::RectList) {
    if (pipe.context.paSuScModeCntl.cullBack) {
      cullMode |= VK_CULL_MODE_BACK_BIT;
    }
    if (pipe.context.paSuScModeCntl.cullFront) {
      cullMode |= VK_CULL_MODE_FRONT_BIT;
    }
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
  pipe.scheduler.wait();
}

void amdgpu::dispatch(Cache &cache, Scheduler &sched,
                      Registers::ComputeConfig &pgm, std::uint32_t groupCountX,
                      std::uint32_t groupCountY, std::uint32_t groupCountZ) {
  auto tag = cache.createComputeTag(sched);
  auto descriptorSet = tag.getDescriptorSet();
  auto shader = tag.getShader(pgm);
  auto pipelineLayout = tag.getComputePipelineLayout();
  tag.buildDescriptors(descriptorSet);

  auto commandBuffer = sched.getCommandBuffer();
  VkShaderStageFlagBits stages[]{VK_SHADER_STAGE_COMPUTE_BIT};
  vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                          pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
  vk::CmdBindShadersEXT(commandBuffer, 1, stages, &shader.handle);
  vkCmdDispatch(commandBuffer, groupCountX, groupCountY, groupCountZ);
  sched.afterSubmit([tag = std::move(tag)] {});
  sched.submit();
  sched.wait();
}

void amdgpu::flip(Cache::Tag &cacheTag, VkExtent2D targetExtent,
                  std::uint64_t address, VkImageView target,
                  VkExtent2D imageExtent, FlipType type, TileMode tileMode,
                  gnm::DataFormat dfmt, gnm::NumericFormat nfmt) {
  ImageViewKey framebuffer{};
  framebuffer.readAddress = address;
  framebuffer.type = gnm::TextureType::Dim2D;
  framebuffer.dfmt = dfmt;
  framebuffer.nfmt = nfmt;
  framebuffer.tileMode = tileMode;
  framebuffer.extent.width = imageExtent.width;
  framebuffer.extent.height = imageExtent.height;
  framebuffer.extent.depth = 1;
  framebuffer.pitch = imageExtent.width;
  framebuffer.mipCount = 1;
  framebuffer.arrayLayerCount = 1;

  SamplerKey framebufferSampler = {
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
  };

  auto imageView = cacheTag.getImageView(framebuffer, Access::Read);
  auto sampler = cacheTag.getSampler(framebufferSampler);

  VkRenderingAttachmentInfo colorAttachments[1]{{
      .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
      .imageView = target,
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
  }};

  VkViewport viewPort{};
  viewPort.width = targetExtent.width;
  viewPort.height = targetExtent.height;

  float imageAspectRatio = float(imageExtent.width) / imageExtent.height;
  float targetAspectRatio = float(targetExtent.width) / targetExtent.height;

  auto aspectDiff = imageAspectRatio / targetAspectRatio;

  if (aspectDiff > 1) {
    viewPort.height = targetExtent.height / aspectDiff;
    viewPort.y = (targetExtent.height - viewPort.height) / 2;
  } else if (aspectDiff < 1) {
    viewPort.width = targetExtent.width * aspectDiff;
    viewPort.x = (targetExtent.width - viewPort.width) / 2;
  }

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

  auto commandBuffer = cacheTag.getScheduler().getCommandBuffer();
  vkCmdBeginRendering(commandBuffer, &renderInfo);

  cacheTag.getDevice()->flipPipeline.bind(cacheTag.getScheduler(), type,
                                          imageView.handle, sampler.handle);

  vkCmdSetViewport(commandBuffer, 0, 1, &viewPort);
  vkCmdSetScissor(commandBuffer, 0, 1, viewPortScissors);

  vkCmdDraw(commandBuffer, 6, 1, 0, 0);
  vkCmdEndRendering(commandBuffer);
}
