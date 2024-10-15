#pragma once

#include "amdgpu/tiler.hpp"
#include "gnm/constants.hpp"
#include <array>
#include <cstdint>
#include <type_traits>

namespace amdgpu {
enum class Engine {
  ME,
  PFP,
  CE,
};

enum class EventIndex {
  OTHER,
  ZPASS_DONE,
  SAMAPE_PIPELINE_STAT,
  SAMPLE_STREAM_OUT_STATS,
  CS_VS_PS_PARTIAL_FLUSH,
  ANY_EOP_TIMESTAMP,
  CS_PS_EOS,
};

enum class ProtectionFaultAccess : std::uint32_t {
  Read = 0,
  Write = 1,
};

namespace detail {
#pragma pack(push, 1)
template <std::size_t Count> struct Padding {
private:
  std::uint32_t _[Count];
};
} // namespace detail

template <std::size_t Offset, typename ImplT = std::uint32_t>
struct Register : detail::Padding<Offset>, ImplT {
  Register() = default;
  Register(const Register &) = default;
  Register &operator=(const Register &) = default;

  Register &operator=(const ImplT &newValue) {
    *static_cast<ImplT *>(this) = newValue;
    return *this;
  }
};

template <std::size_t Offset, typename ImplT>
  requires(std::is_integral_v<ImplT> || std::is_floating_point_v<ImplT> ||
           std::is_enum_v<ImplT>)
struct Register<Offset, ImplT> : detail::Padding<Offset> {
  ImplT value;

  Register() = default;
  Register(const Register &) = default;
  Register &operator=(const Register &) = default;
  Register &operator=(ImplT newValue) {
    value = newValue;
    return *this;
  }

  operator ImplT() { return value; }
};

struct CbColorAttrib {
  union {
    struct {
      std::uint32_t tileModeIndex : 5;
      std::uint32_t fmaskTileModeIndex : 4;
      std::uint32_t : 3;
      std::uint32_t numSamples : 3;
      std::uint32_t numFragments : 2;
      std::uint32_t forceDstAlpha1 : 1;
    };

    std::uint32_t raw;
  };
};

struct CbColorView {
  union {
    struct {
      std::uint32_t sliceStart : 11;
      std::uint32_t : 2;
      std::uint32_t sliceMax : 11;
    };
    std::uint32_t raw;
  };
};

struct CbColorControl {
  union {
    struct {
      std::uint32_t : 3;
      std::uint32_t degammaEnable : 1;
      gnm::CbMode mode : 3;
      std::uint32_t : 9;
      std::uint32_t rop3 : 8;
    };
    std::uint32_t raw;
  };
};

struct CbShaderMask {
  union {
    struct {
      std::uint32_t output0Enable : 4;
      std::uint32_t output1Enable : 4;
      std::uint32_t output2Enable : 4;
      std::uint32_t output3Enable : 4;
      std::uint32_t output4Enable : 4;
      std::uint32_t output5Enable : 4;
      std::uint32_t output6Enable : 4;
      std::uint32_t output7Enable : 4;
    };
    std::uint32_t raw;
  };
};

struct CbTargetMask {
  union {
    struct {
      std::uint32_t target0Enable : 4;
      std::uint32_t target1Enable : 4;
      std::uint32_t target2Enable : 4;
      std::uint32_t target3Enable : 4;
      std::uint32_t target4Enable : 4;
      std::uint32_t target5Enable : 4;
      std::uint32_t target6Enable : 4;
      std::uint32_t target7Enable : 4;
    };
    std::uint32_t raw;
  };
};

enum class CbCompSwap : std::uint32_t {
  Std,
  Alt,
  StdRev,
  AltRev,
};

struct CbColorInfo {
  union {
    struct {
      std::uint32_t endian : 2;
      gnm::DataFormat dfmt : 5;
      std::uint32_t linearGeneral : 1;
      gnm::CbNumericFormat nfmt : 3;
      CbCompSwap compSwap : 2;
      std::uint32_t fastClear : 1;
      std::uint32_t compression : 1;
      std::uint32_t blendClamp : 1;
      std::uint32_t blendBypass : 1;
      std::uint32_t simpleFloat : 1;
      std::uint32_t roundMode : 1;
      std::uint32_t cmaskIsLinear : 1;
      std::uint32_t blendOptDontRdDst : 3;
      std::uint32_t blendOptDiscardPixel : 3;
    };

