#pragma once

#include "amdgpu/bridge/bridge.hpp"
#include "amdgpu/shader/Instruction.hpp"
#include "gpu-scheduler.hpp"
#include "util/area.hpp"

#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace amdgpu::device {
inline constexpr std::uint32_t getBits(std::uint32_t value, int end,
                                       int begin) {
  return (value >> begin) & ((1u << (end - begin + 1)) - 1);
}

inline constexpr std::uint32_t getBit(std::uint32_t value, int bit) {
  return (value >> bit) & 1;
}

inline constexpr std::uint32_t genMask(std::uint32_t offset,
                                       std::uint32_t bitCount) {
  return ((1u << bitCount) - 1u) << offset;
}

inline constexpr std::uint32_t getMaskEnd(std::uint32_t mask) {
  return 32 - std::countl_zero(mask);
}

inline constexpr std::uint32_t fetchMaskedValue(std::uint32_t hex,
                                                std::uint32_t mask) {
  return (hex & mask) >> std::countr_zero(mask);
}

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
inline std::size_t calcStringLen(T value, unsigned base = 10) {
  std::size_t n = 1;
  std::size_t base2 = base * base;
  std::size_t base3 = base2 * base;
  std::size_t base4 = base3 * base;

  while (true) {
    if (value < base) {
      return n;
    }

    if (value < base2) {
      return n + 1;
    }

    if (value < base3) {
      return n + 2;
    }

    if (value < base4) {
      return n + 3;
    }

    value /= base4;
    n += 4;
  }
}

template <typename T, typename = std::enable_if_t<std::is_unsigned_v<T>>>
inline void toHexString(char *dst, std::size_t len, T value) {
  while (len > 0) {
    char digit = value % 16;
    value /= 16;

    dst[--len] = digit < 10 ? '0' + digit : 'a' + digit - 10;
  }
}

inline std::string toHexString(unsigned value) {
  auto len = calcStringLen(value, 16);

  std::string result(len, '\0');
  toHexString(result.data(), len, value);
  return result;
}

inline std::string toHexString(int value) {
  bool isNeg = value < 0;
  unsigned uval = isNeg ? static_cast<unsigned>(~value) + 1 : value;
  auto len = calcStringLen(uval, 16);

  std::string result(len + (isNeg ? 1 : 0), '-');
  toHexString(result.data(), len, uval);
  return result;
}

enum Registers {
  SPI_SHADER_PGM_LO_PS = 0x2c08,
  SPI_SHADER_PGM_HI_PS = 0x2c09,
  SPI_SHADER_PGM_RSRC1_PS = 0x2c0a,
  SPI_SHADER_PGM_RSRC2_PS = 0x2c0b,
  SPI_SHADER_USER_DATA_PS_0 = 0x2c0c,
  SPI_SHADER_USER_DATA_PS_1,
  SPI_SHADER_USER_DATA_PS_2,
  SPI_SHADER_USER_DATA_PS_3,
  SPI_SHADER_USER_DATA_PS_4,
  SPI_SHADER_USER_DATA_PS_5,
  SPI_SHADER_USER_DATA_PS_6,
  SPI_SHADER_USER_DATA_PS_7,
  SPI_SHADER_USER_DATA_PS_8,
  SPI_SHADER_USER_DATA_PS_9,
  SPI_SHADER_USER_DATA_PS_10,
  SPI_SHADER_USER_DATA_PS_11,
  SPI_SHADER_USER_DATA_PS_12,
  SPI_SHADER_USER_DATA_PS_13,
  SPI_SHADER_USER_DATA_PS_14,
  SPI_SHADER_USER_DATA_PS_15,

  SPI_SHADER_PGM_LO_VS = 0x2c48,
  SPI_SHADER_PGM_HI_VS = 0x2c49,
  SPI_SHADER_PGM_RSRC1_VS = 0x2c4a,
  SPI_SHADER_PGM_RSRC2_VS = 0x2c4b,
  SPI_SHADER_USER_DATA_VS_0 = 0x2c4c,
  SPI_SHADER_USER_DATA_VS_1 = 0x2c4d,
  SPI_SHADER_USER_DATA_VS_2 = 0x2c4e,
  SPI_SHADER_USER_DATA_VS_3 = 0x2c4f,
  SPI_SHADER_USER_DATA_VS_4,
  SPI_SHADER_USER_DATA_VS_5,
  SPI_SHADER_USER_DATA_VS_6,
  SPI_SHADER_USER_DATA_VS_7,
  SPI_SHADER_USER_DATA_VS_8,
  SPI_SHADER_USER_DATA_VS_9,
  SPI_SHADER_USER_DATA_VS_10,
  SPI_SHADER_USER_DATA_VS_11,
  SPI_SHADER_USER_DATA_VS_12,
  SPI_SHADER_USER_DATA_VS_13,
  SPI_SHADER_USER_DATA_VS_14,
  SPI_SHADER_USER_DATA_VS_15,

  COMPUTE_NUM_THREAD_X = 0x2e07,
  COMPUTE_NUM_THREAD_Y,
  COMPUTE_NUM_THREAD_Z,
  COMPUTE_PGM_LO = 0x2e0c,
  COMPUTE_PGM_HI,
  COMPUTE_PGM_RSRC1 = 0x2e12,
  COMPUTE_PGM_RSRC2,
  COMPUTE_USER_DATA_0 = 0x2e40,
  COMPUTE_USER_DATA_1,
  COMPUTE_USER_DATA_2,
  COMPUTE_USER_DATA_3,
  COMPUTE_USER_DATA_4,
  COMPUTE_USER_DATA_5,
  COMPUTE_USER_DATA_6,
  COMPUTE_USER_DATA_7,
  COMPUTE_USER_DATA_8,
  COMPUTE_USER_DATA_9,
  COMPUTE_USER_DATA_10,
  COMPUTE_USER_DATA_11,
  COMPUTE_USER_DATA_12,
  COMPUTE_USER_DATA_13,
  COMPUTE_USER_DATA_14,
  COMPUTE_USER_DATA_15,

  DB_RENDER_CONTROL = 0xa000,
  DB_DEPTH_VIEW = 0xA002,
  DB_HTILE_DATA_BASE = 0xA005,
  DB_DEPTH_CLEAR = 0xA00B,
  PA_SC_SCREEN_SCISSOR_TL = 0xa00c,
  PA_SC_SCREEN_SCISSOR_BR = 0xa00d,
  DB_DEPTH_INFO = 0xA00F,
  DB_Z_INFO = 0xA010,
  DB_STENCIL_INFO = 0xA011,
  DB_Z_READ_BASE = 0xA012,
  DB_STENCIL_READ_BASE = 0xA013,
  DB_Z_WRITE_BASE = 0xA014,
  DB_STENCIL_WRITE_BASE = 0xA015,
  DB_DEPTH_SIZE = 0xA016,
  DB_DEPTH_SLICE = 0xA017,
  PA_SU_HARDWARE_SCREEN_OFFSET = 0xa08d,
  CB_TARGET_MASK = 0xA08e,
  CB_SHADER_MASK = 0xa08f,
  PA_SC_VPORT_ZMIN_0 = 0xA0b4,
  PA_SC_VPORT_ZMAX_0 = 0xA0b5,
  PA_CL_VPORT_XSCALE = 0xa10f,
  PA_CL_VPORT_XOFFSET,
  PA_CL_VPORT_YSCALE,
  PA_CL_VPORT_YOFFSET,
  PA_CL_VPORT_ZSCALE,
  PA_CL_VPORT_ZOFFSET,
  SPI_PS_INPUT_CNTL_0 = 0xa191,
  SPI_VS_OUT_CONFIG = 0xa1b1,
  SPI_PS_INPUT_ENA = 0xa1b3,
  SPI_PS_INPUT_ADDR = 0xa1b4,
  SPI_PS_IN_CONTROL = 0xa1b6,
  SPI_BARYC_CNTL = 0xa1b8,
  SPI_SHADER_POS_FORMAT = 0xa1c3,
  SPI_SHADER_Z_FORMAT = 0xa1c4,
  SPI_SHADER_COL_FORMAT = 0xa1c5,
  DB_DEPTH_CONTROL = 0xa200,
  CB_COLOR_CONTROL = 0xa202,
  DB_SHADER_CONTROL = 0xa203,
  PA_CL_CLIP_CNTL = 0xa204,
  PA_SU_SC_MODE_CNTL = 0xa205,
  PA_CL_VTE_CNTL = 0xa206,
  PA_CL_VS_OUT_CNTL = 0xa207,
  DB_HTILE_SURFACE = 0xA2AF,
  VGT_SHADER_STAGES_EN = 0xa2d5,
  PA_CL_GB_VERT_CLIP_ADJ = 0xa2fa,
  PA_CL_GB_VERT_DISC_ADJ,
  PA_CL_GB_HORZ_CLIP_ADJ,
  PA_CL_GB_HORZ_DISC_ADJ,

