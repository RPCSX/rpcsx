#pragma once
#include "rx/die.hpp"
#include <cstdlib>
#include <gnm/constants.hpp>
#include <vulkan/vulkan.h>

namespace gnm {
VkFormat toVkFormat(DataFormat dfmt, NumericFormat nfmt);

inline VkImageType toVkImageType(gnm::TextureType type) {
  switch (type) {
  case gnm::TextureType::Dim1D:
    return VK_IMAGE_TYPE_1D;
  case gnm::TextureType::Dim2D:
    return VK_IMAGE_TYPE_2D;
  case gnm::TextureType::Dim3D:
    return VK_IMAGE_TYPE_3D;
  case gnm::TextureType::Cube:
    return VK_IMAGE_TYPE_2D;
  case gnm::TextureType::Array1D:
    return VK_IMAGE_TYPE_1D;
  case gnm::TextureType::Array2D:
    return VK_IMAGE_TYPE_2D;
  case gnm::TextureType::Msaa2D:
    return VK_IMAGE_TYPE_2D;
  case gnm::TextureType::MsaaArray2D:
    return VK_IMAGE_TYPE_2D;
  }

  rx::die("toVkImageType: unexpected texture type %u",
          static_cast<unsigned>(type));
}

inline VkImageViewType toVkImageViewType(gnm::TextureType type) {
  switch (type) {
  case gnm::TextureType::Dim1D:
    return VK_IMAGE_VIEW_TYPE_1D;
  case gnm::TextureType::Dim2D:
    return VK_IMAGE_VIEW_TYPE_2D;
  case gnm::TextureType::Dim3D:
    return VK_IMAGE_VIEW_TYPE_3D;
  case gnm::TextureType::Cube:
    return VK_IMAGE_VIEW_TYPE_2D;
  case gnm::TextureType::Array1D:
    return VK_IMAGE_VIEW_TYPE_1D_ARRAY;
  case gnm::TextureType::Array2D:
    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  case gnm::TextureType::Msaa2D:
    return VK_IMAGE_VIEW_TYPE_2D;
  case gnm::TextureType::MsaaArray2D:
    return VK_IMAGE_VIEW_TYPE_2D_ARRAY;
  }

  rx::die("toVkImageViewType: unexpected texture type %u",
          static_cast<unsigned>(type));
}

inline VkComponentSwizzle toVkComponentSwizzle(Swizzle swizzle) {
  switch (swizzle) {
  case Swizzle::Zero:
    return VK_COMPONENT_SWIZZLE_ZERO;
  case Swizzle::One:
    return VK_COMPONENT_SWIZZLE_ONE;
  case Swizzle::R:
    return VK_COMPONENT_SWIZZLE_R;
  case Swizzle::G:
    return VK_COMPONENT_SWIZZLE_G;
  case Swizzle::B:
    return VK_COMPONENT_SWIZZLE_B;
  case Swizzle::A:
    return VK_COMPONENT_SWIZZLE_A;
  }

  rx::die("toVkComponentSwizzle: unexpected swizzle %u\n",
          static_cast<unsigned>(swizzle));
}

static VkBlendFactor toVkBlendFactor(BlendMultiplier mul) {
  switch (mul) {
  case BlendMultiplier::Zero:
    return VK_BLEND_FACTOR_ZERO;
  case BlendMultiplier::One:
    return VK_BLEND_FACTOR_ONE;
  case BlendMultiplier::SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case BlendMultiplier::OneMinusSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case BlendMultiplier::SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case BlendMultiplier::OneMinusSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case BlendMultiplier::DestAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case BlendMultiplier::OneMinusDestAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case BlendMultiplier::DestColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case BlendMultiplier::OneMinusDestColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case BlendMultiplier::SrcAlphaSaturate:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  case BlendMultiplier::ConstantColor:
    return VK_BLEND_FACTOR_CONSTANT_COLOR;
  case BlendMultiplier::OneMinusConstantColor:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR;
  case BlendMultiplier::Src1Color:
    return VK_BLEND_FACTOR_SRC1_COLOR;
  case BlendMultiplier::InverseSrc1Color:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR;
  case BlendMultiplier::Src1Alpha:
    return VK_BLEND_FACTOR_SRC1_ALPHA;
  case BlendMultiplier::InverseSrc1Alpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
  case BlendMultiplier::ConstantAlpha:
    return VK_BLEND_FACTOR_CONSTANT_ALPHA;
  case BlendMultiplier::OneMinusConstantAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA;
  }

  rx::die("VkBlendFactor: unexpected value %u\n", static_cast<unsigned>(mul));
}

static VkBlendOp toVkBlendOp(BlendFunc func) {
  switch (func) {
  case BlendFunc::Add:
    return VK_BLEND_OP_ADD;
  case BlendFunc::Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case BlendFunc::Min:
    return VK_BLEND_OP_MIN;
  case BlendFunc::Max:
    return VK_BLEND_OP_MAX;
  case BlendFunc::ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  }

  rx::die("blendFuncToVkBlendOp: unexpected value %u\n",
          static_cast<unsigned>(func));
}

static VkFrontFace toVkFrontFace(Face face) {
  switch (face) {
  case Face::CW:
    return VK_FRONT_FACE_CLOCKWISE;
  case Face::CCW:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }

  rx::die("toVkFrontFace: unexpected value %u\n", static_cast<unsigned>(face));
}

static VkIndexType toVkIndexType(IndexType indexType) {
  switch (indexType) {
  case IndexType::Int16:
    return VK_INDEX_TYPE_UINT16;
  case IndexType::Int32:
    return VK_INDEX_TYPE_UINT32;
  }

  rx::die("toVkIndexType: unexpected value %u\n",
          static_cast<unsigned>(indexType));
}

static VkCompareOp toVkCompareOp(CompareFunc compareFn) {
  return static_cast<VkCompareOp>(compareFn);
}

static VkBorderColor toVkBorderColor(BorderColor color) {
  switch (color) {
  case gnm::BorderColor::OpaqueBlack:
    return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;

  case gnm::BorderColor::TransparentBlack:
    return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

  case gnm::BorderColor::White:
    return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

  case gnm::BorderColor::Custom:
    return VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
  }

  rx::die("toVkBorderColor: unexpected value %u\n",
          static_cast<unsigned>(color));
}

static VkSamplerAddressMode toVkSamplerAddressMode(ClampMode clampMode) {
  switch (clampMode) {
  case ClampMode::Wrap:
    return VK_SAMPLER_ADDRESS_MODE_REPEAT;
  case ClampMode::Mirror:
    return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
  case ClampMode::ClampLastTexel:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  case ClampMode::MirrorOnceLastTexel:
    return VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
  case ClampMode::ClampHalfBorder:
    rx::die("toVkSamplerAddressMode: unimplemented ClampMode::ClampHalfBorder");
  case ClampMode::MirrorOnceHalfBorder:
    rx::die("toVkSamplerAddressMode: unimplemented "
            "ClampMode::MirrorOnceHalfBorder");

  case ClampMode::ClampBorder:
    return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;

  case ClampMode::MirrorOnceBorder:
    rx::die(
        "toVkSamplerAddressMode: unimplemented ClampMode::MirrorOnceBorder");
  }

  rx::die("toVkSamplerAddressMode: unexpected value %u\n",
          static_cast<unsigned>(clampMode));
}

static VkFilter toVkFilter(Filter filter) {
  switch (filter) {
  case Filter::Point:
    return VK_FILTER_NEAREST;
  case Filter::Bilinear:
    return VK_FILTER_LINEAR;
  case Filter::AnisoPoint:
    return VK_FILTER_NEAREST;
  case Filter::AnisoLinear:
    return VK_FILTER_LINEAR;
  }

  rx::die("toVkFilter: unexpected value %u\n", static_cast<unsigned>(filter));
}

static VkSamplerMipmapMode toVkSamplerMipmapMode(MipFilter filter) {
  switch (filter) {
  case MipFilter::None:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case MipFilter::Point:
    return VK_SAMPLER_MIPMAP_MODE_NEAREST;
  case MipFilter::Linear:
    return VK_SAMPLER_MIPMAP_MODE_LINEAR;
  }

  rx::die("toVkSamplerMipmapMode: unexpected value %u\n",
          static_cast<unsigned>(filter));
}
} // namespace gnm