    std::uint32_t raw;
  };
};

struct CbColor {
  std::uint32_t base;
  std::uint32_t pitch;
  std::uint32_t slice;
  CbColorView view;
  CbColorInfo info;
  CbColorAttrib attrib;
  std::uint32_t dccBase;
  std::uint32_t cmask;
  std::uint32_t cmaskSlice : 14;
  std::uint32_t fmask;
  std::uint32_t fmaskSlice;
  std::uint32_t clearWord0;
  std::uint32_t clearWord1;
  std::uint32_t clearWord2;
  std::uint32_t clearWord3;
};

struct PaClVport {
  float xScale;
  float xOffset;
  float yScale;
  float yOffset;
  float zScale;
  float zOffset;
};

struct PaScVportZ {
  float min;
  float max;
};

struct PaScRect {
  std::uint16_t left;
  std::uint16_t top;
  std::uint16_t right;
  std::uint16_t bottom;

  bool isValid() const { return left < right && top < bottom; }
};

struct SpiShaderPgm {
  std::uint32_t rsrc3;
  std::uint64_t address;

  union {
    struct {
      std::uint32_t vgprs : 6;
      std::uint32_t sgprs : 4;
      std::uint32_t priority : 2;
      std::uint32_t floatMode : 8;
      std::uint32_t priv : 1;
      std::uint32_t dx10Clamp : 1;
      std::uint32_t debugMode : 1;
      std::uint32_t ieeeMode : 1;
    };

    struct {
      std::uint32_t : 24;
      std::uint32_t cuGroupEnable : 1;
    } es;

    struct {
      std::uint32_t : 24;
      std::uint32_t cuGroupEnable : 1;
    } gs;

    struct {
      std::uint32_t : 24;
      std::uint32_t vgprCompCnt : 2;
    } ls;

    struct {
      std::uint32_t : 24;
      std::uint32_t cuGroupDisable : 1;
    } ps;

    struct {
      std::uint32_t : 24;
      std::uint32_t vgprCompCnt : 2;
      std::uint32_t cuGroupEnable : 1;
    } vs;

    std::uint8_t getVGprCount() const { return (vgprs + 1) * 4; }
    std::uint8_t getSGprCount() const { return (sgprs + 1) * 8; }

    std::uint32_t raw;
  } rsrc1;

  union {
    struct {
      std::uint32_t scratchEn : 1;
      std::uint32_t userSgpr : 5;
      std::uint32_t trapPresent : 1;
    };

    struct {
      std::uint32_t : 7;
      std::uint32_t ocLdsEn : 1;
      std::uint32_t soBase0En : 1;
      std::uint32_t soBase1En : 1;
      std::uint32_t soBase2En : 1;
      std::uint32_t soBase3En : 1;
      std::uint32_t soEn : 1;
      std::uint32_t excpEn : 7;
    } vs;

    struct {
      std::uint32_t : 7;
      std::uint32_t ocLdsEn : 1;
      std::uint32_t excpEn : 7;
    } es;

    struct {
      std::uint32_t : 7;
      std::uint32_t excpEn : 7;
    } gs;

    struct {
      std::uint32_t : 7;
      std::uint32_t ocLdsEn : 1;
      std::uint32_t tgSizeEn : 1;
      std::uint32_t excpEn : 7;
    } hs;

    struct {
      std::uint32_t : 7;
      std::uint32_t ldsSize : 9;
      std::uint32_t excpEn : 7;
    } ls;
    std::uint32_t raw;
  } rsrc2;

  std::array<std::uint32_t, 16> userData;
};

struct VmProtectionFault {
  std::uint32_t protection : 8;
  std::uint32_t : 4;
  std::uint32_t client : 8;
  std::uint32_t : 4;
  ProtectionFaultAccess rw : 1;
  std::uint32_t vmid : 4;
  std::uint32_t : 3;
};

enum class LsStage : std::uint32_t {
  LsOff,
  LsOn,
  CsOn,
};

enum class EsStage : std::uint32_t {
  EsOff,
  EsDs,
  EsReal,
};

enum class VsStage : std::uint32_t {
  VsReal,
  VsDs,
  VsCopy,
};

struct VgtShaderStagesEn {
  union {
    struct {
      LsStage lsEn : 2;
      bool hsEn : 1;
      EsStage esEn : 2;
      bool gsEn : 1;
      VsStage vsEn : 2;
      bool dynamicHs : 1;
    };
    std::uint32_t raw;
  };
};

struct FbInfo {
  std::uint16_t base; // address >> 24
  std::uint16_t unk;
};

struct DbDepthControl {
  union {
    struct {
      bool stencilEnable : 1;
      bool depthEnable : 1;
      bool depthWriteEnable : 1;
      bool depthBoundsEnable : 1;
      gnm::CompareFunc zFunc : 3;
      bool backFaceEnable : 1;
      gnm::CompareFunc stencilFunc : 3;
      std::uint32_t : 9;
      gnm::CompareFunc stencilFuncBackFace : 3;
      std::uint32_t : 7;
      bool enableColorWritesOnDepthFail : 1;
      bool disableColorWritesOnDepthPass : 1;
    };

