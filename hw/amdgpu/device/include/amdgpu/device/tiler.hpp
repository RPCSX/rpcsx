#pragma once
#include "device.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace amdgpu::device {
namespace Gnm {
enum GpuMode { kGpuModeBase = 0, kGpuModeNeo = 1 };
enum TileMode {
  kTileModeDepth_2dThin_64 = 0x00000000,
  kTileModeDepth_2dThin_128 = 0x00000001,
  kTileModeDepth_2dThin_256 = 0x00000002,
  kTileModeDepth_2dThin_512 = 0x00000003,
  kTileModeDepth_2dThin_1K = 0x00000004,
  kTileModeDepth_2dThinPrt_256 = 0x00000006,

  kTileModeDisplay_LinearAligned = 0x00000008,
  kTileModeDisplay_2dThin = 0x0000000A,
  kTileModeDisplay_ThinPrt = 0x0000000B,
  kTileModeDisplay_2dThinPrt = 0x0000000C,

  kTileModeThin_1dThin = 0x0000000D,
  kTileModeThin_2dThin = 0x0000000E,
  kTileModeThin_ThinPrt = 0x00000010,
  kTileModeThin_2dThinPrt = 0x00000011,
  kTileModeThin_3dThinPrt = 0x00000012,

  kTileModeThick_1dThick = 0x00000013,
  kTileModeThick_2dThick = 0x00000014,
  kTileModeThick_ThickPrt = 0x00000016,
  kTileModeThick_2dThickPrt = 0x00000017,
  kTileModeThick_3dThickPrt = 0x00000018,
  kTileModeThick_2dXThick = 0x00000019,
};

enum MicroTileMode {
  kMicroTileModeDisplay = 0x00000000,
  kMicroTileModeThin = 0x00000001,
  kMicroTileModeDepth = 0x00000002,
  kMicroTileModeRotated = 0x00000003,
  kMicroTileModeThick = 0x00000004,
};

enum ArrayMode {
  kArrayModeLinearGeneral = 0x00000000,
  kArrayModeLinearAligned = 0x00000001,
  kArrayMode1dTiledThin = 0x00000002,
  kArrayMode1dTiledThick = 0x00000003,
  kArrayMode2dTiledThin = 0x00000004,
  kArrayModeTiledThinPrt = 0x00000005,
  kArrayMode2dTiledThinPrt = 0x00000006,
  kArrayMode2dTiledThick = 0x00000007,
  kArrayMode2dTiledXThick = 0x00000008,
  kArrayModeTiledThickPrt = 0x00000009,
  kArrayMode2dTiledThickPrt = 0x0000000a,
  kArrayMode3dTiledThinPrt = 0x0000000b,
  kArrayMode3dTiledThin = 0x0000000c,
  kArrayMode3dTiledThick = 0x0000000d,
  kArrayMode3dTiledXThick = 0x0000000e,
  kArrayMode3dTiledThickPrt = 0x0000000f,
};

enum PipeConfig {
  kPipeConfigP8_32x32_8x16 = 0x0000000a,
  kPipeConfigP8_32x32_16x16 = 0x0000000c,
  kPipeConfigP16 = 0x00000012,
};
} // namespace Gnm

#define GNM_ERROR(msg, ...)                                                    \
  //std::fprintf(stderr, msg, __VA_ARGS__);                                      \
  //std::abort() \
  __builtin_trap();

static constexpr uint32_t kMicroTileWidth = 8;
static constexpr uint32_t kMicroTileHeight = 8;

