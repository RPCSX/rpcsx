
#define FOR_ALL_BASE_TYPES(OP) \
    OP(int8_t) \
    OP(uint8_t) \
    OP(int16_t) \
    OP(uint16_t) \
    OP(float16_t) \
    OP(int32_t) \
    OP(uint32_t) \
    OP(float32_t) \
    OP(int64_t) \
    OP(uint64_t) \
    OP(float64_t) \

#define DEFINE_BUFFER_REFERENCE(TYPE) \
    layout(buffer_reference) buffer buffer_reference_##TYPE { \
        TYPE data; \
    }; \

FOR_ALL_BASE_TYPES(DEFINE_BUFFER_REFERENCE)

#define U32ARRAY_FETCH_BITS(ARRAY, START, BITCOUNT)  ((ARRAY[(START) >> 5] >> ((START) & 31)) & ((1 << (BITCOUNT)) - 1))
#define U64ARRAY_FETCH_BITS(ARRAY, START, BITCOUNT)  ((ARRAY[(START) >> 6] >> ((START) & 63)) & ((uint64_t(1) << (BITCOUNT)) - 1))

uint64_t tbuffer_base(u64vec4 tbuffer) {
    return U64ARRAY_FETCH_BITS(tbuffer, 0, 38);
}
uint32_t tbuffer_mtype_L2(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 38, 2));
}
uint32_t tbuffer_min_lod(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 40, 12));
}
uint32_t tbuffer_dfmt(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 52, 6));
}
uint32_t tbuffer_nfmt(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 58, 4));
}
uint32_t tbuffer_mtype_l1(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 62, 2) | (U64ARRAY_FETCH_BITS(tbuffer, 122, 1) << 2));
}
uint32_t tbuffer_width(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 64, 14));
}
uint32_t tbuffer_height(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 78, 14));
}
uint32_t tbuffer_perfMod(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 92, 3));
}
bool tbuffer_interlaced(u64vec4 tbuffer) {
    return U64ARRAY_FETCH_BITS(tbuffer, 95, 1) != 0;
}
uint32_t tbuffer_dst_sel_x(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 96, 3));
}
uint32_t tbuffer_dst_sel_y(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 99, 3));
}
uint32_t tbuffer_dst_sel_z(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 102, 3));
}
uint32_t tbuffer_dst_sel_w(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 105, 3));
}
uint32_t tbuffer_base_level(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 108, 4));
}
uint32_t tbuffer_last_level(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 112, 4));
}
uint32_t tbuffer_tiling_idx(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 116, 5));
}
bool tbuffer_pow2pad(u64vec4 tbuffer) {
    return U64ARRAY_FETCH_BITS(tbuffer, 121, 1) != 0;
}
uint32_t tbuffer_type(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 124, 4));
}
uint32_t tbuffer_depth(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 128, 13));
}
uint32_t tbuffer_pitch(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 141, 14));
}
uint32_t tbuffer_base_array(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 160, 13));
}
uint32_t tbuffer_last_array(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 173, 13));
}
uint32_t tbuffer_min_lod_warn(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 192, 12));
}
uint32_t tbuffer_counter_bank_id(u64vec4 tbuffer) {
    return uint32_t(U64ARRAY_FETCH_BITS(tbuffer, 204, 8));
}
bool tbuffer_LOD_hdw_cnt_en(u64vec4 tbuffer) {
    return U64ARRAY_FETCH_BITS(tbuffer, 212, 1) != 0;
}

const int kTextureType1D = 8;
const int kTextureType2D = 9;
const int kTextureType3D = 10;
const int kTextureTypeCube = 11;
const int kTextureTypeArray1D = 12;
const int kTextureTypeArray2D = 13;
const int kTextureTypeMsaa2D = 14;
const int kTextureTypeMsaaArray2D = 15;

const uint32_t kMicroTileWidth = 8;
const uint32_t kMicroTileHeight = 8;
const uint32_t kDramRowSize = 0x400;
const uint32_t kPipeInterleaveBytes = 256;