    std::uint32_t raw;
  };
};

struct DbZInfo {
  union {
    struct {
      gnm::ZFormat format : 2;
      std::uint32_t numSamples : 2;
      std::uint32_t : 16;
      std::uint32_t tileModeIndex : 3;
      std::uint32_t : 4;
      bool allowExpClear : 1;
      std::uint32_t readSize : 1; // 0 - 256 bit, 1 - 512 bit
      bool tileSurfaceEnable : 1;
      std::uint32_t : 1;
      bool zRangePrecision : 1;
    };

    std::uint32_t raw;
  };
};

struct DbDepthSize {
  union {
    struct {
      std::uint32_t pitchTileMax : 11;
      std::uint32_t heightTileMax : 11;
    };

    std::uint32_t raw;
  };

  [[nodiscard]] std::uint32_t getPitch() const {
    return (pitchTileMax + 1) * 8;
  }
  [[nodiscard]] std::uint32_t getHeight() const {
    return (heightTileMax + 1) * 8;
  }
};

struct DbRenderControl {
  union {
    struct {
      bool depthClearEnable : 1;
      bool stencilClearEnable : 1;
      bool depthCopy : 1;
      bool stencilCopy : 1;
      bool resummarizeEnable : 1;
      bool stencilCompressDisable : 1;
      bool depthCompressDisable : 1;
      bool copyCentroid : 1;
      std::uint32_t copySample : 4;
    };

    std::uint32_t raw;
  };
};

struct DbDepthView {
  union {
    struct {
      std::uint32_t sliceStart : 11;
      std::uint32_t : 2;
      std::uint32_t sliceMax : 11;
      bool zReadOnly : 1;
      bool stencilReadOnly : 1;
    };

    std::uint32_t raw;
  };
};

struct CbBlendControl {
  union {
    struct {
      gnm::BlendMultiplier colorSrcBlend : 5;
      gnm::BlendFunc colorCombFcn : 3;
      gnm::BlendMultiplier colorDstBlend : 5;
      std::uint32_t : 3;
      gnm::BlendMultiplier alphaSrcBlend : 5;
      gnm::BlendFunc alphaCombFcn : 3;
      gnm::BlendMultiplier alphaDstBlend : 5;

      bool separateAlphaBlend : 1;
      bool enable : 1;
      bool disableRop3 : 1;
    };

    std::uint32_t raw;
  };
};

struct PaSuScModeCntl {
  union {
    struct {
      bool cullFront : 1;
      bool cullBack : 1;
      gnm::Face face : 1;
      gnm::PolyMode polyMode : 2;
      gnm::PolyModePtype polyModeFrontPtype : 3;
      gnm::PolyModePtype polyModeBackPtype : 3;
      bool polyOffsetFrontEnable : 1;
      bool polyOffsetBackEnable : 1;
      bool polyOffsetParaEnable : 1;
      std::uint32_t : 2;
      bool vtxWindowOffsetEnable : 1;
      std::uint32_t : 2;
      bool provokingVtxLast : 1;
      bool perspCorrDis : 1;
      bool multiPrimIbEna : 1;
    };

    std::uint32_t raw;
  };
};

struct PaSuVtxCntl {
  union {
    struct {
      bool pixCenterHalf : 1;
      gnm::RoundMode roundMode : 2;
      gnm::QuantMode quantMode : 3;
    };

    std::uint32_t raw;
  };
};

struct SpiPsInput {
  union {
    struct {
      bool perspSampleEna : 1;
      bool perspCenterEna : 1;
      bool perspCentroidEna : 1;
      bool perspPullModelEna : 1;
      bool linearSampleEna : 1;
      bool linearCenterEna : 1;
      bool linearCentroidEna : 1;
      bool lineStippleTexEna : 1;
      bool posXFloatEna : 1;
      bool posYFloatEna : 1;
      bool posZFloatEna : 1;
      bool posWFloatEna : 1;
      bool frontFaceEna : 1;
      bool ancillaryEna : 1;
      bool sampleCoverageEna : 1;
      bool posFixedPtEna : 1;
    };

    std::uint32_t raw;
  };
};

enum class SpiPsDefaultVal : std::uint8_t {
  X0_Y0_Z0_W0,
  X0_Y0_Z0_W1,
  X1_Y1_Z1_W0,
  X1_Y1_Z1_W1,
};

struct SpiPsInputCntl {
  union {
    struct {
      std::uint32_t offset : 4;
      bool useDefaultVal : 1;
      std::uint32_t : 3;
      SpiPsDefaultVal defaultVal : 2;
      bool flatShade : 1;
      std::uint32_t : 2;
      std::uint32_t cylWrap : 4;
      bool ptSpriteTex : 1;
    };

    std::uint32_t raw;
  };
};
struct Registers {
  static constexpr auto kRegisterCount = 0xf000;

  struct Config {
    static constexpr auto kMmioOffset = 0x2000;

