#include "gnm/constants.hpp"
#include <amdgpu/tiler.hpp>
#include <bit>
#include <gnm/gnm.hpp>

using namespace amdgpu;

static SurfaceInfo
computeTexture2dInfo(TileMode tileMode, gnm::TextureType type,
                     gnm::DataFormat dfmt, std::uint32_t width,
                     std::uint32_t height, std::uint32_t depth,
                     std::uint32_t pitch, int baseArrayLayer, int arrayCount,
                     int baseMipLevel, int mipCount, bool pow2pad) {
  bool isCubemap = type == gnm::TextureType::Cube;
  bool isVolume = type == gnm::TextureType::Dim3D;

  auto bitsPerFragment = getBitsPerElement(dfmt);
  std::uint32_t arraySliceCount = depth;

  if (isCubemap) {
    arraySliceCount *= 6;
  } else if (isVolume) {
    arraySliceCount = 1;
  }

  int numFragments = (type == gnm::TextureType::Msaa2D ||
                      type == gnm::TextureType::MsaaArray2D)
                         ? (baseArrayLayer + arrayCount - 1)
                         : 0;

  auto numFragmentsPerPixel = 1 << numFragments;
  auto isBlockCompressed = getTexelsPerElement(dfmt) > 1;

  std::uint32_t bitsPerElement = bitsPerFragment;
  depth = isVolume ? depth : 1;

  if (isBlockCompressed) {
    switch (bitsPerFragment) {
    case 1:
      bitsPerElement *= 8;
      break;
    case 4:
    case 8:
      bitsPerElement *= 16;
      break;
    case 16:
      std::abort();
      break;

    default:
      std::abort();
      break;
    }
  }

  if (pow2pad) {
    arraySliceCount = std::bit_ceil(arraySliceCount);
  }

  std::uint64_t surfaceOffset = 0;
  std::uint64_t surfaceSize = 0;
  std::uint64_t linearOffset = 0;

  auto macroTileMode = getDefaultMacroTileModes()[computeMacroTileIndex(
      tileMode, bitsPerElement, 1 << numFragments)];

  SurfaceInfo result;
  result.width = width;
  result.height = height;
  result.depth = depth;
  result.pitch = pitch;
  result.numFragments = numFragments;
  result.bitsPerElement = bitsPerElement;
  result.arrayLayerCount = arraySliceCount;
  result.macroTileMode = macroTileMode;

  auto arrayMode = tileMode.arrayMode();
  auto numPipes = getPipeCount(tileMode.pipeConfig());

  for (int mipLevel = 0; mipLevel < baseMipLevel + mipCount; mipLevel++) {
    std::uint32_t elemWidth = std::max<std::uint64_t>(width >> mipLevel, 1);
    std::uint32_t elemPitch = std::max<std::uint64_t>(pitch >> mipLevel, 1);
    std::uint32_t elemHeight = std::max<std::uint64_t>(height >> mipLevel, 1);
    std::uint32_t elemDepth = std::max<std::uint64_t>(depth >> mipLevel, 1);

    std::uint32_t linearPitch = elemPitch;
    std::uint32_t linearWidth = elemWidth;
    std::uint32_t linearHeight = elemHeight;
    std::uint32_t linearDepth = elemDepth;

    if (isBlockCompressed) {
      switch (bitsPerFragment) {
      case 1:
        linearWidth = std::max<std::uint64_t>((linearWidth + 7) / 8, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 7) / 8, 1);
        break;
      case 4:
      case 8:
        linearWidth = std::max<std::uint64_t>((linearWidth + 3) / 4, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 3) / 4, 1);
        linearHeight = std::max<std::uint64_t>((linearHeight + 3) / 4, 1);
        break;
      case 16:
        std::abort();
        break;

      default:
        std::abort();
        break;
      }
    }

    if (pow2pad) {
      linearPitch = std::bit_ceil(linearPitch);
      linearWidth = std::bit_ceil(linearWidth);
      linearHeight = std::bit_ceil(linearHeight);
      linearDepth = std::bit_ceil(linearDepth);
    }

    if (mipLevel > 0 && pitch > 0) {
      linearPitch = linearWidth;
    }

    auto thickness = getMicroTileThickness(arrayMode);

    uint32_t numBanks = 2 << macroTileMode.numBanks();
    uint32_t macroAspect = 1 << macroTileMode.macroTileAspect();
    uint32_t tileBytes1x =
        (thickness * bitsPerElement * kMicroTileWidth * kMicroTileHeight + 7) /
        8;
    auto microTileMode = tileMode.microTileMode();
    uint32_t tileSplit =
        (microTileMode == kMicroTileModeDepth)
            ? (64 << tileMode.sampleSplit())
            : std::max(256U, (1 << tileMode.sampleSplit()) * tileBytes1x);
    uint32_t tileSplitC = std::min(kDramRowSize, tileSplit);
    uint32_t bankWidth = 1 << macroTileMode.bankWidth();
    uint32_t bankHeight = 1 << macroTileMode.bankHeight();

    uint32_t tileSize = std::min(
        tileSplitC, (thickness * bitsPerElement * numFragmentsPerPixel *
                         kMicroTileWidth * kMicroTileHeight +
                     7) /
                        8);
    uint32_t bankHeightAlign =
        std::max(1U, kPipeInterleaveBytes / (tileSize * bankWidth));

    bankHeight = (bankHeight + bankHeightAlign - 1) & ~(bankHeightAlign - 1);

    if (numFragmentsPerPixel == 1) {
      uint32_t macroAspectAlign = std::max(
          1U, kPipeInterleaveBytes / (tileSize * numPipes * bankWidth));
      macroAspect =
          (macroAspect + macroAspectAlign - 1) & ~(macroAspectAlign - 1);
    }

    auto depthAlign = thickness;

    // FIXME: rotate tile mode for mipLevel > 0

    uint32_t outPitch = linearPitch;
    uint32_t outHeight = linearHeight;
    uint32_t outDepth = linearDepth;

    uint32_t macroTileWidth =
        kMicroTileWidth * bankWidth * numPipes * macroAspect;
    uint32_t macroTileHeight =
        kMicroTileHeight * bankHeight * numBanks / macroAspect;

    uint32_t heightAlign = macroTileHeight;
    auto pitchAlign = macroTileWidth;

    outPitch = (outPitch + pitchAlign - 1) & ~(pitchAlign - 1);
    outDepth = (outDepth + depthAlign - 1) & ~(depthAlign - 1);
    outHeight = (outHeight + heightAlign - 1) & ~(heightAlign - 1);

    std::uint64_t logicalSliceSizeBytes = std::uint64_t(outPitch) * outHeight *
                                          bitsPerElement * numFragmentsPerPixel;
    logicalSliceSizeBytes = (logicalSliceSizeBytes + 7) / 8;

    surfaceSize = static_cast<uint64_t>(outPitch) * outHeight *
                  std::bit_ceil(bitsPerElement) * numFragmentsPerPixel;
    surfaceSize = (surfaceSize + 7) / 8;

    auto linearSize = uint64_t(linearPitch) * linearHeight * bitsPerElement *
                      numFragmentsPerPixel;
    linearSize = linearDepth * ((linearSize + 7) / 8);

    result.setSubresourceInfo(mipLevel, {
                                            .tiledWidth = outPitch,
                                            .tiledHeight = outHeight,
                                            .tiledDepth = outDepth,
                                            .tiledOffset = surfaceOffset,
                                            .tiledSize = surfaceSize,
                                            .linearPitch = linearPitch,
                                            .linearWidth = linearWidth,
                                            .linearHeight = linearHeight,
                                            .linearDepth = linearDepth,
                                            .linearOffset = linearOffset,
                                            .linearSize = linearSize,
                                        });

    linearOffset += arraySliceCount * linearSize;
    surfaceOffset += arraySliceCount * surfaceSize;
  }

  result.totalTiledSize = surfaceOffset;
  result.totalLinearSize = linearOffset;
  return result;
}