  CB_COLOR0_BASE = 0xA318,
  CB_COLOR0_PITCH,
  CB_COLOR0_SLICE,
  CB_COLOR0_VIEW,
  CB_COLOR0_INFO,
  CB_COLOR0_ATTRIB,
  CB_COLOR0_DCC_CONTROL,
  CB_COLOR0_CMASK,
  CB_COLOR0_CMASK_SLICE,
  CB_COLOR0_FMASK,
  CB_COLOR0_FMASK_SLICE,
  CB_COLOR0_CLEAR_WORD0,
  CB_COLOR0_CLEAR_WORD1,
  CB_COLOR0_DCC_BASE,
  CB_COLOR0_UNK0,

  CB_COLOR1_BASE,
  CB_COLOR1_PITCH,
  CB_COLOR1_SLICE,
  CB_COLOR1_VIEW,
  CB_COLOR1_INFO,
  CB_COLOR1_ATTRIB,
  CB_COLOR1_DCC_CONTROL,
  CB_COLOR1_CMASK,
  CB_COLOR1_CMASK_SLICE,
  CB_COLOR1_FMASK,
  CB_COLOR1_FMASK_SLICE,
  CB_COLOR1_CLEAR_WORD0,
  CB_COLOR1_CLEAR_WORD1,
  CB_COLOR1_DCC_BASE,
  CB_COLOR1_UNK0,

  CB_COLOR2_BASE,
  CB_COLOR2_PITCH,
  CB_COLOR2_SLICE,
  CB_COLOR2_VIEW,
  CB_COLOR2_INFO,
  CB_COLOR2_ATTRIB,
  CB_COLOR2_DCC_CONTROL,
  CB_COLOR2_CMASK,
  CB_COLOR2_CMASK_SLICE,
  CB_COLOR2_FMASK,
  CB_COLOR2_FMASK_SLICE,
  CB_COLOR2_CLEAR_WORD0,
  CB_COLOR2_CLEAR_WORD1,
  CB_COLOR2_DCC_BASE,
  CB_COLOR2_UNK0,

  CB_COLOR3_BASE,
  CB_COLOR3_PITCH,
  CB_COLOR3_SLICE,
  CB_COLOR3_VIEW,
  CB_COLOR3_INFO,
  CB_COLOR3_ATTRIB,
  CB_COLOR3_DCC_CONTROL,
  CB_COLOR3_CMASK,
  CB_COLOR3_CMASK_SLICE,
  CB_COLOR3_FMASK,
  CB_COLOR3_FMASK_SLICE,
  CB_COLOR3_CLEAR_WORD0,
  CB_COLOR3_CLEAR_WORD1,
  CB_COLOR3_DCC_BASE,
  CB_COLOR3_UNK0,

  CB_COLOR4_BASE,
  CB_COLOR4_PITCH,
  CB_COLOR4_SLICE,
  CB_COLOR4_VIEW,
  CB_COLOR4_INFO,
  CB_COLOR4_ATTRIB,
  CB_COLOR4_DCC_CONTROL,
  CB_COLOR4_CMASK,
  CB_COLOR4_CMASK_SLICE,
  CB_COLOR4_FMASK,
  CB_COLOR4_FMASK_SLICE,
  CB_COLOR4_CLEAR_WORD0,
  CB_COLOR4_CLEAR_WORD1,
  CB_COLOR4_DCC_BASE,
  CB_COLOR4_UNK0,

  CB_COLOR5_BASE,
  CB_COLOR5_PITCH,
  CB_COLOR5_SLICE,
  CB_COLOR5_VIEW,
  CB_COLOR5_INFO,
  CB_COLOR5_ATTRIB,
  CB_COLOR5_DCC_CONTROL,
  CB_COLOR5_CMASK,
  CB_COLOR5_CMASK_SLICE,
  CB_COLOR5_FMASK,
  CB_COLOR5_FMASK_SLICE,
  CB_COLOR5_CLEAR_WORD0,
  CB_COLOR5_CLEAR_WORD1,
  CB_COLOR5_DCC_BASE,
  CB_COLOR5_UNK0,

  CB_COLOR6_BASE,
  CB_COLOR6_PITCH,
  CB_COLOR6_SLICE,
  CB_COLOR6_VIEW,
  CB_COLOR6_INFO,
  CB_COLOR6_ATTRIB,
  CB_COLOR6_DCC_CONTROL,
  CB_COLOR6_CMASK,
  CB_COLOR6_CMASK_SLICE,
  CB_COLOR6_FMASK,
  CB_COLOR6_FMASK_SLICE,
  CB_COLOR6_CLEAR_WORD0,
  CB_COLOR6_CLEAR_WORD1,
  CB_COLOR6_DCC_BASE,
  CB_COLOR6_UNK0,

  CB_BLEND0_CONTROL = 0xa1e0,

  VGT_PRIMITIVE_TYPE = 0xc242,
};