    Register<0xad, std::array<std::uint32_t, 3>> cpPrtLodStatsCntls;
    Register<0x1c0> cpRbRptr;
    Register<0x1bf> cpRb1Rptr;
    Register<0x1be> cpRb2Rptr;
    Register<0x232> vgtEsGsRingSize;
    Register<0x233> vgtGsVsRingSize;
    Register<0x262> vgtTfRingSize;
    Register<0x26e> vgtTfMemoryBase;
    Register<0x3c0, std::array<std::uint32_t, 4>> sqBufRsrcWords;
    Register<0x3c4, std::array<std::uint32_t, 7>> sqImgRsrcWords;
    Register<0x3cc, std::array<std::uint32_t, 4>> sqImgSampWords;
    Register<0x644, std::array<TileMode, 32>> gbTileModes;
    Register<0x664, std::array<MacroTileMode, 16>> gbMacroTileModes;
  };

  struct ComputeConfig {
    static constexpr auto kMmioOffset = 0x2e00;

    std::uint32_t computeDispatchInitiator;
    std::uint32_t _pad0[6];
    std::uint32_t numThreadX;
    std::uint32_t numThreadY;
    std::uint32_t numThreadZ;
    std::uint32_t _pad1[2];
    std::uint64_t address;
    std::uint32_t _pad2[4];
    struct {
      union {
        std::uint32_t raw;

        struct {
          std::uint32_t vgprs : 6;
          std::uint32_t sgprs : 4;
          std::uint32_t priority : 2;
          std::uint32_t floatMode : 8;
          std::uint32_t priv : 1;
          std::uint32_t dx10Clamp : 1;
          std::uint32_t debugMode : 1;
          std::uint32_t ieeeMode : 1;
        };
      };

      [[nodiscard]] std::uint8_t getVGprCount() const {
        return (vgprs + 1) * 4;
      }
      [[nodiscard]] std::uint8_t getSGprCount() const {
        return (sgprs + 1) * 8;
      }
    } rsrc1;
    struct {
      union {
        std::uint32_t raw;

        struct {
          bool scratchEn : 1;
          std::uint32_t userSgpr : 5;
          bool trapPresent : 1;
          bool tgIdXEn : 1;
          bool tgIdYEn : 1;
          bool tgIdZEn : 1;
          bool tgSizeEn : 1;
          std::uint32_t tidIgCompCount : 2;
          std::uint32_t : 2;
          std::uint32_t ldsSize : 9;
          std::uint32_t excpEn : 7;
        };
      };

      [[nodiscard]] std::uint32_t getLdsDwordsCount() const {
        return ldsSize * 64;
      }
    } rsrc2;
    std::uint32_t _pad3[1];

    struct {
      union {
        std::uint32_t raw;
        struct {
          std::uint32_t wavesPerSh : 6;
          std::uint32_t : 6;
          std::uint32_t tgPerCu : 4;
          std::uint32_t lockThreshold : 6;
          std::uint32_t simdDestCntl : 1;
        };
      };
      [[nodiscard]] std::uint32_t getWavesPerSh() const {
        return wavesPerSh << 4;
      }
    } resourceLimits;
    std::uint32_t staticThreadMgmtSe0;
    std::uint32_t staticThreadMgmtSe1;
    std::uint32_t tmpRingSize;
    std::uint32_t _unk0[5];
    std::uint32_t state;
    std::uint32_t _unk1[33];
    std::array<std::uint32_t, 16> userData;
  };

  static_assert(sizeof(ComputeConfig) == 320);

  struct ShaderConfig {
    static constexpr auto kMmioOffset = 0x2c00;

    union {
      Register<0x7, SpiShaderPgm> spiShaderPgmPs;
      Register<0x47, SpiShaderPgm> spiShaderPgmVs;
      Register<0x87, SpiShaderPgm> spiShaderPgmGs;
      Register<0xc7, SpiShaderPgm> spiShaderPgmEs;
      Register<0x107, SpiShaderPgm> spiShaderPgmHs;
      Register<0x147, SpiShaderPgm> spiShaderPgmLs;
      Register<0x200, ComputeConfig> compute;
    };
  };

  struct Context {
    static constexpr auto kMmioOffset = 0xa000;
    static Context Default;