const uint32_t kDataFormatInvalid = 0x00000000;
const uint32_t kDataFormat8 = 0x00000001;
const uint32_t kDataFormat16 = 0x00000002;
const uint32_t kDataFormat8_8 = 0x00000003;
const uint32_t kDataFormat32 = 0x00000004;
const uint32_t kDataFormat16_16 = 0x00000005;
const uint32_t kDataFormat10_11_11 = 0x00000006;
const uint32_t kDataFormat11_11_10 = 0x00000007;
const uint32_t kDataFormat10_10_10_2 = 0x00000008;
const uint32_t kDataFormat2_10_10_10 = 0x00000009;
const uint32_t kDataFormat8_8_8_8 = 0x0000000a;
const uint32_t kDataFormat32_32 = 0x0000000b;
const uint32_t kDataFormat16_16_16_16 = 0x0000000c;
const uint32_t kDataFormat32_32_32 = 0x0000000d;
const uint32_t kDataFormat32_32_32_32 = 0x0000000e;
const uint32_t kDataFormat5_6_5 = 0x00000010;
const uint32_t kDataFormat1_5_5_5 = 0x00000011;
const uint32_t kDataFormat5_5_5_1 = 0x00000012;
const uint32_t kDataFormat4_4_4_4 = 0x00000013;
const uint32_t kDataFormat8_24 = 0x00000014;
const uint32_t kDataFormat24_8 = 0x00000015;
const uint32_t kDataFormatX24_8_32 = 0x00000016;
const uint32_t kDataFormatGB_GR = 0x00000020;
const uint32_t kDataFormatBG_RG = 0x00000021;
const uint32_t kDataFormat5_9_9_9 = 0x00000022;
const uint32_t kDataFormatBc1 = 0x00000023;
const uint32_t kDataFormatBc2 = 0x00000024;
const uint32_t kDataFormatBc3 = 0x00000025;
const uint32_t kDataFormatBc4 = 0x00000026;
const uint32_t kDataFormatBc5 = 0x00000027;
const uint32_t kDataFormatBc6 = 0x00000028;
const uint32_t kDataFormatBc7 = 0x00000029;
const uint32_t kDataFormatFmask8_S2_F1 = 0x0000002C;
const uint32_t kDataFormatFmask8_S4_F1 = 0x0000002D;
const uint32_t kDataFormatFmask8_S8_F1 = 0x0000002E;
const uint32_t kDataFormatFmask8_S2_F2 = 0x0000002F;
const uint32_t kDataFormatFmask8_S4_F2 = 0x00000030;
const uint32_t kDataFormatFmask8_S4_F4 = 0x00000031;
const uint32_t kDataFormatFmask16_S16_F1 = 0x00000032;
const uint32_t kDataFormatFmask16_S8_F2 = 0x00000033;
const uint32_t kDataFormatFmask32_S16_F2 = 0x00000034;
const uint32_t kDataFormatFmask32_S8_F4 = 0x00000035;
const uint32_t kDataFormatFmask32_S8_F8 = 0x00000036;
const uint32_t kDataFormatFmask64_S16_F4 = 0x00000037;
const uint32_t kDataFormatFmask64_S16_F8 = 0x00000038;
const uint32_t kDataFormat4_4 = 0x00000039;
const uint32_t kDataFormat6_5_5 = 0x0000003A;
const uint32_t kDataFormat1 = 0x0000003B;
const uint32_t kDataFormat1Reversed = 0x0000003C;

const uint32_t kNumericFormatUNorm = 0x00000000;
const uint32_t kNumericFormatSNorm = 0x00000001;
const uint32_t kNumericFormatUScaled = 0x00000002;
const uint32_t kNumericFormatSScaled = 0x00000003;
const uint32_t kNumericFormatUInt = 0x00000004;
const uint32_t kNumericFormatSInt = 0x00000005;
const uint32_t kNumericFormatSNormNoZero = 0x00000006;
const uint32_t kNumericFormatFloat = 0x00000007;
const uint32_t kNumericFormatSrgb = 0x00000009;
const uint32_t kNumericFormatUBNorm = 0x0000000A;
const uint32_t kNumericFormatUBNormNoZero = 0x0000000B;
const uint32_t kNumericFormatUBInt = 0x0000000C;
const uint32_t kNumericFormatUBScaled = 0x0000000D;