static constexpr uint32_t getElementIndex(uint32_t x, uint32_t y, uint32_t z,
                                          uint32_t bitsPerElement,
                                          Gnm::MicroTileMode microTileMode,
                                          Gnm::ArrayMode arrayMode) {
  uint32_t elem = 0;

  if (microTileMode == Gnm::kMicroTileModeDisplay) {
    switch (bitsPerElement) {
    case 8:
      elem |= ((x >> 0) & 0x1) << 0;
      elem |= ((x >> 1) & 0x1) << 1;
      elem |= ((x >> 2) & 0x1) << 2;
      elem |= ((y >> 1) & 0x1) << 3;
      elem |= ((y >> 0) & 0x1) << 4;
      elem |= ((y >> 2) & 0x1) << 5;
      break;
    case 16:
      elem |= ((x >> 0) & 0x1) << 0;
      elem |= ((x >> 1) & 0x1) << 1;
      elem |= ((x >> 2) & 0x1) << 2;
      elem |= ((y >> 0) & 0x1) << 3;
      elem |= ((y >> 1) & 0x1) << 4;
      elem |= ((y >> 2) & 0x1) << 5;
      break;
    case 32:
      elem |= ((x >> 0) & 0x1) << 0;
      elem |= ((x >> 1) & 0x1) << 1;
      elem |= ((y >> 0) & 0x1) << 2;
      elem |= ((x >> 2) & 0x1) << 3;
      elem |= ((y >> 1) & 0x1) << 4;
      elem |= ((y >> 2) & 0x1) << 5;
      break;
    case 64:
      elem |= ((x >> 0) & 0x1) << 0;
      elem |= ((y >> 0) & 0x1) << 1;
      elem |= ((x >> 1) & 0x1) << 2;
      elem |= ((x >> 2) & 0x1) << 3;
      elem |= ((y >> 1) & 0x1) << 4;
      elem |= ((y >> 2) & 0x1) << 5;
      break;
    default:
      GNM_ERROR("Unsupported bitsPerElement (%u) for displayable surface.",
                bitsPerElement);
    }
  } else if (microTileMode == Gnm::kMicroTileModeThin ||
             microTileMode == Gnm::kMicroTileModeDepth) {
    elem |= ((x >> 0) & 0x1) << 0;
    elem |= ((y >> 0) & 0x1) << 1;
    elem |= ((x >> 1) & 0x1) << 2;
    elem |= ((y >> 1) & 0x1) << 3;
    elem |= ((x >> 2) & 0x1) << 4;
    elem |= ((y >> 2) & 0x1) << 5;
    // Use Z too, if the array mode is Thick/XThick
    switch (arrayMode) {
    case Gnm::kArrayMode2dTiledXThick:
    case Gnm::kArrayMode3dTiledXThick:
      elem |= ((z >> 2) & 0x1) << 8;
      // Intentional fall-through
    case Gnm::kArrayMode1dTiledThick:
    case Gnm::kArrayMode2dTiledThick:
    case Gnm::kArrayMode3dTiledThick:
    case Gnm::kArrayModeTiledThickPrt:
    case Gnm::kArrayMode2dTiledThickPrt:
    case Gnm::kArrayMode3dTiledThickPrt:
      elem |= ((z >> 0) & 0x1) << 6;
      elem |= ((z >> 1) & 0x1) << 7;
    default:
      break; // no other thick modes
    }
  } else if (microTileMode == Gnm::kMicroTileModeThick) // thick/xthick
  {
    switch (arrayMode) {
    case Gnm::kArrayMode2dTiledXThick:
    case Gnm::kArrayMode3dTiledXThick:
      elem |= ((z >> 2) & 0x1) << 8;
      // intentional fall-through
    case Gnm::kArrayMode1dTiledThick:
    case Gnm::kArrayMode2dTiledThick:
    case Gnm::kArrayMode3dTiledThick:
    case Gnm::kArrayModeTiledThickPrt:
    case Gnm::kArrayMode2dTiledThickPrt:
    case Gnm::kArrayMode3dTiledThickPrt:
      if (bitsPerElement == 8 || bitsPerElement == 16) {
        elem |= ((x >> 0) & 0x1) << 0;
        elem |= ((y >> 0) & 0x1) << 1;
        elem |= ((x >> 1) & 0x1) << 2;
        elem |= ((y >> 1) & 0x1) << 3;
        elem |= ((z >> 0) & 0x1) << 4;
        elem |= ((z >> 1) & 0x1) << 5;
        elem |= ((x >> 2) & 0x1) << 6;
        elem |= ((y >> 2) & 0x1) << 7;
      } else if (bitsPerElement == 32) {
        elem |= ((x >> 0) & 0x1) << 0;
        elem |= ((y >> 0) & 0x1) << 1;
        elem |= ((x >> 1) & 0x1) << 2;
        elem |= ((z >> 0) & 0x1) << 3;
        elem |= ((y >> 1) & 0x1) << 4;
        elem |= ((z >> 1) & 0x1) << 5;
        elem |= ((x >> 2) & 0x1) << 6;
        elem |= ((y >> 2) & 0x1) << 7;
      } else if (bitsPerElement == 64 || bitsPerElement == 128) {
        elem |= ((x >> 0) & 0x1) << 0;
        elem |= ((y >> 0) & 0x1) << 1;
        elem |= ((z >> 0) & 0x1) << 2;
        elem |= ((x >> 1) & 0x1) << 3;
        elem |= ((y >> 1) & 0x1) << 4;
        elem |= ((z >> 1) & 0x1) << 5;
        elem |= ((x >> 2) & 0x1) << 6;
        elem |= ((y >> 2) & 0x1) << 7;
      } else {
        GNM_ERROR("Invalid bitsPerElement (%u) for "
                  "microTileMode=kMicroTileModeThick.",
                  bitsPerElement);
      }
      break;
    default:
      GNM_ERROR("Invalid arrayMode (0x%02X) for thick/xthick "
                "microTileMode=kMicroTileModeThick.",
                arrayMode);
    }
  }
  // TODO: rotated

  return elem;
}
static constexpr uint32_t getPipeIndex(uint32_t x, uint32_t y,
                                       Gnm::PipeConfig pipeCfg) {
  uint32_t pipe = 0;
  switch (pipeCfg) {
  case Gnm::kPipeConfigP8_32x32_8x16:
    pipe |= (((x >> 4) ^ (y >> 3) ^ (x >> 5)) & 0x1) << 0;
    pipe |= (((x >> 3) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    break;
  case Gnm::kPipeConfigP8_32x32_16x16:
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    break;
  case Gnm::kPipeConfigP16:
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    pipe |= (((x >> 6) ^ (y >> 5)) & 0x1) << 3;
    break;
  default:
    GNM_ERROR("Unsupported pipeCfg (0x%02X).", pipeCfg);
  }
  return pipe;
}

inline constexpr uint32_t fastIntLog2(uint32_t i) {
  return 31 - __builtin_clz(i | 1);
}

static constexpr uint32_t getBankIndex(uint32_t x, uint32_t y,
                                       uint32_t bank_width,
                                       uint32_t bank_height, uint32_t num_banks,
                                       uint32_t num_pipes) {

  // bank_width=1, bank_height=1, num_banks = 16, num_pipes=8
  const uint32_t x_shift_offset = fastIntLog2(bank_width * num_pipes);
  const uint32_t y_shift_offset = fastIntLog2(bank_height);
  const uint32_t xs = x >> x_shift_offset;
  const uint32_t ys = y >> y_shift_offset;

  uint32_t bank = 0;
  switch (num_banks) {
  case 2:
    bank |= (((xs >> 3) ^ (ys >> 3)) & 0x1) << 0;
    break;
  case 4:
    bank |= (((xs >> 3) ^ (ys >> 4)) & 0x1) << 0;
    bank |= (((xs >> 4) ^ (ys >> 3)) & 0x1) << 1;
    break;
  case 8:
    bank |= (((xs >> 3) ^ (ys >> 5)) & 0x1) << 0;
    bank |= (((xs >> 4) ^ (ys >> 4) ^ (ys >> 5)) & 0x1) << 1;
    bank |= (((xs >> 5) ^ (ys >> 3)) & 0x1) << 2;
    break;
  case 16:
    bank |= (((xs >> 3) ^ (ys >> 6)) & 0x1) << 0;
    bank |= (((xs >> 4) ^ (ys >> 5) ^ (ys >> 6)) & 0x1) << 1;
    bank |= (((xs >> 5) ^ (ys >> 4)) & 0x1) << 2;
    bank |= (((xs >> 6) ^ (ys >> 3)) & 0x1) << 3;
    break;
  default:
    GNM_ERROR("invalid num_banks (%u) -- must be 2, 4, 8, or 16.", num_banks);
  }

  return bank;
}

inline std::uint32_t getTexelsPerElement(SurfaceFormat format) {
  if (format >= kSurfaceFormatBc1 && format <= kSurfaceFormatBc7) {
    return 16;
  }

  if (format >= kSurfaceFormat1) {
    return 8;
  }

  return 1;
}

inline std::uint32_t getBitsPerElement(SurfaceFormat format) {
  static constexpr int bitsPerElement[] = {
      0,  8,  16, 16, 32, 32, 32, 32, 32, 32, 32, 64, 64, 96, 128, -1,
      16, 16, 16, 16, 32, 32, 64, -1, -1, -1, -1, -1, -1, -1, -1,  -1,
      16, 16, 32, 4,  8,  8,  4,  8,  8,  8,  -1, -1, 8,  8,  8,   8,
      8,  8,  16, 16, 32, 32, 32, 64, 64, 8,  16, 1,  1};

  auto rawFormat = static_cast<unsigned>(format);

  if (rawFormat >= sizeof(bitsPerElement)) {
    return 0;
  }

  return bitsPerElement[rawFormat];
}

struct Tiler1d {
  Gnm::ArrayMode m_arrayMode;
  uint32_t m_bitsPerElement;

  Gnm::MicroTileMode m_microTileMode;
  uint32_t m_tileThickness;
  uint32_t m_tileBytes;
  uint32_t m_tilesPerRow;
  uint32_t m_tilesPerSlice;

  Tiler1d(const GnmTBuffer *texture) {
    /*
    m_arrayMode = Gnm::ArrayMode::kArrayMode1dTiledThin;
    m_bitsPerElement = 128;// getBitsPerElement(texture->dfmt);
    m_microTileMode = Gnm::MicroTileMode::kMicroTileModeThin;
    m_tileThickness = (m_arrayMode == Gnm::kArrayMode1dTiledThick) ? 4 : 1;
    m_tileBytes     = (kMicroTileWidth * kMicroTileHeight * m_tileThickness *
    m_bitsPerElement + 7) / 8;

    auto width = texture->width + 1;
    auto height = texture->height + 1;
    width = (width + 3) / 4;
    height = (height + 3) / 4;
    m_tilesPerRow   = width / kMicroTileWidth;
    m_tilesPerSlice = std::max(m_tilesPerRow * (height / kMicroTileHeight), 1U);
    */

    m_arrayMode = (Gnm::ArrayMode)2;
    m_bitsPerElement = 128;
    m_microTileMode = (Gnm::MicroTileMode)1;
    m_tileThickness = 1;
    m_tileBytes = 1024;
    m_tilesPerRow = 16;
    m_tilesPerSlice = 256;
  }

  uint64_t getTiledElementBitOffset(uint32_t x, uint32_t y, uint32_t z) const {
    uint64_t element_index = getElementIndex(x, y, z, m_bitsPerElement,
                                             m_microTileMode, m_arrayMode);

    uint64_t slice_offset =
        (z / m_tileThickness) * m_tilesPerSlice * m_tileBytes;

    uint64_t tile_row_index = y / kMicroTileHeight;
    uint64_t tile_column_index = x / kMicroTileWidth;
    uint64_t tile_offset =
        ((tile_row_index * m_tilesPerRow) + tile_column_index) * m_tileBytes;

    uint64_t element_offset = element_index * m_bitsPerElement;

    return (slice_offset + tile_offset) * 8 + element_offset;
  }

  int32_t getTiledElementByteOffset(uint32_t x, uint32_t y, uint32_t z) const {
    return getTiledElementBitOffset(x, y, z) / 8;
  }
};

struct Tiler2d {
  static constexpr int m_bitsPerElement = 32;
  static constexpr Gnm::MicroTileMode m_microTileMode =
      Gnm::kMicroTileModeDisplay;
  static constexpr Gnm::ArrayMode m_arrayMode = Gnm::kArrayMode2dTiledThin;
  static constexpr uint32_t m_macroTileWidth = 128;
  static constexpr uint32_t m_macroTileHeight = 64;
  static constexpr Gnm::PipeConfig m_pipeConfig =
      Gnm::kPipeConfigP8_32x32_16x16;
  static constexpr uint32_t m_bankWidth = 1;
  static constexpr uint32_t m_bankHeight = 1;
  static constexpr uint32_t m_numBanks = 16;
  static constexpr uint32_t m_numPipes = 8;
  static constexpr uint32_t m_tileThickness = 1;
  static constexpr uint32_t m_numFragmentsPerPixel = 1;
  static constexpr uint32_t m_tileSplitBytes = 512;
  static constexpr uint32_t m_pipeInterleaveBytes = 256;
  static constexpr uint32_t m_macroTileAspect = 2;
  static constexpr uint32_t m_paddedWidth = 1280;
  static constexpr uint32_t m_paddedHeight = 768;
  static constexpr uint32_t m_arraySlice = 0;
  static constexpr uint64_t m_bankSwizzleMask = 0;
  static constexpr uint64_t m_pipeSwizzleMask = 0;
  static constexpr uint64_t m_pipeInterleaveMask = 255;
  static constexpr uint64_t m_pipeInterleaveBits = 8;
  static constexpr uint64_t m_pipeBits = 3;
  static constexpr uint64_t m_bankBits = 4;

  static constexpr uint32_t kDramRowSize = 0x400;
  static constexpr uint32_t kNumLogicalBanks = 16;
  static constexpr uint32_t kPipeInterleaveBytes = 256;
  static constexpr uint32_t kBankInterleave = 1;
  static constexpr uint32_t kMicroTileWidth = 8;
  static constexpr uint32_t kMicroTileHeight = 8;
  static constexpr uint32_t kNumMicroTilePixels =
      kMicroTileWidth * kMicroTileHeight;
  static constexpr uint32_t kCmaskCacheBits = 0x400;
  static constexpr uint32_t kHtileCacheBits = 0x4000;

  int32_t getTiledElementBitOffset(uint64_t *outTiledBitOffset, uint32_t x,
                                   uint32_t y, uint32_t z,
                                   uint32_t fragmentIndex, bool log = false);

  int32_t getTiledElementByteOffset(uint64_t *outTiledByteOffset, uint32_t x,
                                    uint32_t y, uint32_t z,
                                    uint32_t fragmentIndex, bool log = false) {
    uint64_t bitOffset = 0;
    int32_t status =
        getTiledElementBitOffset(&bitOffset, x, y, z, fragmentIndex, log);
    *outTiledByteOffset = bitOffset / 8;
    return status;
  }
};

inline int32_t Tiler2d::getTiledElementBitOffset(uint64_t *outTiledBitOffset,
                                                 uint32_t x, uint32_t y,
                                                 uint32_t z,
                                                 uint32_t fragmentIndex,
                                                 bool log) {
  uint64_t element_index =
      getElementIndex(x, y, z, m_bitsPerElement, m_microTileMode, m_arrayMode);

  uint32_t xh = x, yh = y;
  if (m_arrayMode == Gnm::kArrayModeTiledThinPrt ||
      m_arrayMode == Gnm::kArrayModeTiledThickPrt) {
    xh %= m_macroTileWidth;
    yh %= m_macroTileHeight;
  }
  uint64_t pipe = getPipeIndex(xh, yh, m_pipeConfig);
  uint64_t bank =
      getBankIndex(xh, yh, m_bankWidth, m_bankHeight, m_numBanks, m_numPipes);

  constexpr uint32_t tile_bytes =
      (kMicroTileWidth * kMicroTileHeight * m_tileThickness * m_bitsPerElement *
           m_numFragmentsPerPixel +
       7) /
      8;

  uint64_t element_offset = 0;
  if (m_microTileMode == Gnm::kMicroTileModeDepth) {
    uint64_t pixel_offset =
        element_index * m_bitsPerElement * m_numFragmentsPerPixel;
    element_offset = pixel_offset + (fragmentIndex * m_bitsPerElement);
  } else {
    uint64_t fragment_offset =
        fragmentIndex * (tile_bytes / m_numFragmentsPerPixel) * 8;
    element_offset = fragment_offset + (element_index * m_bitsPerElement);
  }

  uint64_t slices_per_tile = 1;
  uint64_t tile_split_slice = 0;

  uint64_t macro_tile_bytes = (m_macroTileWidth / kMicroTileWidth) *
                              (m_macroTileHeight / kMicroTileHeight) *
                              tile_bytes / (m_numPipes * m_numBanks);
  uint64_t macro_tiles_per_row = m_paddedWidth / m_macroTileWidth;
  uint64_t macro_tile_row_index = y / m_macroTileHeight;
  uint64_t macro_tile_column_index = x / m_macroTileWidth;
  uint64_t macro_tile_index =
      (macro_tile_row_index * macro_tiles_per_row) + macro_tile_column_index;
  uint64_t macro_tile_offset = macro_tile_index * macro_tile_bytes;
  uint64_t macro_tiles_per_slice =
      macro_tiles_per_row * (m_paddedHeight / m_macroTileHeight);
  uint64_t slice_bytes = macro_tiles_per_slice * macro_tile_bytes;

  uint32_t slice = z;

  uint64_t slice_offset =
      (tile_split_slice + slices_per_tile * slice / m_tileThickness) *
      slice_bytes;
  if (m_arraySlice != 0) {
    slice = m_arraySlice;
  }

  uint64_t tile_row_index = (y / kMicroTileHeight) % m_bankHeight;
  uint64_t tile_column_index =
      ((x / kMicroTileWidth) / m_numPipes) % m_bankWidth;
  uint64_t tile_index = (tile_row_index * m_bankWidth) + tile_column_index;
  uint64_t tile_offset = tile_index * tile_bytes;

  // Bank and pipe rotation/swizzling.
  uint64_t bank_swizzle = m_bankSwizzleMask;
  uint64_t pipe_swizzle = m_pipeSwizzleMask;

  uint64_t pipe_slice_rotation = 0;
  switch (m_arrayMode) {
  case Gnm::kArrayMode3dTiledThin:
  case Gnm::kArrayMode3dTiledThick:
  case Gnm::kArrayMode3dTiledXThick:
    pipe_slice_rotation =
        std::max(1UL, (m_numPipes / 2UL) - 1UL) * (slice / m_tileThickness);
    break;
  default:
    break;
  }
  pipe_swizzle += pipe_slice_rotation;
  pipe_swizzle &= (m_numPipes - 1);
  pipe = pipe ^ pipe_swizzle;

  uint32_t slice_rotation = 0;
  switch (m_arrayMode) {
  case Gnm::kArrayMode2dTiledThin:
  case Gnm::kArrayMode2dTiledThick:
  case Gnm::kArrayMode2dTiledXThick:
    slice_rotation = ((m_numBanks / 2) - 1) * (slice / m_tileThickness);
    break;
  case Gnm::kArrayMode3dTiledThin:
  case Gnm::kArrayMode3dTiledThick:
  case Gnm::kArrayMode3dTiledXThick:
    slice_rotation = std::max(1UL, (m_numPipes / 2UL) - 1UL) *
                     (slice / m_tileThickness) / m_numPipes;
    break;
  default:
    break;
  }
  uint64_t tile_split_slice_rotation = 0;
  switch (m_arrayMode) {
  case Gnm::kArrayMode2dTiledThin:
  case Gnm::kArrayMode3dTiledThin:
  case Gnm::kArrayMode2dTiledThinPrt:
  case Gnm::kArrayMode3dTiledThinPrt:
    tile_split_slice_rotation = ((m_numBanks / 2) + 1) * tile_split_slice;
    break;
  default:
    break;
  }

  bank ^= bank_swizzle + slice_rotation;
  bank ^= tile_split_slice_rotation;
  bank &= (m_numBanks - 1);

  uint64_t total_offset =
      (slice_offset + macro_tile_offset + tile_offset) * 8 + element_offset;
  uint64_t bitOffset = total_offset & 0x7;
  total_offset /= 8;

  uint64_t pipe_interleave_offset = total_offset & m_pipeInterleaveMask;
  uint64_t offset = total_offset >> m_pipeInterleaveBits;

  uint64_t finalByteOffset =
      pipe_interleave_offset | (pipe << (m_pipeInterleaveBits)) |
      (bank << (m_pipeInterleaveBits + m_pipeBits)) |
      (offset << (m_pipeInterleaveBits + m_pipeBits + m_bankBits));
  *outTiledBitOffset = (finalByteOffset << 3) | bitOffset;
  return 0;
}

namespace surfaceTiler {
constexpr std::uint32_t getElementIndex(std::uint32_t x, std::uint32_t y) {
  std::uint32_t elem = 0;

  elem |= ((x >> 0) & 0x1) << 0;
  elem |= ((x >> 1) & 0x1) << 1;
  elem |= ((y >> 0) & 0x1) << 2;
  elem |= ((x >> 2) & 0x1) << 3;
  elem |= ((y >> 1) & 0x1) << 4;
  elem |= ((y >> 2) & 0x1) << 5;

  return elem;
}

constexpr std::uint32_t getPipeIndex(std::uint32_t x, std::uint32_t y) {
  std::uint32_t pipe = 0;

  pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
  pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
  pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;

  return pipe;
}

constexpr std::uint32_t getBankIndex(std::uint32_t x, std::uint32_t y) {
  std::uint32_t bank = 0;

  bank |= (((x >> 6) ^ (y >> 6)) & 0x1) << 0;
  bank |= (((x >> 7) ^ (y >> 5) ^ (y >> 6)) & 0x1) << 1;
  bank |= (((x >> 8) ^ (y >> 4)) & 0x1) << 2;
  bank |= (((x >> 9) ^ (y >> 3)) & 0x1) << 3;

  return bank;
}

inline std::uint64_t getTiledElementByteOffsetImpl(std::uint32_t x,
                                                   std::uint32_t y,
                                                   std::uint32_t width) {
  std::uint32_t elementIndex = getElementIndex(x, y);
  std::uint32_t pipe = getPipeIndex(x, y);
  std::uint32_t bank = getBankIndex(x, y);

  uint64_t macroTileIndex =
      (static_cast<std::uint64_t>(y / 64) * (width / 128)) + x / 128;
  uint64_t macroTileOffset = macroTileIndex * 256;

  std::uint64_t totalOffset = macroTileOffset + elementIndex * 4;

  std::uint64_t pipeInterleaveOffset = totalOffset & 255;
  std::uint64_t offset = totalOffset >> 8;

  return pipeInterleaveOffset | (pipe << 8) | (bank << 11) | (offset << 15);
}

static constexpr std::uint32_t kMaxPrecalculatedCount = 8;
static constexpr std::uint32_t kMaxPrecalculatedWidth = 2048;
static constexpr std::uint32_t kMaxPrecalculatedHeight = 2048;

static std::uint64_t gPrecalculatedTiledOffsets[kMaxPrecalculatedCount]
                                               [kMaxPrecalculatedWidth *
                                                kMaxPrecalculatedHeight];

struct PrecalculatedTiler {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t stride;
  int index;
};

static PrecalculatedTiler gPrecalculatedTilers[kMaxPrecalculatedCount];
static int gPrecalculatedCount;

static int findPrecalculatedTile(std::uint32_t width, std::uint32_t height) {
  for (int i = 0; i < gPrecalculatedCount; ++i) {
    if (gPrecalculatedTilers[i].width == width &&
        gPrecalculatedTilers[i].height == height) {
      return i;
    }
  }

  return -1;
}

inline int precalculateTiles(std::uint32_t width, std::uint32_t height) {
  int index = findPrecalculatedTile(width, height);
  if (index >= 0) {
    if (index >= kMaxPrecalculatedCount / 2 &&
        gPrecalculatedCount > kMaxPrecalculatedCount / 2) {
      auto tmp = gPrecalculatedTilers[index];

      for (int i = index; i > 0; --i) {
        gPrecalculatedTilers[i] = gPrecalculatedTilers[i - 1];
      }

      gPrecalculatedTilers[0] = tmp;
      return 0;
    }

    return index;
  }

  PrecalculatedTiler tiler;
  tiler.width = width;
  tiler.height = height;
  tiler.stride = std::min(width, kMaxPrecalculatedWidth);

  if (gPrecalculatedCount >= kMaxPrecalculatedCount) {
    // TODO: insert in the middle?
    tiler.index = gPrecalculatedTilers[kMaxPrecalculatedCount - 1].index;
    index = kMaxPrecalculatedCount - 1;
  } else {
    tiler.index = gPrecalculatedCount++;
    index = tiler.index;
  }

  gPrecalculatedTilers[index - 1] = tiler;

  for (std::uint32_t y = 0; y < height; ++y) {
    for (std::uint32_t x = 0; x < width; ++x) {
      gPrecalculatedTiledOffsets[index][y * tiler.stride + x] =
          getTiledElementByteOffsetImpl(x, y, tiler.width);
    }
  }

  return index;
}

inline std::uint64_t getTiledElementByteOffset(int index, std::uint32_t x,
                                               std::uint32_t y) {
  auto tiler = gPrecalculatedTilers[index];
  if (x < kMaxPrecalculatedWidth && y < kMaxPrecalculatedHeight) [[likely]] {
    return gPrecalculatedTiledOffsets[index][x + y * tiler.stride];
  }

  return getTiledElementByteOffsetImpl(x, y, tiler.width);
}
} // namespace surfaceTiler
} // namespace amdgpu::device
