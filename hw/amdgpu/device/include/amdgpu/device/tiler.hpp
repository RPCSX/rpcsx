#pragma once

#include "util/unreachable.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace amdgpu::device {
enum TileMode {
  kTileModeDepth_2dThin_64,
  kTileModeDepth_2dThin_128,
  kTileModeDepth_2dThin_256,
  kTileModeDepth_2dThin_512,
  kTileModeDepth_2dThin_1K,
  kTileModeDepth_1dThin,
  kTileModeDepth_2dThinPrt_256,
  kTileModeDepth_2dThinPrt_1K,

  kTileModeDisplay_LinearAligned,
  kTileModeDisplay_1dThin,
  kTileModeDisplay_2dThin,
  kTileModeDisplay_ThinPrt,
  kTileModeDisplay_2dThinPrt,

  kTileModeThin_1dThin,
  kTileModeThin_2dThin,
  kTileModeThin_3dThin,
  kTileModeThin_ThinPrt,
  kTileModeThin_2dThinPrt,
  kTileModeThin_3dThinPrt,

  kTileModeThick_1dThick,
  kTileModeThick_2dThick,
  kTileModeThick_3dThick,
  kTileModeThick_ThickPrt,
  kTileModeThick_2dThickPrt,
  kTileModeThick_3dThickPrt,
  kTileModeThick_2dXThick,
  kTileModeThick_3dXThick,
};

enum MacroTileMode {
  kMacroTileMode_1x4_16,
  kMacroTileMode_1x2_16,
  kMacroTileMode_1x1_16,
  kMacroTileMode_1x1_16_dup,
  kMacroTileMode_1x1_8,
  kMacroTileMode_1x1_4,
  kMacroTileMode_1x1_2,
  kMacroTileMode_1x1_2_dup,
  kMacroTileMode_1x8_16,
  kMacroTileMode_1x4_16_dup,
  kMacroTileMode_1x2_16_dup,
  kMacroTileMode_1x1_16_dup2,
  kMacroTileMode_1x1_8_dup,
  kMacroTileMode_1x1_4_dup,
  kMacroTileMode_1x1_2_dup2,
  kMacroTileMode_1x1_2_dup3,
};

inline constexpr auto kMicroTileWidth = 8;
inline constexpr auto kMicroTileHeight = 8;

inline uint64_t computeLinearElementByteOffset(
    uint32_t x, uint32_t y, uint32_t z, uint32_t fragmentIndex, uint32_t pitch,
    uint32_t slicePitchElems, uint32_t bitsPerElement,
    uint32_t numFragmentsPerPixel) {
  uint64_t absoluteElementIndex = z * slicePitchElems + y * pitch + x;
  return (absoluteElementIndex * bitsPerElement * numFragmentsPerPixel) +
         (bitsPerElement * fragmentIndex);
}

inline uint32_t getThinElementIndex(uint32_t x, uint32_t y) {
  uint32_t elem = 0;

  elem |= ((x >> 0) & 0x1) << 0;
  elem |= ((y >> 0) & 0x1) << 1;
  elem |= ((x >> 1) & 0x1) << 2;
  elem |= ((y >> 1) & 0x1) << 3;
  elem |= ((x >> 2) & 0x1) << 4;
  elem |= ((y >> 2) & 0x1) << 5;

  return elem;
}