const uint32_t kArrayModeLinearGeneral = 0x00000000;
const uint32_t kArrayModeLinearAligned = 0x00000001;
const uint32_t kArrayMode1dTiledThin = 0x00000002;
const uint32_t kArrayMode1dTiledThick = 0x00000003;
const uint32_t kArrayMode2dTiledThin = 0x00000004;
const uint32_t kArrayModeTiledThinPrt = 0x00000005;
const uint32_t kArrayMode2dTiledThinPrt = 0x00000006;
const uint32_t kArrayMode2dTiledThick = 0x00000007;
const uint32_t kArrayMode2dTiledXThick = 0x00000008;
const uint32_t kArrayModeTiledThickPrt = 0x00000009;
const uint32_t kArrayMode2dTiledThickPrt = 0x0000000a;
const uint32_t kArrayMode3dTiledThinPrt = 0x0000000b;
const uint32_t kArrayMode3dTiledThin = 0x0000000c;
const uint32_t kArrayMode3dTiledThick = 0x0000000d;
const uint32_t kArrayMode3dTiledXThick = 0x0000000e;
const uint32_t kArrayMode3dTiledThickPrt = 0x0000000f;

const uint32_t kMicroTileModeDisplay = 0x00000000;
const uint32_t kMicroTileModeThin = 0x00000001;
const uint32_t kMicroTileModeDepth = 0x00000002;
const uint32_t kMicroTileModeRotated = 0x00000003;
const uint32_t kMicroTileModeThick = 0x00000004;

const uint32_t kPipeConfigP8_32x32_8x16 = 0x0000000a;
const uint32_t kPipeConfigP8_32x32_16x16 = 0x0000000c;
const uint32_t kPipeConfigP16 = 0x00000012;



uint32_t getMicroTileThickness(uint32_t arrayMode) {
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

  return 1;
}

bool isMacroTiled(uint32_t arrayMode) {
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

  return false;
}

bool isPrt(uint32_t arrayMode) {
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

  return false;
}