inline std::string registerToString(int reg) {
  switch (reg) {
  case SPI_SHADER_PGM_LO_PS:
    return "SPI_SHADER_PGM_LO_PS";
  case SPI_SHADER_PGM_HI_PS:
    return "SPI_SHADER_PGM_HI_PS";
  case SPI_SHADER_PGM_RSRC1_PS:
    return "SPI_SHADER_PGM_RSRC1_PS";
  case SPI_SHADER_PGM_RSRC2_PS:
    return "SPI_SHADER_PGM_RSRC2_PS";
  case SPI_SHADER_USER_DATA_PS_0:
    return "SPI_SHADER_USER_DATA_PS_0";
  case SPI_SHADER_USER_DATA_PS_1:
    return "SPI_SHADER_USER_DATA_PS_1";
  case SPI_SHADER_USER_DATA_PS_2:
    return "SPI_SHADER_USER_DATA_PS_2";
  case SPI_SHADER_USER_DATA_PS_3:
    return "SPI_SHADER_USER_DATA_PS_3";
  case SPI_SHADER_USER_DATA_PS_4:
    return "SPI_SHADER_USER_DATA_PS_4";
  case SPI_SHADER_USER_DATA_PS_5:
    return "SPI_SHADER_USER_DATA_PS_5";
  case SPI_SHADER_USER_DATA_PS_6:
    return "SPI_SHADER_USER_DATA_PS_6";
  case SPI_SHADER_USER_DATA_PS_7:
    return "SPI_SHADER_USER_DATA_PS_7";
  case SPI_SHADER_USER_DATA_PS_8:
    return "SPI_SHADER_USER_DATA_PS_8";
  case SPI_SHADER_USER_DATA_PS_9:
    return "SPI_SHADER_USER_DATA_PS_9";
  case SPI_SHADER_USER_DATA_PS_10:
    return "SPI_SHADER_USER_DATA_PS_10";
  case SPI_SHADER_USER_DATA_PS_11:
    return "SPI_SHADER_USER_DATA_PS_11";
  case SPI_SHADER_USER_DATA_PS_12:
    return "SPI_SHADER_USER_DATA_PS_12";
  case SPI_SHADER_USER_DATA_PS_13:
    return "SPI_SHADER_USER_DATA_PS_13";
  case SPI_SHADER_USER_DATA_PS_14:
    return "SPI_SHADER_USER_DATA_PS_14";
  case SPI_SHADER_USER_DATA_PS_15:
    return "SPI_SHADER_USER_DATA_PS_15";
  case SPI_SHADER_PGM_LO_VS:
    return "SPI_SHADER_PGM_LO_VS";
  case SPI_SHADER_PGM_HI_VS:
    return "SPI_SHADER_PGM_HI_VS";
  case SPI_SHADER_PGM_RSRC1_VS:
    return "SPI_SHADER_PGM_RSRC1_VS";
  case SPI_SHADER_PGM_RSRC2_VS:
    return "SPI_SHADER_PGM_RSRC2_VS";
  case SPI_SHADER_USER_DATA_VS_0:
    return "SPI_SHADER_USER_DATA_VS_0";
  case SPI_SHADER_USER_DATA_VS_1:
    return "SPI_SHADER_USER_DATA_VS_1";
  case SPI_SHADER_USER_DATA_VS_2:
    return "SPI_SHADER_USER_DATA_VS_2";
  case SPI_SHADER_USER_DATA_VS_3:
    return "SPI_SHADER_USER_DATA_VS_3";
  case SPI_SHADER_USER_DATA_VS_4:
    return "SPI_SHADER_USER_DATA_VS_4";
  case SPI_SHADER_USER_DATA_VS_5:
    return "SPI_SHADER_USER_DATA_VS_5";
  case SPI_SHADER_USER_DATA_VS_6:
    return "SPI_SHADER_USER_DATA_VS_6";
  case SPI_SHADER_USER_DATA_VS_7:
    return "SPI_SHADER_USER_DATA_VS_7";
  case SPI_SHADER_USER_DATA_VS_8:
    return "SPI_SHADER_USER_DATA_VS_8";
  case SPI_SHADER_USER_DATA_VS_9:
    return "SPI_SHADER_USER_DATA_VS_9";
  case SPI_SHADER_USER_DATA_VS_10:
    return "SPI_SHADER_USER_DATA_VS_10";
  case SPI_SHADER_USER_DATA_VS_11:
    return "SPI_SHADER_USER_DATA_VS_11";
  case SPI_SHADER_USER_DATA_VS_12:
    return "SPI_SHADER_USER_DATA_VS_12";
  case SPI_SHADER_USER_DATA_VS_13:
    return "SPI_SHADER_USER_DATA_VS_13";
  case SPI_SHADER_USER_DATA_VS_14:
    return "SPI_SHADER_USER_DATA_VS_14";
  case SPI_SHADER_USER_DATA_VS_15:
    return "SPI_SHADER_USER_DATA_VS_15";
  case COMPUTE_NUM_THREAD_X:
    return "COMPUTE_NUM_THREAD_X";
  case COMPUTE_NUM_THREAD_Y:
    return "COMPUTE_NUM_THREAD_Y";
  case COMPUTE_NUM_THREAD_Z:
    return "COMPUTE_NUM_THREAD_Z";
  case COMPUTE_PGM_LO:
    return "COMPUTE_PGM_LO";
  case COMPUTE_PGM_HI:
    return "COMPUTE_PGM_HI";
  case COMPUTE_PGM_RSRC1:
    return "COMPUTE_PGM_RSRC1";
  case COMPUTE_PGM_RSRC2:
    return "COMPUTE_PGM_RSRC2";
  case COMPUTE_USER_DATA_0:
    return "COMPUTE_USER_DATA_0";
  case COMPUTE_USER_DATA_1:
    return "COMPUTE_USER_DATA_1";
  case COMPUTE_USER_DATA_2:
    return "COMPUTE_USER_DATA_2";
  case COMPUTE_USER_DATA_3:
    return "COMPUTE_USER_DATA_3";
  case COMPUTE_USER_DATA_4:
    return "COMPUTE_USER_DATA_4";
  case COMPUTE_USER_DATA_5:
    return "COMPUTE_USER_DATA_5";
  case COMPUTE_USER_DATA_6:
    return "COMPUTE_USER_DATA_6";
  case COMPUTE_USER_DATA_7:
    return "COMPUTE_USER_DATA_7";
  case COMPUTE_USER_DATA_8:
    return "COMPUTE_USER_DATA_8";
  case COMPUTE_USER_DATA_9:
    return "COMPUTE_USER_DATA_9";
  case COMPUTE_USER_DATA_10:
    return "COMPUTE_USER_DATA_10";
  case COMPUTE_USER_DATA_11:
    return "COMPUTE_USER_DATA_11";
  case COMPUTE_USER_DATA_12:
    return "COMPUTE_USER_DATA_12";
  case COMPUTE_USER_DATA_13:
    return "COMPUTE_USER_DATA_13";
  case COMPUTE_USER_DATA_14:
    return "COMPUTE_USER_DATA_14";
  case COMPUTE_USER_DATA_15:
    return "COMPUTE_USER_DATA_15";
  case DB_DEPTH_CLEAR:
    return "DB_DEPTH_CLEAR";
  case DB_RENDER_CONTROL:
    return "DB_RENDER_CONTROL";
  case DB_DEPTH_VIEW:
    return "DB_DEPTH_VIEW";
  case DB_HTILE_DATA_BASE:
    return "DB_HTILE_DATA_BASE";
  case PA_SC_SCREEN_SCISSOR_TL:
    return "PA_SC_SCREEN_SCISSOR_TL";
  case PA_SC_SCREEN_SCISSOR_BR:
    return "PA_SC_SCREEN_SCISSOR_BR";
  case DB_DEPTH_INFO:
    return "DB_DEPTH_INFO";
  case DB_Z_INFO:
    return "DB_Z_INFO";
  case DB_STENCIL_INFO:
    return "DB_STENCIL_INFO";
  case DB_Z_READ_BASE:
    return "DB_Z_READ_BASE";
  case DB_STENCIL_READ_BASE:
    return "DB_STENCIL_READ_BASE";
  case DB_Z_WRITE_BASE:
    return "DB_Z_WRITE_BASE";
  case DB_STENCIL_WRITE_BASE:
    return "DB_STENCIL_WRITE_BASE";
  case DB_DEPTH_SIZE:
    return "DB_DEPTH_SIZE";
  case DB_DEPTH_SLICE:
    return "DB_DEPTH_SLICE";
  case PA_SU_HARDWARE_SCREEN_OFFSET:
    return "PA_SU_HARDWARE_SCREEN_OFFSET";
  case CB_TARGET_MASK:
    return "CB_TARGET_MASK";
  case CB_SHADER_MASK:
    return "CB_SHADER_MASK";
  case PA_SC_VPORT_ZMIN_0:
    return "PA_SC_VPORT_ZMIN_0";
  case PA_SC_VPORT_ZMAX_0:
    return "PA_SC_VPORT_ZMAX_0";
  case PA_CL_VPORT_XSCALE:
    return "PA_CL_VPORT_XSCALE";
  case PA_CL_VPORT_XOFFSET:
    return "PA_CL_VPORT_XOFFSET";
  case PA_CL_VPORT_YSCALE:
    return "PA_CL_VPORT_YSCALE";
  case PA_CL_VPORT_YOFFSET:
    return "PA_CL_VPORT_YOFFSET";
  case PA_CL_VPORT_ZSCALE:
    return "PA_CL_VPORT_ZSCALE";
  case PA_CL_VPORT_ZOFFSET:
    return "PA_CL_VPORT_ZOFFSET";
  case SPI_PS_INPUT_CNTL_0:
    return "SPI_PS_INPUT_CNTL_0";
  case SPI_VS_OUT_CONFIG:
    return "SPI_VS_OUT_CONFIG";
  case SPI_PS_INPUT_ENA:
    return "SPI_PS_INPUT_ENA";
  case SPI_PS_INPUT_ADDR:
    return "SPI_PS_INPUT_ADDR";
  case SPI_PS_IN_CONTROL:
    return "SPI_PS_IN_CONTROL";
  case SPI_BARYC_CNTL:
    return "SPI_BARYC_CNTL";
  case SPI_SHADER_POS_FORMAT:
    return "SPI_SHADER_POS_FORMAT";
  case SPI_SHADER_Z_FORMAT:
    return "SPI_SHADER_Z_FORMAT";
  case SPI_SHADER_COL_FORMAT:
    return "SPI_SHADER_COL_FORMAT";
  case DB_DEPTH_CONTROL:
    return "DB_DEPTH_CONTROL";
  case CB_COLOR_CONTROL:
    return "DB_COLOR_CONTROL";
  case DB_SHADER_CONTROL:
    return "DB_SHADER_CONTROL";
  case PA_CL_CLIP_CNTL:
    return "PA_CL_CLIP_CNTL";
  case PA_SU_SC_MODE_CNTL:
    return "PA_SU_SC_MODE_CNTL";
  case PA_CL_VTE_CNTL:
    return "PA_CL_VTE_CNTL";
  case PA_CL_VS_OUT_CNTL:
    return "PA_CL_VS_OUT_CNTL";
  case DB_HTILE_SURFACE:
    return "DB_HTILE_SURFACE";
  case VGT_SHADER_STAGES_EN:
    return "VGT_SHADER_STAGES_EN";
  case PA_CL_GB_VERT_CLIP_ADJ:
    return "PA_CL_GB_VERT_CLIP_ADJ";
  case PA_CL_GB_VERT_DISC_ADJ:
    return "PA_CL_GB_VERT_DISC_ADJ";
  case PA_CL_GB_HORZ_CLIP_ADJ:
    return "PA_CL_GB_HORZ_CLIP_ADJ";
  case PA_CL_GB_HORZ_DISC_ADJ:
    return "PA_CL_GB_HORZ_DISC_ADJ";
  case CB_COLOR0_BASE:
    return "CB_COLOR0_BASE";
  case CB_COLOR0_PITCH:
    return "CB_COLOR0_PITCH";
  case CB_COLOR0_SLICE:
    return "CB_COLOR0_SLICE";
  case CB_COLOR0_VIEW:
    return "CB_COLOR0_VIEW";
  case CB_COLOR0_INFO:
    return "CB_COLOR0_INFO";
  case CB_COLOR0_ATTRIB:
    return "CB_COLOR0_ATTRIB";
  case CB_COLOR0_DCC_CONTROL:
    return "CB_COLOR0_DCC_CONTROL";
  case CB_COLOR0_CMASK:
    return "CB_COLOR0_CMASK";
  case CB_COLOR0_CMASK_SLICE:
    return "CB_COLOR0_CMASK_SLICE";
  case CB_COLOR0_FMASK:
    return "CB_COLOR0_FMASK";
  case CB_COLOR0_FMASK_SLICE:
    return "CB_COLOR0_FMASK_SLICE";
  case CB_COLOR0_CLEAR_WORD0:
    return "CB_COLOR0_CLEAR_WORD0";
  case CB_COLOR0_CLEAR_WORD1:
    return "CB_COLOR0_CLEAR_WORD1";
  case CB_COLOR0_DCC_BASE:
    return "CB_COLOR0_DCC_BASE";
  case CB_COLOR1_BASE:
    return "CB_COLOR1_BASE";
  case CB_COLOR1_PITCH:
    return "CB_COLOR1_PITCH";
  case CB_COLOR1_SLICE:
    return "CB_COLOR1_SLICE";
  case CB_COLOR1_VIEW:
    return "CB_COLOR1_VIEW";
  case CB_COLOR1_INFO:
    return "CB_COLOR1_INFO";
  case CB_COLOR1_ATTRIB:
    return "CB_COLOR1_ATTRIB";
  case CB_COLOR1_DCC_CONTROL:
    return "CB_COLOR1_DCC_CONTROL";
  case CB_COLOR1_CMASK:
    return "CB_COLOR1_CMASK";
  case CB_COLOR1_CMASK_SLICE:
    return "CB_COLOR1_CMASK_SLICE";
  case CB_COLOR1_FMASK:
    return "CB_COLOR1_FMASK";
  case CB_COLOR1_FMASK_SLICE:
    return "CB_COLOR1_FMASK_SLICE";
  case CB_COLOR1_CLEAR_WORD0:
    return "CB_COLOR1_CLEAR_WORD0";
  case CB_COLOR1_CLEAR_WORD1:
    return "CB_COLOR1_CLEAR_WORD1";
  case CB_COLOR1_DCC_BASE:
    return "CB_COLOR1_DCC_BASE";
  case CB_COLOR2_BASE:
    return "CB_COLOR2_BASE";
  case CB_COLOR2_PITCH:
    return "CB_COLOR2_PITCH";
  case CB_COLOR2_SLICE:
    return "CB_COLOR2_SLICE";
  case CB_COLOR2_VIEW:
    return "CB_COLOR2_VIEW";
  case CB_COLOR2_INFO:
    return "CB_COLOR2_INFO";
  case CB_COLOR2_ATTRIB:
    return "CB_COLOR2_ATTRIB";
  case CB_COLOR2_DCC_CONTROL:
    return "CB_COLOR2_DCC_CONTROL";
  case CB_COLOR2_CMASK:
    return "CB_COLOR2_CMASK";
  case CB_COLOR2_CMASK_SLICE:
    return "CB_COLOR2_CMASK_SLICE";
  case CB_COLOR2_FMASK:
    return "CB_COLOR2_FMASK";
  case CB_COLOR2_FMASK_SLICE:
    return "CB_COLOR2_FMASK_SLICE";
  case CB_COLOR2_CLEAR_WORD0:
    return "CB_COLOR2_CLEAR_WORD0";
  case CB_COLOR2_CLEAR_WORD1:
    return "CB_COLOR2_CLEAR_WORD1";
  case CB_COLOR2_DCC_BASE:
    return "CB_COLOR2_DCC_BASE";
  case CB_COLOR3_BASE:
    return "CB_COLOR3_BASE";
  case CB_COLOR3_PITCH:
    return "CB_COLOR3_PITCH";
  case CB_COLOR3_SLICE:
    return "CB_COLOR3_SLICE";
  case CB_COLOR3_VIEW:
    return "CB_COLOR3_VIEW";
  case CB_COLOR3_INFO:
    return "CB_COLOR3_INFO";
  case CB_COLOR3_ATTRIB:
    return "CB_COLOR3_ATTRIB";
  case CB_COLOR3_DCC_CONTROL:
    return "CB_COLOR3_DCC_CONTROL";
  case CB_COLOR3_CMASK:
    return "CB_COLOR3_CMASK";
  case CB_COLOR3_CMASK_SLICE:
    return "CB_COLOR3_CMASK_SLICE";
  case CB_COLOR3_FMASK:
    return "CB_COLOR3_FMASK";
  case CB_COLOR3_FMASK_SLICE:
    return "CB_COLOR3_FMASK_SLICE";
  case CB_COLOR3_CLEAR_WORD0:
    return "CB_COLOR3_CLEAR_WORD0";
  case CB_COLOR3_CLEAR_WORD1:
    return "CB_COLOR3_CLEAR_WORD1";
  case CB_COLOR3_DCC_BASE:
    return "CB_COLOR3_DCC_BASE";
  case CB_COLOR4_BASE:
    return "CB_COLOR4_BASE";
  case CB_COLOR4_PITCH:
    return "CB_COLOR4_PITCH";
  case CB_COLOR4_SLICE:
    return "CB_COLOR4_SLICE";
  case CB_COLOR4_VIEW:
    return "CB_COLOR4_VIEW";
  case CB_COLOR4_INFO:
    return "CB_COLOR4_INFO";
  case CB_COLOR4_ATTRIB:
    return "CB_COLOR4_ATTRIB";
  case CB_COLOR4_DCC_CONTROL:
    return "CB_COLOR4_DCC_CONTROL";
  case CB_COLOR4_CMASK:
    return "CB_COLOR4_CMASK";
  case CB_COLOR4_CMASK_SLICE:
    return "CB_COLOR4_CMASK_SLICE";
  case CB_COLOR4_FMASK:
    return "CB_COLOR4_FMASK";
  case CB_COLOR4_FMASK_SLICE:
    return "CB_COLOR4_FMASK_SLICE";
  case CB_COLOR4_CLEAR_WORD0:
    return "CB_COLOR4_CLEAR_WORD0";
  case CB_COLOR4_CLEAR_WORD1:
    return "CB_COLOR4_CLEAR_WORD1";
  case CB_COLOR4_DCC_BASE:
    return "CB_COLOR4_DCC_BASE";
  case CB_COLOR5_BASE:
    return "CB_COLOR5_BASE";
  case CB_COLOR5_PITCH:
    return "CB_COLOR5_PITCH";
  case CB_COLOR5_SLICE:
    return "CB_COLOR5_SLICE";
  case CB_COLOR5_VIEW:
    return "CB_COLOR5_VIEW";
  case CB_COLOR5_INFO:
    return "CB_COLOR5_INFO";
  case CB_COLOR5_ATTRIB:
    return "CB_COLOR5_ATTRIB";
  case CB_COLOR5_DCC_CONTROL:
    return "CB_COLOR5_DCC_CONTROL";
  case CB_COLOR5_CMASK:
    return "CB_COLOR5_CMASK";
  case CB_COLOR5_CMASK_SLICE:
    return "CB_COLOR5_CMASK_SLICE";
  case CB_COLOR5_FMASK:
    return "CB_COLOR5_FMASK";
  case CB_COLOR5_FMASK_SLICE:
    return "CB_COLOR5_FMASK_SLICE";
  case CB_COLOR5_CLEAR_WORD0:
    return "CB_COLOR5_CLEAR_WORD0";
  case CB_COLOR5_CLEAR_WORD1:
    return "CB_COLOR5_CLEAR_WORD1";
  case CB_COLOR5_DCC_BASE:
    return "CB_COLOR5_DCC_BASE";
  case CB_COLOR6_BASE:
    return "CB_COLOR6_BASE";
  case CB_COLOR6_PITCH:
    return "CB_COLOR6_PITCH";
  case CB_COLOR6_SLICE:
    return "CB_COLOR6_SLICE";
  case CB_COLOR6_VIEW:
    return "CB_COLOR6_VIEW";
  case CB_COLOR6_INFO:
    return "CB_COLOR6_INFO";
  case CB_COLOR6_ATTRIB:
    return "CB_COLOR6_ATTRIB";
  case CB_COLOR6_DCC_CONTROL:
    return "CB_COLOR6_DCC_CONTROL";
  case CB_COLOR6_CMASK:
    return "CB_COLOR6_CMASK";
  case CB_COLOR6_CMASK_SLICE:
    return "CB_COLOR6_CMASK_SLICE";
  case CB_COLOR6_FMASK:
    return "CB_COLOR6_FMASK";
  case CB_COLOR6_FMASK_SLICE:
    return "CB_COLOR6_FMASK_SLICE";
  case CB_COLOR6_CLEAR_WORD0:
    return "CB_COLOR6_CLEAR_WORD0";
  case CB_COLOR6_CLEAR_WORD1:
    return "CB_COLOR6_CLEAR_WORD1";
  case CB_COLOR6_DCC_BASE:
    return "CB_COLOR6_DCC_BASE";
  case CB_BLEND0_CONTROL:
    return "CB_BLEND0_CONTROL";

  case VGT_PRIMITIVE_TYPE:
    return "VGT_PRIMITIVE_TYPE";
  }

  return "<unknown " + toHexString(reg) + ">";
}

