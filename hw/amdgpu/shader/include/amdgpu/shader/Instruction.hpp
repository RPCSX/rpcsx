#pragma once

#include <bit>
#include <cstdint>

namespace amdgpu::shader {
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

enum SurfaceFormat {
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
};
enum TextureChannelType {
  kTextureChannelTypeUNorm = 0x00000000,
  kTextureChannelTypeSNorm = 0x00000001,
  kTextureChannelTypeUScaled = 0x00000002,
  kTextureChannelTypeSScaled = 0x00000003,
  kTextureChannelTypeUInt = 0x00000004,
  kTextureChannelTypeSInt = 0x00000005,
  kTextureChannelTypeSNormNoZero = 0x00000006,
  kTextureChannelTypeFloat = 0x00000007,
};

inline int getScalarInstSize(int id) { return id == 255 ? 1 : 0; }

struct Sop1 {
  enum class Op {
    S_MOV_B32 = 3,
    S_MOV_B64,
    S_CMOV_B32,
    S_CMOV_B64,
    S_NOT_B32,
    S_NOT_B64,
    S_WQM_B32,
    S_WQM_B64,
    S_BREV_B32,
    S_BREV_B64,
    S_BCNT0_I32_B32,
    S_BCNT0_I32_B64,
    S_BCNT1_I32_B32,
    S_BCNT1_I32_B64,
    S_FF0_I32_B32,
    S_FF0_I32_B64,
    S_FF1_I32_B32,
    S_FF1_I32_B64,
    S_FLBIT_I32_B32,
    S_FLBIT_I32_B64,
    S_FLBIT_I32,
    S_FLBIT_I32_I64,
    S_SEXT_I32_I8,
    S_SEXT_I32_I16,
    S_BITSET0_B32,
    S_BITSET0_B64,
    S_BITSET1_B32,
    S_BITSET1_B64,
    S_GETPC_B64,
    S_SETPC_B64,
    S_SWAPPC_B64,
    S_RFE_B64,
    S_AND_SAVEEXEC_B64 = 36,
    S_OR_SAVEEXEC_B64,
    S_XOR_SAVEEXEC_B64,
    S_ANDN2_SAVEEXEC_B64,
    S_ORN2_SAVEEXEC_B64,
    S_NAND_SAVEEXEC_B64,
    S_NOR_SAVEEXEC_B64,
    S_XNOR_SAVEEXEC_B64,
    S_QUADMASK_B32,
    S_QUADMASK_B64,
    S_MOVRELS_B32,
    S_MOVRELS_B64,
    S_MOVRELD_B32,
    S_MOVRELD_B64,
    S_CBRANCH_JOIN,
    S_ABS_I32 = 52,
    S_MOV_FED_B32,
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto ssrc0Mask = genMask(0, 8);
  static constexpr auto opMask = genMask(getMaskEnd(ssrc0Mask), 8);
  static constexpr auto sdstMask = genMask(getMaskEnd(opMask), 7);

  const std::uint32_t *inst;

  const std::uint32_t ssrc0 = fetchMaskedValue(inst[0], ssrc0Mask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));
  const std::uint32_t sdst = fetchMaskedValue(inst[0], sdstMask);

  Sop1(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize + getScalarInstSize(ssrc0); }

  void dump() const;
};

struct Sopk {
  enum class Op {
    S_MOVK_I32,
    S_CMOVK_I32 = 2,
    S_CMPK_EQ_I32,
    S_CMPK_LG_I32,
    S_CMPK_GT_I32,
    S_CMPK_GE_I32,
    S_CMPK_LT_I32,
    S_CMPK_LE_I32,
    S_CMPK_EQ_U32,
    S_CMPK_LG_U32,
    S_CMPK_GT_U32,
    S_CMPK_GE_U32,
    S_CMPK_LT_U32,
    S_CMPK_LE_U32,
    S_ADDK_I32,
    S_MULK_I32,
    S_CBRANCH_I_FORK,
    S_GETREG_B32,
    S_SETREG_B32,
    S_SETREG_IMM
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto simmMask = genMask(0, 16);
  static constexpr auto sdstMask = genMask(getMaskEnd(simmMask), 7);
  static constexpr auto opMask = genMask(getMaskEnd(sdstMask), 5);

  const std::uint32_t *inst;

  const std::int16_t simm = (std::int16_t)fetchMaskedValue(inst[0], simmMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));
  const std::uint32_t sdst = fetchMaskedValue(inst[0], sdstMask);

