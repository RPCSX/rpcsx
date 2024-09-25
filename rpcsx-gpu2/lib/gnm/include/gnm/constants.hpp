#pragma once

#include <cstdint>

namespace gnm {
enum DataFormat : unsigned {
  kDataFormatInvalid = 0x00000000,
  kDataFormat8 = 0x00000001,
  kDataFormat16 = 0x00000002,
  kDataFormat8_8 = 0x00000003,
  kDataFormat32 = 0x00000004,
  kDataFormat16_16 = 0x00000005,
  kDataFormat10_11_11 = 0x00000006,
  kDataFormat11_11_10 = 0x00000007,
  kDataFormat10_10_10_2 = 0x00000008,
  kDataFormat2_10_10_10 = 0x00000009,
  kDataFormat8_8_8_8 = 0x0000000a,
  kDataFormat32_32 = 0x0000000b,
  kDataFormat16_16_16_16 = 0x0000000c,
  kDataFormat32_32_32 = 0x0000000d,
  kDataFormat32_32_32_32 = 0x0000000e,
  kDataFormat5_6_5 = 0x00000010,
  kDataFormat1_5_5_5 = 0x00000011,
  kDataFormat5_5_5_1 = 0x00000012,
  kDataFormat4_4_4_4 = 0x00000013,
  kDataFormat8_24 = 0x00000014,
  kDataFormat24_8 = 0x00000015,
  kDataFormatX24_8_32 = 0x00000016,
  kDataFormatGB_GR = 0x00000020,
  kDataFormatBG_RG = 0x00000021,
  kDataFormat5_9_9_9 = 0x00000022,
  kDataFormatBc1 = 0x00000023,
  kDataFormatBc2 = 0x00000024,
  kDataFormatBc3 = 0x00000025,
  kDataFormatBc4 = 0x00000026,
  kDataFormatBc5 = 0x00000027,
  kDataFormatBc6 = 0x00000028,
  kDataFormatBc7 = 0x00000029,
  kDataFormatFmask8_S2_F1 = 0x0000002C,
  kDataFormatFmask8_S4_F1 = 0x0000002D,
  kDataFormatFmask8_S8_F1 = 0x0000002E,
  kDataFormatFmask8_S2_F2 = 0x0000002F,
  kDataFormatFmask8_S4_F2 = 0x00000030,
  kDataFormatFmask8_S4_F4 = 0x00000031,
  kDataFormatFmask16_S16_F1 = 0x00000032,
  kDataFormatFmask16_S8_F2 = 0x00000033,
  kDataFormatFmask32_S16_F2 = 0x00000034,
  kDataFormatFmask32_S8_F4 = 0x00000035,
  kDataFormatFmask32_S8_F8 = 0x00000036,
  kDataFormatFmask64_S16_F4 = 0x00000037,
  kDataFormatFmask64_S16_F8 = 0x00000038,
  kDataFormat4_4 = 0x00000039,
  kDataFormat6_5_5 = 0x0000003A,
  kDataFormat1 = 0x0000003B,
  kDataFormat1Reversed = 0x0000003C,
};

enum NumericFormat : unsigned {
  kNumericFormatUNorm = 0x00000000,
  kNumericFormatSNorm = 0x00000001,
  kNumericFormatUScaled = 0x00000002,
  kNumericFormatSScaled = 0x00000003,
  kNumericFormatUInt = 0x00000004,
  kNumericFormatSInt = 0x00000005,
  kNumericFormatSNormNoZero = 0x00000006,
  kNumericFormatFloat = 0x00000007,
  kNumericFormatSrgb = 0x00000009,
  kNumericFormatUBNorm = 0x0000000A,
  kNumericFormatUBNormNoZero = 0x0000000B,
  kNumericFormatUBInt = 0x0000000C,
  kNumericFormatUBScaled = 0x0000000D,
};

enum ZFormat {
  kZFormatInvalid = 0,
  kZFormat16 = 1,
  kZFormat32Float = 3,
};

enum StencilFormat {
  kStencilInvalid = 0,
  kStencil8 = 1,
};

enum class TextureType : std::uint8_t {
  Dim1D = 8,
  Dim2D,
  Dim3D,
  Cube,
  Array1D,
  Array2D,
  Msaa2D,
  MsaaArray2D,
};

enum class IndexType : std::uint8_t {
  Int16,
  Int32,
};

enum class PrimitiveType : std::uint8_t {
  None = 0x00,
  PointList = 0x01,
  LineList = 0x02,
  LineStrip = 0x03,
  TriList = 0x04,
  TriFan = 0x05,
  TriStrip = 0x06,
  Patch = 0x09,
  LineListAdjacency = 0x0a,
  LineStripAdjacency = 0x0b,
  TriListAdjacency = 0x0c,
  TriStripAdjacency = 0x0d,
  RectList = 0x11,
  LineLoop = 0x12,
  QuadList = 0x13,
  QuadStrip = 0x14,
  Polygon = 0x15,
};

enum class StencilOp : std::uint8_t {
  Keep,
  Zero,
  Ones,
  ReplaceTest,
  ReplaceOp,
  AddClamp,
  SubClamp,
  Invert,
  AddWrap,
  SubWrap,
  And,
  Or,
  Xor,
  Nand,
  Nor,
  Xnor,
};

enum class RasterOp : std::uint8_t {
  Blackness = 0x00,
  Nor = 0x05,
  AndInverted = 0x0a,
  CopyInverted = 0x0f,
  AndReverse = 0x44,
  Invert = 0x55,
  Xor = 0x5a,
  Nand = 0x5f,
  And = 0x88,
  Equiv = 0x99,
  Noop = 0xaa,
  OrInverted = 0xaf,
  Copy = 0xcc,
  OrReverse = 0xdd,
  Or = 0xee,
  Set = 0xff,
};

enum class CompareFunc : std::uint8_t {
  Never,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  Always,
};

enum class BorderColor : std::uint8_t {
  OpaqueBlack,
  TransparentBlack,
  White,
  Custom,
};

enum class FilterMode : std::uint8_t {
  Blend,
  Min,
  Max,
};
enum class Filter : std::uint8_t {
  Point,
  Bilinear,
  AnisoPoint,
  AnisoLinear,
};
enum class MipFilter : std::uint8_t {
  None = 0,
  Point = 1,
  Linear = 2,
};

enum class CbMode : std::uint8_t {
  Disable = 0,
  Normal = 1,
  EliminateFastClear = 2,
  Resolve = 3,
  FmaskDecompress = 5,
  DccDecompress = 6,
};

enum class Swizzle : std::uint8_t {
  Zero = 0,
  One = 1,
  R = 4,
  G = 5,
  B = 6,
  A = 7,
};

enum class BlendMultiplier : std::uint8_t {
  Zero = 0x00,
  One = 0x01,
  SrcColor = 0x02,
  OneMinusSrcColor = 0x03,
  SrcAlpha = 0x04,
  OneMinusSrcAlpha = 0x05,
  DestAlpha = 0x06,
  OneMinusDestAlpha = 0x07,
  DestColor = 0x08,
  OneMinusDestColor = 0x09,
  SrcAlphaSaturate = 0x0a,
  ConstantColor = 0x0d,
  OneMinusConstantColor = 0x0e,
  Src1Color = 0x0f,
  InverseSrc1Color = 0x10,
  Src1Alpha = 0x11,
  InverseSrc1Alpha = 0x12,
  ConstantAlpha = 0x13,
  OneMinusConstantAlpha = 0x14,
};

enum class BlendFunc : std::uint8_t {
  Add = 0,
  Subtract = 1,
  Min = 2,
  Max = 3,
  ReverseSubtract = 4,
};

enum class Face : std::uint8_t { CCW, CW };

enum class PolyMode : std::uint8_t { Disable, Dual };

enum class PolyModePtype : std::uint8_t {
  Points,
  Lines,
  Triangles,
};

enum class RoundMode : std::uint8_t {
  Truncate,
  Round,
  RoundToEven,
  RoundToOdd,
};

enum class QuantMode : std::uint8_t {
  Fp16_8_4,
  Fp16_8_3,
  Fp16_8_2,
  Fp16_8_1,
  Fp16_8_0,
  Fp16_8_8,
  Fp14_10,
  Fp12_12,
};

enum class ClampMode : std::uint8_t {
  Wrap,
  Mirror,
  ClampLastTexel,
  MirrorOnceLastTexel,
  ClampHalfBorder,
  MirrorOnceHalfBorder,
  ClampBorder,
  MirrorOnceBorder,
};

enum class AnisoRatio : std::uint8_t {
  x1,
  x2,
  x4,
  x8,
  x16,
};

} // namespace gnm