    union {
      Register<0x0, DbRenderControl> dbRenderControl;
      Register<0x1> dbCountControl;
      Register<0x2, DbDepthView> dbDepthView;
      Register<0x3> dbRenderOverride;
      Register<0x4> dbRenderOverride2;
      Register<0x5> dbHTileDataBase;
      Register<0x8, float> dbDepthBoundsMin;
      Register<0x9, float> dbDepthBoundsMax;
      Register<0xa> dbStencilClear;
      Register<0xb, float> dbDepthClear;
      Register<0xc, PaScRect> paScScreenScissor;
      Register<0xf> dbDepthInfo;
      Register<0x10, DbZInfo> dbZInfo;
      Register<0x11> dbStencilInfo;
      Register<0x12> dbZReadBase;
      Register<0x13> dbStencilReadBase;
      Register<0x14> dbZWriteBase;
      Register<0x15> dbStencilWriteBase;
      Register<0x16, DbDepthSize> dbDepthSize;
      Register<0x17> dbDepthSlice;
      Register<0x20> taBcBaseAddr;
      Register<0x80> paScWindowOffset;
      Register<0x81, PaScRect> paScWindowScissor;
      Register<0x83> paScClipRectRule;
      Register<0x84, std::array<PaScRect, 4>> paScClipRect;
      Register<0x8c> unk_8c;
      Register<0x8d> paSuHardwareScreenOffset;
      Register<0x8e, CbTargetMask> cbTargetMask;
      Register<0x8f, CbShaderMask> cbShaderMask;
      Register<0x90, PaScRect> paScGenericScissor;
      Register<0x94, std::array<PaScRect, 16>> paScVportScissor;
      Register<0xb4, std::array<PaScVportZ, 16>> paScVportZ;
      Register<0xd4> unk_d4;
      Register<0xd8> cpPerfMonCntxCntl;
      Register<0x100> vgtMaxVtxIndx;
      Register<0x101> vgtMinVtxIndx;
      Register<0x102> vgtIndxOffset;
      Register<0x103> vgtMultiPrimIbResetIndx;
      Register<0x105, float> cbBlendRed;
      Register<0x106, float> cbBlendGreen;
      Register<0x107, float> cbBlendBlue;
      Register<0x108, float> cbBlendAlpha;
      Register<0x10b> dbStencilControl;
      Register<0x10c> dbStencilRefMask;
      Register<0x10d> dbStencilRefMaskBf;
      Register<0x10f, std::array<PaClVport, 16>> paClVports;
      Register<0x16f> paClUcp0X;
      Register<0x170> paClUcp0Y;
      Register<0x171> paClUcp0Z;
      Register<0x172> paClUcp0W;
      Register<0x191, std::array<SpiPsInputCntl, 32>> spiPsInputCntl;
      Register<0x1b1> spiVsOutConfig;
      Register<0x1b3, SpiPsInput> spiPsInputEna;
      Register<0x1b4, SpiPsInput> spiPsInputAddr;
      Register<0x1b6> spiPsInControl;
      Register<0x1b8> spiBarycCntl;
      Register<0x1ba> spiTmpRingSize;
      Register<0x1c3> spiShaderPosFormat;
      Register<0x1c4> spiShaderZFormat;
      Register<0x1c5> spiShaderColFormat;
      Register<0x1e0, std::array<CbBlendControl, 8>> cbBlendControl;
      Register<0x1f9> vgtDmaBaseHi;
      Register<0x1fa> vgtDmaBase;
      Register<0x1fc> vgtDrawInitiator;
      Register<0x1fd> vgtImmedData;
      Register<0x200, DbDepthControl> dbDepthControl;
      Register<0x201> dbEqaa;
      Register<0x202, CbColorControl> cbColorControl;
      Register<0x203> dbShaderControl;
      Register<0x204> paClClipCntl;
      Register<0x205, PaSuScModeCntl> paSuScModeCntl;
      Register<0x206> paClVteCntl;
      Register<0x207> paClVsOutCntl;
      Register<0x280> paSuPointSize;
      Register<0x281> paSuPointMinmax;
      Register<0x282> paSuLineCntl;
      Register<0x284> vgtOutputPathCntl;
      Register<0x286> vgtHosMaxTessLevel;
      Register<0x287> vgtHosMinTessLevel;
      Register<0x290> vgtGsMode;
      Register<0x291> vgtGsOnChipCntl;
      Register<0x292> paScModeCntl0;
      Register<0x293> paScModeCntl1;
      Register<0x295> vgtGsPerEs;
      Register<0x296> vgtEsPerGs;
      Register<0x297> vgtGsPerVs;
      Register<0x298, std::array<std::uint32_t, 3>> vgtGsVsRingOffsets;
      Register<0x29b> vgtGsOutPrimType;
      Register<0x29d> vgtDmaSize;
      Register<0x29e> vgtDmaMaxSize;
      Register<0x29f> vgtDmaIndexType;
      Register<0x2a1> vgtPrimitiveIdEn;
      Register<0x2a2> vgtDmaNumInstances;
      Register<0x2a4> vgtEventInitiator;
      Register<0x2a5> vgtMultiPrimIbResetEn;
      Register<0x2a8> vgtInstanceStepRate0;
      Register<0x2a9> vgtInstanceStepRate1;
      Register<0x2aa> iaMultiVgtParam;
      Register<0x2ab> vgtEsGsRingItemSize;
      Register<0x2ac> vgtGsVsRingItemSize;
      Register<0x2ad> vgtReuseOff;
      Register<0x2ae> vgtVtxCntEn;
      Register<0x2af> dbHTileSurface;
      Register<0x2b0> dbSResultsCompareState0;
      Register<0x2b1> dbSResultsCompareState1;
      Register<0x2b4> vgtStrmOutBufferSize0;
      Register<0x2b5> vgtStrmOutVtxStride0;
      Register<0x2b8> vgtStrmOutBufferSize1;
      Register<0x2b9> vgtStrmOutVtxStride1;
      Register<0x2bc> vgtStrmOutBufferSize2;
      Register<0x2bd> vgtStrmOutVtxStride2;
      Register<0x2c0> vgtStrmOutBufferSize3;
      Register<0x2c1> vgtStrmOutVtxStride3;
      Register<0x2ca> vgtStrmOutDrawOpaqueOffset;
      Register<0x2cb> vgtStrmOutDrawOpaqueBufferFilledSize;
      Register<0x2cc> vgtStrmOutDrawOpaqueVertexStride;
      Register<0x2ce> vgtGsMaxVertOut;
      Register<0x2d5, VgtShaderStagesEn> vgtShaderStagesEn;
      Register<0x2d6> vgtLsHsConfig;
      Register<0x2d7, std::array<std::uint32_t, 4>> vgtGsVertItemSizes;
      Register<0x2db> vgtTfParam;
      Register<0x2dc> dbAlphaToMask;
      Register<0x2dd> vgtDispatchDrawIndex;
      Register<0x2de> paSuPolyOffsetDbFmtCntl;
      Register<0x2df> paSuPolyOffsetClamp;
      Register<0x2e0> paSuPolyOffsetFrontScale;
      Register<0x2e1> paSuPolyOffsetFrontOffset;
      Register<0x2e2> paSuPolyOffsetBackScale;
      Register<0x2e3> paSuPolyOffsetBackOffset;
      Register<0x2e4> vgtGsInstanceCnt;
      Register<0x2e5> vgtStrmOutConfig;
      Register<0x2e6> vgtStrmOutBufferConfig;
      Register<0x2f5> paScCentroidPriority0;
      Register<0x2f6> paScCentroidPriority1;
      Register<0x2f7> unk_2f7;
      Register<0x2f8> paScAaConfig;
      Register<0x2f9, PaSuVtxCntl> paSuVtxCntl;
      Register<0x2fa, float> paClGbVertClipAdj;
      Register<0x2fb, float> paClGbVertDiscAdj;
      Register<0x2fc, float> paClGbHorzClipAdj;
      Register<0x2fd, float> paClGbHorzDiscAdj;
      Register<0x2fe, std::array<std::uint32_t, 4>> paScAaSampleLocsPixelX0Y0;
      Register<0x302, std::array<std::uint32_t, 4>> paScAaSampleLocsPixelX1Y0;
      Register<0x306, std::array<std::uint32_t, 4>> paScAaSampleLocsPixelX0Y1;
      Register<0x30a, std::array<std::uint32_t, 4>> paScAaSampleLocsPixelX1Y1;
      Register<0x30e> paScAaMaskX0Y0_X1Y0;
      Register<0x30f> paScAaMaskX0Y1_X1Y1;
      Register<0x316> unk_316;
      Register<0x317> vgtOutDeallocCntl;
      Register<0x318, std::array<CbColor, 8>> cbColor;
    };
  };

