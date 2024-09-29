#include "vulkan.hpp"
#include "rx/die.hpp"

VkFormat gnm::toVkFormat(DataFormat dfmt, NumericFormat nfmt) {
  switch (dfmt) {
  case kDataFormat4_4_4_4: {
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_R4G4B4A4_UNORM_PACK16;
    default:
      break;
    }

    break;
  }

  case kDataFormat2_10_10_10:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    case kNumericFormatSNorm:
      return VK_FORMAT_A2B10G10R10_SNORM_PACK32;
    case kNumericFormatSInt:
      return VK_FORMAT_A2B10G10R10_SINT_PACK32;
    case kNumericFormatUInt:
      return VK_FORMAT_A2B10G10R10_UINT_PACK32;
    case kNumericFormatUScaled:
      return VK_FORMAT_A2B10G10R10_USCALED_PACK32;
    case kNumericFormatSScaled:
      return VK_FORMAT_A2B10G10R10_SSCALED_PACK32;
    default:
      break;
    }
    break;

  case kDataFormat10_10_10_2:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_A2R10G10B10_UNORM_PACK32;
    case kNumericFormatSNorm:
      return VK_FORMAT_A2R10G10B10_SNORM_PACK32;
    case kNumericFormatSInt:
      return VK_FORMAT_A2R10G10B10_SINT_PACK32;
    case kNumericFormatUInt:
      return VK_FORMAT_A2R10G10B10_UINT_PACK32;
    case kNumericFormatUScaled:
      return VK_FORMAT_A2R10G10B10_USCALED_PACK32;
    case kNumericFormatSScaled:
      return VK_FORMAT_A2R10G10B10_SSCALED_PACK32;
    default:
      break;
    }
    break;

  case kDataFormat8: {
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_R8_UNORM;
    case kNumericFormatSNorm:
      return VK_FORMAT_R8_SNORM;
    case kNumericFormatUInt:
      return VK_FORMAT_R8_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R8_SINT;
    case kNumericFormatSrgb:
      return VK_FORMAT_R8_SRGB;
    default:
      break;
    }

    break;
  }
  case kDataFormat32:
    switch (nfmt) {
    case kNumericFormatUInt:
      return VK_FORMAT_R32_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R32_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R32_SFLOAT;
    case kNumericFormatSrgb:
      return VK_FORMAT_R32_UINT; // FIXME
    default:
      break;
    }
    break;

  case kDataFormat8_8:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_R8G8_UNORM;
    case kNumericFormatSNorm:
      return VK_FORMAT_R8G8_SNORM;
    case kNumericFormatUInt:
      return VK_FORMAT_R8G8_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R8G8_SINT;
    default:
      break;
    }
    break;

  case kDataFormat5_9_9_9:
    switch (nfmt) {
    case kNumericFormatFloat:
      return VK_FORMAT_E5B9G9R9_UFLOAT_PACK32;
    default:
      break;
    }
    break;

  case kDataFormat5_6_5:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_B5G6R5_UNORM_PACK16;

    default:
      break;
    }
    break;

  case kDataFormat16_16:
    switch (nfmt) {
    case kNumericFormatUInt:
      return VK_FORMAT_R16G16_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R16G16_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R16G16_SFLOAT;
    default:
      break;
    }
    break;

  case kDataFormat32_32:
    switch (nfmt) {
    case kNumericFormatUInt:
      return VK_FORMAT_R32G32_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R32G32_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R32G32_SFLOAT;
    default:
      break;
    }
    break;

  case kDataFormat16_16_16_16:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_R16G16B16A16_UNORM;
    case kNumericFormatSNorm:
      return VK_FORMAT_R16G16B16A16_SNORM;
    case kNumericFormatUScaled:
      return VK_FORMAT_R16G16B16A16_USCALED;
    case kNumericFormatSScaled:
      return VK_FORMAT_R16G16B16A16_SSCALED;
    case kNumericFormatUInt:
      return VK_FORMAT_R16G16B16A16_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R16G16B16A16_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R16G16B16A16_SFLOAT;
    case kNumericFormatSrgb:
      return VK_FORMAT_R16G16B16A16_UNORM; // FIXME: wrong

    default:
      break;
    }
    break;

  case kDataFormat32_32_32:
    switch (nfmt) {
    case kNumericFormatUInt:
      return VK_FORMAT_R32G32B32_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R32G32B32_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R32G32B32_SFLOAT;
    default:
      break;
    }
    break;
  case kDataFormat32_32_32_32:
    switch (nfmt) {
    case kNumericFormatUInt:
      return VK_FORMAT_R32G32B32A32_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R32G32B32A32_SINT;
    case kNumericFormatFloat:
      return VK_FORMAT_R32G32B32A32_SFLOAT;
    default:
      break;
    }
    break;

  case kDataFormat24_8:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_D32_SFLOAT_S8_UINT; // HACK for amdgpu

    default:
      break;
    }

    break;

  case kDataFormat8_8_8_8:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_R8G8B8A8_UNORM;
    case kNumericFormatSNorm:
      return VK_FORMAT_R8G8B8A8_SNORM;
    case kNumericFormatUScaled:
      return VK_FORMAT_R8G8B8A8_USCALED;
    case kNumericFormatSScaled:
      return VK_FORMAT_R8G8B8A8_SSCALED;
    case kNumericFormatUInt:
      return VK_FORMAT_R8G8B8A8_UINT;
    case kNumericFormatSInt:
      return VK_FORMAT_R8G8B8A8_SINT;
    case kNumericFormatSNormNoZero:
    case kNumericFormatSrgb:
      return VK_FORMAT_R8G8B8A8_SRGB;

    default:
      break;
    }
    break;

  case kDataFormatBc1:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case kNumericFormatSrgb:
      return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    default:
      break;
    }
    break;

  case kDataFormatBc2:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC2_UNORM_BLOCK;
    case kNumericFormatSrgb:
      return VK_FORMAT_BC2_SRGB_BLOCK;
    default:
      break;
    }
    break;

  case kDataFormatBc3:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC3_UNORM_BLOCK;
    case kNumericFormatSrgb:
      return VK_FORMAT_BC3_SRGB_BLOCK;
    default:
      break;
    }
    break;

  case kDataFormatBc4:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC4_UNORM_BLOCK;

    case kNumericFormatSNorm:
      return VK_FORMAT_BC4_SNORM_BLOCK;

    default:
      break;
    }
    break;
  case kDataFormatBc5:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC5_UNORM_BLOCK;

    case kNumericFormatSNorm:
      return VK_FORMAT_BC5_SNORM_BLOCK;

    default:
      break;
    }
    break;

  case kDataFormatBc6:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC6H_UFLOAT_BLOCK;

    case kNumericFormatSNorm:
      return VK_FORMAT_BC6H_SFLOAT_BLOCK;

    default:
      break;
    }
    break;

  case kDataFormatBc7:
    switch (nfmt) {
    case kNumericFormatUNorm:
      return VK_FORMAT_BC7_UNORM_BLOCK;

    case kNumericFormatSrgb:
      return VK_FORMAT_BC7_SRGB_BLOCK;

    default:
      break;
    }
    break;

  default:
    break;
  }

  rx::die("unimplemented surface format. %x.%x\n", (int)dfmt, (int)nfmt);
}