enum Opcodes {
  kOpcodeNOP = 0x10,
  kOpcodeSET_BASE = 0x11,
  kOpcodeCLEAR_STATE = 0x12,
  kOpcodeINDEX_BUFFER_SIZE = 0x13,
  kOpcodeDISPATCH_DIRECT = 0x15,
  kOpcodeDISPATCH_INDIRECT = 0x16,
  kOpcodeINDIRECT_BUFFER_END = 0x17,
  kOpcodeMODE_CONTROL = 0x18,
  kOpcodeATOMIC_GDS = 0x1D,
  kOpcodeATOMIC_MEM = 0x1E,
  kOpcodeOCCLUSION_QUERY = 0x1F,
  kOpcodeSET_PREDICATION = 0x20,
  kOpcodeREG_RMW = 0x21,
  kOpcodeCOND_EXEC = 0x22,
  kOpcodePRED_EXEC = 0x23,
  kOpcodeDRAW_INDIRECT = 0x24,
  kOpcodeDRAW_INDEX_INDIRECT = 0x25,
  kOpcodeINDEX_BASE = 0x26,
  kOpcodeDRAW_INDEX_2 = 0x27,
  kOpcodeCONTEXT_CONTROL = 0x28,
  kOpcodeDRAW_INDEX_OFFSET = 0x29,
  kOpcodeINDEX_TYPE = 0x2A,
  kOpcodeDRAW_INDEX = 0x2B,
  kOpcodeDRAW_INDIRECT_MULTI = 0x2C,
  kOpcodeDRAW_INDEX_AUTO = 0x2D,
  kOpcodeDRAW_INDEX_IMMD = 0x2E,
  kOpcodeNUM_INSTANCES = 0x2F,
  kOpcodeDRAW_INDEX_MULTI_AUTO = 0x30,
  kOpcodeINDIRECT_BUFFER_32 = 0x32,
  kOpcodeINDIRECT_BUFFER_CONST = 0x33,
  kOpcodeSTRMOUT_BUFFER_UPDATE = 0x34,
  kOpcodeDRAW_INDEX_OFFSET_2 = 0x35,
  kOpcodeDRAW_PREAMBLE = 0x36,
  kOpcodeWRITE_DATA = 0x37,
  kOpcodeDRAW_INDEX_INDIRECT_MULTI = 0x38,
  kOpcodeMEM_SEMAPHORE = 0x39,
  kOpcodeMPEG_INDEX = 0x3A,
  kOpcodeCOPY_DW = 0x3B,
  kOpcodeWAIT_REG_MEM = 0x3C,
  kOpcodeMEM_WRITE = 0x3D,
  kOpcodeINDIRECT_BUFFER_3F = 0x3F,
  kOpcodeCOPY_DATA = 0x40,
  kOpcodeCP_DMA = 0x41,
  kOpcodePFP_SYNC_ME = 0x42,
  kOpcodeSURFACE_SYNC = 0x43,
  kOpcodeME_INITIALIZE = 0x44,
  kOpcodeCOND_WRITE = 0x45,
  kOpcodeEVENT_WRITE = 0x46,
  kOpcodeEVENT_WRITE_EOP = 0x47,
  kOpcodeEVENT_WRITE_EOS = 0x48,
  kOpcodeRELEASE_MEM = 0x49,
  kOpcodePREAMBLE_CNTL = 0x4A,
  kOpcodeRB_OFFSET = 0x4B,
  kOpcodeALU_PS_CONST_BUFFER_COPY = 0x4C,
  kOpcodeALU_VS_CONST_BUFFER_COPY = 0x4D,
  kOpcodeALU_PS_CONST_UPDATE = 0x4E,
  kOpcodeALU_VS_CONST_UPDATE = 0x4F,
  kOpcodeDMA_DATA = 0x50,
  kOpcodeONE_REG_WRITE = 0x57,
  kOpcodeACQUIRE_MEM = 0x58,
  kOpcodeREWIND = 0x59,
  kOpcodeLOAD_UCONFIG_REG = 0x5E,
  kOpcodeLOAD_SH_REG = 0x5F,
  kOpcodeLOAD_CONFIG_REG = 0x60,
  kOpcodeLOAD_CONTEXT_REG = 0x61,
  kOpcodeSET_CONFIG_REG = 0x68,
  kOpcodeSET_CONTEXT_REG = 0x69,
  kOpcodeSET_ALU_CONST = 0x6A,
  kOpcodeSET_BOOL_CONST = 0x6B,
  kOpcodeSET_LOOP_CONST = 0x6C,
  kOpcodeSET_RESOURCE = 0x6D,
  kOpcodeSET_SAMPLER = 0x6E,
  kOpcodeSET_CTL_CONST = 0x6F,
  kOpcodeSET_RESOURCE_OFFSET = 0x70,
  kOpcodeSET_ALU_CONST_VS = 0x71,
  kOpcodeSET_ALU_CONST_DI = 0x72,
  kOpcodeSET_CONTEXT_REG_INDIRECT = 0x73,
  kOpcodeSET_RESOURCE_INDIRECT = 0x74,
  kOpcodeSET_APPEND_CNT = 0x75,
  kOpcodeSET_SH_REG = 0x76,
  kOpcodeSET_SH_REG_OFFSET = 0x77,
  kOpcodeSET_QUEUE_REG = 0x78,
  kOpcodeSET_UCONFIG_REG = 0x79,
  kOpcodeSCRATCH_RAM_WRITE = 0x7D,
  kOpcodeSCRATCH_RAM_READ = 0x7E,
  kOpcodeLOAD_CONST_RAM = 0x80,
  kOpcodeWRITE_CONST_RAM = 0x81,
  kOpcodeDUMP_CONST_RAM = 0x83,
  kOpcodeINCREMENT_CE_COUNTER = 0x84,
  kOpcodeINCREMENT_DE_COUNTER = 0x85,
  kOpcodeWAIT_ON_CE_COUNTER = 0x86,
  kOpcodeWAIT_ON_DE_COUNTER_DIFF = 0x88,
  kOpcodeSWITCH_BUFFER = 0x8B,
};