  struct UConfig {
    static constexpr auto kMmioOffset = 0xc000;

    union {
      Register<0x3f> cpStrmOutCntl;
      Register<0x79> cpCoherBaseHi;
      Register<0x7d> cpCoherSize;
      Register<0x7e> cpCoherBase;
      Register<0x8b> cpDmaReadTags;
      Register<0x8c> cpCoherSizeHi;
      Register<0x200> grbmGfxIndex;
      Register<0x242, gnm::PrimitiveType> vgtPrimitiveType;
      Register<0x243, gnm::IndexType> vgtIndexType;
      Register<0x24c> vgtNumIndices;
      Register<0x24d> vgtNumInstances;
      Register<0x340, std::array<std::uint32_t, 4>> sqThreadTraceUserdata;
      Register<0x41d> gdsOaCntl;
      Register<0x41e> gdsOaCounter;
      Register<0x41f> gdsOaAddress;
    };
  };

  struct Counters {
    static constexpr auto kMmioOffset = 0xd000;

    union {
      Register<0x0, std::uint64_t> cpgPerfCounter1;
      Register<0x2, std::uint64_t> cpgPerfCounter0;
      Register<0x4, std::uint64_t> cpcPerfCounter1;
      Register<0x6, std::uint64_t> cpcPerfCounter0;
      Register<0x8, std::uint64_t> cpfPerfCounter1;
      Register<0xa, std::uint64_t> cpfPerfCounter0;
      Register<0x80, std::array<std::uint64_t, 4>> wdPerfCounters;
      Register<0x88, std::array<std::uint64_t, 4>> iaPerfCounters;
      Register<0x90, std::array<std::uint64_t, 4>> vgtPerfCounters;
      Register<0x100, std::array<std::uint64_t, 4>> paSuPerfCounters;
      Register<0x140, std::array<std::uint64_t, 8>> paScPerfCounters;
      Register<0x180> spiPerfCounter0Hi;
      Register<0x181> spiPerfCounter0Lo;
      Register<0x182> spiPerfCounter1Hi;
      Register<0x183> spiPerfCounter1Lo;
      Register<0x184> spiPerfCounter2Hi;
      Register<0x185> spiPerfCounter2Lo;
      Register<0x186> spiPerfCounter3Hi;
      Register<0x187> spiPerfCounter3Lo;
      Register<0x188> spiPerfCounter4Hi;
      Register<0x189> spiPerfCounter4Lo;
      Register<0x18a> spiPerfCounter5Hi;
      Register<0x18b> spiPerfCounter5Lo;
      Register<0x1c0, std::array<std::uint64_t, 16>> sqPerfCounters;
      Register<0x240, std::array<std::uint64_t, 4>> sxPerfCounters;
      Register<0x280, std::array<std::uint64_t, 4>> gdsPerfCounters;
      Register<0x2c0, std::array<std::uint64_t, 2>> taPerfCounters;
      Register<0x300, std::array<std::uint64_t, 2>> tdPerfCounters;
      Register<0x340, std::array<std::uint64_t, 4>> tcpPerfCounters;
      Register<0x380, std::array<std::uint64_t, 4>> tccPerfCounters;
      Register<0x390, std::array<std::uint64_t, 4>> tcaPerfCounters;
      Register<0x3a0, std::array<std::uint64_t, 4>> tcsPerfCounters;
      Register<0x406, std::array<std::uint64_t, 4>> cbPerfCounters;
      Register<0x440, std::array<std::uint64_t, 4>> dbPerfCounters;
      Register<0x800> cpgPerfCounter1Select;
      Register<0x801> cpgPerfCounter0Select1;
      Register<0x802> cpgPerfCounter0Select;
      Register<0x803> cpcPerfCounter1Select;
      Register<0x804> cpcPerfCounter0Select1;
      Register<0x805> cpfPerfCounter1Select;
      Register<0x806> cpfPerfCounter0Select1;
      Register<0x807> cpfPerfCounter0Select;
      Register<0x808> cpPerfMonCntl;
      Register<0x809> cpcPerfCounter0Select;
      Register<0x880> wdPerfCounter0Select;
      Register<0x881> wdPerfCounter1Select;
      Register<0x882> wdPerfCounter2Select;
      Register<0x883> wdPerfCounter3Select;
      Register<0x884> iaPerfCounter0Select;
      Register<0x885> iaPerfCounter1Select;
      Register<0x886> iaPerfCounter2Select;
      Register<0x887> iaPerfCounter3Select;
      Register<0x888> iaPerfCounter0Select1;
      Register<0x88c> vgtPerfCounter0Select;
      Register<0x88d> vgtPerfCounter1Select;
      Register<0x88e> vgtPerfCounter2Select;
      Register<0x88f> vgtPerfCounter3Select;
      Register<0x890> vgtPerfCounter0Select1;
      Register<0x891> vgtPerfCounter1Select1;
      Register<0x900> paSuPerfCounter0Select;
      Register<0x901> paSuPerfCounter0Select1;
      Register<0x902> paSuPerfCounter1Select;
      Register<0x903> paSuPerfCounter1Select1;
      Register<0x904> paSuPerfCounter2Select;
      Register<0x905> paSuPerfCounter3Select;
      Register<0x940> paScPerfCounter0Select;
      Register<0x941> paScPerfCounter0Select1;
      Register<0x942> paScPerfCounter1Select;
      Register<0x943> paScPerfCounter2Select;
      Register<0x944> paScPerfCounter3Select;
      Register<0x945> paScPerfCounter4Select;
      Register<0x946> paScPerfCounter5Select;
      Register<0x947> paScPerfCounter6Select;
      Register<0x948> paScPerfCounter7Select;
      Register<0x980> spiPerfCounter0Select;
      Register<0x981> spiPerfCounter1Select;
      Register<0x982> spiPerfCounter2Select;
      Register<0x983> spiPerfCounter3Select;
      Register<0x984> spiPerfCounter0Select1;
      Register<0x985> spiPerfCounter1Select1;
      Register<0x986> spiPerfCounter2Select1;
      Register<0x987> spiPerfCounter3Select1;
      Register<0x988> spiPerfCounter4Select;
      Register<0x989> spiPerfCounter5Select;
      Register<0x98a> spiPerfCounterBins;
      Register<0x9c0, std::array<std::uint32_t, 16>> sqPerfCountersSelect;
      Register<0x9e0> sqPerfCounterCtrl;
      Register<0xa40> sxPerfCounter0Select;
      Register<0xa41> sxPerfCounter1Select;
      Register<0xa42> sxPerfCounter2Select;
      Register<0xa43> sxPerfCounter3Select;
      Register<0xa44> sxPerfCounter0Select1;
      Register<0xa45> sxPerfCounter1Select1;
      Register<0xa80> gdsPerfCounter0Select;
      Register<0xa81> gdsPerfCounter1Select;
      Register<0xa82> gdsPerfCounter2Select;
      Register<0xa83> gdsPerfCounter3Select;
      Register<0xa84> gdsPerfCounter0Select1;
      Register<0xac0> taPerfCounter0Select;
      Register<0xac1> taPerfCounter0Select1;
      Register<0xac2> taPerfCounter1Select;
      Register<0xb00> tdPerfCounter0Select;
      Register<0xb01> tdPerfCounter0Select1;
      Register<0xb02> tdPerfCounter1Select;
      Register<0xb40> tcpPerfCounter0Select;
      Register<0xb41> tcpPerfCounter0Select1;
      Register<0xb42> tcpPerfCounter1Select;
      Register<0xb43> tcpPerfCounter1Select1;
      Register<0xb44> tcpPerfCounter2Select;
      Register<0xb45> tcpPerfCounter3Select;
      Register<0xb80> tccPerfCounter0Select;
      Register<0xb81> tccPerfCounter0Select1;
      Register<0xb82> tccPerfCounter1Select;
      Register<0xb83> tccPerfCounter1Select1;
      Register<0xb84> tccPerfCounter2Select;
      Register<0xb85> tccPerfCounter3Select;
      Register<0xb90> tcaPerfCounter0Select;
      Register<0xb91> tcaPerfCounter0Select1;
      Register<0xb92> tcaPerfCounter1Select;
      Register<0xb93> tcaPerfCounter1Select1;
      Register<0xb94> tcaPerfCounter2Select;
      Register<0xb95> tcaPerfCounter3Select;
      Register<0xba0> tcsPerfCounter0Select;
      Register<0xba1> tcsPerfCounter0Select1;
      Register<0xba2> tcsPerfCounter1Select;
      Register<0xba3> tcsPerfCounter2Select;
      Register<0xba4> tcsPerfCounter3Select;
      Register<0xc00> cbPerfCounterFilter;
      Register<0xc01> cbPerfCounter0Select;
      Register<0xc02> cbPerfCounter0Select1;
      Register<0xc03> cbPerfCounter1Select;
      Register<0xc04> cbPerfCounter2Select;
      Register<0xc05> cbPerfCounter3Select;
      Register<0xc40> dbPerfCounter0Select;
      Register<0xc41> dbPerfCounter0Select1;
      Register<0xc42> dbPerfCounter1Select;
      Register<0xc43> dbPerfCounter1Select1;
      Register<0xc44> dbPerfCounter2Select;
      Register<0xc46> dbPerfCounter3Select;
    };
  };