static SurfaceInfo
computeTexture1dInfo(ArrayMode arrayMode, gnm::TextureType type,
                     gnm::DataFormat dfmt, std::uint32_t width,
                     std::uint32_t height, std::uint32_t depth,
                     std::uint32_t pitch, int baseArrayLayer, int arrayCount,
                     int baseMipLevel, int mipCount, bool pow2pad) {
  bool isCubemap = type == gnm::TextureType::Cube;
  bool isVolume = type == gnm::TextureType::Dim3D;

  auto bitsPerFragment = getBitsPerElement(dfmt);
  std::uint32_t arraySliceCount = depth;

  if (isCubemap) {
    arraySliceCount *= 6;
  } else if (isVolume) {
    arraySliceCount = 1;
  }

  int numFragments = (type == gnm::TextureType::Msaa2D ||
                      type == gnm::TextureType::MsaaArray2D)
                         ? (baseArrayLayer + arrayCount - 1)
                         : 0;

  auto numFragmentsPerPixel = 1 << numFragments;
  auto isBlockCompressed = getTexelsPerElement(dfmt) > 1;

  auto bitsPerElement = bitsPerFragment;
  depth = isVolume ? depth : 1;

  if (isBlockCompressed) {
    switch (bitsPerFragment) {
    case 1:
      bitsPerElement *= 8;
      break;
    case 4:
    case 8:
      bitsPerElement *= 16;
      break;
    case 16:
      std::abort();
      break;

    default:
      std::abort();
      break;
    }
  }

  if (pow2pad) {
    arraySliceCount = std::bit_ceil(arraySliceCount);
  }

  std::uint64_t surfaceOffset = 0;
  std::uint64_t surfaceSize = 0;
  std::uint64_t linearOffset = 0;

  SurfaceInfo result;
  result.width = width;
  result.height = height;
  result.depth = depth;
  result.pitch = pitch;
  result.numFragments = numFragments;
  result.bitsPerElement = bitsPerElement;
  result.arrayLayerCount = arraySliceCount;

  auto thickness = getMicroTileThickness(arrayMode);

  for (int mipLevel = 0; mipLevel < baseMipLevel + mipCount; mipLevel++) {
    std::uint32_t elemWidth = std::max<std::uint64_t>(width >> mipLevel, 1);
    std::uint32_t elemPitch = std::max<std::uint64_t>(pitch >> mipLevel, 1);
    std::uint32_t elemHeight = std::max<std::uint64_t>(height >> mipLevel, 1);
    std::uint32_t elemDepth = std::max<std::uint64_t>(depth >> mipLevel, 1);

    std::uint32_t linearPitch = elemPitch;
    std::uint32_t linearWidth = elemWidth;
    std::uint32_t linearHeight = elemHeight;
    std::uint32_t linearDepth = elemDepth;

    if (isBlockCompressed) {
      switch (bitsPerFragment) {
      case 1:
        linearWidth = std::max<std::uint64_t>((linearWidth + 7) / 8, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 7) / 8, 1);
        break;
      case 4:
      case 8:
        linearWidth = std::max<std::uint64_t>((linearWidth + 3) / 4, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 3) / 4, 1);
        linearHeight = std::max<std::uint64_t>((linearHeight + 3) / 4, 1);
        break;
      case 16:
        std::abort();
        break;

      default:
        std::abort();
        break;
      }
    }

    if (pow2pad) {
      linearPitch = std::bit_ceil(linearPitch);
      linearWidth = std::bit_ceil(linearWidth);
      linearHeight = std::bit_ceil(linearHeight);
      linearDepth = std::bit_ceil(linearDepth);
    }

    if (mipLevel > 0 && pitch > 0) {
      linearPitch = linearWidth;
    }

    std::uint32_t paddedPitch =
        (linearPitch + kMicroTileWidth - 1) & ~(kMicroTileWidth - 1);
    std::uint32_t paddedHeight =
        (linearHeight + kMicroTileHeight - 1) & ~(kMicroTileHeight - 1);
    std::uint32_t paddedDepth = linearDepth;

    if (!isCubemap || (mipLevel > 0 && linearDepth > 1)) {
      if (isCubemap) {
        linearDepth = std::bit_ceil(linearDepth);
      }

      paddedDepth = (linearDepth + thickness - 1) & ~(thickness - 1);
    }

    std::uint32_t tempPitch = paddedPitch;
    std::uint64_t logicalSliceSizeBytes = std::uint64_t(tempPitch) *
                                          paddedHeight * bitsPerElement *
                                          numFragmentsPerPixel;
    logicalSliceSizeBytes = (logicalSliceSizeBytes + 7) / 8;

    uint64_t physicalSliceSizeBytes = logicalSliceSizeBytes * thickness;
    while ((physicalSliceSizeBytes % kPipeInterleaveBytes) != 0) {
      tempPitch += kMicroTileWidth;
      logicalSliceSizeBytes = std::uint64_t(tempPitch) * paddedHeight *
                              bitsPerElement * numFragmentsPerPixel;
      logicalSliceSizeBytes = (logicalSliceSizeBytes + 7) / 8;
      physicalSliceSizeBytes = logicalSliceSizeBytes * thickness;
    }

    surfaceSize = logicalSliceSizeBytes * paddedDepth;
    auto linearSize = uint64_t(linearPitch) * linearHeight * bitsPerElement *
                      numFragmentsPerPixel;
    linearSize = linearDepth * ((linearSize + 7) / 8);

    result.setSubresourceInfo(mipLevel, {
                                            .tiledWidth = linearPitch,
                                            .tiledHeight = linearHeight,
                                            .tiledDepth = linearDepth,
                                            .tiledOffset = surfaceOffset,
                                            .tiledSize = surfaceSize,
                                            .linearPitch = linearPitch,
                                            .linearWidth = linearWidth,
                                            .linearHeight = linearHeight,
                                            .linearDepth = linearDepth,
                                            .linearOffset = linearOffset,
                                            .linearSize = linearSize,
                                        });

    surfaceOffset += arraySliceCount * surfaceSize;
    linearOffset += arraySliceCount * linearSize;
  }

  result.totalTiledSize = surfaceOffset;
  result.totalLinearSize = linearOffset;
  return result;
}

