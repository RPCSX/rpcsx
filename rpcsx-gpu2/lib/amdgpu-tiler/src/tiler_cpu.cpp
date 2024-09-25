#include "amdgpu/tiler_cpu.hpp"
#include "amdgpu/tiler.hpp"
#include "gnm/gnm.hpp"

constexpr std::uint64_t
getTiledOffset1D(gnm::TextureType texType, bool isPow2Padded,
                 gnm::DataFormat dfmt, amdgpu::TileMode tileMode, int mipLevel,
                 int arraySlice, int numFragments, int width, int height,
                 int depth, int pitch, int x, int y, int z) {

  using namespace amdgpu;
  bool isCubemap = texType == gnm::TextureType::Cube;
  bool isVolume = texType == gnm::TextureType::Dim3D;

  auto bitsPerFragment = getBitsPerElement(dfmt);
  uint32_t arraySliceCount = depth;

  if (isCubemap) {
    arraySliceCount *= 6;
  } else if (isVolume) {
    arraySliceCount = 1;
  }

  auto numFragmentsPerPixel = 1 << numFragments;
  auto isBlockCompressed = getTexelsPerElement(dfmt) > 1;
  auto arrayMode = tileMode.arrayMode();

  auto bitsPerElement = bitsPerFragment;
  auto paddedWidth = std::max((mipLevel != 0 ? pitch : width) >> mipLevel, 1);
  auto paddedHeight = std::max(height >> mipLevel, 1);

  auto tileThickness = (arrayMode == amdgpu::kArrayMode1dTiledThick) ? 4 : 1;

  if (isBlockCompressed) {
    switch (bitsPerFragment) {
    case 1:
      bitsPerElement *= 8;
      paddedWidth = std::max((paddedWidth + 7) / 8, 1);
      break;
    case 4:
    case 8:
      bitsPerElement *= 16;
      paddedWidth = std::max((paddedWidth + 3) / 4, 1);
      paddedHeight = std::max((paddedHeight + 3) / 4, 1);
      break;
    case 16:
      std::abort();
      break;

    default:
      std::abort();
      break;
    }
  }

  if (isPow2Padded) {
    arraySliceCount = std::bit_ceil(arraySliceCount);
    paddedWidth = std::bit_ceil(unsigned(paddedWidth));
    paddedHeight = std::bit_ceil(unsigned(paddedHeight));
  }

  uint64_t finalSurfaceOffset = 0;
  uint64_t finalSurfaceSize = 0;

  auto thickness = getMicroTileThickness(arrayMode);

  for (int i = 0; i <= mipLevel; i++) {
    finalSurfaceOffset += arraySliceCount * finalSurfaceSize;

    std::uint32_t elemWidth =
        std::max<std::uint64_t>((i > 0 ? pitch : width) >> i, 1);
    std::uint32_t elemHeight = std::max<std::uint64_t>(height >> i, 1);
    std::uint32_t elemDepth =
        std::max<std::uint64_t>((isVolume ? depth : 1) >> i, 1);

    if (isBlockCompressed) {
      switch (bitsPerFragment) {
      case 1:
        elemWidth = std::max<std::uint64_t>((elemWidth + 7) / 8, 1);
        break;
      case 4:
      case 8:
        elemWidth = std::max<std::uint64_t>((elemWidth + 3) / 4, 1);
        elemHeight = std::max<std::uint64_t>((elemHeight + 3) / 4, 1);
        break;
      case 16:
        std::abort();
        break;

      default:
        std::abort();
        break;
      }
    }

    if (isPow2Padded) {
      elemWidth = std::bit_ceil(elemWidth);
      elemHeight = std::bit_ceil(elemHeight);
      elemDepth = std::bit_ceil(elemDepth);
    }

    elemWidth = (elemWidth + kMicroTileWidth - 1) & ~(kMicroTileWidth - 1);
    elemHeight = (elemHeight + kMicroTileHeight - 1) & ~(kMicroTileHeight - 1);
    elemDepth = (elemDepth + thickness - 1) & ~(thickness - 1);

    std::uint32_t tempPitch = elemWidth;
    std::uint64_t logicalSliceSizeBytes = std::uint64_t(tempPitch) *
                                          elemHeight * bitsPerElement *
                                          numFragmentsPerPixel;
    logicalSliceSizeBytes = (logicalSliceSizeBytes + 7) / 8;

    uint64_t physicalSliceSizeBytes = logicalSliceSizeBytes * thickness;
    while ((physicalSliceSizeBytes % kPipeInterleaveBytes) != 0) {
      tempPitch += 8;
      logicalSliceSizeBytes = std::uint64_t(tempPitch) * elemHeight *
                              bitsPerElement * numFragmentsPerPixel;
      logicalSliceSizeBytes = (logicalSliceSizeBytes + 7) / 8;
      physicalSliceSizeBytes = logicalSliceSizeBytes * thickness;
    }

    finalSurfaceSize = logicalSliceSizeBytes * elemDepth;
  }

  finalSurfaceOffset += finalSurfaceSize * (uint64_t)arraySlice;

  auto tileBytes =
      (kMicroTileWidth * kMicroTileHeight * tileThickness * bitsPerElement +
       7) /
      8;
  auto tilesPerRow = paddedWidth / kMicroTileWidth;
  auto tilesPerSlice =
      std::max(tilesPerRow * (paddedHeight / kMicroTileHeight), 1U);

  uint64_t elementIndex = getElementIndex(x, y, z, bitsPerElement,
                                          tileMode.microTileMode(), arrayMode);

  uint64_t sliceOffset = (z / tileThickness) * tilesPerSlice * tileBytes;

  uint64_t tileRowIndex = y / kMicroTileHeight;
  uint64_t tileColumnIndex = x / kMicroTileWidth;
  uint64_t tileOffset =
      (tileRowIndex * tilesPerRow + tileColumnIndex) * tileBytes;

  uint64_t elementOffset = elementIndex * bitsPerElement;
  uint64_t finalOffset = (sliceOffset + tileOffset) * 8 + elementOffset;

  return finalOffset + finalSurfaceOffset * 8;
}