inline const std::string opcodeToString(int op) {
  switch (op) {
  case kOpcodeNOP:
    return "IT_NOP";
  case kOpcodeSET_BASE:
    return "IT_SET_BASE";
  case kOpcodeCLEAR_STATE:
    return "IT_CLEAR_STATE";
  case kOpcodeINDEX_BUFFER_SIZE:
    return "IT_INDEX_BUFFER_SIZE";
  case kOpcodeDISPATCH_DIRECT:
    return "IT_DISPATCH_DIRECT";
  case kOpcodeDISPATCH_INDIRECT:
    return "IT_DISPATCH_INDIRECT";
  case kOpcodeINDIRECT_BUFFER_END:
    return "IT_INDIRECT_BUFFER_END";
  case kOpcodeATOMIC_GDS:
    return "IT_ATOMIC_GDS";
  case kOpcodeATOMIC_MEM:
    return "IT_ATOMIC_MEM";
  case kOpcodeOCCLUSION_QUERY:
    return "IT_OCCLUSION_QUERY";
  case kOpcodeSET_PREDICATION:
    return "IT_SET_PREDICATION";
  case kOpcodeREG_RMW:
    return "IT_REG_RMW";
  case kOpcodeCOND_EXEC:
    return "IT_COND_EXEC";
  case kOpcodePRED_EXEC:
    return "IT_PRED_EXEC";
  case kOpcodeDRAW_INDIRECT:
    return "IT_DRAW_INDIRECT";
  case kOpcodeDRAW_INDEX_INDIRECT:
    return "IT_DRAW_INDEX_INDIRECT";
  case kOpcodeINDEX_BASE:
    return "IT_INDEX_BASE";
  case kOpcodeDRAW_INDEX_2:
    return "IT_DRAW_INDEX_2";
  case kOpcodeCONTEXT_CONTROL:
    return "IT_CONTEXT_CONTROL";
  case kOpcodeINDEX_TYPE:
    return "IT_INDEX_TYPE";
  case kOpcodeDRAW_INDEX:
    return "IT_DRAW_INDEX";
  case kOpcodeDRAW_INDIRECT_MULTI:
    return "IT_DRAW_INDIRECT_MULTI";
  case kOpcodeDRAW_INDEX_AUTO:
    return "IT_DRAW_INDEX_AUTO";
  case kOpcodeDRAW_INDEX_IMMD:
    return "IT_DRAW_INDEX_IMMD";
  case kOpcodeNUM_INSTANCES:
    return "IT_NUM_INSTANCES";
  case kOpcodeDRAW_INDEX_MULTI_AUTO:
    return "IT_DRAW_INDEX_MULTI_AUTO";
  case kOpcodeINDIRECT_BUFFER_32:
    return "IT_INDIRECT_BUFFER_32";
  case kOpcodeINDIRECT_BUFFER_CONST:
    return "IT_INDIRECT_BUFFER_CONST";
  case kOpcodeSTRMOUT_BUFFER_UPDATE:
    return "IT_STRMOUT_BUFFER_UPDATE";
  case kOpcodeDRAW_INDEX_OFFSET_2:
    return "IT_DRAW_INDEX_OFFSET_2";
  case kOpcodeDRAW_PREAMBLE:
    return "IT_DRAW_PREAMBLE";
  case kOpcodeWRITE_DATA:
    return "IT_WRITE_DATA";
  case kOpcodeDRAW_INDEX_INDIRECT_MULTI:
    return "IT_DRAW_INDEX_INDIRECT_MULTI";
  case kOpcodeMEM_SEMAPHORE:
    return "IT_MEM_SEMAPHORE";
  case kOpcodeMPEG_INDEX:
    return "IT_MPEG_INDEX";
  case kOpcodeCOPY_DW:
    return "IT_COPY_DW";
  case kOpcodeWAIT_REG_MEM:
    return "IT_WAIT_REG_MEM";
  case kOpcodeMEM_WRITE:
    return "IT_MEM_WRITE";
  case kOpcodeINDIRECT_BUFFER_3F:
    return "IT_INDIRECT_BUFFER_3F";
  case kOpcodeCOPY_DATA:
    return "IT_COPY_DATA";
  case kOpcodeCP_DMA:
    return "IT_CP_DMA";
  case kOpcodePFP_SYNC_ME:
    return "IT_PFP_SYNC_ME";
  case kOpcodeSURFACE_SYNC:
    return "IT_SURFACE_SYNC";
  case kOpcodeME_INITIALIZE:
    return "IT_ME_INITIALIZE";
  case kOpcodeCOND_WRITE:
    return "IT_COND_WRITE";
  case kOpcodeEVENT_WRITE:
    return "IT_EVENT_WRITE";
  case kOpcodeEVENT_WRITE_EOP:
    return "IT_EVENT_WRITE_EOP";
  case kOpcodeEVENT_WRITE_EOS:
    return "IT_EVENT_WRITE_EOS";
  case kOpcodeRELEASE_MEM:
    return "IT_RELEASE_MEM";
  case kOpcodePREAMBLE_CNTL:
    return "IT_PREAMBLE_CNTL";
  case kOpcodeDMA_DATA:
    return "IT_DMA_DATA";
  case kOpcodeONE_REG_WRITE:
    return "IT_ONE_REG_WRITE";
  case kOpcodeACQUIRE_MEM:
    return "IT_ACQUIRE_MEM";
  case kOpcodeREWIND:
    return "IT_REWIND";
  case kOpcodeLOAD_UCONFIG_REG:
    return "IT_LOAD_UCONFIG_REG";
  case kOpcodeLOAD_SH_REG:
    return "IT_LOAD_SH_REG";
  case kOpcodeLOAD_CONFIG_REG:
    return "IT_LOAD_CONFIG_REG";
  case kOpcodeLOAD_CONTEXT_REG:
    return "IT_LOAD_CONTEXT_REG";
  case kOpcodeSET_CONFIG_REG:
    return "IT_SET_CONFIG_REG";
  case kOpcodeSET_CONTEXT_REG:
    return "IT_SET_CONTEXT_REG";
  case kOpcodeSET_ALU_CONST:
    return "IT_SET_ALU_CONST";
  case kOpcodeSET_BOOL_CONST:
    return "IT_SET_BOOL_CONST";
  case kOpcodeSET_LOOP_CONST:
    return "IT_SET_LOOP_CONST";
  case kOpcodeSET_RESOURCE:
    return "IT_SET_RESOURCE";
  case kOpcodeSET_SAMPLER:
    return "IT_SET_SAMPLER";
  case kOpcodeSET_CTL_CONST:
    return "IT_SET_CTL_CONST";
  case kOpcodeSET_CONTEXT_REG_INDIRECT:
    return "IT_SET_CONTEXT_REG_INDIRECT";
  case kOpcodeSET_SH_REG:
    return "IT_SET_SH_REG";
  case kOpcodeSET_SH_REG_OFFSET:
    return "IT_SET_SH_REG_OFFSET";
  case kOpcodeSET_QUEUE_REG:
    return "IT_SET_QUEUE_REG";
  case kOpcodeSET_UCONFIG_REG:
    return "IT_SET_UCONFIG_REG";
  case kOpcodeSCRATCH_RAM_WRITE:
    return "IT_SCRATCH_RAM_WRITE";
  case kOpcodeSCRATCH_RAM_READ:
    return "IT_SCRATCH_RAM_READ";
  case kOpcodeLOAD_CONST_RAM:
    return "IT_LOAD_CONST_RAM";
  case kOpcodeWRITE_CONST_RAM:
    return "IT_WRITE_CONST_RAM";
  case kOpcodeDUMP_CONST_RAM:
    return "IT_DUMP_CONST_RAM";
  case kOpcodeINCREMENT_CE_COUNTER:
    return "IT_INCREMENT_CE_COUNTER";
  case kOpcodeINCREMENT_DE_COUNTER:
    return "IT_INCREMENT_DE_COUNTER";
  case kOpcodeWAIT_ON_CE_COUNTER:
    return "IT_WAIT_ON_CE_COUNTER";
  case kOpcodeWAIT_ON_DE_COUNTER_DIFF:
    return "IT_WAIT_ON_DE_COUNTER_DIFF";
  case kOpcodeSWITCH_BUFFER:
    return "IT_SWITCH_BUFFER";
  }

  return "<invalid " + std::to_string(op) + ">";
}