  union {
    Register<0x50c, std::uint32_t> vmContext0ProtectionIntrCtl;
    Register<0x50d, std::uint32_t> vmContext1ProtectionIntrCtl;
    Register<0x536, VmProtectionFault> vmContext0ProtectionFault;
    Register<0x537, VmProtectionFault> vmContext1ProtectionFault;
    Register<0x53e, std::uint32_t>
        vmContext0ProtectionFaultPage; // address >> 12
    Register<0x53f, std::uint32_t>
        vmContext1ProtectionFaultPage; // address >> 12
    Register<0x809, FbInfo> fbInfo;
    Register<0xf82, std::uint32_t> ihRptr;
    Register<0xf83, std::uint32_t> ihWptr;

    Register<Config::kMmioOffset, Config> config;
    Register<ShaderConfig::kMmioOffset, ShaderConfig> sh;

    Register<0x3045> cpRbWptr;
    Register<0x3064> cpRb1Wptr;
    Register<0x3069> cpRb2Wptr;
    Register<0x3049> cpIntCntl;
    Register<0x304a> cpIntStatus;
    Register<0x306a, std::array<std::uint32_t, 3>> cpIntCntlRings;
    Register<0x306d, std::array<std::uint32_t, 3>> cpIntStatusRings;
    Register<0x324b> cpHqdQueuePriority;
    Register<0x324c> cpHqdQuantum;

    Register<Context::kMmioOffset, Context> context;
    Register<UConfig::kMmioOffset, UConfig> uconfig;
    Register<Counters::kMmioOffset, Counters> counters;

    std::uint32_t raw[kRegisterCount];
  };
};

#pragma pack(pop)
} // namespace amdgpu