constexpr std::uint64_t getTiledOffsetLinear(gnm::DataFormat dfmt, int height,
                                             int pitch, int x, int y, int z) {
  auto bitsPerFragment = getBitsPerElement(dfmt);

  auto bitsPerElement = bitsPerFragment;
  auto paddedHeight = height;
  auto paddedWidth = pitch;

  if (bitsPerFragment == 1) {
    bitsPerElement *= 8;
    paddedWidth = std::max((paddedWidth + 7) / 8, 1);
  }

  uint64_t tiledRowSizeBits = bitsPerElement * paddedWidth;
  uint64_t tiledSliceBits = paddedWidth * paddedHeight * bitsPerElement;
  return tiledSliceBits * z + tiledRowSizeBits * y + bitsPerElement * x;
}

constexpr std::uint64_t
getTiledOffset2D(gnm::TextureType texType, bool isPow2Padded,
                 gnm::DataFormat dfmt, amdgpu::TileMode tileMode,
                 amdgpu::MacroTileMode macroTileMode, int mipLevel,
                 int arraySlice, int numFragments, int width, int height,
                 int depth, int pitch, int x, int y, int z, int fragmentIndex) {
  using namespace amdgpu;

  bool isCubemap = texType == gnm::TextureType::Cube;
  bool isVolume = texType == gnm::TextureType::Dim3D;
  auto m_bitsPerFragment = getBitsPerElement(dfmt);

  auto m_isBlockCompressed = getTexelsPerElement(dfmt) > 1;
  auto tileSwizzleMask = 0;
  auto numFragmentsPerPixel = 1 << numFragments;
  auto arrayMode = tileMode.arrayMode();

  auto tileThickness = 1;

  switch (arrayMode) {
  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayModeTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThinPrt:
  case amdgpu::kArrayMode3dTiledThinPrt:
    tileThickness = 1;
    break;
  case amdgpu::kArrayMode1dTiledThick:
  case amdgpu::kArrayMode2dTiledThick:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayModeTiledThickPrt:
  case amdgpu::kArrayMode2dTiledThickPrt:
  case amdgpu::kArrayMode3dTiledThickPrt:
    tileThickness = 4;
    break;
  case amdgpu::kArrayMode2dTiledXThick:
  case amdgpu::kArrayMode3dTiledXThick:
    tileThickness = 8;
    break;
  default:
    break;
  }

  auto bitsPerElement = m_bitsPerFragment;
  auto paddedWidth = pitch;
  auto paddedHeight = height;

  if (m_isBlockCompressed) {
    switch (m_bitsPerFragment) {
    case 1:
      bitsPerElement *= 8;
      paddedWidth = std::max((paddedWidth + 7) / 8, 1);
      break;
    case 4:
    case 8:
      bitsPerElement *= 16;
      paddedWidth = std::max((paddedWidth + 3) / 4, 1);
      paddedHeight = std::max((paddedHeight + 3) / 4, 1);
      break;
    case 16:
      std::abort();
      break;
    default:
      std::abort();
      break;
    }
  }

  auto bankWidthHW = macroTileMode.bankWidth();
  auto bankHeightHW = macroTileMode.bankHeight();
  auto macroAspectHW = macroTileMode.macroTileAspect();
  auto numBanksHW = macroTileMode.numBanks();

  auto bankWidth = 1 << bankWidthHW;
  auto bankHeight = 1 << bankHeightHW;
  unsigned numBanks = 2 << numBanksHW;
  auto macroTileAspect = 1 << macroAspectHW;

  uint32_t tileBytes1x =
      (tileThickness * bitsPerElement * kMicroTileWidth * kMicroTileHeight +
       7) /
      8;

  auto sampleSplitHw = tileMode.sampleSplit();
  auto tileSplitHw = tileMode.tileSplit();
  uint32_t sampleSplit = 1 << sampleSplitHw;
  uint32_t tileSplitC =
      (tileMode.microTileMode() == amdgpu::kMicroTileModeDepth)
          ? (64 << tileSplitHw)
          : std::max(256U, tileBytes1x * sampleSplit);

  auto tileSplitBytes = std::min(kDramRowSize, tileSplitC);

  auto numPipes = getPipeCount(tileMode.pipeConfig());
  auto pipeInterleaveBits = std::countr_zero(kPipeInterleaveBytes);
  auto pipeInterleaveMask = (1 << pipeInterleaveBits) - 1;
  auto pipeBits = std::countr_zero(numPipes);
  auto bankBits = std::countr_zero(numBanks);
  // auto pipeMask = (numPipes - 1) << pipeInterleaveBits;
  auto bankSwizzleMask = tileSwizzleMask;
  auto pipeSwizzleMask = 0;
  auto macroTileWidth =
      (kMicroTileWidth * bankWidth * numPipes) * macroTileAspect;
  auto macroTileHeight =
      (kMicroTileHeight * bankHeight * numBanks) / macroTileAspect;

  auto microTileMode = tileMode.microTileMode();

  uint64_t elementIndex =
      getElementIndex(x, y, z, bitsPerElement, microTileMode, arrayMode);

  uint32_t xh = x, yh = y;
  if (arrayMode == amdgpu::kArrayModeTiledThinPrt ||
      arrayMode == amdgpu::kArrayModeTiledThickPrt) {
    xh %= macroTileWidth;
    yh %= macroTileHeight;
  }
  uint64_t pipe = getPipeIndex(xh, yh, tileMode.pipeConfig());
  uint64_t bank =
      getBankIndex(xh, yh, bankWidth, bankHeight, numBanks, numPipes);

  uint32_t tileBytes = (kMicroTileWidth * kMicroTileHeight * tileThickness *
                            bitsPerElement * numFragmentsPerPixel +
                        7) /
                       8;

  uint64_t elementOffset = 0;
  if (microTileMode == amdgpu::kMicroTileModeDepth) {
    uint64_t pixelOffset = elementIndex * bitsPerElement * numFragmentsPerPixel;
    elementOffset = pixelOffset + (fragmentIndex * bitsPerElement);
  } else {
    uint64_t fragmentOffset =
        fragmentIndex * (tileBytes / numFragmentsPerPixel) * 8;
    elementOffset = fragmentOffset + (elementIndex * bitsPerElement);
  }

  uint64_t slicesPerTile = 1;
  uint64_t tileSplitSlice = 0;
  if (tileBytes > tileSplitBytes && tileThickness == 1) {
    slicesPerTile = tileBytes / tileSplitBytes;
    tileSplitSlice = elementOffset / (tileSplitBytes * 8);
    elementOffset %= (tileSplitBytes * 8);
    tileBytes = tileSplitBytes;
  }

  uint64_t macroTileBytes = (macroTileWidth / kMicroTileWidth) *
                            (macroTileHeight / kMicroTileHeight) * tileBytes /
                            (numPipes * numBanks);
  uint64_t macroTilesPerRow = paddedWidth / macroTileWidth;
  uint64_t macroTileRowIndex = y / macroTileHeight;
  uint64_t macroTileColumnIndex = x / macroTileWidth;
  uint64_t macroTileIndex =
      (macroTileRowIndex * macroTilesPerRow) + macroTileColumnIndex;
  uint64_t macro_tile_offset = macroTileIndex * macroTileBytes;
  uint64_t macroTilesPerSlice =
      macroTilesPerRow * (paddedHeight / macroTileHeight);
  uint64_t sliceBytes = macroTilesPerSlice * macroTileBytes;

  uint32_t slice = z;
  uint64_t sliceOffset =
      (tileSplitSlice + slicesPerTile * slice / tileThickness) * sliceBytes;
  if (arraySlice != 0) {
    slice = arraySlice;
  }

  uint64_t tileRowIndex = (y / kMicroTileHeight) % bankHeight;
  uint64_t tileColumnIndex = ((x / kMicroTileWidth) / numPipes) % bankWidth;
  uint64_t tileIndex = (tileRowIndex * bankWidth) + tileColumnIndex;
  uint64_t tileOffset = tileIndex * tileBytes;

  uint64_t bankSwizzle = bankSwizzleMask;
  uint64_t pipeSwizzle = pipeSwizzleMask;

  uint64_t pipeSliceRotation = 0;
  switch (arrayMode) {
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayMode3dTiledXThick:
    pipeSliceRotation =
        std::max(1UL, (numPipes / 2UL) - 1UL) * (slice / tileThickness);
    break;
  default:
    break;
  }
  pipeSwizzle += pipeSliceRotation;
  pipeSwizzle &= (numPipes - 1);
  pipe = pipe ^ pipeSwizzle;

  uint32_t sliceRotation = 0;
  switch (arrayMode) {
  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayMode2dTiledThick:
  case amdgpu::kArrayMode2dTiledXThick:
    sliceRotation = ((numBanks / 2) - 1) * (slice / tileThickness);
    break;
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayMode3dTiledXThick:
    sliceRotation = std::max(1UL, (numPipes / 2UL) - 1UL) *
                    (slice / tileThickness) / numPipes;
    break;
  default:
    break;
  }
  uint64_t tileSplitSliceRotation = 0;
  switch (arrayMode) {
  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode2dTiledThinPrt:
  case amdgpu::kArrayMode3dTiledThinPrt:
    tileSplitSliceRotation = ((numBanks / 2) + 1) * tileSplitSlice;
    break;
  default:
    break;
  }
  bank ^= bankSwizzle + sliceRotation;
  bank ^= tileSplitSliceRotation;
  bank &= (numBanks - 1);

  uint64_t totalOffset =
      (sliceOffset + macro_tile_offset + tileOffset) * 8 + elementOffset;
  uint64_t bitOffset = totalOffset & 0x7;
  totalOffset /= 8;

  uint64_t pipeInterleaveOffset = totalOffset & pipeInterleaveMask;
  uint64_t offset = totalOffset >> pipeInterleaveBits;

  uint64_t finalByteOffset =
      pipeInterleaveOffset | (pipe << (pipeInterleaveBits)) |
      (bank << (pipeInterleaveBits + pipeBits)) |
      (offset << (pipeInterleaveBits + pipeBits + bankBits));
  return (finalByteOffset << 3) | bitOffset;
}