inline void dumpShader(const std::uint32_t *data) {
  flockfile(stdout);
  while (true) {
    auto instHex = *data;
    bool isEnd = instHex == 0xBF810000 || instHex == 0xBE802000;

    shader::Instruction inst(data);

    for (int i = 0; i < inst.size(); ++i) {
      std::printf("%08X ", data[i]);
    }

    inst.dump();
    printf("\n");
    data += inst.size();

    if (isEnd) {
      break;
    }
  }
  funlockfile(stdout);
}

enum BlendMultiplier {
  kBlendMultiplierZero = 0x00000000,
  kBlendMultiplierOne = 0x00000001,
  kBlendMultiplierSrcColor = 0x00000002,
  kBlendMultiplierOneMinusSrcColor = 0x00000003,
  kBlendMultiplierSrcAlpha = 0x00000004,
  kBlendMultiplierOneMinusSrcAlpha = 0x00000005,
  kBlendMultiplierDestAlpha = 0x00000006,
  kBlendMultiplierOneMinusDestAlpha = 0x00000007,
  kBlendMultiplierDestColor = 0x00000008,
  kBlendMultiplierOneMinusDestColor = 0x00000009,
  kBlendMultiplierSrcAlphaSaturate = 0x0000000a,
  kBlendMultiplierConstantColor = 0x0000000d,
  kBlendMultiplierOneMinusConstantColor = 0x0000000e,
  kBlendMultiplierSrc1Color = 0x0000000f,
  kBlendMultiplierInverseSrc1Color = 0x00000010,
  kBlendMultiplierSrc1Alpha = 0x00000011,
  kBlendMultiplierInverseSrc1Alpha = 0x00000012,
  kBlendMultiplierConstantAlpha = 0x00000013,
  kBlendMultiplierOneMinusConstantAlpha = 0x00000014,
};

