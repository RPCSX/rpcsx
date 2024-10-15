#pragma once
#include "constants.hpp"
#include "descriptors.hpp"

namespace gnm {

constexpr int getTexelsPerElement(gnm::DataFormat dfmt) {
  switch (dfmt) {
  case kDataFormatBc1:
  case kDataFormatBc2:
  case kDataFormatBc3:
  case kDataFormatBc4:
  case kDataFormatBc5:
  case kDataFormatBc6:
  case kDataFormatBc7:
    return 16;
  case kDataFormat1:
  case kDataFormat1Reversed:
    return 8;
  case kDataFormatGB_GR:
  case kDataFormatBG_RG:
    return 2;
  default:
    return 1;
  }
}

constexpr int getBitsPerElement(DataFormat dfmt) {
  switch (dfmt) {
  case kDataFormatInvalid:
    return 0;
  case kDataFormat8:
    return 8;
  case kDataFormat16:
    return 16;
  case kDataFormat8_8:
    return 16;
  case kDataFormat32:
    return 32;
  case kDataFormat16_16:
    return 32;
  case kDataFormat10_11_11:
    return 32;
  case kDataFormat11_11_10:
    return 32;
  case kDataFormat10_10_10_2:
    return 32;
  case kDataFormat2_10_10_10:
    return 32;
  case kDataFormat8_8_8_8:
    return 32;
  case kDataFormat32_32:
    return 64;
  case kDataFormat16_16_16_16:
    return 64;
  case kDataFormat32_32_32:
    return 96;
  case kDataFormat32_32_32_32:
    return 128;
  case kDataFormat5_6_5:
    return 16;
  case kDataFormat1_5_5_5:
    return 16;
  case kDataFormat5_5_5_1:
    return 16;
  case kDataFormat4_4_4_4:
    return 16;
  case kDataFormat8_24:
    return 32;
  case kDataFormat24_8:
    return 32;
  case kDataFormatX24_8_32:
    return 64;
  case kDataFormatGB_GR:
    return 16;
  case kDataFormatBG_RG:
    return 16;
  case kDataFormat5_9_9_9:
    return 32;
  case kDataFormatBc1:
    return 4;
  case kDataFormatBc2:
    return 8;
  case kDataFormatBc3:
    return 8;
  case kDataFormatBc4:
    return 4;
  case kDataFormatBc5:
    return 8;
  case kDataFormatBc6:
    return 8;
  case kDataFormatBc7:
    return 8;
  case kDataFormatFmask8_S2_F1:
    return 8;
  case kDataFormatFmask8_S4_F1:
    return 8;
  case kDataFormatFmask8_S8_F1:
    return 8;
  case kDataFormatFmask8_S2_F2:
    return 8;
  case kDataFormatFmask8_S4_F2:
    return 8;
  case kDataFormatFmask8_S4_F4:
    return 8;
  case kDataFormatFmask16_S16_F1:
    return 16;
  case kDataFormatFmask16_S8_F2:
    return 16;
  case kDataFormatFmask32_S16_F2:
    return 32;
  case kDataFormatFmask32_S8_F4:
    return 32;
  case kDataFormatFmask32_S8_F8:
    return 32;
  case kDataFormatFmask64_S16_F4:
    return 64;
  case kDataFormatFmask64_S16_F8:
    return 64;
  case kDataFormat4_4:
    return 8;
  case kDataFormat6_5_5:
    return 16;
  case kDataFormat1:
    return 1;
  case kDataFormat1Reversed:
    return 1;
  }

  return -1;
}

constexpr int getTotalBitsPerElement(DataFormat dfmt) {
  return getBitsPerElement(dfmt) * getTexelsPerElement(dfmt);
}
constexpr int getNumComponentsPerElement(DataFormat dfmt) {
  switch (dfmt) {
  case kDataFormatInvalid:
    return 0;
  case kDataFormat8:
    return 1;
  case kDataFormat16:
    return 1;
  case kDataFormat8_8:
    return 2;
  case kDataFormat32:
    return 1;
  case kDataFormat16_16:
    return 2;
  case kDataFormat10_11_11:
    return 3;
  case kDataFormat11_11_10:
    return 3;
  case kDataFormat10_10_10_2:
    return 4;
  case kDataFormat2_10_10_10:
    return 4;
  case kDataFormat8_8_8_8:
    return 4;
  case kDataFormat32_32:
    return 2;
  case kDataFormat16_16_16_16:
    return 4;
  case kDataFormat32_32_32:
    return 3;
  case kDataFormat32_32_32_32:
    return 4;
  case kDataFormat5_6_5:
    return 3;
  case kDataFormat1_5_5_5:
    return 4;
  case kDataFormat5_5_5_1:
    return 4;
  case kDataFormat4_4_4_4:
    return 4;
  case kDataFormat8_24:
    return 2;
  case kDataFormat24_8:
    return 2;
  case kDataFormatX24_8_32:
    return 2;
  case kDataFormatGB_GR:
    return 3;
  case kDataFormatBG_RG:
    return 3;
  case kDataFormat5_9_9_9:
    return 3;
  case kDataFormatBc1:
    return 4;
  case kDataFormatBc2:
    return 4;
  case kDataFormatBc3:
    return 4;
  case kDataFormatBc4:
    return 1;
  case kDataFormatBc5:
    return 2;
  case kDataFormatBc6:
    return 3;
  case kDataFormatBc7:
    return 4;
  case kDataFormatFmask8_S2_F1:
    return 2;
  case kDataFormatFmask8_S4_F1:
    return 2;
  case kDataFormatFmask8_S8_F1:
    return 2;
  case kDataFormatFmask8_S2_F2:
    return 2;
  case kDataFormatFmask8_S4_F2:
    return 2;
  case kDataFormatFmask8_S4_F4:
    return 2;
  case kDataFormatFmask16_S16_F1:
    return 2;
  case kDataFormatFmask16_S8_F2:
    return 2;
  case kDataFormatFmask32_S16_F2:
    return 2;
  case kDataFormatFmask32_S8_F4:
    return 2;
  case kDataFormatFmask32_S8_F8:
    return 2;
  case kDataFormatFmask64_S16_F4:
    return 2;
  case kDataFormatFmask64_S16_F8:
    return 2;
  case kDataFormat4_4:
    return 2;
  case kDataFormat6_5_5:
    return 3;
  case kDataFormat1:
    return 1;
  case kDataFormat1Reversed:
    return 1;
  }

  return -1;
}
constexpr ZFormat getZFormat(DataFormat dfmt) {
  if (dfmt == kDataFormat32) {
    return kZFormat32Float;
  }

  if (dfmt == kDataFormat16) {
    return kZFormat16;
  }

  return kZFormatInvalid;
}

constexpr StencilFormat getStencilFormat(DataFormat dfmt) {
  return dfmt == kDataFormat8 ? kStencil8 : kStencilInvalid;
}

constexpr DataFormat getDataFormat(ZFormat format) {
  switch (format) {
  case kZFormat32Float:
    return kDataFormat32;
  case kZFormat16:
    return kDataFormat16;

  case kZFormatInvalid:
    break;
  }

  return kDataFormatInvalid;
}

constexpr NumericFormat getNumericFormat(ZFormat format) {
  switch (format) {
  case kZFormat32Float:
    return kNumericFormatFloat;
  case kZFormat16:
    return kNumericFormatUNorm;

  case kZFormatInvalid:
    break;
  }

  return kNumericFormatUNorm;
}

constexpr DataFormat getDataFormat(StencilFormat format) {
  switch (format) {
  case kStencil8:
    return kDataFormat8;

  case kStencilInvalid:
    break;
  }

  return kDataFormatInvalid;
}

constexpr NumericFormat getNumericFormat(StencilFormat format) {
  switch (format) {
  case kStencil8:
    return kNumericFormatUInt;

  case kStencilInvalid:
    break;
  }

  return kNumericFormatUNorm;
}
} // namespace gnm