std::uint64_t amdgpu::getTiledOffset(gnm::TextureType texType,
                                     bool isPow2Padded, int numFragments,
                                     gnm::DataFormat dfmt,
                                     amdgpu::TileMode tileMode,
                                     amdgpu::MacroTileMode macroTileMode,
                                     int mipLevel, int arraySlice, int width,
                                     int height, int depth, int pitch, int x,
                                     int y, int z, int fragmentIndex) {
  switch (tileMode.arrayMode()) {
  case amdgpu::kArrayModeLinearGeneral:
  case amdgpu::kArrayModeLinearAligned:
    return getTiledOffsetLinear(dfmt, height, pitch, x, y, z);

  case amdgpu::kArrayMode1dTiledThin:
  case amdgpu::kArrayMode1dTiledThick: {
    return getTiledOffset1D(texType, isPow2Padded, dfmt, tileMode, mipLevel,
                            arraySlice, numFragments, width, height, depth,
                            pitch, x, y, z);
  }

  case amdgpu::kArrayMode2dTiledThin:
  case amdgpu::kArrayMode2dTiledThick:
  case amdgpu::kArrayMode2dTiledXThick:
  case amdgpu::kArrayMode3dTiledThin:
  case amdgpu::kArrayMode3dTiledThick:
  case amdgpu::kArrayMode3dTiledXThick:
  case amdgpu::kArrayModeTiledThinPrt:
  case amdgpu::kArrayModeTiledThickPrt:
  case amdgpu::kArrayMode2dTiledThinPrt:
  case amdgpu::kArrayMode2dTiledThickPrt:
  case amdgpu::kArrayMode3dTiledThinPrt:
  case amdgpu::kArrayMode3dTiledThickPrt:
    return getTiledOffset2D(texType, isPow2Padded, dfmt, tileMode,
                            macroTileMode, mipLevel, arraySlice, numFragments,
                            width, height, depth, pitch, x, y, z,
                            fragmentIndex);
  }

  std::abort();
}
