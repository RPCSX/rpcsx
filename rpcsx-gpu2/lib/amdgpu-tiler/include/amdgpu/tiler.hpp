#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <gnm/constants.hpp>
#include <gnm/descriptors.hpp>
#include <bit>

namespace amdgpu {
inline constexpr uint32_t kMicroTileWidth = 8;
inline constexpr uint32_t kMicroTileHeight = 8;
inline constexpr uint32_t kDramRowSize = 0x400;
inline constexpr uint32_t kPipeInterleaveBytes = 256;

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

enum MicroTileMode {
  kMicroTileModeDisplay = 0x00000000,
  kMicroTileModeThin = 0x00000001,
  kMicroTileModeDepth = 0x00000002,
  kMicroTileModeRotated = 0x00000003,
  kMicroTileModeThick = 0x00000004,
};

enum PipeConfig {
  kPipeConfigP8_32x32_8x16 = 0x0000000a,
  kPipeConfigP8_32x32_16x16 = 0x0000000c,
  kPipeConfigP16 = 0x00000012,
};

enum TileSplit {
  kTileSplit64B = 0x00000000,
  kTileSplit128B = 0x00000001,
  kTileSplit256B = 0x00000002,
  kTileSplit512B = 0x00000003,
  kTileSplit1KB = 0x00000004,
  kTileSplit2KB = 0x00000005,
  kTileSplit4KB = 0x00000006,
};

enum SampleSplit {
  kSampleSplit1 = 0x00000000,
  kSampleSplit2 = 0x00000001,
  kSampleSplit4 = 0x00000002,
  kSampleSplit8 = 0x00000003,
};

enum NumBanks {
  kNumBanks2 = 0x00000000,
  kNumBanks4 = 0x00000001,
  kNumBanks8 = 0x00000002,
  kNumBanks16 = 0x00000003,
};

enum BankWidth {
  kBankWidth1 = 0x00000000,
  kBankWidth2 = 0x00000001,
  kBankWidth4 = 0x00000002,
  kBankWidth8 = 0x00000003,
};

enum BankHeight {
  kBankHeight1 = 0x00000000,
  kBankHeight2 = 0x00000001,
  kBankHeight4 = 0x00000002,
  kBankHeight8 = 0x00000003,
};

enum MacroTileAspect {
  kMacroTileAspect1 = 0x00000000,
  kMacroTileAspect2 = 0x00000001,
  kMacroTileAspect4 = 0x00000002,
  kMacroTileAspect8 = 0x00000003,
};

struct TileMode {
  std::uint32_t raw;

  constexpr ArrayMode arrayMode() const {
    return ArrayMode((raw & 0x0000003c) >> 2);
  }
  constexpr PipeConfig pipeConfig() const {
    return PipeConfig((raw & 0x000007c0) >> 6);
  }
  constexpr TileSplit tileSplit() const {
    return TileSplit((raw & 0x00003800) >> 11);
  }
  constexpr MicroTileMode microTileMode() const {
    return MicroTileMode((raw & 0x01c00000) >> 22);
  }
  constexpr SampleSplit sampleSplit() const {
    return SampleSplit((raw & 0x06000000) >> 25);
  }
  constexpr std::uint32_t altPipeConfig() const {
    return (raw & 0xf8000000) >> 27;
  }

  constexpr TileMode &arrayMode(ArrayMode mode) {
    raw = (raw & ~0x0000003c) |
          (static_cast<std::uint32_t>(mode) << 2) & 0x0000003c;
    return *this;
  }
  constexpr TileMode &pipeConfig(PipeConfig mode) {
    raw = (raw & ~0x000007c0) |
          (static_cast<std::uint32_t>(mode) << 6) & 0x000007c0;
    return *this;
  }
  constexpr TileMode &tileSplit(TileSplit mode) {
    raw = (raw & ~0x00003800) |
          (static_cast<std::uint32_t>(mode) << 11) & 0x00003800;
    return *this;
  }
  constexpr TileMode &microTileMode(MicroTileMode mode) {
    raw = (raw & ~0x01c00000) |
          (static_cast<std::uint32_t>(mode) << 22) & 0x01c00000;
    return *this;
  }
  constexpr TileMode &sampleSplit(SampleSplit mode) {
    raw = (raw & ~0x06000000) |
          (static_cast<std::uint32_t>(mode) << 25) & 0x06000000;
    return *this;
  }
};

struct MacroTileMode {
  std::uint32_t raw;

  constexpr std::uint32_t bankWidth() const { return (raw & 0x00000003) >> 0; }
  constexpr std::uint32_t bankHeight() const { return (raw & 0x0000000c) >> 2; }
  constexpr MacroTileAspect macroTileAspect() const {
    return MacroTileAspect((raw & 0x00000030) >> 4);
  }
  constexpr std::uint32_t numBanks() const { return (raw & 0x000000c0) >> 6; }