  Sopk(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Sopc {
  enum class Op {
    S_CMP_EQ_I32,
    S_CMP_LG_I32,
    S_CMP_GT_I32,
    S_CMP_GE_I32,
    S_CMP_LT_I32,
    S_CMP_LE_I32,
    S_CMP_EQ_U32,
    S_CMP_LG_U32,
    S_CMP_GT_U32,
    S_CMP_GE_U32,
    S_CMP_LT_U32,
    S_CMP_LE_U32,
    S_BITCMP0_B32,
    S_BITCMP1_B32,
    S_BITCMP0_B64,
    S_BITCMP1_B64,
    S_SETVSKIP,
    S_ILLEGALD
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto ssrc0Mask = genMask(0, 8);
  static constexpr auto ssrc1Mask = genMask(getMaskEnd(ssrc0Mask), 8);
  static constexpr auto opMask = genMask(getMaskEnd(ssrc1Mask), 7);

  const std::uint32_t *inst;

  const std::uint32_t ssrc0 = fetchMaskedValue(inst[0], ssrc0Mask);
  const std::uint32_t ssrc1 = fetchMaskedValue(inst[0], ssrc1Mask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Sopc(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize + getScalarInstSize(ssrc0); }

  void dump() const;
};

struct Sop2 {
  enum class Op {
    S_ADD_U32,
    S_SUB_U32,
    S_ADD_I32,
    S_SUB_I32,
    S_ADDC_U32,
    S_SUBB_U32,
    S_MIN_I32,
    S_MIN_U32,
    S_MAX_I32,
    S_MAX_U32,
    S_CSELECT_B32,
    S_CSELECT_B64,
    S_AND_B32 = 14,
    S_AND_B64,
    S_OR_B32,
    S_OR_B64,
    S_XOR_B32,
    S_XOR_B64,
    S_ANDN2_B32,
    S_ANDN2_B64,
    S_ORN2_B32,
    S_ORN2_B64,
    S_NAND_B32,
    S_NAND_B64,
    S_NOR_B32,
    S_NOR_B64,
    S_XNOR_B32,
    S_XNOR_B64,
    S_LSHL_B32,
    S_LSHL_B64,
    S_LSHR_B32,
    S_LSHR_B64,
    S_ASHR_I32,
    S_ASHR_I64,
    S_BFM_B32,
    S_BFM_B64,
    S_MUL_I32,
    S_BFE_U32,
    S_BFE_I32,
    S_BFE_U64,
    S_BFE_I64,
    S_CBRANCH_G_FORK,
    S_ABSDIFF_I32,
    S_LSHL1_ADD_U32,
    S_LSHL2_ADD_U32,
    S_LSHL3_ADD_U32,
    S_LSHL4_ADD_U32,
    S_PACK_LL_B32_B16,
    S_PACK_LH_B32_B16,
    S_PACK_HH_B32_B16,
    S_MUL_HI_U32,
    S_MUL_HI_I32,
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto ssrc0Mask = genMask(0, 8);
  static constexpr auto ssrc1Mask = genMask(getMaskEnd(ssrc0Mask), 8);
  static constexpr auto sdstMask = genMask(getMaskEnd(ssrc1Mask), 7);
  static constexpr auto opMask = genMask(getMaskEnd(sdstMask), 7);

  const std::uint32_t *inst;
  const std::uint32_t ssrc0 = fetchMaskedValue(inst[0], ssrc0Mask);
  const std::uint32_t ssrc1 = fetchMaskedValue(inst[0], ssrc1Mask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));
  const std::uint32_t sdst = fetchMaskedValue(inst[0], sdstMask);

  Sop2(const std::uint32_t *inst) : inst(inst) {}

  int size() const {
    return kMinInstSize + getScalarInstSize(ssrc0) + getScalarInstSize(ssrc1);
  }

  void dump() const;
};

struct Sopp {
  enum class Op {
    S_NOP,
    S_ENDPGM,
    S_BRANCH,
    S_CBRANCH_SCC0 = 4,
    S_CBRANCH_SCC1,
    S_CBRANCH_VCCZ,
    S_CBRANCH_VCCNZ,
    S_CBRANCH_EXECZ,
    S_CBRANCH_EXECNZ,
    S_BARRIER,
    S_WAITCNT = 12,
    S_SETHALT,
    S_SLEEP,
    S_SETPRIO,
    S_SENDMSG,
    S_SENDMSGHALT,
    S_TRAP,
    S_ICACHE_INV,
    S_INCPERFLEVEL,
    S_DECPERFLEVEL,
    S_TTRACEDATA,
    S_CBRANCH_CDBGSYS = 23,
    S_CBRANCH_CDBGUSER = 24,
    S_CBRANCH_CDBGSYS_OR_USER = 25,
    S_CBRANCH_CDBGSYS_AND_USER = 26,
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto simmMask = genMask(0, 16);
  static constexpr auto opMask = genMask(getMaskEnd(simmMask), 7);

  const std::uint32_t *inst;
  const std::int16_t simm = (std::int16_t)fetchMaskedValue(inst[0], simmMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Sopp(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Vop1 {
  enum class Op {
    V_NOP,
    V_MOV_B32,
    V_READFIRSTLANE_B32,
    V_CVT_I32_F64,
    V_CVT_F64_I32,
    V_CVT_F32_I32,
    V_CVT_F32_U32,
    V_CVT_U32_F32,
    V_CVT_I32_F32,
    V_MOV_FED_B32,
    V_CVT_F16_F32,
    V_CVT_F32_F16,
    V_CVT_RPI_I32_F32,
    V_CVT_FLR_I32_F32,
    V_CVT_OFF_F32_I4,
    V_CVT_F32_F64,
    V_CVT_F64_F32,
    V_CVT_F32_UBYTE0,
    V_CVT_F32_UBYTE1,
    V_CVT_F32_UBYTE2,
    V_CVT_F32_UBYTE3,
    V_CVT_U32_F64,
    V_CVT_F64_U32,
    V_FRACT_F32 = 32,
    V_TRUNC_F32,
    V_CEIL_F32,
    V_RNDNE_F32,
    V_FLOOR_F32,
    V_EXP_F32,
    V_LOG_CLAMP_F32,
    V_LOG_F32,
    V_RCP_CLAMP_F32,
    V_RCP_LEGACY_F32,
    V_RCP_F32,
    V_RCP_IFLAG_F32,
    V_RSQ_CLAMP_F32,
    V_RSQ_LEGACY_F32,
    V_RSQ_F32,
    V_RCP_F64,
    V_RCP_CLAMP_F64,
    V_RSQ_F64,
    V_RSQ_CLAMP_F64,
    V_SQRT_F32,
    V_SQRT_F64,
    V_SIN_F32,
    V_COS_F32,
    V_NOT_B32,
    V_BFREV_B32,
    V_FFBH_U32,
    V_FFBL_B32,
    V_FFBH_I32,
    V_FREXP_EXP_I32_F64,
    V_FREXP_MANT_F64,
    V_FRACT_F64,
    V_FREXP_EXP_I32_F32,
    V_FREXP_MANT_F32,
    V_CLREXCP,
    V_MOVRELD_B32,
    V_MOVRELS_B32,
    V_MOVRELSD_B32,
    V_CVT_F16_U16 = 80,
    V_CVT_F16_I16,
    V_CVT_U16_F16,
    V_CVT_I16_F16,
    V_RCP_F16,
    V_SQRT_F16,
    V_RSQ_F16,
    V_LOG_F16,
    V_EXP_F16,
    V_FREXP_MANT_F16,
    V_FREXP_EXP_I16_F16,
    V_FLOOR_F16,
    V_CEIL_F16,
    V_TRUNC_F16,
    V_RNDNE_F16,
    V_FRACT_F16,
    V_SIN_F16,
    V_COS_F16,
    V_SAT_PK_U8_I16,
    V_CVT_NORM_I16_F16,
    V_CVT_NORM_U16_F16,
    V_SWAP_B32,
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto src0Mask = genMask(0, 9);
  static constexpr auto opMask = genMask(getMaskEnd(src0Mask), 8);
  static constexpr auto vdstMask = genMask(getMaskEnd(opMask), 8);

  const std::uint32_t *inst;
  const std::uint32_t src0 = fetchMaskedValue(inst[0], src0Mask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));
  const std::uint32_t vdst = fetchMaskedValue(inst[0], vdstMask);

  int size() const { return kMinInstSize + getScalarInstSize(src0); }

  Vop1(const std::uint32_t *inst) : inst(inst) {}

  void dump() const;
};

struct Vop2 {
  enum class Op {
    V_CNDMASK_B32,
    V_READLANE_B32,
    V_WRITELANE_B32,
    V_ADD_F32,
    V_SUB_F32,
    V_SUBREV_F32,
    V_MAC_LEGACY_F32,
    V_MUL_LEGACY_F32,
    V_MUL_F32,
    V_MUL_I32_I24,
    V_MUL_HI_I32_I24,
    V_MUL_U32_U24,
    V_MUL_HI_U32_U24,
    V_MIN_LEGACY_F32,
    V_MAX_LEGACY_F32,
    V_MIN_F32,
    V_MAX_F32,
    V_MIN_I32,
    V_MAX_I32,
    V_MIN_U32,
    V_MAX_U32,
    V_LSHR_B32,
    V_LSHRREV_B32,
    V_ASHR_I32,
    V_ASHRREV_I32,
    V_LSHL_B32,
    V_LSHLREV_B32,
    V_AND_B32,
    V_OR_B32,
    V_XOR_B32,
    V_BFM_B32,
    V_MAC_F32,
    V_MADMK_F32,
    V_MADAK_F32,
    V_BCNT_U32_B32,
    V_MBCNT_LO_U32_B32,
    V_MBCNT_HI_U32_B32,
    V_ADD_I32,
    V_SUB_I32,
    V_SUBREV_I32,
    V_ADDC_U32,
    V_SUBB_U32,
    V_SUBBREV_U32,
    V_LDEXP_F32,
    V_CVT_PKACCUM_U8_F32,
    V_CVT_PKNORM_I16_F32,
    V_CVT_PKNORM_U16_F32,
    V_CVT_PKRTZ_F16_F32,
    V_CVT_PK_U16_U32,
    V_CVT_PK_I16_I32,
  };

  static constexpr int kMinInstSize = 1;
  static constexpr auto src0Mask = genMask(0, 9);
  static constexpr auto vsrc1Mask = genMask(getMaskEnd(src0Mask), 8);
  static constexpr auto vdstMask = genMask(getMaskEnd(vsrc1Mask), 8);
  static constexpr auto opMask = genMask(getMaskEnd(vdstMask), 6);

  const std::uint32_t *inst;
  const std::uint32_t src0 = fetchMaskedValue(inst[0], src0Mask);
  const std::uint32_t vsrc1 = fetchMaskedValue(inst[0], vsrc1Mask);
  const std::uint32_t vdst = fetchMaskedValue(inst[0], vdstMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Vop2(const std::uint32_t *inst) : inst(inst) {}

  int size() const {
    int result = kMinInstSize + getScalarInstSize(src0);

    if (op == Vop2::Op::V_MADMK_F32 || op == Vop2::Op::V_MADAK_F32) {
      result += 1;
    }

    return result;
  }
  void dump() const;
};

struct Vop3 {
  enum class Op {
    V3_CMP_F_F32,
    V3_CMP_LT_F32,
    V3_CMP_EQ_F32,
    V3_CMP_LE_F32,
    V3_CMP_GT_F32,
    V3_CMP_LG_F32,
    V3_CMP_GE_F32,
    V3_CMP_O_F32,
    V3_CMP_U_F32,
    V3_CMP_NGE_F32,
    V3_CMP_NLG_F32,
    V3_CMP_NGT_F32,
    V3_CMP_NLE_F32,
    V3_CMP_NEQ_F32,
    V3_CMP_NLT_F32,
    V3_CMP_TRU_F32,
    V3_CMPX_F_F32,
    V3_CMPX_LT_F32,
    V3_CMPX_EQ_F32,
    V3_CMPX_LE_F32,
    V3_CMPX_GT_F32,
    V3_CMPX_LG_F32,
    V3_CMPX_GE_F32,
    V3_CMPX_O_F32,
    V3_CMPX_U_F32,
    V3_CMPX_NGE_F32,
    V3_CMPX_NLG_F32,
    V3_CMPX_NGT_F32,
    V3_CMPX_NLE_F32,
    V3_CMPX_NEQ_F32,
    V3_CMPX_NLT_F32,
    V3_CMPX_TRU_F32,
    V3_CMP_F_F64,
    V3_CMP_LT_F64,
    V3_CMP_EQ_F64,
    V3_CMP_LE_F64,
    V3_CMP_GT_F64,
    V3_CMP_LG_F64,
    V3_CMP_GE_F64,
    V3_CMP_O_F64,
    V3_CMP_U_F64,
    V3_CMP_NGE_F64,
    V3_CMP_NLG_F64,
    V3_CMP_NGT_F64,
    V3_CMP_NLE_F64,
    V3_CMP_NEQ_F64,
    V3_CMP_NLT_F64,
    V3_CMP_TRU_F64,
    V3_CMPX_F_F64,
    V3_CMPX_LT_F64,
    V3_CMPX_EQ_F64,
    V3_CMPX_LE_F64,
    V3_CMPX_GT_F64,
    V3_CMPX_LG_F64,
    V3_CMPX_GE_F64,
    V3_CMPX_O_F64,
    V3_CMPX_U_F64,
    V3_CMPX_NGE_F64,
    V3_CMPX_NLG_F64,
    V3_CMPX_NGT_F64,
    V3_CMPX_NLE_F64,
    V3_CMPX_NEQ_F64,
    V3_CMPX_NLT_F64,
    V3_CMPX_TRU_F64,
    V3_CMPS_F_F32,
    V3_CMPS_LT_F32,
    V3_CMPS_EQ_F32,
    V3_CMPS_LE_F32,
    V3_CMPS_GT_F32,
    V3_CMPS_LG_F32,
    V3_CMPS_GE_F32,
    V3_CMPS_O_F32,
    V3_CMPS_U_F32,
    V3_CMPS_NGE_F32,
    V3_CMPS_NLG_F32,
    V3_CMPS_NGT_F32,
    V3_CMPS_NLE_F32,
    V3_CMPS_NEQ_F32,
    V3_CMPS_NLT_F32,
    V3_CMPS_TRU_F32,
    V3_CMPSX_F_F32,
    V3_CMPSX_LT_F32,
    V3_CMPSX_EQ_F32,
    V3_CMPSX_LE_F32,
    V3_CMPSX_GT_F32,
    V3_CMPSX_LG_F32,
    V3_CMPSX_GE_F32,
    V3_CMPSX_O_F32,
    V3_CMPSX_U_F32,
    V3_CMPSX_NGE_F32,
    V3_CMPSX_NLG_F32,
    V3_CMPSX_NGT_F32,
    V3_CMPSX_NLE_F32,
    V3_CMPSX_NEQ_F32,
    V3_CMPSX_NLT_F32,
    V3_CMPSX_TRU_F32,
    V3_CMPS_F_F64,
    V3_CMPS_LT_F64,
    V3_CMPS_EQ_F64,
    V3_CMPS_LE_F64,
    V3_CMPS_GT_F64,
    V3_CMPS_LG_F64,
    V3_CMPS_GE_F64,
    V3_CMPS_O_F64,
    V3_CMPS_U_F64,
    V3_CMPS_NGE_F64,
    V3_CMPS_NLG_F64,
    V3_CMPS_NGT_F64,
    V3_CMPS_NLE_F64,
    V3_CMPS_NEQ_F64,
    V3_CMPS_NLT_F64,
    V3_CMPS_TRU_F64,
    V3_CMPSX_F_F64,
    V3_CMPSX_LT_F64,
    V3_CMPSX_EQ_F64,
    V3_CMPSX_LE_F64,
    V3_CMPSX_GT_F64,
    V3_CMPSX_LG_F64,
    V3_CMPSX_GE_F64,
    V3_CMPSX_O_F64,
    V3_CMPSX_U_F64,
    V3_CMPSX_NGE_F64,
    V3_CMPSX_NLG_F64,
    V3_CMPSX_NGT_F64,
    V3_CMPSX_NLE_F64,
    V3_CMPSX_NEQ_F64,
    V3_CMPSX_NLT_F64,
    V3_CMPSX_TRU_F64,
    V3_CMP_F_I32,
    V3_CMP_LT_I32,
    V3_CMP_EQ_I32,
    V3_CMP_LE_I32,
    V3_CMP_GT_I32,
    V3_CMP_NE_I32,
    V3_CMP_GE_I32,
    V3_CMP_T_I32,
    V3_CMP_CLASS_F32,
    V3_CMP_LT_I16,
    V3_CMP_EQ_I16,
    V3_CMP_LE_I16,
    V3_CMP_GT_I16,
    V3_CMP_NE_I16,
    V3_CMP_GE_I16,
    V3_CMP_CLASS_F16,
    V3_CMPX_F_I32,
    V3_CMPX_LT_I32,
    V3_CMPX_EQ_I32,
    V3_CMPX_LE_I32,
    V3_CMPX_GT_I32,
    V3_CMPX_NE_I32,
    V3_CMPX_GE_I32,
    V3_CMPX_T_I32,
    V3_CMPX_CLASS_F32,
    V3_CMPX_LT_I16,
    V3_CMPX_EQ_I16,
    V3_CMPX_LE_I16,
    V3_CMPX_GT_I16,
    V3_CMPX_NE_I16,
    V3_CMPX_GE_I16,
    V3_CMPX_CLASS_F16,
    V3_CMP_F_I64,
    V3_CMP_LT_I64,
    V3_CMP_EQ_I64,
    V3_CMP_LE_I64,
    V3_CMP_GT_I64,
    V3_CMP_NE_I64,
    V3_CMP_GE_I64,
    V3_CMP_T_I64,
    V3_CMP_CLASS_F64,
    V3_CMP_LT_U16,
    V3_CMP_EQ_U16,
    V3_CMP_LE_U16,
    V3_CMP_GT_U16,
    V3_CMP_NE_U16,
    V3_CMP_GE_U16,
    V3_CMPX_F_I64 = 176,
    V3_CMPX_LT_I64,
    V3_CMPX_EQ_I64,
    V3_CMPX_LE_I64,
    V3_CMPX_GT_I64,
    V3_CMPX_NE_I64,
    V3_CMPX_GE_I64,
    V3_CMPX_T_I64,
    V3_CMPX_CLASS_F64,
    V3_CMPX_LT_U16,
    V3_CMPX_EQ_U16,
    V3_CMPX_LE_U16,
    V3_CMPX_GT_U16,
    V3_CMPX_NE_U16,
    V3_CMPX_GE_U16,
    V3_CMP_F_U32 = 192,
    V3_CMP_LT_U32,
    V3_CMP_EQ_U32,
    V3_CMP_LE_U32,
    V3_CMP_GT_U32,
    V3_CMP_NE_U32,
    V3_CMP_GE_U32,
    V3_CMP_T_U32,
    V3_CMP_F_F16,
    V3_CMP_LT_F16,
    V3_CMP_EQ_F16,
    V3_CMP_LE_F16,
    V3_CMP_GT_F16,
    V3_CMP_LG_F16,
    V3_CMP_GE_F16,
    V3_CMP_O_F16,
    V3_CMPX_F_U32,
    V3_CMPX_LT_U32,
    V3_CMPX_EQ_U32,
    V3_CMPX_LE_U32,
    V3_CMPX_GT_U32,
    V3_CMPX_NE_U32,
    V3_CMPX_GE_U32,
    V3_CMPX_T_U32,
    V3_CMPX_F_F16,
    V3_CMPX_LT_F16,
    V3_CMPX_EQ_F16,
    V3_CMPX_LE_F16,
    V3_CMPX_GT_F16,
    V3_CMPX_LG_F16,
    V3_CMPX_GE_F16,
    V3_CMPX_O_F16,
    V3_CMP_F_U64,
    V3_CMP_LT_U64,
    V3_CMP_EQ_U64,
    V3_CMP_LE_U64,
    V3_CMP_GT_U64,
    V3_CMP_NE_U64,
    V3_CMP_GE_U64,
    V3_CMP_T_U64,
    V3_CMP_U_F16,
    V3_CMP_NGE_F16,
    V3_CMP_NLG_F16,
    V3_CMP_NGT_F16,
    V3_CMP_NLE_F16,
    V3_CMP_NEQ_F16,
    V3_CMP_NLT_F16,
    V3_CMP_TRU_F16,
    V3_CMPX_F_U64,
    V3_CMPX_LT_U64,
    V3_CMPX_EQ_U64,
    V3_CMPX_LE_U64,
    V3_CMPX_GT_U64,
    V3_CMPX_NE_U64,
    V3_CMPX_GE_U64,
    V3_CMPX_T_U64,
    V3_CNDMASK_B32 = 256,
    V3_READLANE_B32,
    V3_WRITELANE_B32,
    V3_ADD_F32,
    V3_SUB_F32,
    V3_SUBREV_F32,
    V3_MAC_LEGACY_F32,
    V3_MUL_LEGACY_F32,
    V3_MUL_F32,
    V3_MUL_I32_I24,
    V3_MUL_HI_I32_I24,
    V3_MUL_U32_U24,
    V3_MUL_HI_U32_U24,
    V3_MIN_LEGACY_F32,
    V3_MAX_LEGACY_F32,
    V3_MIN_F32,
    V3_MAX_F32,
    V3_MIN_I32,
    V3_MAX_I32,
    V3_MIN_U32,
    V3_MAX_U32,
    V3_LSHR_B32,
    V3_LSHRREV_B32,
    V3_ASHR_I32,
    V3_ASHRREV_I32,
    V3_LSHL_B32,
    V3_LSHLREV_B32,
    V3_AND_B32,
    V3_OR_B32,
    V3_XOR_B32,
    V3_BFM_B32,
    V3_MAC_F32,
    V3_MADMK_F32,
    V3_MADAK_F32,
    V3_BCNT_U32_B32,
    V3_MBCNT_LO_U32_B32,
    V3_MBCNT_HI_U32_B32,
    V3_ADD_I32,
    V3_SUB_I32,
    V3_SUBREV_I32,
    V3_ADDC_U32,
    V3_SUBB_U32,
    V3_SUBBREV_U32,
    V3_LDEXP_F32,
    V3_CVT_PKACCUM_U8_F32,
    V3_CVT_PKNORM_I16_F32,
    V3_CVT_PKNORM_U16_F32,
    V3_CVT_PKRTZ_F16_F32,
    V3_CVT_PK_U16_U32,
    V3_CVT_PK_I16_I32,
    V3_MAD_LEGACY_F32 = 320,
    V3_MAD_F32,
    V3_MAD_I32_I24,
    V3_MAD_U32_U24,
    V3_CUBEID_F32,
    V3_CUBESC_F32,
    V3_CUBETC_F32,
    V3_CUBEMA_F32,
    V3_BFE_U32,
    V3_BFE_I32,
    V3_BFI_B32,
    V3_FMA_F32,
    V3_FMA_F64,
    V3_LERP_U8,
    V3_ALIGNBIT_B32,
    V3_ALIGNBYTE_B32,
    V3_MULLIT_F32,
    V3_MIN3_F32,
    V3_MIN3_I32,
    V3_MIN3_U32,
    V3_MAX3_F32,
    V3_MAX3_I32,
    V3_MAX3_U32,
    V3_MED3_F32,
    V3_MED3_I32,
    V3_MED3_U32,
    V3_SAD_U8,
    V3_SAD_HI_U8,
    V3_SAD_U16,
    V3_SAD_U32,
    V3_CVT_PK_U8_F32,
    V3_DIV_FIXUP_F32,
    V3_DIV_FIXUP_F64,
    V3_LSHL_B64,
    V3_LSHR_B64,
    V3_ASHR_I64,
    V3_ADD_F64,
    V3_MUL_F64,
    V3_MIN_F64,
    V3_MAX_F64,
    V3_LDEXP_F64,
    V3_MUL_LO_U32,
    V3_MUL_HI_U32,
    V3_MUL_LO_I32,
    V3_MUL_HI_I32,
    V3_DIV_SCALE_F32,
    V3_DIV_SCALE_F64,
    V3_DIV_FMAS_F32,
    V3_DIV_FMAS_F64,
    V3_MSAD_U8,
    V3_QSAD_U8,
    V3_MQSAD_U8,
    V3_TRIG_PREOP_F64,
    V3_NOP = 384,
    V3_MOV_B32,
    V3_READFIRSTLANE_B32,
    V3_CVT_I32_F64,
    V3_CVT_F64_I32,
    V3_CVT_F32_I32,
    V3_CVT_F32_U32,
    V3_CVT_U32_F32,
    V3_CVT_I32_F32,
    V3_MOV_FED_B32,
    V3_CVT_F16_F32,
    V3_CVT_F32_F16,
    V3_CVT_RPI_I32_F32,
    V3_CVT_FLR_I32_F32,
    V3_CVT_OFF_F32_I4,
    V3_CVT_F32_F64,
    V3_CVT_F64_F32,
    V3_CVT_F32_UBYTE0,
    V3_CVT_F32_UBYTE1,
    V3_CVT_F32_UBYTE2,
    V3_CVT_F32_UBYTE3,
    V3_CVT_U32_F64,
    V3_CVT_F64_U32,
    V3_FRACT_F32 = 416,
    V3_TRUNC_F32,
    V3_CEIL_F32,
    V3_RNDNE_F32,
    V3_FLOOR_F32,
    V3_EXP_F32,
    V3_LOG_CLAMP_F32,
    V3_LOG_F32,
    V3_RCP_CLAMP_F32,
    V3_RCP_LEGACY_F32,
    V3_RCP_F32,
    V3_RCP_IFLAG_F32,
    V3_RSQ_CLAMP_F32,
    V3_RSQ_LEGACY_F32,
    V3_RSQ_F32,
    V3_RCP_F64,
    V3_RCP_CLAMP_F64,
    V3_RSQ_F64,
    V3_RSQ_CLAMP_F64,
    V3_SQRT_F32,
    V3_SQRT_F64,
    V3_SIN_F32,
    V3_COS_F32,
    V3_NOT_B32,
    V3_BFREV_B32,
    V3_FFBH_U32,
    V3_FFBL_B32,
    V3_FFBH_I32,
    V3_FREXP_EXP_I32_F64,
    V3_FREXP_MANT_F64,
    V3_FRACT_F64,
    V3_FREXP_EXP_I32_F32,
    V3_FREXP_MANT_F32,
    V3_CLREXCP,
    V3_MOVRELD_B32,
    V3_MOVRELS_B32,
    V3_MOVRELSD_B32,
  };

  static constexpr int kMinInstSize = 2;
  static constexpr auto vdstMask = genMask(0, 8);

  static constexpr auto absMask = genMask(getMaskEnd(vdstMask), 3);
  static constexpr auto abs0Mask = genMask(getMaskEnd(vdstMask), 1);
  static constexpr auto abs1Mask = genMask(getMaskEnd(abs0Mask), 1);
  static constexpr auto abs2Mask = genMask(getMaskEnd(abs1Mask), 1);
  static constexpr auto clmpMask = genMask(getMaskEnd(absMask), 1);

  static constexpr auto sdstMask = genMask(getMaskEnd(vdstMask), 7);

  static_assert(getMaskEnd(clmpMask) + 5 == getMaskEnd(sdstMask) + 2);

  static constexpr auto opMask = genMask(getMaskEnd(clmpMask) + 5, 9);

  static constexpr auto src0Mask = genMask(0, 9);
  static constexpr auto src1Mask = genMask(getMaskEnd(src0Mask), 9);
  static constexpr auto src2Mask = genMask(getMaskEnd(src1Mask), 9);
  static constexpr auto omodMask = genMask(getMaskEnd(src2Mask), 2);
  static constexpr auto negMask = genMask(getMaskEnd(omodMask), 3);
  static constexpr auto neg0Mask = genMask(getMaskEnd(omodMask), 1);
  static constexpr auto neg1Mask = genMask(getMaskEnd(neg0Mask), 1);
  static constexpr auto neg2Mask = genMask(getMaskEnd(neg1Mask), 1);

  const std::uint32_t *inst;
  const std::uint32_t vdst = fetchMaskedValue(inst[0], vdstMask);
  const std::uint32_t abs = fetchMaskedValue(inst[0], absMask);
  const std::uint32_t clmp = fetchMaskedValue(inst[0], clmpMask);
  const std::uint32_t sdst = fetchMaskedValue(inst[0], sdstMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  const std::uint32_t src0 = fetchMaskedValue(inst[1], src0Mask);
  const std::uint32_t src1 = fetchMaskedValue(inst[1], src1Mask);
  const std::uint32_t src2 = fetchMaskedValue(inst[1], src2Mask);
  const std::uint32_t omod = fetchMaskedValue(inst[1], omodMask);
  const std::uint32_t neg = fetchMaskedValue(inst[1], negMask);

  Vop3(const std::uint32_t *inst) : inst(inst) {}

  int size() const {
    return kMinInstSize + getScalarInstSize(src0) + getScalarInstSize(src1) +
           getScalarInstSize(src2);
  }

  void dump() const;
};

struct Vopc {
  enum class Op {
    V_CMP_F_F32,
    V_CMP_LT_F32,
    V_CMP_EQ_F32,
    V_CMP_LE_F32,
    V_CMP_GT_F32,
    V_CMP_LG_F32,
    V_CMP_GE_F32,
    V_CMP_O_F32,
    V_CMP_U_F32,
    V_CMP_NGE_F32,
    V_CMP_NLG_F32,
    V_CMP_NGT_F32,
    V_CMP_NLE_F32,
    V_CMP_NEQ_F32,
    V_CMP_NLT_F32,
    V_CMP_TRU_F32,
    V_CMPX_F_F32,
    V_CMPX_LT_F32,
    V_CMPX_EQ_F32,
    V_CMPX_LE_F32,
    V_CMPX_GT_F32,
    V_CMPX_LG_F32,
    V_CMPX_GE_F32,
    V_CMPX_O_F32,
    V_CMPX_U_F32,
    V_CMPX_NGE_F32,
    V_CMPX_NLG_F32,
    V_CMPX_NGT_F32,
    V_CMPX_NLE_F32,
    V_CMPX_NEQ_F32,
    V_CMPX_NLT_F32,
    V_CMPX_TRU_F32,
    V_CMP_F_F64,
    V_CMP_LT_F64,
    V_CMP_EQ_F64,
    V_CMP_LE_F64,
    V_CMP_GT_F64,
    V_CMP_LG_F64,
    V_CMP_GE_F64,
    V_CMP_O_F64,
    V_CMP_U_F64,
    V_CMP_NGE_F64,
    V_CMP_NLG_F64,
    V_CMP_NGT_F64,
    V_CMP_NLE_F64,
    V_CMP_NEQ_F64,
    V_CMP_NLT_F64,
    V_CMP_TRU_F64,
    V_CMPX_F_F64,
    V_CMPX_LT_F64,
    V_CMPX_EQ_F64,
    V_CMPX_LE_F64,
    V_CMPX_GT_F64,
    V_CMPX_LG_F64,
    V_CMPX_GE_F64,
    V_CMPX_O_F64,
    V_CMPX_U_F64,
    V_CMPX_NGE_F64,
    V_CMPX_NLG_F64,
    V_CMPX_NGT_F64,
    V_CMPX_NLE_F64,
    V_CMPX_NEQ_F64,
    V_CMPX_NLT_F64,
    V_CMPX_TRU_F64,
    V_CMPS_F_F32,
    V_CMPS_LT_F32,
    V_CMPS_EQ_F32,
    V_CMPS_LE_F32,
    V_CMPS_GT_F32,
    V_CMPS_LG_F32,
    V_CMPS_GE_F32,
    V_CMPS_O_F32,
    V_CMPS_U_F32,
    V_CMPS_NGE_F32,
    V_CMPS_NLG_F32,
    V_CMPS_NGT_F32,
    V_CMPS_NLE_F32,
    V_CMPS_NEQ_F32,
    V_CMPS_NLT_F32,
    V_CMPS_TRU_F32,
    V_CMPSX_F_F32,
    V_CMPSX_LT_F32,
    V_CMPSX_EQ_F32,
    V_CMPSX_LE_F32,
    V_CMPSX_GT_F32,
    V_CMPSX_LG_F32,
    V_CMPSX_GE_F32,
    V_CMPSX_O_F32,
    V_CMPSX_U_F32,
    V_CMPSX_NGE_F32,
    V_CMPSX_NLG_F32,
    V_CMPSX_NGT_F32,
    V_CMPSX_NLE_F32,
    V_CMPSX_NEQ_F32,
    V_CMPSX_NLT_F32,
    V_CMPSX_TRU_F32,
    V_CMPS_F_F64,
    V_CMPS_LT_F64,
    V_CMPS_EQ_F64,
    V_CMPS_LE_F64,
    V_CMPS_GT_F64,
    V_CMPS_LG_F64,
    V_CMPS_GE_F64,
    V_CMPS_O_F64,
    V_CMPS_U_F64,
    V_CMPS_NGE_F64,
    V_CMPS_NLG_F64,
    V_CMPS_NGT_F64,
    V_CMPS_NLE_F64,
    V_CMPS_NEQ_F64,
    V_CMPS_NLT_F64,
    V_CMPS_TRU_F64,
    V_CMPSX_F_F64,
    V_CMPSX_LT_F64,
    V_CMPSX_EQ_F64,
    V_CMPSX_LE_F64,
    V_CMPSX_GT_F64,
    V_CMPSX_LG_F64,
    V_CMPSX_GE_F64,
    V_CMPSX_O_F64,
    V_CMPSX_U_F64,
    V_CMPSX_NGE_F64,
    V_CMPSX_NLG_F64,
    V_CMPSX_NGT_F64,
    V_CMPSX_NLE_F64,
    V_CMPSX_NEQ_F64,
    V_CMPSX_NLT_F64,
    V_CMPSX_TRU_F64,
    V_CMP_F_I32,
    V_CMP_LT_I32,
    V_CMP_EQ_I32,
    V_CMP_LE_I32,
    V_CMP_GT_I32,
    V_CMP_NE_I32,
    V_CMP_GE_I32,
    V_CMP_T_I32,
    V_CMP_CLASS_F32,
    V_CMP_LT_I16,
    V_CMP_EQ_I16,
    V_CMP_LE_I16,
    V_CMP_GT_I16,
    V_CMP_NE_I16,
    V_CMP_GE_I16,
    V_CMP_CLASS_F16,
    V_CMPX_F_I32,
    V_CMPX_LT_I32,
    V_CMPX_EQ_I32,
    V_CMPX_LE_I32,
    V_CMPX_GT_I32,
    V_CMPX_NE_I32,
    V_CMPX_GE_I32,
    V_CMPX_T_I32,
    V_CMPX_CLASS_F32,
    V_CMPX_LT_I16,
    V_CMPX_EQ_I16,
    V_CMPX_LE_I16,
    V_CMPX_GT_I16,
    V_CMPX_NE_I16,
    V_CMPX_GE_I16,
    V_CMPX_CLASS_F16,
    V_CMP_F_I64,
    V_CMP_LT_I64,
    V_CMP_EQ_I64,
    V_CMP_LE_I64,
    V_CMP_GT_I64,
    V_CMP_NE_I64,
    V_CMP_GE_I64,
    V_CMP_T_I64,
    V_CMP_CLASS_F64,
    V_CMP_LT_U16,
    V_CMP_EQ_U16,
    V_CMP_LE_U16,
    V_CMP_GT_U16,
    V_CMP_NE_U16,
    V_CMP_GE_U16,
    V_CMPX_F_I64 = 176,
    V_CMPX_LT_I64,
    V_CMPX_EQ_I64,
    V_CMPX_LE_I64,
    V_CMPX_GT_I64,
    V_CMPX_NE_I64,
    V_CMPX_GE_I64,
    V_CMPX_T_I64,
    V_CMPX_CLASS_F64,
    V_CMPX_LT_U16,
    V_CMPX_EQ_U16,
    V_CMPX_LE_U16,
    V_CMPX_GT_U16,
    V_CMPX_NE_U16,
    V_CMPX_GE_U16,
    V_CMP_F_U32 = 192,
    V_CMP_LT_U32,
    V_CMP_EQ_U32,
    V_CMP_LE_U32,
    V_CMP_GT_U32,
    V_CMP_NE_U32,
    V_CMP_GE_U32,
    V_CMP_T_U32,
    V_CMP_F_F16,
    V_CMP_LT_F16,
    V_CMP_EQ_F16,
    V_CMP_LE_F16,
    V_CMP_GT_F16,
    V_CMP_LG_F16,
    V_CMP_GE_F16,
    V_CMP_O_F16,
    V_CMPX_F_U32,
    V_CMPX_LT_U32,
    V_CMPX_EQ_U32,
    V_CMPX_LE_U32,
    V_CMPX_GT_U32,
    V_CMPX_NE_U32,
    V_CMPX_GE_U32,
    V_CMPX_T_U32,
    V_CMPX_F_F16,
    V_CMPX_LT_F16,
    V_CMPX_EQ_F16,
    V_CMPX_LE_F16,
    V_CMPX_GT_F16,
    V_CMPX_LG_F16,
    V_CMPX_GE_F16,
    V_CMPX_O_F16,
    V_CMP_F_U64,
    V_CMP_LT_U64,
    V_CMP_EQ_U64,
    V_CMP_LE_U64,
    V_CMP_GT_U64,
    V_CMP_NE_U64,
    V_CMP_GE_U64,
    V_CMP_T_U64,
    V_CMP_U_F16,
    V_CMP_NGE_F16,
    V_CMP_NLG_F16,
    V_CMP_NGT_F16,
    V_CMP_NLE_F16,
    V_CMP_NEQ_F16,
    V_CMP_NLT_F16,
    V_CMP_TRU_F16,
    V_CMPX_F_U64,
    V_CMPX_LT_U64,
    V_CMPX_EQ_U64,
    V_CMPX_LE_U64,
    V_CMPX_GT_U64,
    V_CMPX_NE_U64,
    V_CMPX_GE_U64,
    V_CMPX_T_U64,
    V_CMPX_U_F16,
    V_CMPX_NGE_F16,
    V_CMPX_NLG_F16,
    V_CMPX_NGT_F16,
    V_CMPX_NLE_F16,
    V_CMPX_NEQ_F16,
    V_CMPX_NLT_F16,
    V_CMPX_TRU_F16,
  };

  static constexpr int kMinInstSize = 1;

  static constexpr auto src0Mask = genMask(0, 9);
  static constexpr auto vsrc1Mask = genMask(getMaskEnd(src0Mask), 8);
  static constexpr auto opMask = genMask(getMaskEnd(vsrc1Mask), 8);

  const std::uint32_t *inst;
  const std::uint16_t src0 = fetchMaskedValue(inst[0], src0Mask);
  const std::uint8_t vsrc1 = fetchMaskedValue(inst[0], vsrc1Mask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Vopc(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Smrd {
  enum class Op {
    S_LOAD_DWORD,
    S_LOAD_DWORDX2,
    S_LOAD_DWORDX4,
    S_LOAD_DWORDX8,
    S_LOAD_DWORDX16,
    S_BUFFER_LOAD_DWORD = 8,
    S_BUFFER_LOAD_DWORDX2,
    S_BUFFER_LOAD_DWORDX4,
    S_BUFFER_LOAD_DWORDX8,
    S_BUFFER_LOAD_DWORDX16,
    S_DCACHE_INV_VOL = 29,
    S_MEMTIME,
    S_DCACHE_INV,
  };

  static constexpr int kMinInstSize = 1;
  static constexpr auto offsetMask = genMask(0, 8);
  static constexpr auto immMask = genMask(getMaskEnd(offsetMask), 1);
  static constexpr auto sbaseMask = genMask(getMaskEnd(immMask), 6);
  static constexpr auto sdstMask = genMask(getMaskEnd(sbaseMask), 7);
  static constexpr auto opMask = genMask(getMaskEnd(sdstMask), 5);

  const std::uint32_t *inst;
  const std::uint32_t offset = fetchMaskedValue(inst[0], offsetMask);
  const std::uint32_t imm = fetchMaskedValue(inst[0], immMask);
  const std::uint32_t sbase = fetchMaskedValue(inst[0], sbaseMask);
  const std::uint32_t sdst = fetchMaskedValue(inst[0], sdstMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Smrd(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }
  void dump() const;
};

struct Mubuf {
  enum class Op {
    BUFFER_LOAD_FORMAT_X,
    BUFFER_LOAD_FORMAT_XY,
    BUFFER_LOAD_FORMAT_XYZ,
    BUFFER_LOAD_FORMAT_XYZW,
    BUFFER_STORE_FORMAT_X,
    BUFFER_STORE_FORMAT_XY,
    BUFFER_STORE_FORMAT_XYZ,
    BUFFER_STORE_FORMAT_XYZW,
    BUFFER_LOAD_UBYTE,
    BUFFER_LOAD_SBYTE,
    BUFFER_LOAD_USHORT,
    BUFFER_LOAD_SSHORT,
    BUFFER_LOAD_DWORD,
    BUFFER_LOAD_DWORDX2,
    BUFFER_LOAD_DWORDX4,
    BUFFER_LOAD_DWORDX3,
    BUFFER_STORE_BYTE = 24,
    BUFFER_STORE_SHORT = 26,
    BUFFER_STORE_DWORD = 28,
    BUFFER_STORE_DWORDX2,
    BUFFER_STORE_DWORDX4,
    BUFFER_STORE_DWORDX3,
    BUFFER_ATOMIC_SWAP = 48,
    BUFFER_ATOMIC_CMPSWAP,
    BUFFER_ATOMIC_ADD,
    BUFFER_ATOMIC_SUB,
    BUFFER_ATOMIC_RSUB,
    BUFFER_ATOMIC_SMIN,
    BUFFER_ATOMIC_UMIN,
    BUFFER_ATOMIC_SMAX,
    BUFFER_ATOMIC_UMAX,
    BUFFER_ATOMIC_AND,
    BUFFER_ATOMIC_OR,
    BUFFER_ATOMIC_XOR,
    BUFFER_ATOMIC_INC,
    BUFFER_ATOMIC_DEC,
    BUFFER_ATOMIC_FCMPSWAP,
    BUFFER_ATOMIC_FMIN,
    BUFFER_ATOMIC_FMAX,
    BUFFER_ATOMIC_SWAP_X2 = 80,
    BUFFER_ATOMIC_CMPSWAP_X2,
    BUFFER_ATOMIC_ADD_X2,
    BUFFER_ATOMIC_SUB_X2,
    BUFFER_ATOMIC_RSUB_X2,
    BUFFER_ATOMIC_SMIN_X2,
    BUFFER_ATOMIC_UMIN_X2,
    BUFFER_ATOMIC_SMAX_X2,
    BUFFER_ATOMIC_UMAX_X2,
    BUFFER_ATOMIC_AND_X2,
    BUFFER_ATOMIC_OR_X2,
    BUFFER_ATOMIC_XOR_X2,
    BUFFER_ATOMIC_INC_X2,
    BUFFER_ATOMIC_DEC_X2,
    BUFFER_ATOMIC_FCMPSWAP_X2,
    BUFFER_ATOMIC_FMIN_X2,
    BUFFER_ATOMIC_FMAX_X2,
    BUFFER_WBINVL1_SC_VOL = 112,
    BUFFER_WBINVL1,
  };

  static constexpr int kMinInstSize = 2;
  static constexpr auto offsetMask = genMask(0, 12);
  static constexpr auto offenMask = genMask(getMaskEnd(offsetMask), 1);
  static constexpr auto idxenMask = genMask(getMaskEnd(offenMask), 1);
  static constexpr auto glcMask = genMask(getMaskEnd(idxenMask), 1);
  static constexpr auto ldsMask = genMask(getMaskEnd(glcMask) + 1, 1);
  static constexpr auto opMask = genMask(getMaskEnd(ldsMask) + 1, 7);

  static constexpr auto vaddrMask = genMask(0, 8);
  static constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  static constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  static constexpr auto slcMask = genMask(getMaskEnd(srsrcMask) + 1, 1);
  static constexpr auto tfeMask = genMask(getMaskEnd(slcMask), 1);
  static constexpr auto soffsetMask = genMask(getMaskEnd(tfeMask), 8);

  const std::uint32_t *inst;
  std::uint16_t offset = fetchMaskedValue(inst[0], offsetMask);
  bool offen = fetchMaskedValue(inst[0], offenMask);
  bool idxen = fetchMaskedValue(inst[0], idxenMask);
  bool glc = fetchMaskedValue(inst[0], glcMask);
  bool lds = fetchMaskedValue(inst[0], ldsMask);
  Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  std::uint8_t vaddr = fetchMaskedValue(inst[1], vaddrMask);
  std::uint8_t vdata = fetchMaskedValue(inst[1], vdataMask);
  std::uint8_t srsrc = fetchMaskedValue(inst[1], srsrcMask);
  bool slc = fetchMaskedValue(inst[1], slcMask);
  bool tfe = fetchMaskedValue(inst[1], tfeMask);
  std::uint8_t soffset = fetchMaskedValue(inst[1], soffsetMask);

  Mubuf(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Mtbuf {
  enum class Op {
    TBUFFER_LOAD_FORMAT_X,
    TBUFFER_LOAD_FORMAT_XY,
    TBUFFER_LOAD_FORMAT_XYZ,
    TBUFFER_LOAD_FORMAT_XYZW,
    TBUFFER_STORE_FORMAT_X,
    TBUFFER_STORE_FORMAT_XY,
    TBUFFER_STORE_FORMAT_XYZ,
    TBUFFER_STORE_FORMAT_XYZW,
  };
  static constexpr int kMinInstSize = 2;

  static constexpr auto offsetMask = genMask(0, 12);
  static constexpr auto offenMask = genMask(getMaskEnd(offsetMask), 1);
  static constexpr auto idxenMask = genMask(getMaskEnd(offenMask), 1);
  static constexpr auto glcMask = genMask(getMaskEnd(idxenMask), 1);
  static constexpr auto opMask = genMask(getMaskEnd(glcMask) + 1, 3);
  static constexpr auto dfmtMask = genMask(getMaskEnd(opMask), 4);
  static constexpr auto nfmtMask = genMask(getMaskEnd(dfmtMask), 4);

  static constexpr auto vaddrMask = genMask(0, 8);
  static constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  static constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  static constexpr auto slcMask = genMask(getMaskEnd(srsrcMask) + 1, 1);
  static constexpr auto tfeMask = genMask(getMaskEnd(slcMask), 1);
  static constexpr auto soffsetMask = genMask(getMaskEnd(tfeMask), 8);

  const std::uint32_t *inst;
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  std::uint16_t offset = fetchMaskedValue(inst[0], offsetMask);
  bool offen = fetchMaskedValue(inst[0], offenMask);
  bool idxen = fetchMaskedValue(inst[0], idxenMask);
  bool glc = fetchMaskedValue(inst[0], glcMask);
  SurfaceFormat dfmt = (SurfaceFormat)fetchMaskedValue(inst[0], dfmtMask);
  TextureChannelType nfmt =
      (TextureChannelType)fetchMaskedValue(inst[0], nfmtMask);

  std::uint8_t vaddr = fetchMaskedValue(inst[1], vaddrMask);
  std::uint8_t vdata = fetchMaskedValue(inst[1], vdataMask);
  std::uint8_t srsrc = fetchMaskedValue(inst[1], srsrcMask);
  bool slc = fetchMaskedValue(inst[1], slcMask);
  bool tfe = fetchMaskedValue(inst[1], tfeMask);
  std::uint8_t soffset = fetchMaskedValue(inst[1], soffsetMask);

  Mtbuf(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Mimg {
  enum class Op {
    IMAGE_LOAD,
    IMAGE_LOAD_MIP,
    IMAGE_LOAD_PCK,
    IMAGE_LOAD_PCK_SGN,
    IMAGE_LOAD_MIP_PCK,
    IMAGE_LOAD_MIP_PCK_SGN,
    IMAGE_STORE = 8,
    IMAGE_STORE_MIP,
    IMAGE_STORE_PCK,
    IMAGE_STORE_MIP_PCK,
    IMAGE_GET_RESINFO = 14,
    IMAGE_ATOMIC_SWAP,
    IMAGE_ATOMIC_CMPSWAP,
    IMAGE_ATOMIC_ADD,
    IMAGE_ATOMIC_SUB,
    IMAGE_ATOMIC_RSUB,
    IMAGE_ATOMIC_SMIN,
    IMAGE_ATOMIC_UMIN,
    IMAGE_ATOMIC_SMAX,
    IMAGE_ATOMIC_UMAX,
    IMAGE_ATOMIC_AND,
    IMAGE_ATOMIC_OR,
    IMAGE_ATOMIC_XOR,
    IMAGE_ATOMIC_INC,
    IMAGE_ATOMIC_DEC,
    IMAGE_ATOMIC_FCMPSWAP,
    IMAGE_ATOMIC_FMIN,
    IMAGE_ATOMIC_FMAX,
    IMAGE_SAMPLE,
    IMAGE_SAMPLE_CL,
    IMAGE_SAMPLE_D,
    IMAGE_SAMPLE_D_CL,
    IMAGE_SAMPLE_L,
    IMAGE_SAMPLE_B,
    IMAGE_SAMPLE_B_CL,
    IMAGE_SAMPLE_LZ,
    IMAGE_SAMPLE_C,
    IMAGE_SAMPLE_C_CL,
    IMAGE_SAMPLE_C_D,
    IMAGE_SAMPLE_C_D_CL,
    IMAGE_SAMPLE_C_L,
    IMAGE_SAMPLE_C_B,
    IMAGE_SAMPLE_C_B_CL,
    IMAGE_SAMPLE_C_LZ,
    IMAGE_SAMPLE_O,
    IMAGE_SAMPLE_CL_O,
    IMAGE_SAMPLE_D_O,
    IMAGE_SAMPLE_D_CL_O,
    IMAGE_SAMPLE_L_O,
    IMAGE_SAMPLE_B_O,
    IMAGE_SAMPLE_B_CL_O,
    IMAGE_SAMPLE_LZ_O,
    IMAGE_SAMPLE_C_O,
    IMAGE_SAMPLE_C_CL_O,
    IMAGE_SAMPLE_C_D_O,
    IMAGE_SAMPLE_C_D_CL_O,
    IMAGE_SAMPLE_C_L_O,
    IMAGE_SAMPLE_C_B_O,
    IMAGE_SAMPLE_C_B_CL_O,
    IMAGE_SAMPLE_C_LZ_O,
    IMAGE_GATHER4,
    IMAGE_GATHER4_CL,
    IMAGE_GATHER4_L = 68,
    IMAGE_GATHER4_B,
    IMAGE_GATHER4_B_CL,
    IMAGE_GATHER4_LZ,
    IMAGE_GATHER4_C,
    IMAGE_GATHER4_C_CL,
    IMAGE_GATHER4_C_L = 76,
    IMAGE_GATHER4_C_B,
    IMAGE_GATHER4_C_B_CL,
    IMAGE_GATHER4_C_LZ,
    IMAGE_GATHER4_O,
    IMAGE_GATHER4_CL_O,
    IMAGE_GATHER4_L_O = 84,
    IMAGE_GATHER4_B_O,
    IMAGE_GATHER4_B_CL_O,
    IMAGE_GATHER4_LZ_O,
    IMAGE_GATHER4_C_O,
    IMAGE_GATHER4_C_CL_O,
    IMAGE_GATHER4_C_L_O = 92,
    IMAGE_GATHER4_C_B_O,
    IMAGE_GATHER4_C_B_CL_O,
    IMAGE_GATHER4_C_LZ_O,
    IMAGE_GET_LOD,
    IMAGE_SAMPLE_CD = 104,
    IMAGE_SAMPLE_CD_CL,
    IMAGE_SAMPLE_C_CD,
    IMAGE_SAMPLE_C_CD_CL,
    IMAGE_SAMPLE_CD_O,
    IMAGE_SAMPLE_CD_CL_O,
    IMAGE_SAMPLE_C_CD_O,
    IMAGE_SAMPLE_C_CD_CL_O,
  };

  static constexpr int kMinInstSize = 2;

  static constexpr auto dmaskMask = genMask(8, 4);
  static constexpr auto unrmMask = genMask(getMaskEnd(dmaskMask), 1);
  static constexpr auto glcMask = genMask(getMaskEnd(unrmMask), 1);
  static constexpr auto daMask = genMask(getMaskEnd(glcMask), 1);
  static constexpr auto r128Mask = genMask(getMaskEnd(daMask), 1);
  static constexpr auto tfeMask = genMask(getMaskEnd(r128Mask), 1);
  static constexpr auto lweMask = genMask(getMaskEnd(tfeMask), 1);
  static constexpr auto opMask = genMask(getMaskEnd(lweMask), 7);
  static constexpr auto slcMask = genMask(getMaskEnd(opMask), 1);

  static constexpr auto vaddrMask = genMask(0, 8);
  static constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  static constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  static constexpr auto ssampMask = genMask(getMaskEnd(srsrcMask), 5);

  const std::uint32_t *inst;
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  std::uint8_t dmask = fetchMaskedValue(inst[0], dmaskMask);
  bool unrm = fetchMaskedValue(inst[0], unrmMask);
  bool glc = fetchMaskedValue(inst[0], glcMask);
  bool da = fetchMaskedValue(inst[0], daMask);
  bool r128 = fetchMaskedValue(inst[0], r128Mask);
  bool tfe = fetchMaskedValue(inst[0], tfeMask);
  bool lwe = fetchMaskedValue(inst[0], lweMask);
  bool slc = fetchMaskedValue(inst[0], slcMask);

  std::uint8_t vaddr = fetchMaskedValue(inst[1], vaddrMask);
  std::uint8_t vdata = fetchMaskedValue(inst[1], vdataMask);
  std::uint8_t srsrc = fetchMaskedValue(inst[1], srsrcMask);
  std::uint8_t ssamp = fetchMaskedValue(inst[1], ssampMask);

  Mimg(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Ds {
  enum class Op {
    DS_ADD_U32,
    DS_SUB_U32,
    DS_RSUB_U32,
    DS_INC_U32,
    DS_DEC_U32,
    DS_MIN_I32,
    DS_MAX_I32,
    DS_MIN_U32,
    DS_MAX_U32,
    DS_AND_B32,
    DS_OR_B32,
    DS_XOR_B32,
    DS_MSKOR_B32,
    DS_WRITE_B32,
    DS_WRITE2_B32,
    DS_WRITE2ST64_B32,
    DS_CMPST_B32,
    DS_CMPST_F32,
    DS_MIN_F32,
    DS_MAX_F32,
    DS_NOP,
    DS_GWS_SEMA_RELEASE_ALL = 24,
    DS_GWS_INIT,
    DS_GWS_SEMA_V,
    DS_GWS_SEMA_BR,
    DS_GWS_SEMA_P,
    DS_GWS_BARRIER,
    DS_WRITE_B8,
    DS_WRITE_B16,
    DS_ADD_RTN_U32,
    DS_SUB_RTN_U32,
    DS_RSUB_RTN_U32,
    DS_INC_RTN_U32,
    DS_DEC_RTN_U32,
    DS_MIN_RTN_I32,
    DS_MAX_RTN_I32,
    DS_MIN_RTN_U32,
    DS_MAX_RTN_U32,
    DS_AND_RTN_B32,
    DS_OR_RTN_B32,
    DS_XOR_RTN_B32,
    DS_MSKOR_RTN_B32,
    DS_WRXCHG_RTN_B32,
    DS_WRXCHG2_RTN_B32,
    DS_WRXCHG2ST64_RTN_B32,
    DS_CMPST_RTN_B32,
    DS_CMPST_RTN_F32,
    DS_MIN_RTN_F32,
    DS_MAX_RTN_F32,
    DS_WRAP_RTN_B32,
    DS_SWIZZLE_B32,
    DS_READ_B32,
    DS_READ2_B32,
    DS_READ2ST64_B32,
    DS_READ_I8,
    DS_READ_U8,
    DS_READ_I16,
    DS_READ_U16,
    DS_CONSUME,
    DS_APPEND,
    DS_ORDERED_COUNT,
    DS_ADD_U64,
    DS_SUB_U64,
    DS_RSUB_U64,
    DS_INC_U64,
    DS_DEC_U64,
    DS_MIN_I64,
    DS_MAX_I64,
    DS_MIN_U64,
    DS_MAX_U64,
    DS_AND_B64,
    DS_OR_B64,
    DS_XOR_B64,
    DS_MSKOR_B64,
    DS_WRITE_B64,
    DS_WRITE2_B64,
    DS_WRITE2ST64_B64,
    DS_CMPST_B64,
    DS_CMPST_F64,
    DS_MIN_F64,
    DS_MAX_F64,
    DS_ADD_RTN_U64 = 96,
    DS_SUB_RTN_U64,
    DS_RSUB_RTN_U64,
    DS_INC_RTN_U64,
    DS_DEC_RTN_U64,
    DS_MIN_RTN_I64,
    DS_MAX_RTN_I64,
    DS_MIN_RTN_U64,
    DS_MAX_RTN_U64,
    DS_AND_RTN_B64,
    DS_OR_RTN_B64,
    DS_XOR_RTN_B64,
    DS_MSKOR_RTN_B64,
    DS_WRXCHG_RTN_B64,
    DS_WRXCHG2_RTN_B64,
    DS_WRXCHG2ST64_RTN_B64,
    DS_CMPST_RTN_B64,
    DS_CMPST_RTN_F64,
    DS_MIN_RTN_F64,
    DS_MAX_RTN_F64,
    DS_READ_B64 = 118,
    DS_READ2_B64,
    DS_READ2ST64_B64,
    DS_CONDXCHG32_RTN_B64 = 126,
    DS_ADD_SRC2_U32 = 128,
    DS_SUB_SRC2_U32,
    DS_RSUB_SRC2_U32,
    DS_INC_SRC2_U32,
    DS_DEC_SRC2_U32,
    DS_MIN_SRC2_I32,
    DS_MAX_SRC2_I32,
    DS_MIN_SRC2_U32,
    DS_MAX_SRC2_U32,
    DS_AND_SRC2_B32,
    DS_OR_SRC2_B32,
    DS_XOR_SRC2_B32,
    DS_WRITE_SRC2_B32,
    DS_MIN_SRC2_F32 = 146,
    DS_MAX_SRC2_F32,
    DS_ADD_SRC2_U64 = 192,
    DS_SUB_SRC2_U64,
    DS_RSUB_SRC2_U64,
    DS_INC_SRC2_U64,
    DS_DEC_SRC2_U64,
    DS_MIN_SRC2_I64,
    DS_MAX_SRC2_I64,
    DS_MIN_SRC2_U64,
    DS_MAX_SRC2_U64,
    DS_AND_SRC2_B64,
    DS_OR_SRC2_B64,
    DS_XOR_SRC2_B64,
    DS_WRITE_SRC2_B64,
    DS_MIN_SRC2_F64 = 210,
    DS_MAX_SRC2_F64,
    DS_WRITE_B96 = 222,
    DS_WRITE_B128,
    DS_CONDXCHG32_RTN_B128 = 253,
    DS_READ_B96,
    DS_READ_B128,
  };

  static constexpr int kMinInstSize = 2;
  static constexpr auto offset0Mask = genMask(0, 8);
  static constexpr auto offset1Mask = genMask(getMaskEnd(offset0Mask), 8);
  static constexpr auto gdsMask = genMask(getMaskEnd(offset1Mask) + 1, 1);
  static constexpr auto opMask = genMask(getMaskEnd(gdsMask), 8);

  static constexpr auto addrMask = genMask(0, 8);
  static constexpr auto data0Mask = genMask(getMaskEnd(addrMask), 8);
  static constexpr auto data1Mask = genMask(getMaskEnd(data0Mask), 8);
  static constexpr auto vdstMask = genMask(getMaskEnd(data1Mask), 8);

  const std::uint32_t *inst;
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));

  Ds(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Vintrp {
  enum class Op { V_INTERP_P1_F32, V_INTERP_P2_F32, V_INTERP_MOV_F32 };

  static constexpr int kMinInstSize = 1;
  static constexpr auto vsrcMask = genMask(0, 8);
  static constexpr auto attrChanMask = genMask(getMaskEnd(vsrcMask), 2);
  static constexpr auto attrMask = genMask(getMaskEnd(attrChanMask), 6);
  static constexpr auto opMask = genMask(getMaskEnd(attrMask), 2);
  static constexpr auto vdstMask = genMask(getMaskEnd(opMask), 8);

  const std::uint32_t *inst;
  uint32_t vsrc = fetchMaskedValue(inst[0], vsrcMask);
  uint32_t attrChan = fetchMaskedValue(inst[0], attrChanMask);
  uint32_t attr = fetchMaskedValue(inst[0], attrMask);
  const Op op = static_cast<Op>(fetchMaskedValue(inst[0], opMask));
  uint32_t vdst = fetchMaskedValue(inst[0], vdstMask);

  Vintrp(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

struct Exp {
  static constexpr int kMinInstSize = 2;

  static constexpr auto enMask = genMask(0, 4);
  static constexpr auto targetMask = genMask(getMaskEnd(enMask), 6);
  static constexpr auto comprMask = genMask(getMaskEnd(targetMask), 1);
  static constexpr auto doneMask = genMask(getMaskEnd(comprMask), 1);
  static constexpr auto vmMask = genMask(getMaskEnd(doneMask), 1);

  static constexpr auto vsrc0Mask = genMask(0, 8);
  static constexpr auto vsrc1Mask = genMask(getMaskEnd(vsrc0Mask), 8);
  static constexpr auto vsrc2Mask = genMask(getMaskEnd(vsrc1Mask), 8);
  static constexpr auto vsrc3Mask = genMask(getMaskEnd(vsrc2Mask), 8);

  const std::uint32_t *inst;

  std::uint8_t en = fetchMaskedValue(inst[0], enMask);
  std::uint8_t target = fetchMaskedValue(inst[0], targetMask);
  bool compr = fetchMaskedValue(inst[0], comprMask);
  bool done = fetchMaskedValue(inst[0], doneMask);
  bool vm = fetchMaskedValue(inst[0], vmMask);
  std::uint8_t vsrc0 = fetchMaskedValue(inst[1], vsrc0Mask);
  std::uint8_t vsrc1 = fetchMaskedValue(inst[1], vsrc1Mask);
  std::uint8_t vsrc2 = fetchMaskedValue(inst[1], vsrc2Mask);
  std::uint8_t vsrc3 = fetchMaskedValue(inst[1], vsrc3Mask);

  Exp(const std::uint32_t *inst) : inst(inst) {}

  int size() const { return kMinInstSize; }

  void dump() const;
};

enum class InstructionClass : std::uint8_t {
  Invalid,
  Vop2,
  Sop2,
  Sopk,
  Smrd,
  Vop3,
  Mubuf,
  Mtbuf,
  Mimg,
  Ds,
  Vintrp,
  Exp,
  Vop1,
  Vopc,
  Sop1,
  Sopc,
  Sopp,
};

static constexpr std::uint32_t kInstMask1 =
    static_cast<std::uint32_t>(~0u << (32 - 1));
static constexpr std::uint32_t kInstMask2 =
    static_cast<std::uint32_t>(~0u << (32 - 2));
static constexpr std::uint32_t kInstMask4 =
    static_cast<std::uint32_t>(~0u << (32 - 4));
static constexpr std::uint32_t kInstMask5 =
    static_cast<std::uint32_t>(~0u << (32 - 5));
static constexpr std::uint32_t kInstMask6 =
    static_cast<std::uint32_t>(~0u << (32 - 6));
static constexpr std::uint32_t kInstMask7 =
    static_cast<std::uint32_t>(~0u << (32 - 7));
static constexpr std::uint32_t kInstMask9 =
    static_cast<std::uint32_t>(~0u << (32 - 9));

static constexpr std::uint32_t kInstMaskValVop2 = 0b0u << (32 - 1);
static constexpr std::uint32_t kInstMaskValSop2 = 0b10u << (32 - 2);
static constexpr std::uint32_t kInstMaskValSopk = 0b1011u << (32 - 4);
static constexpr std::uint32_t kInstMaskValSmrd = 0b11000u << (32 - 5);
static constexpr std::uint32_t kInstMaskValVop3 = 0b110100u << (32 - 6);
static constexpr std::uint32_t kInstMaskValMubuf = 0b111000u << (32 - 6);
static constexpr std::uint32_t kInstMaskValMtbuf = 0b111010u << (32 - 6);
static constexpr std::uint32_t kInstMaskValMimg = 0b111100u << (32 - 6);
static constexpr std::uint32_t kInstMaskValDs = 0b110110u << (32 - 6);
static constexpr std::uint32_t kInstMaskValVintrp = 0b110010u << (32 - 6);
static constexpr std::uint32_t kInstMaskValExp = 0b111110u << (32 - 6);
static constexpr std::uint32_t kInstMaskValVop1 = 0b0111111u << (32 - 7);
static constexpr std::uint32_t kInstMaskValVopC = 0b0111110u << (32 - 7);
static constexpr std::uint32_t kInstMaskValSop1 = 0b101111101u << (32 - 9);
static constexpr std::uint32_t kInstMaskValSopc = 0b101111110u << (32 - 9);
static constexpr std::uint32_t kInstMaskValSopp = 0b101111111u << (32 - 9);

inline InstructionClass getInstructionClass(std::uint32_t instr) {
  switch (instr & kInstMask9) {
  case kInstMaskValSop1:
    return InstructionClass::Sop1;
  case kInstMaskValSopc:
    return InstructionClass::Sopc;
  case kInstMaskValSopp:
    return InstructionClass::Sopp;
  }

  switch (instr & kInstMask7) {
  case kInstMaskValVop1:
    return InstructionClass::Vop1;
  case kInstMaskValVopC:
    return InstructionClass::Vopc;
  }

  switch (instr & kInstMask6) {
  case kInstMaskValVop3:
    return InstructionClass::Vop3;
  case kInstMaskValMubuf:
    return InstructionClass::Mubuf;
  case kInstMaskValMtbuf:
    return InstructionClass::Mtbuf;
  case kInstMaskValMimg:
    return InstructionClass::Mimg;
  case kInstMaskValDs:
    return InstructionClass::Ds;
  case kInstMaskValVintrp:
    return InstructionClass::Vintrp;
  case kInstMaskValExp:
    return InstructionClass::Exp;
  }

  if ((instr & kInstMask5) == kInstMaskValSmrd) {
    return InstructionClass::Smrd;
  }

  if ((instr & kInstMask4) == kInstMaskValSopk) {
    return InstructionClass::Sopk;
  }

  if ((instr & kInstMask2) == kInstMaskValSop2) {
    return InstructionClass::Sop2;
  }

  if ((instr & kInstMask1) == kInstMaskValVop2) {
    return InstructionClass::Vop2;
  }

  return InstructionClass::Invalid;
}

struct Instruction {
  const std::uint32_t *inst;
  InstructionClass instClass = getInstructionClass(*inst);

  Instruction(const std::uint32_t *inst) : inst(inst) {}

  int size() const {
    switch (instClass) {
    case InstructionClass::Vop2:
      return Vop2(inst).size();
    case InstructionClass::Sop2:
      return Sop2(inst).size();
    case InstructionClass::Sopk:
      return Sopk(inst).size();
    case InstructionClass::Smrd:
      return Smrd(inst).size();
    case InstructionClass::Vop3:
      return Vop3(inst).size();
    case InstructionClass::Mubuf:
      return Mubuf(inst).size();
    case InstructionClass::Mtbuf:
      return Mtbuf(inst).size();
    case InstructionClass::Mimg:
      return Mimg(inst).size();
    case InstructionClass::Ds:
      return Ds(inst).size();
    case InstructionClass::Vintrp:
      return Vintrp(inst).size();
    case InstructionClass::Exp:
      return Exp(inst).size();
    case InstructionClass::Vop1:
      return Vop1(inst).size();
    case InstructionClass::Vopc:
      return Vopc(inst).size();
    case InstructionClass::Sop1:
      return Sop1(inst).size();
    case InstructionClass::Sopc:
      return Sopc(inst).size();
    case InstructionClass::Sopp:
      return Sopp(inst).size();

    case InstructionClass::Invalid:
      break;
    }

    return 1;
  }

  void dump() const;
};

const char *instructionClassToString(InstructionClass instrClass);
const char *opcodeToString(InstructionClass instClass, int op);

const char *sop1OpcodeToString(Sop1::Op op);
const char *sop2OpcodeToString(Sop2::Op op);
const char *sopkOpcodeToString(Sopk::Op op);
const char *sopcOpcodeToString(Sopc::Op op);
const char *soppOpcodeToString(Sopp::Op op);
const char *vop2OpcodeToString(Vop2::Op op);
const char *vop1OpcodeToString(Vop1::Op op);
const char *vopcOpcodeToString(Vopc::Op op);
const char *vop3OpcodeToString(Vop3::Op op);
const char *smrdOpcodeToString(Smrd::Op op);
const char *mubufOpcodeToString(Mubuf::Op op);
const char *mtbufOpcodeToString(Mtbuf::Op op);
const char *mimgOpcodeToString(Mimg::Op op);
const char *dsOpcodeToString(Ds::Op op);
const char *vintrpOpcodeToString(Vintrp::Op op);
} // namespace amdgpu::shader