inline uint32_t getDisplayElementIndex(uint32_t x, uint32_t y, uint32_t z,
                                       uint32_t bpp) {
  uint32_t elem = 0;
  switch (bpp) {
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

  return elem;
}
inline uint64_t computeThin1dThinTileElementOffset(std::uint32_t bpp,
                                                   uint32_t x, uint32_t y,
                                                   uint32_t z,
                                                   std::uint64_t height,
                                                   std::uint64_t pitch) {
  uint64_t elementIndex = getThinElementIndex(x, y);

  auto tileBytes = kMicroTileWidth * kMicroTileHeight * bpp;

  auto paddedWidth = pitch;

  auto tilesPerRow = paddedWidth / kMicroTileWidth;
  auto tilesPerSlice = std::max(tilesPerRow * (height / kMicroTileHeight), 1UL);

  uint64_t sliceOffset = z * tilesPerSlice * tileBytes;

  uint64_t tileRowIndex = y / kMicroTileHeight;
  uint64_t tileColumnIndex = x / kMicroTileWidth;
  uint64_t tileOffset =
      (tileRowIndex * tilesPerRow + tileColumnIndex) * tileBytes;

  return (sliceOffset + tileOffset) + elementIndex * bpp;
}

static constexpr auto kPipeInterleaveBytes = 256;

inline void getMacroTileData(MacroTileMode macroTileMode, uint32_t &bankWidth,
                             uint32_t &bankHeight, uint32_t &macroTileAspect,
                             uint32_t &numBanks) {
  switch (macroTileMode) {
  case kMacroTileMode_1x4_16:
    bankWidth = 1;
    bankHeight = 4;
    macroTileAspect = 4;
    numBanks = 16;
    break;
  case kMacroTileMode_1x2_16:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 2;
    numBanks = 16;
    break;
  case kMacroTileMode_1x1_16:
    bankWidth = 1;
    bankHeight = 2;
    macroTileAspect = 2;
    numBanks = 16;
    break;
  case kMacroTileMode_1x1_16_dup:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 2;
    numBanks = 16;
    break;
  case kMacroTileMode_1x1_8:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 8;
    break;
  case kMacroTileMode_1x1_4:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 4;
    break;
  case kMacroTileMode_1x1_2:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 2;
    break;
  case kMacroTileMode_1x1_2_dup:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 2;
    break;
  case kMacroTileMode_1x8_16:
    bankWidth = 1;
    bankHeight = 8;
    macroTileAspect = 4;
    numBanks = 16;
    break;
  case kMacroTileMode_1x4_16_dup:
    bankWidth = 1;
    bankHeight = 4;
    macroTileAspect = 4;
    numBanks = 16;
    break;
  case kMacroTileMode_1x2_16_dup:
    bankWidth = 1;
    bankHeight = 2;
    macroTileAspect = 2;
    numBanks = 16;
    break;
  case kMacroTileMode_1x1_16_dup2:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 2;
    numBanks = 16;
    break;
  case kMacroTileMode_1x1_8_dup:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 8;
    break;
  case kMacroTileMode_1x1_4_dup:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 4;
    break;
  case kMacroTileMode_1x1_2_dup2:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 2;
    break;
  case kMacroTileMode_1x1_2_dup3:
    bankWidth = 1;
    bankHeight = 1;
    macroTileAspect = 1;
    numBanks = 2;
    break;
  default:
    util::unreachable();
  }
}

static constexpr uint32_t log2(uint32_t i) { return 31 - __builtin_clz(i | 1); }

inline constexpr uint32_t kDramRowSize = 0x400;

inline constexpr uint32_t getPipeP8_32x32_8x16Index(uint32_t x, uint32_t y) {
  std::uint32_t pipe = 0;
  pipe |= (((x >> 4) ^ (y >> 3) ^ (x >> 5)) & 0x1) << 0;
  pipe |= (((x >> 3) ^ (y >> 4)) & 0x1) << 1;
  pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
  return pipe;
}

inline constexpr uint32_t getPipeP8_32x32_16x16Index(uint32_t x, uint32_t y) {
  std::uint32_t pipe = 0;
  pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
  pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
  pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
  return pipe;
}

inline constexpr uint32_t getPipeP16Index(uint32_t x, uint32_t y) {
  std::uint32_t pipe = 0;
  pipe |= (((x >> 3) ^ (y >> 3) ^ (x >> 4)) & 0x1) << 0;
  pipe |= (((x >> 4) ^ (y >> 4)) & 0x1) << 1;
  pipe |= (((x >> 5) ^ (y >> 5)) & 0x1) << 2;
  pipe |= (((x >> 6) ^ (y >> 5)) & 0x1) << 3;
  return pipe;
}

inline constexpr uint32_t getBankIndex(uint32_t x, uint32_t y,
                                       uint32_t bankWidth, uint32_t bankHeight,
                                       uint32_t numBanks, uint32_t numPipes) {
  const uint32_t xShiftOffset = log2(bankWidth * numPipes);
  const uint32_t yShiftOffset = log2(bankHeight);
  const uint32_t xs = x >> xShiftOffset;
  const uint32_t ys = y >> yShiftOffset;

  uint32_t bank = 0;
  switch (numBanks) {
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
    util::unreachable();
  }

  return bank;
}

inline uint64_t compute2dThinTileElementOffset(
    std::uint32_t bpp, MacroTileMode macroTileMode, uint64_t elementIndex,
    std::uint8_t tileSwizzleMask, std::uint32_t fragmentIndex,
    std::uint32_t arraySlice, uint32_t x, uint32_t y, uint32_t z,
    std::uint64_t height, std::uint64_t pitch) {
  // P8_32x32_8x16
  constexpr auto numPipes = 8;
  constexpr auto pipeInterleaveBytes = 256;

  std::uint32_t bankWidth;
  std::uint32_t bankHeight;
  std::uint32_t macroTileAspect;
  std::uint32_t numBanks;

  getMacroTileData(macroTileMode, bankWidth, bankHeight, macroTileAspect,
                   numBanks);

  uint32_t tileBytes1x = (bpp * kMicroTileWidth * kMicroTileHeight + 7) / 8;
  constexpr auto sampleSplit = 1 << 2;
  auto tileSplitC = std::max<std::uint32_t>(256, tileBytes1x * sampleSplit);
  auto tileSplitBytes = std::min(kDramRowSize, tileSplitC);
  std::uint32_t numFragmentsPerPixel = 1; // TODO

  constexpr auto pipeInterleaveBits = log2(pipeInterleaveBytes);
  constexpr auto pipeInterleaveMask = (1 << (pipeInterleaveBits)) - 1;
  constexpr auto pipeBits = log2(numPipes);
  auto bankBits = log2(numBanks);
  auto bankSwizzleMask = tileSwizzleMask;
  constexpr auto pipeSwizzleMask = 0;
  auto macroTileWidth =
      (kMicroTileWidth * bankWidth * numPipes) * macroTileAspect;
  auto macroTileHeight =
      (kMicroTileHeight * bankHeight * numBanks) / macroTileAspect;

  uint64_t pipe = getPipeP8_32x32_8x16Index(x, y);
  uint64_t bank = getBankIndex(x, y, bankWidth, bankHeight, numBanks, numPipes);

  uint32_t tileBytes =
      (kMicroTileWidth * kMicroTileHeight * bpp * numFragmentsPerPixel + 7) / 8;

  uint64_t fragmentOffset =
      fragmentIndex * (tileBytes / numFragmentsPerPixel) * 8;
  uint64_t elementOffset = fragmentOffset + (elementIndex * bpp);

  uint64_t slicesPerTile = 1;
  uint64_t tileSplitSlice = 0;
  if (tileBytes > tileSplitBytes) {
    slicesPerTile = tileBytes / tileSplitBytes;
    tileSplitSlice = elementOffset / (tileSplitBytes * 8);
    elementOffset %= (tileSplitBytes * 8);
    tileBytes = tileSplitBytes;
  }

  uint64_t macroTileBytes = (macroTileWidth / kMicroTileWidth) *
                            (macroTileHeight / kMicroTileHeight) * tileBytes /
                            (numPipes * numBanks);
  uint64_t macroTilesPerRow = pitch / macroTileWidth;
  uint64_t macroTileRowIndex = y / macroTileHeight;
  uint64_t macroTileColumnIndex = x / macroTileWidth;
  uint64_t macroTileIndex =
      (macroTileRowIndex * macroTilesPerRow) + macroTileColumnIndex;
  uint64_t macroTileOffset = macroTileIndex * macroTileBytes;
  uint64_t macroTilesPerSlice = macroTilesPerRow * (height / macroTileHeight);
  uint64_t sliceBytes = macroTilesPerSlice * macroTileBytes;
  uint32_t slice = z;
  uint64_t sliceOffset = (tileSplitSlice + slicesPerTile * slice) * sliceBytes;
  if (arraySlice != 0) {
    slice = arraySlice;
  }

  uint64_t tileRowIndex = (y / kMicroTileHeight) % bankHeight;
  uint64_t tileColumnIndex = ((x / kMicroTileWidth) / numPipes) % bankWidth;
  uint64_t tileIndex = (tileRowIndex * bankWidth) + tileColumnIndex;
  uint64_t tileOffset = tileIndex * tileBytes;

  uint64_t bankSwizzle = bankSwizzleMask;
  uint64_t pipeSwizzle = pipeSwizzleMask;

  uint64_t pipe_slice_rotation = 0;
  pipeSwizzle += pipe_slice_rotation;
  pipeSwizzle &= (numPipes - 1);
  pipe = pipe ^ pipeSwizzle;

  uint32_t sliceRotation = ((numBanks / 2) - 1) * slice;
  uint64_t tileSplitSliceRotation = ((numBanks / 2) + 1) * tileSplitSlice;

  bank ^= bankSwizzle + sliceRotation;
  bank ^= tileSplitSliceRotation;
  bank &= (numBanks - 1);

  uint64_t totalOffset =
      (sliceOffset + macroTileOffset + tileOffset) * 8 + elementOffset;
  uint64_t bitOffset = totalOffset & 0x7;
  totalOffset /= 8;

  uint64_t pipeInterleaveOffset = totalOffset & pipeInterleaveMask;
  uint64_t offset = totalOffset >> pipeInterleaveBits;

  uint64_t byteOffset = pipeInterleaveOffset | (pipe << (pipeInterleaveBits)) |
                        (bank << (pipeInterleaveBits + pipeBits)) |
                        (offset << (pipeInterleaveBits + pipeBits + bankBits));

  return (byteOffset << 3) | bitOffset;
}

inline uint64_t computeTiledElementByteOffset(
    TileMode tileMode, std::uint32_t bpp, uint32_t x, uint32_t y, uint32_t z,
    MacroTileMode macroTileMode, std::uint8_t tileSwizzleMask,
    std::uint32_t fragmentIndex, std::uint32_t mipLevel,
    std::uint32_t arraySlice, uint64_t width, std::uint64_t height,
    std::uint64_t depth, std::uint64_t pitch, std::uint64_t depthPitch) {
  switch (tileMode) {
  case kTileModeDepth_2dThin_64:
    util::unreachable();
  case kTileModeDepth_2dThin_128:
    util::unreachable();
  case kTileModeDepth_2dThin_256:
    util::unreachable();
  case kTileModeDepth_2dThin_512:
    util::unreachable();
  case kTileModeDepth_2dThin_1K:
    util::unreachable();
  case kTileModeDepth_1dThin:
    util::unreachable();
  case kTileModeDepth_2dThinPrt_256:
    util::unreachable();
  case kTileModeDepth_2dThinPrt_1K:
    util::unreachable();

  case kTileModeDisplay_LinearAligned:
    return x * y * z * ((bpp + 7) / 8);

  case kTileModeDisplay_1dThin:
    util::unreachable();
  case kTileModeDisplay_2dThin:
    return compute2dThinTileElementOffset(bpp, macroTileMode,
                                          getDisplayElementIndex(x, y, z, bpp),
                                          tileSwizzleMask, fragmentIndex,
                                          arraySlice, x, y, z, height, pitch) /
           8;
  case kTileModeDisplay_ThinPrt:
    util::unreachable();
  case kTileModeDisplay_2dThinPrt:
    util::unreachable();
  case kTileModeThin_1dThin:
    return computeThin1dThinTileElementOffset(((bpp + 7) / 8), x, y, z, height,
                                              pitch);
  case kTileModeThin_2dThin:
    return compute2dThinTileElementOffset(
               bpp, macroTileMode, getThinElementIndex(x, y), tileSwizzleMask,
               fragmentIndex, arraySlice, x, y, z, height, pitch) /
           8;
  case kTileModeThin_3dThin:
    util::unreachable();
  case kTileModeThin_ThinPrt:
    util::unreachable();
  case kTileModeThin_2dThinPrt:
    util::unreachable();
  case kTileModeThin_3dThinPrt:
    util::unreachable();
  case kTileModeThick_1dThick:
    util::unreachable();
  case kTileModeThick_2dThick:
    util::unreachable();
  case kTileModeThick_3dThick:
    util::unreachable();
  case kTileModeThick_ThickPrt:
    util::unreachable();
  case kTileModeThick_2dThickPrt:
    util::unreachable();
  case kTileModeThick_3dThickPrt:
    util::unreachable();
  case kTileModeThick_2dXThick:
    util::unreachable();
  case kTileModeThick_3dXThick:
    util::unreachable();
  }

  util::unreachable();
}
} // namespace amdgpu::device