static SurfaceInfo computeTextureLinearInfo(
    ArrayMode arrayMode, gnm::TextureType type, gnm::DataFormat dfmt,
    std::uint32_t width, std::uint32_t height, std::uint32_t depth,
    std::uint32_t pitch, int baseArrayLayer, int arrayCount, int baseMipLevel,
    int mipCount, bool pow2pad) {
  bool isCubemap = type == gnm::TextureType::Cube;
  bool isVolume = type == gnm::TextureType::Dim3D;

  auto bitsPerFragment = getBitsPerElement(dfmt);
  std::uint32_t arraySliceCount = depth;

  if (isCubemap) {
    arraySliceCount *= 6;
  } else if (isVolume) {
    arraySliceCount = 1;
  }

  int numFragments = (type == gnm::TextureType::Msaa2D ||
                      type == gnm::TextureType::MsaaArray2D)
                         ? (baseArrayLayer + arrayCount - 1)
                         : 0;

  auto numFragmentsPerPixel = 1 << numFragments;
  auto isBlockCompressed = getTexelsPerElement(dfmt) > 1;

  auto bitsPerElement = bitsPerFragment;
  depth = isVolume ? depth : 1;

  if (isBlockCompressed) {
    switch (bitsPerFragment) {
    case 1:
      bitsPerElement *= 8;
      break;
    case 4:
    case 8:
      bitsPerElement *= 16;
      break;
    case 16:
      std::abort();
      break;

    default:
      std::abort();
      break;
    }
  }

  if (pow2pad) {
    arraySliceCount = std::bit_ceil(arraySliceCount);
  }

  std::uint64_t surfaceOffset = 0;
  std::uint64_t surfaceSize = 0;
  std::uint64_t linearOffset = 0;

  SurfaceInfo result;
  result.width = width;
  result.height = height;
  result.depth = depth;
  result.pitch = pitch;
  result.numFragments = numFragments;
  result.bitsPerElement = bitsPerElement;
  result.arrayLayerCount = arraySliceCount;

  for (int mipLevel = 0; mipLevel < baseMipLevel + mipCount; mipLevel++) {
    std::uint32_t elemWidth = std::max<std::uint64_t>(width >> mipLevel, 1);
    std::uint32_t elemPitch = std::max<std::uint64_t>(pitch >> mipLevel, 1);
    std::uint32_t elemHeight = std::max<std::uint64_t>(height >> mipLevel, 1);
    std::uint32_t elemDepth = std::max<std::uint64_t>(depth >> mipLevel, 1);

    std::uint32_t linearPitch = elemPitch;
    std::uint32_t linearWidth = elemWidth;
    std::uint32_t linearHeight = elemHeight;
    std::uint32_t linearDepth = elemDepth;

    if (isBlockCompressed) {
      switch (bitsPerFragment) {
      case 1:
        linearWidth = std::max<std::uint64_t>((linearWidth + 7) / 8, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 7) / 8, 1);
        break;
      case 4:
      case 8:
        linearWidth = std::max<std::uint64_t>((linearWidth + 3) / 4, 1);
        linearPitch = std::max<std::uint64_t>((linearPitch + 3) / 4, 1);
        linearHeight = std::max<std::uint64_t>((linearHeight + 3) / 4, 1);
        break;
      case 16:
        std::abort();
        break;

      default:
        std::abort();
        break;
      }
    }

    if (pow2pad) {
      linearPitch = std::bit_ceil(linearPitch);
      linearWidth = std::bit_ceil(linearWidth);
      linearHeight = std::bit_ceil(linearHeight);
      linearDepth = std::bit_ceil(linearDepth);
    }

    if (mipLevel > 0 && pitch > 0) {
      linearPitch = linearWidth;
    }

    auto linearSize = static_cast<uint64_t>(linearPitch) *
                      (linearHeight)*bitsPerElement * numFragmentsPerPixel;

    linearSize = linearDepth * ((linearSize + 7) / 8);

    if (arrayMode == kArrayModeLinearGeneral) {
      surfaceSize = linearSize;
      result.setSubresourceInfo(mipLevel, {
                                              .tiledWidth = linearPitch,
                                              .tiledHeight = linearHeight,
                                              .tiledDepth = linearDepth,
                                              .tiledOffset = surfaceOffset,
                                              .tiledSize = surfaceSize,
                                              .linearPitch = linearPitch,
                                              .linearWidth = linearWidth,
                                              .linearHeight = linearHeight,
                                              .linearDepth = linearDepth,
                                              .linearOffset = linearOffset,
                                              .linearSize = linearSize,
                                          });
    } else {
      if (mipLevel > 0 && pitch > 0) {
        linearPitch = linearWidth;
      }

      auto pitchAlign = std::max(8UL, 64UL / ((bitsPerElement + 7) / 8UL));
      std::uint32_t paddedPitch =
          (linearPitch + pitchAlign - 1) & ~(pitchAlign - 1);
      std::uint32_t paddedHeight = linearHeight;
      std::uint32_t paddedDepth = linearDepth;

      if (!isCubemap || (mipLevel > 0 && linearDepth > 1)) {
        if (isCubemap) {
          linearDepth = std::bit_ceil(linearDepth);
        }

        auto thickness = getMicroTileThickness(arrayMode);
        paddedDepth = (linearDepth + thickness - 1) & ~(thickness - 1);
      }

      std::uint32_t pixelsPerPipeInterleave =
          kPipeInterleaveBytes / ((bitsPerElement + 7) / 8);
      std::uint32_t sliceAlignInPixel =
          pixelsPerPipeInterleave < 64 ? 64 : pixelsPerPipeInterleave;
      auto pixelsPerSlice = static_cast<uint64_t>(paddedPitch) * paddedHeight *
                            numFragmentsPerPixel;
      while (pixelsPerSlice % sliceAlignInPixel) {
        paddedPitch += pitchAlign;
        pixelsPerSlice = static_cast<uint64_t>(paddedPitch) * paddedHeight *
                         numFragmentsPerPixel;
      }

      surfaceSize = (pixelsPerSlice * bitsPerElement + 7) / 8 * paddedDepth;

      result.setSubresourceInfo(mipLevel, {
                                              .tiledWidth = paddedPitch,
                                              .tiledHeight = paddedHeight,
                                              .tiledDepth = paddedDepth,
                                              .tiledOffset = surfaceOffset,
                                              .tiledSize = surfaceSize,
                                              .linearPitch = linearPitch,
                                              .linearWidth = linearWidth,
                                              .linearHeight = linearHeight,
                                              .linearDepth = linearDepth,
                                              .linearOffset = linearOffset,
                                              .linearSize = linearSize,
                                          });
    }

    surfaceOffset += arraySliceCount * surfaceSize;
    surfaceOffset += arraySliceCount * linearSize;
  }

  result.totalTiledSize = surfaceOffset;
  result.totalLinearSize = linearOffset;
  return result;
}