enum BlendFunc {
  kBlendFuncAdd = 0x00000000,
  kBlendFuncSubtract = 0x00000001,
  kBlendFuncMin = 0x00000002,
  kBlendFuncMax = 0x00000003,
  kBlendFuncReverseSubtract = 0x00000004,
};

enum PrimitiveType : unsigned {
  kPrimitiveTypeNone = 0x00000000,
  kPrimitiveTypePointList = 0x00000001,
  kPrimitiveTypeLineList = 0x00000002,
  kPrimitiveTypeLineStrip = 0x00000003,
  kPrimitiveTypeTriList = 0x00000004,
  kPrimitiveTypeTriFan = 0x00000005,
  kPrimitiveTypeTriStrip = 0x00000006,
  kPrimitiveTypePatch = 0x00000009,
  kPrimitiveTypeLineListAdjacency = 0x0000000a,
  kPrimitiveTypeLineStripAdjacency = 0x0000000b,
  kPrimitiveTypeTriListAdjacency = 0x0000000c,
  kPrimitiveTypeTriStripAdjacency = 0x0000000d,
  kPrimitiveTypeRectList = 0x00000011,
  kPrimitiveTypeLineLoop = 0x00000012,
  kPrimitiveTypeQuadList = 0x00000013,
  kPrimitiveTypeQuadStrip = 0x00000014,
  kPrimitiveTypePolygon = 0x00000015
};