  constexpr std::uint32_t altBankHeight() const {
    return (raw & 0x00000300) >> 8;
  }
  constexpr std::uint32_t altMacroTileAspect() const {
    return (raw & 0x00000c00) >> 10;
  }
  constexpr std::uint32_t altNumBanks() const {
    return (raw & 0x00003000) >> 12;
  }
};

struct SurfaceInfo {
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t depth;
  std::uint32_t pitch;
  int arrayLayerCount;
  int numFragments;
  int bitsPerElement;
  std::uint64_t totalSize;

  struct SubresourceInfo {
    std::uint32_t dataWidth;
    std::uint32_t dataHeight;
    std::uint32_t dataDepth;
    std::uint64_t offset;
    std::uint64_t tiledSize;
    std::uint64_t linearSize;
  };

  SubresourceInfo subresources[16];

  void setSubresourceInfo(int mipLevel, const SubresourceInfo &subresource) {
    subresources[mipLevel] = subresource;
  }

  const SubresourceInfo &getSubresourceInfo(int mipLevel) const {
    return subresources[mipLevel];
  }
};

constexpr uint32_t getMicroTileThickness(ArrayMode arrayMode) {
  switch (arrayMode) {
  case kArrayMode1dTiledThick:
  case kArrayMode2dTiledThick:
  case kArrayMode3dTiledThick:
  case kArrayModeTiledThickPrt:
  case kArrayMode2dTiledThickPrt:
  case kArrayMode3dTiledThickPrt:
    return 4;
  case kArrayMode2dTiledXThick:
  case kArrayMode3dTiledXThick:
    return 8;
  case kArrayModeLinearGeneral:
  case kArrayModeLinearAligned:
  case kArrayMode1dTiledThin:
  case kArrayMode2dTiledThin:
  case kArrayModeTiledThinPrt:
  case kArrayMode2dTiledThinPrt:
  case kArrayMode3dTiledThinPrt:
  case kArrayMode3dTiledThin:
    return 1;
  }

  std::abort();
}

constexpr bool isMacroTiled(ArrayMode arrayMode) {
  switch (arrayMode) {
  case kArrayModeLinearGeneral:
  case kArrayModeLinearAligned:
  case kArrayMode1dTiledThin:
  case kArrayMode1dTiledThick:
    return false;
  case kArrayMode2dTiledThin:
  case kArrayModeTiledThinPrt:
  case kArrayMode2dTiledThinPrt:
  case kArrayMode2dTiledThick:
  case kArrayMode2dTiledXThick:
  case kArrayModeTiledThickPrt:
  case kArrayMode2dTiledThickPrt:
  case kArrayMode3dTiledThinPrt:
  case kArrayMode3dTiledThin:
  case kArrayMode3dTiledThick:
  case kArrayMode3dTiledXThick:
  case kArrayMode3dTiledThickPrt:
    return true;
  }

  std::abort();
}

constexpr bool isPrt(ArrayMode arrayMode) {
  switch (arrayMode) {
  case kArrayModeLinearGeneral:
  case kArrayModeLinearAligned:
  case kArrayMode1dTiledThin:
  case kArrayMode1dTiledThick:
  case kArrayMode2dTiledThin:
  case kArrayMode2dTiledThick:
  case kArrayMode2dTiledXThick:
  case kArrayMode3dTiledThin:
  case kArrayMode3dTiledThick:
  case kArrayMode3dTiledXThick:
    return false;

  case kArrayModeTiledThinPrt:
  case kArrayMode2dTiledThinPrt:
  case kArrayModeTiledThickPrt:
  case kArrayMode2dTiledThickPrt:
  case kArrayMode3dTiledThinPrt:
  case kArrayMode3dTiledThickPrt:
    return true;
  }

  std::abort();
}

constexpr std::array<MacroTileMode, 16> getDefaultMacroTileModes() {
  return {{
      {.raw = 0x26e8},
      {.raw = 0x26d4},
      {.raw = 0x21d0},
      {.raw = 0x21d0},
      {.raw = 0x2080},
      {.raw = 0x2040},
      {.raw = 0x1000},
      {.raw = 0x0000},
      {.raw = 0x36ec},
      {.raw = 0x26e8},
      {.raw = 0x21d4},
      {.raw = 0x20d0},
      {.raw = 0x1080},
      {.raw = 0x1040},
      {.raw = 0x0000},
      {.raw = 0x0000},
  }};
}

constexpr std::array<TileMode, 32> getDefaultTileModes() {
  return {{
      {.raw = 0x90800310}, {.raw = 0x90800b10}, {.raw = 0x90801310},
      {.raw = 0x90801b10}, {.raw = 0x90802310}, {.raw = 0x90800308},
      {.raw = 0x90801318}, {.raw = 0x90802318}, {.raw = 0x90000304},
      {.raw = 0x90000308}, {.raw = 0x92000310}, {.raw = 0x92000294},
      {.raw = 0x92000318}, {.raw = 0x90400308}, {.raw = 0x92400310},
      {.raw = 0x924002b0}, {.raw = 0x92400294}, {.raw = 0x92400318},
      {.raw = 0x9240032c}, {.raw = 0x9100030c}, {.raw = 0x9100031c},
      {.raw = 0x910002b4}, {.raw = 0x910002a4}, {.raw = 0x91000328},
      {.raw = 0x910002bc}, {.raw = 0x91000320}, {.raw = 0x910002b8},
      {.raw = 0x90c00308}, {.raw = 0x92c00310}, {.raw = 0x92c00294},
      {.raw = 0x92c00318}, {.raw = 0x00000000},
  }};
}

constexpr std::uint32_t getElementIndex(std::uint32_t x, std::uint32_t y,
                                        std::uint32_t z,
                                        std::uint32_t bitsPerElement,
                                        MicroTileMode microTileMode,
                                        ArrayMode arrayMode) {
  std::uint32_t elem = 0;

  if (microTileMode == kMicroTileModeDisplay) {
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
      std::abort();
    }
  } else if (microTileMode == kMicroTileModeThin ||
             microTileMode == kMicroTileModeDepth) {
    elem |= ((x >> 0) & 0x1) << 0;
    elem |= ((y >> 0) & 0x1) << 1;
    elem |= ((x >> 1) & 0x1) << 2;
    elem |= ((y >> 1) & 0x1) << 3;
    elem |= ((x >> 2) & 0x1) << 4;
    elem |= ((y >> 2) & 0x1) << 5;

    switch (arrayMode) {
    case kArrayMode2dTiledXThick:
    case kArrayMode3dTiledXThick:
      elem |= ((z >> 2) & 0x1) << 8;
    case kArrayMode1dTiledThick:
    case kArrayMode2dTiledThick:
    case kArrayMode3dTiledThick:
    case kArrayModeTiledThickPrt:
    case kArrayMode2dTiledThickPrt:
    case kArrayMode3dTiledThickPrt:
      elem |= ((z >> 0) & 0x1) << 6;
      elem |= ((z >> 1) & 0x1) << 7;
    default:
      break;
    }
  } else if (microTileMode == kMicroTileModeThick) {
    switch (arrayMode) {
    case kArrayMode2dTiledXThick:
    case kArrayMode3dTiledXThick:
      elem |= ((z >> 2) & 0x1) << 8;

    case kArrayMode1dTiledThick:
    case kArrayMode2dTiledThick:
    case kArrayMode3dTiledThick:
    case kArrayModeTiledThickPrt:
    case kArrayMode2dTiledThickPrt:
    case kArrayMode3dTiledThickPrt:
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
        std::abort();
      }
      break;
    default:
      std::abort();
    }
  }
  return elem;
}

