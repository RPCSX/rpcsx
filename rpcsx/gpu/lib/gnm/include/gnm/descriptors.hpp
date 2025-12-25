#pragma once

#include "constants.hpp"
#include <compare>
#include <cstdint>
#include <rx/bits.hpp>

namespace gnm {
struct VBuffer {
  std::uint32_t words[4];

  [[nodiscard]] constexpr std::uint64_t getAddress() const {
    return words[0] |
           (static_cast<std::uint64_t>(rx::getBits(words[1], 11, 0)) << 32);
  }

  constexpr void setAddress(std::uint64_t address) {
    words[0] = static_cast<std::uint32_t>(address);
    words[1] = rx::setBits(words[1], 11, 0, static_cast<std::uint32_t>(address >> 32));
  }

  [[nodiscard]] constexpr std::uint8_t getMTypeL1() const {
    return static_cast<std::uint8_t>(rx::getBits(words[1], 13, 12));
  }

  constexpr void setMTypeL1(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 13, 12, value);
  }

  [[nodiscard]] constexpr std::uint8_t getMTypeL2() const {
    return static_cast<std::uint8_t>(rx::getBits(words[1], 15, 14));
  }

  constexpr void setMTypeL2(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 15, 14, value);
  }

  [[nodiscard]] constexpr std::uint16_t getStride() const {
    return static_cast<std::uint16_t>(rx::getBits(words[1], 29, 16));
  }

  constexpr void setStride(std::uint16_t value) {
    words[1] = rx::setBits(words[1], 29, 16, value);
  }

  [[nodiscard]] constexpr bool isCacheSwizzle() const {
    return rx::getBit(words[1], 30);
  }

  constexpr void setCacheSwizzle(bool value) {
    words[1] = rx::setBit(words[1], 30, value);
  }

  [[nodiscard]] constexpr bool isSwizzleEn() const {
    return rx::getBit(words[1], 31);
  }

  constexpr void setSwizzleEn(bool value) {
    words[1] = rx::setBit(words[1], 31, value);
  }

  [[nodiscard]] constexpr std::uint32_t getNumRecords() const {
    return words[2];
  }

  constexpr void setNumRecords(std::uint32_t value) {
    words[2] = value;
  }

  [[nodiscard]] constexpr Swizzle getDstSelX() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 2, 0));
  }

  constexpr void setDstSelX(Swizzle value) {
    words[3] = rx::setBits(words[3], 2, 0, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelY() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 5, 3));
  }

  constexpr void setDstSelY(Swizzle value) {
    words[3] = rx::setBits(words[3], 5, 3, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelZ() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 8, 6));
  }

  constexpr void setDstSelZ(Swizzle value) {
    words[3] = rx::setBits(words[3], 8, 6, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelW() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 11, 9));
  }

  constexpr void setDstSelW(Swizzle value) {
    words[3] = rx::setBits(words[3], 11, 9, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr NumericFormat getNumericFormat() const {
    return static_cast<NumericFormat>(rx::getBits(words[3], 14, 12));
  }

  constexpr void setNumericFormat(NumericFormat value) {
    words[3] = rx::setBits(words[3], 14, 12, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr DataFormat getDataFormat() const {
    return static_cast<DataFormat>(rx::getBits(words[3], 18, 15));
  }

  constexpr void setDataFormat(DataFormat value) {
    words[3] = rx::setBits(words[3], 18, 15, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint32_t getElementSize() const {
    return rx::getBits(words[3], 20, 19);
  }

  constexpr void setElementSize(std::uint32_t value) {
    words[3] = rx::setBits(words[3], 20, 19, value);
  }

  [[nodiscard]] constexpr std::uint32_t getIndexStride() const {
    return rx::getBits(words[3], 22, 21);
  }

  constexpr void setIndexStride(std::uint32_t value) {
    words[3] = rx::setBits(words[3], 22, 21, value);
  }

  [[nodiscard]] constexpr bool isAddTidEn() const {
    return rx::getBit(words[3], 23);
  }

  constexpr void setAddTidEn(bool value) {
    words[3] = rx::setBit(words[3], 23, value);
  }

  [[nodiscard]] constexpr bool isHashEn() const {
    return rx::getBit(words[3], 25);
  }

  constexpr void setHashEn(bool value) {
    words[3] = rx::setBit(words[3], 25, value);
  }

  [[nodiscard]] constexpr std::uint8_t getMType() const {
    return rx::getBits(words[3], 29, 27);
  }

  constexpr void setMType(std::uint8_t value) {
    words[3] = rx::setBits(words[3], 29, 27, value);
  }

  [[nodiscard]] constexpr std::uint8_t getType() const {
    return rx::getBits(words[3], 31, 30);
  }

  constexpr void setType(std::uint8_t value) {
    words[3] = rx::setBits(words[3], 31, 30, value);
  }

  [[nodiscard]] constexpr std::uint64_t size() const {
    std::uint64_t result = getNumRecords();

    if (auto stride = getStride()) {
      result *= stride;
    }

    return result;
  }

  auto operator<=>(const VBuffer &) const = default;
};

struct TBuffer {
  std::uint32_t words[8];

  [[nodiscard]] constexpr std::uint64_t getAddress() const {
    return (words[0] |
            (static_cast<std::uint64_t>(rx::getBits(words[1], 5, 0)) << 32))
           << 8;
  }

  constexpr void setAddress(std::uint64_t address) {
    address >>= 8;
    words[0] = static_cast<std::uint32_t>(address);
    words[1] = rx::setBits(words[1], 5, 0, static_cast<std::uint32_t>(address >> 32));
  }

  [[nodiscard]] constexpr std::uint8_t getMTypeL2() const {
    return static_cast<std::uint8_t>(rx::getBits(words[1], 7, 6));
  }

  constexpr void setMTypeL2(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 7, 6, value);
  }

  [[nodiscard]] constexpr std::uint16_t getMinLod() const {
    return static_cast<std::uint16_t>(rx::getBits(words[1], 19, 8));
  }

  constexpr void setMinLod(std::uint16_t value) {
    words[1] = rx::setBits(words[1], 19, 8, value);
  }

  [[nodiscard]] constexpr DataFormat getDataFormat() const {
    return static_cast<DataFormat>(rx::getBits(words[1], 25, 20));
  }

  constexpr void setDataFormat(DataFormat value) {
    words[1] = rx::setBits(words[1], 25, 20, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr NumericFormat getNumericFormat() const {
    return static_cast<NumericFormat>(rx::getBits(words[1], 29, 26));
  }

  constexpr void setNumericFormat(NumericFormat value) {
    words[1] = rx::setBits(words[1], 29, 26, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint8_t getMType() const {
    return rx::getBits(words[1], 31, 30) | (rx::getBits(words[2], 27, 26) << 2);
  }

  constexpr void setMType(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 31, 30, value & 0x3);
    words[2] = rx::setBits(words[2], 27, 26, (value >> 2) & 0x3);
  }

  [[nodiscard]] constexpr std::uint16_t getWidth() const {
    return rx::getBits(words[2], 13, 0) + 1;
  }

  constexpr void setWidth(std::uint16_t value) {
    words[2] = rx::setBits(words[2], 13, 0, value - 1);
  }

  [[nodiscard]] constexpr std::uint16_t getHeight() const {
    return rx::getBits(words[2], 27, 14) + 1;
  }

  constexpr void setHeight(std::uint16_t value) {
    words[2] = rx::setBits(words[2], 27, 14, value - 1);
  }

  [[nodiscard]] constexpr std::uint16_t getPerfMod() const {
    return rx::getBits(words[2], 30, 28);
  }

  constexpr void setPerfMod(std::uint16_t value) {
    words[2] = rx::setBits(words[2], 30, 28, value);
  }

  [[nodiscard]] constexpr bool isInterlaced() const {
    return rx::getBit(words[2], 31);
  }

  constexpr void setInterlaced(bool value) {
    words[2] = rx::setBit(words[2], 31, value);
  }

  [[nodiscard]] constexpr Swizzle getDstSelX() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 2, 0));
  }

  constexpr void setDstSelX(Swizzle value) {
    words[3] = rx::setBits(words[3], 2, 0, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelY() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 5, 3));
  }

  constexpr void setDstSelY(Swizzle value) {
    words[3] = rx::setBits(words[3], 5, 3, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelZ() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 8, 6));
  }

  constexpr void setDstSelZ(Swizzle value) {
    words[3] = rx::setBits(words[3], 8, 6, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Swizzle getDstSelW() const {
    return static_cast<Swizzle>(rx::getBits(words[3], 11, 9));
  }

  constexpr void setDstSelW(Swizzle value) {
    words[3] = rx::setBits(words[3], 11, 9, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint32_t getBaseLevel() const {
    return rx::getBits(words[3], 15, 12);
  }

  constexpr void setBaseLevel(std::uint32_t value) {
    words[3] = rx::setBits(words[3], 15, 12, value);
  }

  [[nodiscard]] constexpr std::uint32_t getLastLevel() const {
    return rx::getBits(words[3], 19, 16);
  }

  constexpr void setLastLevel(std::uint32_t value) {
    words[3] = rx::setBits(words[3], 19, 16, value);
  }

  [[nodiscard]] constexpr std::uint32_t getTilingIndex() const {
    return rx::getBits(words[3], 24, 20);
  }

  constexpr void setTilingIndex(std::uint32_t value) {
    words[3] = rx::setBits(words[3], 24, 20, value);
  }

  [[nodiscard]] constexpr bool isPow2Pad() const {
    return rx::getBit(words[3], 25);
  }

  constexpr void setPow2Pad(bool value) {
    words[3] = rx::setBit(words[3], 25, value);
  }

  [[nodiscard]] constexpr TextureType getType() const {
    return static_cast<TextureType>(rx::getBits(words[3], 31, 28));
  }

  constexpr void setType(TextureType value) {
    words[3] = rx::setBits(words[3], 31, 28, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint16_t getDepth() const {
    return rx::getBits(words[4], 12, 0) + 1;
  }

  constexpr void setDepth(std::uint16_t value) {
    words[4] = rx::setBits(words[4], 12, 0, value - 1);
  }

  [[nodiscard]] constexpr std::uint16_t getPitch() const {
    return rx::getBits(words[4], 26, 13) + 1;
  }

  constexpr void setPitch(std::uint16_t value) {
    words[4] = rx::setBits(words[4], 26, 13, value - 1);
  }

  [[nodiscard]] constexpr std::uint16_t getBaseArray() const {
    return rx::getBits(words[5], 12, 0);
  }

  constexpr void setBaseArray(std::uint16_t value) {
    words[5] = rx::setBits(words[5], 12, 0, value);
  }

  [[nodiscard]] constexpr std::uint16_t getLastArray() const {
    return rx::getBits(words[5], 25, 13);
  }

  constexpr void setLastArray(std::uint16_t value) {
    words[5] = rx::setBits(words[5], 25, 13, value);
  }

  [[nodiscard]] constexpr std::uint16_t getMinLodWarn() const {
    return rx::getBits(words[6], 11, 0);
  }

  constexpr void setMinLodWarn(std::uint16_t value) {
    words[6] = rx::setBits(words[6], 11, 0, value);
  }

  [[nodiscard]] constexpr std::uint16_t getCounterBankId() const {
    return rx::getBits(words[6], 19, 12);
  }

  constexpr void setCounterBankId(std::uint16_t value) {
    words[6] = rx::setBits(words[6], 19, 12, value);
  }

  [[nodiscard]] constexpr bool isLodHwCounterEn() const {
    return rx::getBit(words[6], 20);
  }

  constexpr void setLodHwCounterEn(bool value) {
    words[6] = rx::setBit(words[6], 20, value);
  }

  [[nodiscard]] constexpr std::uint32_t getTotalArrayCount() const {
    return getLastArray() + 1;
  }

  [[nodiscard]] constexpr std::uint32_t getTotalLevelCount() const {
    return getLastLevel() + 1;
  }

  [[nodiscard]] constexpr std::uint32_t getArrayCount() const {
    return getTotalArrayCount() - getBaseArray();
  }

  [[nodiscard]] constexpr std::uint32_t getLevelCount() const {
    return getTotalLevelCount() - getBaseLevel();
  }

  auto operator<=>(const TBuffer &) const = default;
};

struct Sampler {
  std::uint32_t words[4];

  [[nodiscard]] constexpr ClampMode getClampX() const {
    return static_cast<ClampMode>(rx::getBits(words[0], 2, 0));
  }

  constexpr void setClampX(ClampMode value) {
    words[0] = rx::setBits(words[0], 2, 0, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr ClampMode getClampY() const {
    return static_cast<ClampMode>(rx::getBits(words[0], 5, 3));
  }

  constexpr void setClampY(ClampMode value) {
    words[0] = rx::setBits(words[0], 5, 3, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr ClampMode getClampZ() const {
    return static_cast<ClampMode>(rx::getBits(words[0], 8, 6));
  }

  constexpr void setClampZ(ClampMode value) {
    words[0] = rx::setBits(words[0], 8, 6, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr AnisoRatio getAnisoRatio() const {
    return static_cast<AnisoRatio>(rx::getBits(words[0], 11, 9));
  }

  constexpr void setAnisoRatio(AnisoRatio value) {
    words[0] = rx::setBits(words[0], 11, 9, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr CompareFunc getDepthCompareFunc() const {
    return static_cast<CompareFunc>(rx::getBits(words[0], 14, 12));
  }

  constexpr void setDepthCompareFunc(CompareFunc value) {
    words[0] = rx::setBits(words[0], 14, 12, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr bool isForceUnormCoords() const {
    return rx::getBit(words[0], 15);
  }

  constexpr void setForceUnormCoords(bool value) {
    words[0] = rx::setBit(words[0], 15, value);
  }

  [[nodiscard]] constexpr std::uint8_t getAnisoThreshold() const {
    return rx::getBits(words[0], 18, 16);
  }

  constexpr void setAnisoThreshold(std::uint8_t value) {
    words[0] = rx::setBits(words[0], 18, 16, value);
  }

  [[nodiscard]] constexpr bool isMcCoordTrunc() const {
    return rx::getBit(words[0], 19);
  }

  constexpr void setMcCoordTrunc(bool value) {
    words[0] = rx::setBit(words[0], 19, value);
  }

  [[nodiscard]] constexpr bool isForceDegamma() const {
    return rx::getBit(words[0], 20);
  }

  constexpr void setForceDegamma(bool value) {
    words[0] = rx::setBit(words[0], 20, value);
  }

  [[nodiscard]] constexpr std::uint8_t getAnisoBias() const {
    return rx::getBits(words[0], 26, 21);
  }

  constexpr void setAnisoBias(std::uint8_t value) {
    words[0] = rx::setBits(words[0], 26, 21, value);
  }

  [[nodiscard]] constexpr bool isTruncCoord() const {
    return rx::getBit(words[0], 27);
  }

  constexpr void setTruncCoord(bool value) {
    words[0] = rx::setBit(words[0], 27, value);
  }

  [[nodiscard]] constexpr bool isDisableCubeWarp() const {
    return rx::getBit(words[0], 28);
  }

  constexpr void setDisableCubeWarp(bool value) {
    words[0] = rx::setBit(words[0], 28, value);
  }

  [[nodiscard]] constexpr FilterMode getFiterMode() const {
    return static_cast<FilterMode>(rx::getBits(words[0], 30, 29));
  }

  constexpr void setFiterMode(FilterMode value) {
    words[0] = rx::setBits(words[0], 30, 29, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint16_t getMinLod() const {
    return rx::getBits(words[1], 11, 0);
  }

  constexpr void setMinLod(std::uint16_t value) {
    words[1] = rx::setBits(words[1], 11, 0, value);
  }

  [[nodiscard]] constexpr std::uint16_t getMaxLod() const {
    return rx::getBits(words[1], 23, 12);
  }

  constexpr void setMaxLod(std::uint16_t value) {
    words[1] = rx::setBits(words[1], 23, 12, value);
  }

  [[nodiscard]] constexpr std::uint8_t getPerfMip() const {
    return rx::getBits(words[1], 27, 24);
  }

  constexpr void setPerfMip(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 27, 24, value);
  }

  [[nodiscard]] constexpr std::uint8_t getPerfZ() const {
    return rx::getBits(words[1], 31, 28);
  }

  constexpr void setPerfZ(std::uint8_t value) {
    words[1] = rx::setBits(words[1], 31, 28, value);
  }

  [[nodiscard]] constexpr std::uint16_t getLodBias() const {
    return rx::getBits(words[2], 13, 0);
  }

  constexpr void setLodBias(std::uint16_t value) {
    words[2] = rx::setBits(words[2], 13, 0, value);
  }

  [[nodiscard]] constexpr std::uint16_t getLodBiasSec() const {
    return rx::getBits(words[2], 19, 14);
  }

  constexpr void setLodBiasSec(std::uint16_t value) {
    words[2] = rx::setBits(words[2], 19, 14, value);
  }

  [[nodiscard]] constexpr Filter getXYMagFilter() const {
    return static_cast<Filter>(rx::getBits(words[2], 21, 20));
  }

  constexpr void setXYMagFilter(Filter value) {
    words[2] = rx::setBits(words[2], 21, 20, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Filter getXYMinFilter() const {
    return static_cast<Filter>(rx::getBits(words[2], 23, 22));
  }

  constexpr void setXYMinFilter(Filter value) {
    words[2] = rx::setBits(words[2], 23, 22, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr Filter getZFilter() const {
    return static_cast<Filter>(rx::getBits(words[2], 25, 24));
  }

  constexpr void setZFilter(Filter value) {
    words[2] = rx::setBits(words[2], 25, 24, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr MipFilter getMipFilter() const {
    return static_cast<MipFilter>(rx::getBits(words[2], 27, 26));
  }

  constexpr void setMipFilter(MipFilter value) {
    words[2] = rx::setBits(words[2], 27, 26, static_cast<std::uint32_t>(value));
  }

  [[nodiscard]] constexpr std::uint16_t getBorderColorPtr() const {
    return rx::getBits(words[3], 11, 0);
  }

  constexpr void setBorderColorPtr(std::uint16_t value) {
    words[3] = rx::setBits(words[3], 11, 0, value);
  }

  [[nodiscard]] constexpr BorderColor getBorderColor() const {
    return static_cast<BorderColor>(rx::getBits(words[2], 31, 30));
  }

  constexpr void setBorderColor(BorderColor value) {
    words[2] = rx::setBits(words[2], 31, 30, static_cast<std::uint32_t>(value));
  }

  auto operator<=>(const Sampler &) const = default;
};
} // namespace gnm