int getTexelsPerElement(uint32_t dfmt) {
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

int getBitsPerElement(uint32_t dfmt) {
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

int getTotalBitsPerElement(uint32_t dfmt) {
  return getBitsPerElement(dfmt) * getTexelsPerElement(dfmt);
}

int getNumComponentsPerElement(uint32_t dfmt) {
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

uint32_t tileMode_getArrayMode(uint32_t tileMode) {
    return (tileMode & 0x0000003c) >> 2;
}
uint32_t tileMode_getPipeConfig(uint32_t tileMode) {
    return (tileMode & 0x000007c0) >> 6;
}
uint32_t tileMode_getTileSplit(uint32_t tileMode) {
    return (tileMode & 0x00003800) >> 11;
}
uint32_t tileMode_getMicroTileMode(uint32_t tileMode) {
    return (tileMode & 0x01c00000) >> 22;
}
uint32_t tileMode_getSampleSplit(uint32_t tileMode) {
    return (tileMode & 0x06000000) >> 25;
}

uint32_t macroTileMode_getBankWidth(uint32_t tileMode) {
    return (tileMode & 0x00000003) >> 0;
}
uint32_t macroTileMode_getBankHeight(uint32_t tileMode) {
    return (tileMode & 0x0000000c) >> 2;
}
uint32_t macroTileMode_getMacroTileAspect(uint32_t tileMode) {
    return (tileMode & 0x00000030) >> 4;
}
uint32_t macroTileMode_getNumBanks(uint32_t tileMode) {
    return (tileMode & 0x000000c0) >> 6;
}

uint32_t getPipeCount(uint32_t pipeConfig) {
  switch (pipeConfig) {
  case kPipeConfigP8_32x32_8x16:
  case kPipeConfigP8_32x32_16x16:
    return 8;
  case kPipeConfigP16:
    return 16;
  default:
    return 0;
  }
}

uint32_t getPipeIndex(uint32_t x, uint32_t y, uint32_t pipeCfg) {
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
  }
  return pipe;
}

uint32_t getBankIndex(uint32_t x, uint32_t y, uint32_t bank_width, uint32_t bank_height, uint32_t num_banks, uint32_t num_pipes) {
  uint32_t x_shift_offset = findLSB(bank_width * num_pipes);
  uint32_t y_shift_offset = findLSB(bank_height);
  uint32_t xs = x >> x_shift_offset;
  uint32_t ys = y >> y_shift_offset;
  uint32_t bank = 0;
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
    break;
  }

  return bank;
}

uint32_t bit_ceil(uint32_t x) {
  x = x - 1;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

uint32_t getElementIndex(uvec3 pos, uint32_t bitsPerElement, uint32_t microTileMode, uint32_t arrayMode) {
  uint32_t elem = 0;

  if (microTileMode == kMicroTileModeDisplay) {
    switch (bitsPerElement) {
    case 8:
      elem |= ((pos.x >> 0) & 0x1) << 0;
      elem |= ((pos.x >> 1) & 0x1) << 1;
      elem |= ((pos.x >> 2) & 0x1) << 2;
      elem |= ((pos.y >> 1) & 0x1) << 3;
      elem |= ((pos.y >> 0) & 0x1) << 4;
      elem |= ((pos.y >> 2) & 0x1) << 5;
      break;
    case 16:
      elem |= ((pos.x >> 0) & 0x1) << 0;
      elem |= ((pos.x >> 1) & 0x1) << 1;
      elem |= ((pos.x >> 2) & 0x1) << 2;
      elem |= ((pos.y >> 0) & 0x1) << 3;
      elem |= ((pos.y >> 1) & 0x1) << 4;
      elem |= ((pos.y >> 2) & 0x1) << 5;
      break;
    case 32:
      elem |= ((pos.x >> 0) & 0x1) << 0;
      elem |= ((pos.x >> 1) & 0x1) << 1;
      elem |= ((pos.y >> 0) & 0x1) << 2;
      elem |= ((pos.x >> 2) & 0x1) << 3;
      elem |= ((pos.y >> 1) & 0x1) << 4;
      elem |= ((pos.y >> 2) & 0x1) << 5;
      break;
    case 64:
      elem |= ((pos.x >> 0) & 0x1) << 0;
      elem |= ((pos.y >> 0) & 0x1) << 1;
      elem |= ((pos.x >> 1) & 0x1) << 2;
      elem |= ((pos.x >> 2) & 0x1) << 3;
      elem |= ((pos.y >> 1) & 0x1) << 4;
      elem |= ((pos.y >> 2) & 0x1) << 5;
      break;
    }
  } else if (microTileMode == kMicroTileModeThin ||
             microTileMode == kMicroTileModeDepth) {
    elem |= ((pos.x >> 0) & 0x1) << 0;
    elem |= ((pos.y >> 0) & 0x1) << 1;
    elem |= ((pos.x >> 1) & 0x1) << 2;
    elem |= ((pos.y >> 1) & 0x1) << 3;
    elem |= ((pos.x >> 2) & 0x1) << 4;
    elem |= ((pos.y >> 2) & 0x1) << 5;

    switch (arrayMode) {
    case kArrayMode2dTiledXThick:
    case kArrayMode3dTiledXThick:
      elem |= ((pos.z >> 2) & 0x1) << 8;
    case kArrayMode1dTiledThick:
    case kArrayMode2dTiledThick:
    case kArrayMode3dTiledThick:
    case kArrayModeTiledThickPrt:
    case kArrayMode2dTiledThickPrt:
    case kArrayMode3dTiledThickPrt:
      elem |= ((pos.z >> 0) & 0x1) << 6;
      elem |= ((pos.z >> 1) & 0x1) << 7;
    default:
      break;
    }
  } else if (microTileMode == kMicroTileModeThick) {
    switch (arrayMode) {
    case kArrayMode2dTiledXThick:
    case kArrayMode3dTiledXThick:
      elem |= ((pos.z >> 2) & 0x1) << 8;

    case kArrayMode1dTiledThick:
    case kArrayMode2dTiledThick:
    case kArrayMode3dTiledThick:
    case kArrayModeTiledThickPrt:
    case kArrayMode2dTiledThickPrt:
    case kArrayMode3dTiledThickPrt:
      if (bitsPerElement == 8 || bitsPerElement == 16) {
        elem |= ((pos.x >> 0) & 0x1) << 0;
        elem |= ((pos.y >> 0) & 0x1) << 1;
        elem |= ((pos.x >> 1) & 0x1) << 2;
        elem |= ((pos.y >> 1) & 0x1) << 3;
        elem |= ((pos.z >> 0) & 0x1) << 4;
        elem |= ((pos.z >> 1) & 0x1) << 5;
        elem |= ((pos.x >> 2) & 0x1) << 6;
        elem |= ((pos.y >> 2) & 0x1) << 7;
      } else if (bitsPerElement == 32) {
        elem |= ((pos.x >> 0) & 0x1) << 0;
        elem |= ((pos.y >> 0) & 0x1) << 1;
        elem |= ((pos.x >> 1) & 0x1) << 2;
        elem |= ((pos.z >> 0) & 0x1) << 3;
        elem |= ((pos.y >> 1) & 0x1) << 4;
        elem |= ((pos.z >> 1) & 0x1) << 5;
        elem |= ((pos.x >> 2) & 0x1) << 6;
        elem |= ((pos.y >> 2) & 0x1) << 7;
      } else if (bitsPerElement == 64 || bitsPerElement == 128) {
        elem |= ((pos.x >> 0) & 0x1) << 0;
        elem |= ((pos.y >> 0) & 0x1) << 1;
        elem |= ((pos.z >> 0) & 0x1) << 2;
        elem |= ((pos.x >> 1) & 0x1) << 3;
        elem |= ((pos.y >> 1) & 0x1) << 4;
        elem |= ((pos.z >> 1) & 0x1) << 5;
        elem |= ((pos.x >> 2) & 0x1) << 6;
        elem |= ((pos.y >> 2) & 0x1) << 7;
      }
      break;
    }
  }
  return elem;
}

uint64_t computeLinearElementByteOffset(
    uvec3 pos, uint32_t fragmentIndex, uint32_t pitch,
    uint32_t slicePitchElems, uint32_t bitsPerElement,
    uint32_t numFragmentsPerPixel) {
  uint64_t absoluteElementIndex = pos.z * slicePitchElems + pos.y * pitch + pos.x;
  return ((absoluteElementIndex * bitsPerElement * numFragmentsPerPixel) +
          (bitsPerElement * fragmentIndex)) / 8;
}

uint64_t computeLinearOffset(uint32_t bitsPerElement, uint height, uint pitch, uvec3 pos) {
  uint paddedHeight = height;
  uint paddedWidth = pitch;

  if (bitsPerElement == 1) {
    bitsPerElement *= 8;
    paddedWidth = max((paddedWidth + 7) / 8, 1);
  }

  uint64_t tiledRowSizeBits = uint64_t(bitsPerElement) * paddedWidth;
  uint64_t tiledSliceBits = uint64_t(paddedWidth) * paddedHeight * bitsPerElement;
  return tiledSliceBits * pos.z + tiledRowSizeBits * pos.y + bitsPerElement * pos.x;
}

uint64_t getTiledBitOffset1D(uint32_t tileMode, uvec3 pos, uvec2 dataSize, uint32_t bitsPerElement) {
    uint32_t arrayMode = tileMode_getArrayMode(tileMode);

    uint32_t paddedWidth = dataSize.x;
    uint32_t paddedHeight = dataSize.y;

    int tileThickness = (arrayMode == kArrayMode1dTiledThick) ? 4 : 1;

    uint64_t tileBytes = (kMicroTileWidth * kMicroTileHeight * tileThickness * bitsPerElement + 7) / 8;
    uint32_t tilesPerRow = paddedWidth / kMicroTileWidth;
    uint32_t tilesPerSlice = max(tilesPerRow * (paddedHeight / kMicroTileHeight), 1);

    uint64_t elementIndex = getElementIndex(pos, bitsPerElement,
                                            tileMode_getMicroTileMode(tileMode), arrayMode);

    uint64_t sliceOffset = (pos.z / tileThickness) * tilesPerSlice * tileBytes;

    uint64_t tileRowIndex = pos.y / kMicroTileHeight;
    uint64_t tileColumnIndex = pos.x / kMicroTileWidth;
    uint64_t tileOffset =
        (tileRowIndex * tilesPerRow + tileColumnIndex) * tileBytes;

    uint64_t elementOffset = elementIndex * bitsPerElement;
    return (sliceOffset + tileOffset) * 8 + elementOffset;
}


uint64_t getTiledBitOffset2D(uint32_t dfmt, uint32_t tileMode, uint32_t macroTileMode,
                            uvec2 dataSize, int arraySlice, uint32_t numFragments, u32vec3 pos, int fragmentIndex) {
  uint32_t bitsPerFragment = getBitsPerElement(dfmt);

  bool isBlockCompressed = getTexelsPerElement(dfmt) > 1;
  uint32_t tileSwizzleMask = 0;
  uint32_t numFragmentsPerPixel = 1 << numFragments;
  uint32_t arrayMode = tileMode_getArrayMode(tileMode);

  uint32_t tileThickness = 1;

  switch (arrayMode) {
  case kArrayMode2dTiledThin:
  case kArrayMode3dTiledThin:
  case kArrayModeTiledThinPrt:
  case kArrayMode2dTiledThinPrt:
  case kArrayMode3dTiledThinPrt:
    tileThickness = 1;
    break;
  case kArrayMode1dTiledThick:
  case kArrayMode2dTiledThick:
  case kArrayMode3dTiledThick:
  case kArrayModeTiledThickPrt:
  case kArrayMode2dTiledThickPrt:
  case kArrayMode3dTiledThickPrt:
    tileThickness = 4;
    break;
  case kArrayMode2dTiledXThick:
  case kArrayMode3dTiledXThick:
    tileThickness = 8;
    break;
  default:
    break;
  }

  uint32_t bitsPerElement = bitsPerFragment;
  uint32_t paddedWidth = dataSize.x;
  uint32_t paddedHeight = dataSize.y;

  uint32_t bankWidthHW = macroTileMode_getBankWidth(macroTileMode);
  uint32_t bankHeightHW = macroTileMode_getBankHeight(macroTileMode);
  uint32_t macroAspectHW = macroTileMode_getMacroTileAspect(macroTileMode);
  uint32_t numBanksHW = macroTileMode_getNumBanks(macroTileMode);

  uint32_t bankWidth = 1 << bankWidthHW;
  uint32_t bankHeight = 1 << bankHeightHW;
  uint32_t numBanks = 2 << numBanksHW;
  uint32_t macroTileAspect = 1 << macroAspectHW;

  uint32_t tileBytes1x =
      (tileThickness * bitsPerElement * kMicroTileWidth * kMicroTileHeight +
       7) /
      8;

  uint32_t sampleSplitHw = tileMode_getSampleSplit(tileMode);
  uint32_t tileSplitHw = tileMode_getTileSplit(tileMode);
  uint32_t sampleSplit = 1 << sampleSplitHw;
  uint32_t tileSplitC =
      (tileMode_getMicroTileMode(tileMode) == kMicroTileModeDepth)
          ? (64 << tileSplitHw)
          : max(256U, tileBytes1x * sampleSplit);

  uint32_t tileSplitBytes = min(kDramRowSize, tileSplitC);

  uint32_t numPipes = getPipeCount(tileMode_getPipeConfig(tileMode));
  uint32_t pipeInterleaveBits = findLSB(kPipeInterleaveBytes);
  uint32_t pipeInterleaveMask = (1 << pipeInterleaveBits) - 1;
  uint32_t pipeBits = findLSB(numPipes);
  uint32_t bankBits = findLSB(numBanks);
  uint32_t bankSwizzleMask = tileSwizzleMask;
  uint32_t pipeSwizzleMask = 0;
  uint32_t macroTileWidth =
      (kMicroTileWidth * bankWidth * numPipes) * macroTileAspect;
  uint32_t macroTileHeight =
      (kMicroTileHeight * bankHeight * numBanks) / macroTileAspect;

  uint32_t microTileMode = tileMode_getMicroTileMode(tileMode);

  uint64_t elementIndex =
      getElementIndex(pos, bitsPerElement, microTileMode, arrayMode);

  uint32_t xh = pos.x;
  uint32_t yh = pos.y;
  if (arrayMode == kArrayModeTiledThinPrt ||
      arrayMode == kArrayModeTiledThickPrt) {
    xh %= macroTileWidth;
    yh %= macroTileHeight;
  }
  uint64_t pipe = getPipeIndex(xh, yh, tileMode_getPipeConfig(tileMode));
  uint64_t bank =
      getBankIndex(xh, yh, bankWidth, bankHeight, numBanks, numPipes);

  uint32_t tileBytes = (kMicroTileWidth * kMicroTileHeight * tileThickness *
                            bitsPerElement * numFragmentsPerPixel +
                        7) /
                       8;

  uint64_t elementOffset = 0;
  if (microTileMode == kMicroTileModeDepth) {
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
  uint64_t macroTileRowIndex = pos.y / macroTileHeight;
  uint64_t macroTileColumnIndex = pos.x / macroTileWidth;
  uint64_t macroTileIndex =
      (macroTileRowIndex * macroTilesPerRow) + macroTileColumnIndex;
  uint64_t macro_tile_offset = macroTileIndex * macroTileBytes;
  uint64_t macroTilesPerSlice =
      macroTilesPerRow * (paddedHeight / macroTileHeight);
  uint64_t sliceBytes = macroTilesPerSlice * macroTileBytes;

  uint32_t slice = pos.z;
  uint64_t sliceOffset =
      (tileSplitSlice + slicesPerTile * slice / tileThickness) * sliceBytes;
  if (arraySlice != 0) {
    slice = arraySlice;
  }

  uint64_t tileRowIndex = (pos.y / kMicroTileHeight) % bankHeight;
  uint64_t tileColumnIndex = ((pos.x / kMicroTileWidth) / numPipes) % bankWidth;
  uint64_t tileIndex = (tileRowIndex * bankWidth) + tileColumnIndex;
  uint64_t tileOffset = tileIndex * tileBytes;

  uint64_t bankSwizzle = bankSwizzleMask;
  uint64_t pipeSwizzle = pipeSwizzleMask;

  uint64_t pipeSliceRotation = 0;
  switch (arrayMode) {
  case kArrayMode3dTiledThin:
  case kArrayMode3dTiledThick:
  case kArrayMode3dTiledXThick:
    pipeSliceRotation =
        max(1UL, (numPipes / 2UL) - 1UL) * (slice / tileThickness);
    break;
  default:
    break;
  }
  pipeSwizzle += pipeSliceRotation;
  pipeSwizzle &= (numPipes - 1);
  pipe = pipe ^ pipeSwizzle;

  uint64_t sliceRotation = 0;
  switch (arrayMode) {
  case kArrayMode2dTiledThin:
  case kArrayMode2dTiledThick:
  case kArrayMode2dTiledXThick:
    sliceRotation = ((numBanks / 2) - 1) * (slice / tileThickness);
    break;
  case kArrayMode3dTiledThin:
  case kArrayMode3dTiledThick:
  case kArrayMode3dTiledXThick:
    sliceRotation = max(1UL, (numPipes / 2UL) - 1UL) * (slice / tileThickness) / numPipes;
    break;
  default:
    break;
  }
  uint64_t tileSplitSliceRotation = 0;
  switch (arrayMode) {
  case kArrayMode2dTiledThin:
  case kArrayMode3dTiledThin:
  case kArrayMode2dTiledThinPrt:
  case kArrayMode3dTiledThinPrt:
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


layout(push_constant) uniform Config {
    uint64_t srcAddress;
    uint64_t srcEndAddress;
    uint64_t dstAddress;
    uint64_t dstEndAddress;
    uvec2 dataSize;
    uint32_t tileMode;
    uint32_t macroTileMode;
    uint32_t dfmt;
    uint32_t numFragments;
    uint32_t bitsPerElement;
    uint32_t tiledSurfaceSize;
    uint32_t linearSurfaceSize;
} config;