enum SurfaceFormat : unsigned {
  kSurfaceFormatInvalid = 0x00000000,
  kSurfaceFormat8 = 0x00000001,
  kSurfaceFormat16 = 0x00000002,
  kSurfaceFormat8_8 = 0x00000003,
  kSurfaceFormat32 = 0x00000004,
  kSurfaceFormat16_16 = 0x00000005,
  kSurfaceFormat10_11_11 = 0x00000006,
  kSurfaceFormat11_11_10 = 0x00000007,
  kSurfaceFormat10_10_10_2 = 0x00000008,
  kSurfaceFormat2_10_10_10 = 0x00000009,
  kSurfaceFormat8_8_8_8 = 0x0000000a,
  kSurfaceFormat32_32 = 0x0000000b,
  kSurfaceFormat16_16_16_16 = 0x0000000c,
  kSurfaceFormat32_32_32 = 0x0000000d,
  kSurfaceFormat32_32_32_32 = 0x0000000e,
  kSurfaceFormat5_6_5 = 0x00000010,
  kSurfaceFormat1_5_5_5 = 0x00000011,
  kSurfaceFormat5_5_5_1 = 0x00000012,
  kSurfaceFormat4_4_4_4 = 0x00000013,
  kSurfaceFormat8_24 = 0x00000014,
  kSurfaceFormat24_8 = 0x00000015,
  kSurfaceFormatX24_8_32 = 0x00000016,
  kSurfaceFormatGB_GR = 0x00000020,
  kSurfaceFormatBG_RG = 0x00000021,
  kSurfaceFormat5_9_9_9 = 0x00000022,
  kSurfaceFormatBc1 = 0x00000023,
  kSurfaceFormatBc2 = 0x00000024,
  kSurfaceFormatBc3 = 0x00000025,
  kSurfaceFormatBc4 = 0x00000026,
  kSurfaceFormatBc5 = 0x00000027,
  kSurfaceFormatBc6 = 0x00000028,
  kSurfaceFormatBc7 = 0x00000029,
  kSurfaceFormatFmask8_S2_F1 = 0x0000002C,
  kSurfaceFormatFmask8_S4_F1 = 0x0000002D,
  kSurfaceFormatFmask8_S8_F1 = 0x0000002E,
  kSurfaceFormatFmask8_S2_F2 = 0x0000002F,
  kSurfaceFormatFmask8_S4_F2 = 0x00000030,
  kSurfaceFormatFmask8_S4_F4 = 0x00000031,
  kSurfaceFormatFmask16_S16_F1 = 0x00000032,
  kSurfaceFormatFmask16_S8_F2 = 0x00000033,
  kSurfaceFormatFmask32_S16_F2 = 0x00000034,
  kSurfaceFormatFmask32_S8_F4 = 0x00000035,
  kSurfaceFormatFmask32_S8_F8 = 0x00000036,
  kSurfaceFormatFmask64_S16_F4 = 0x00000037,
  kSurfaceFormatFmask64_S16_F8 = 0x00000038,
  kSurfaceFormat4_4 = 0x00000039,
  kSurfaceFormat6_5_5 = 0x0000003A,
  kSurfaceFormat1 = 0x0000003B,
  kSurfaceFormat1Reversed = 0x0000003C,
};

enum TextureChannelType : unsigned {
  kTextureChannelTypeUNorm = 0x00000000,
  kTextureChannelTypeSNorm = 0x00000001,
  kTextureChannelTypeUScaled = 0x00000002,
  kTextureChannelTypeSScaled = 0x00000003,
  kTextureChannelTypeUInt = 0x00000004,
  kTextureChannelTypeSInt = 0x00000005,
  kTextureChannelTypeSNormNoZero = 0x00000006,
  kTextureChannelTypeFloat = 0x00000007,
  kTextureChannelTypeSrgb = 0x00000009,
  kTextureChannelTypeUBNorm = 0x0000000A,
  kTextureChannelTypeUBNormNoZero = 0x0000000B,
  kTextureChannelTypeUBInt = 0x0000000C,
  kTextureChannelTypeUBScaled = 0x0000000D,
};

struct GnmVBuffer {
  uint64_t base : 44;
  uint64_t mtype_L1s : 2;
  uint64_t mtype_L2 : 2;
  uint64_t stride : 14;
  uint64_t cache_swizzle : 1;
  uint64_t swizzle_en : 1;

  uint32_t num_records;

  uint32_t dst_sel_x : 3;
  uint32_t dst_sel_y : 3;
  uint32_t dst_sel_z : 3;
  uint32_t dst_sel_w : 3;

  uint32_t nfmt : 3;
  uint32_t dfmt : 4;
  uint32_t element_size : 2;
  uint32_t index_stride : 2;
  uint32_t addtid_en : 1;
  uint32_t reserved0 : 1;
  uint32_t hash_en : 1;
  uint32_t reserved1 : 1;
  uint32_t mtype : 3;
  uint32_t type : 2;

  std::uint64_t getAddress() const { return base; }

  uint32_t getStride() const { return stride; }

  uint32_t getSize() const {
    uint32_t stride = getStride();
    uint32_t numElements = getNumRecords();
    return stride ? numElements * stride : numElements;
  }

  uint32_t getNumRecords() const { return num_records; }
  uint32_t getElementSize() const { return element_size; }
  uint32_t getIndexStrideSize() const { return index_stride; }
  SurfaceFormat getSurfaceFormat() const { return (SurfaceFormat)dfmt; }
  TextureChannelType getChannelType() const { return (TextureChannelType)nfmt; }
};

static_assert(sizeof(GnmVBuffer) == sizeof(std::uint64_t) * 2);

enum class TextureType : uint64_t {
  Dim1D = 8,
  Dim2D,
  Dim3D,
  Cube,
  Array1D,
  Array2D,
  Msaa2D,
  MsaaArray2D,
};

struct GnmTBuffer {
  uint64_t baseaddr256 : 38;
  uint64_t mtype_L2 : 2;
  uint64_t min_lod : 12;
  SurfaceFormat dfmt : 6;
  TextureChannelType nfmt : 4;
  uint64_t mtype01 : 2;

  uint64_t width : 14;
  uint64_t height : 14;
  uint64_t perfMod : 3;
  uint64_t interlaced : 1;
  uint64_t dst_sel_x : 3;
  uint64_t dst_sel_y : 3;
  uint64_t dst_sel_z : 3;
  uint64_t dst_sel_w : 3;
  uint64_t base_level : 4;
  uint64_t last_level : 4;
  uint64_t tiling_idx : 5;
  uint64_t pow2pad : 1;
  uint64_t mtype2 : 1;
  uint64_t : 1; // reserved
  TextureType type : 4;

  uint64_t depth : 13;
  uint64_t pitch : 14;
  uint64_t : 5; // reserved
  uint64_t base_array : 13;
  uint64_t last_array : 13;
  uint64_t : 6; // reserved

  uint64_t min_lod_warn : 12; // fixed point 4.8
  uint64_t counter_bank_id : 8;
  uint64_t LOD_hdw_cnt_en : 1;
  uint64_t : 42; // reserved

  std::uint64_t getAddress() const {
    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(baseaddr256))
           << 8;
  }
};

static_assert(sizeof(GnmTBuffer) == sizeof(std::uint64_t) * 4);

constexpr auto kPageSize = 0x4000;

void setVkDevice(VkDevice device,
                 VkPhysicalDeviceMemoryProperties memProperties,
                 VkPhysicalDeviceProperties devProperties);

struct AmdgpuDevice {
  void handleProtectMemory(std::uint64_t address, std::uint64_t size,
                           std::uint32_t prot);
  void handleCommandBuffer(std::uint64_t queueId, std::uint64_t address,
                           std::uint64_t size);
  bool handleFlip(VkQueue queue, VkCommandBuffer cmdBuffer,
                  TaskChain &initTaskChain, std::uint32_t bufferIndex,
                  std::uint64_t arg, VkImage targetImage,
                  VkExtent2D targetExtent, VkSemaphore waitSemaphore,
                  VkSemaphore signalSemaphore, VkFence fence);

  AmdgpuDevice(amdgpu::bridge::BridgeHeader *bridge);

  ~AmdgpuDevice();
};
} // namespace amdgpu::device