SurfaceInfo amdgpu::computeSurfaceInfo(
    TileMode tileMode, gnm::TextureType type, gnm::DataFormat dfmt,
    std::uint32_t width, std::uint32_t height, std::uint32_t depth,
    std::uint32_t pitch, int baseArrayLayer, int arrayCount, int baseMipLevel,
    int mipCount, bool pow2pad) {
  switch (tileMode.arrayMode()) {
  case kArrayModeLinearGeneral:
  case kArrayModeLinearAligned:
    return computeTextureLinearInfo(
        tileMode.arrayMode(), type, dfmt, width, height, depth, pitch,
        baseArrayLayer, arrayCount, baseMipLevel, mipCount, pow2pad);

  case kArrayMode1dTiledThin:
  case kArrayMode1dTiledThick:
    return computeTexture1dInfo(tileMode.arrayMode(), type, dfmt, width, height,
                                depth, pitch, baseArrayLayer, arrayCount,
                                baseMipLevel, mipCount, pow2pad);

  case kArrayMode2dTiledThin:
  case kArrayMode2dTiledThick:
  case kArrayMode2dTiledXThick:
  case kArrayMode3dTiledThin:
  case kArrayMode3dTiledThick:
  case kArrayMode3dTiledXThick:
  case kArrayModeTiledThinPrt:
  case kArrayModeTiledThickPrt:
  case kArrayMode2dTiledThinPrt:
  case kArrayMode2dTiledThickPrt:
  case kArrayMode3dTiledThinPrt:
  case kArrayMode3dTiledThickPrt:
    return computeTexture2dInfo(tileMode, type, dfmt, width, height, depth,
                                pitch, baseArrayLayer, arrayCount, baseMipLevel,
                                mipCount, pow2pad);
  }

  std::abort();
}