constexpr uint32_t getPipeIndex(uint32_t x, uint32_t y, PipeConfig pipeCfg) {
  uint32_t pipe = 0;
  switch (pipeCfg) {
  case kPipeConfigP8_32x32_8x16:
    pipe |= (((x >> 4) ^ (y >> 3) ^ (x >> 5)) & 0x1) << 0;
    pipe |= (((x >> 3) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    break;
  case kPipeConfigP8_32x32_16x16:
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    break;
  case kPipeConfigP16:
    pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
    pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
    pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
    pipe |= (((x >> 6) ^ (y >> 5)) & 0x1) << 3;
    break;
  default:
    std::abort();
  }
  return pipe;
}

constexpr uint32_t getBankIndex(std::uint32_t x, std::uint32_t y,
                                std::uint32_t bank_width,
                                std::uint32_t bank_height,
                                std::uint32_t num_banks,
                                std::uint32_t num_pipes) {
  std::uint32_t x_shift_offset = std::countr_zero(bank_width * num_pipes);
  std::uint32_t y_shift_offset = std::countr_zero(bank_height);
  std::uint32_t xs = x >> x_shift_offset;
  std::uint32_t ys = y >> y_shift_offset;
  std::uint32_t bank = 0;
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
    std::abort();
  }

  return bank;
}

constexpr std::uint32_t getPipeCount(PipeConfig pipeConfig) {
  switch (pipeConfig) {
  case kPipeConfigP8_32x32_8x16:
  case kPipeConfigP8_32x32_16x16:
    return 8;
  case kPipeConfigP16:
    return 16;
  default:
    std::abort();
  }
}

SurfaceInfo computeSurfaceInfo(TileMode tileMode, gnm::TextureType type,
                               gnm::DataFormat dfmt, std::uint32_t width,
                               std::uint32_t height, std::uint32_t depth,
                               std::uint32_t pitch, int baseArrayLayer,
                               int arrayCount, int baseMipLevel, int mipCount,
                               bool pow2pad);
SurfaceInfo computeSurfaceInfo(const gnm::TBuffer &tbuffer, TileMode tileMode);
} // namespace amdgpu
