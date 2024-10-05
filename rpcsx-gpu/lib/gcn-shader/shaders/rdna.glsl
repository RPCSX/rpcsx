#version 460

#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_buffer_reference_uvec2 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_shader_explicit_arithmetic_types : require
#extension GL_EXT_shader_atomic_int64 : require
#extension GL_EXT_shader_atomic_float : require
#extension GL_EXT_shader_image_load_formatted : require
#extension GL_KHR_memory_scope_semantics : require
#extension GL_EXT_shared_memory_block : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_null_initializer : require
#extension GL_EXT_shader_atomic_float2 : require
#extension GL_EXT_nonuniform_qualifier: require
#extension GL_EXT_samplerless_texture_functions : require
#extension GL_EXT_debug_printf : enable

#define FLT_MAX 3.402823466e+38
#define FLT_MIN 1.175494351e-38
#define DBL_MAX 1.7976931348623158e+308
#define DBL_MIN 2.2250738585072014e-308

#define ClampInfToFltMax(x) (isinf(x) ? ((x) < 0 ? -FLT_MAX : FLT_MAX) : (x))
#define ConvertInfToZero(x) (isinf(x) ? 0.0 : (x))
#define Rsqrt(x) (inversesqrt(x))
#define Rcp(x) (1.0 / x)

#define U32ARRAY_FETCH_BITS(ARRAY, START, BITCOUNT)  ((ARRAY[(START) >> 5] >> ((START) & 31)) & ((1 << (BITCOUNT)) - 1))
#define U64ARRAY_FETCH_BITS(ARRAY, START, BITCOUNT)  ((ARRAY[(START) >> 6] >> ((START) & 63)) & ((uint64_t(1) << (BITCOUNT)) - 1))

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

#define SIZEOF(x) sizeof_##x
#define DEFINE_SIZEOF(x, size) const int SIZEOF(x) = size

DEFINE_SIZEOF(int8_t, 1);
DEFINE_SIZEOF(uint8_t, 1);

DEFINE_SIZEOF(int16_t, 2);
DEFINE_SIZEOF(uint16_t, 2);
DEFINE_SIZEOF(float16_t, 2);

DEFINE_SIZEOF(int32_t, 4);
DEFINE_SIZEOF(uint32_t, 4);
DEFINE_SIZEOF(float32_t, 4);
DEFINE_SIZEOF(int64_t, 8);
DEFINE_SIZEOF(uint64_t, 8);
DEFINE_SIZEOF(float64_t, 8);

uint thread_id;
uint64_t exec;

int32_t sext(int32_t x, uint bits) {
    return bits == 32 ? x : (x << (32 - bits)) >> (32 - bits);
}
uint32_t zext(uint32_t x, uint bits) {
    return bits == 32 ? x : (x << (32 - bits)) >> (32 - bits);
}

uint32_t mul24lo(uint32_t a, uint32_t b) { return (a & 0xffffff) * (b & 0xffffff); }
int32_t mul24lo(int32_t a, int32_t b) { return sext(a, 24) * sext(b, 24); }

uint32_t mul24hi(uint32_t a, uint32_t b) {
    uint32_t hi, lo;
    umulExtended((a & 0xffffff), (b & 0xffffff), hi, lo);
    return hi;
}
int32_t mul24hi(int32_t a, int32_t b) {
    int32_t hi, lo;
    imulExtended(sext(a, 24), sext(b, 24), hi, lo);
    return hi;
}

bool exec_test() {
    return (exec & (uint64_t(1) << thread_id)) != 0;
}

uint32_t absdiff(uint32_t x, uint32_t y) {
    return x > y ? x - y : y - x;
}

int32_t get_ieee_exponent(float32_t x) {
    int32_t result;
    frexp(x, result);
    return result;
}

int32_t get_ieee_exponent(float64_t x) {
    int32_t result;
    frexp(x, result);
    return result;
}

uint64_t vbuffer_base(u32vec4 vbuffer) {
    uint64_t baseLo = vbuffer[0];
    uint64_t baseHi = U32ARRAY_FETCH_BITS(vbuffer, 32, 12);
    uint64_t base = baseLo | (baseHi << 32);
    return base;
}
uint32_t vbuffer_stride(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 48, 14);
}
bool vbuffer_swizzle_en(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 63, 1) != 0;
}
uint32_t vbuffer_num_records(u32vec4 vbuffer) {
    return vbuffer[2];
}
uint32_t vbuffer_dst_sel_x(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 96, 3);
}
uint32_t vbuffer_dst_sel_y(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 99, 3);
}
uint32_t vbuffer_dst_sel_z(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 102, 3);
}
uint32_t vbuffer_dst_sel_w(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 105, 3);
}
u32vec4 vbuffer_dst_sel(u32vec4 vbuffer) {
    return u32vec4(vbuffer_dst_sel_x(vbuffer), vbuffer_dst_sel_y(vbuffer), vbuffer_dst_sel_z(vbuffer), vbuffer_dst_sel_w(vbuffer));
}
uint32_t vbuffer_nfmt(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 108, 3);
}
uint32_t vbuffer_dfmt(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 111, 4);
}
uint32_t vbuffer_element_size(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 115, 2);
}
uint32_t vbuffer_index_stride(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 117, 2);
}
bool vbuffer_addtid_en(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 119, 1) != 0;
}
bool vbuffer_hash_en(u32vec4 vbuffer) {
    return U32ARRAY_FETCH_BITS(vbuffer, 121, 1) != 0;
}

const int kPsVGprInputIPerspSample = 0;
const int kPsVGprInputJPerspSample = 1;
const int kPsVGprInputIPerspCenter = 2;
const int kPsVGprInputJPerspCenter = 3;
const int kPsVGprInputIPerspCentroid = 4;
const int kPsVGprInputJPerspCentroid = 5;
const int kPsVGprInputIW = 6;
const int kPsVGprInputJW = 7;
const int kPsVGprInput1W = 8;
const int kPsVGprInputILinearSample = 9;
const int kPsVGprInputJLinearSample = 10;
const int kPsVGprInputILinearCenter = 11;
const int kPsVGprInputJLinearCenter = 12;
const int kPsVGprInputILinearCentroid = 13;
const int kPsVGprInputJLinearCentroid = 14;
const int kPsVGprInputX = 15;
const int kPsVGprInputY = 16;
const int kPsVGprInputZ = 17;
const int kPsVGprInputW = 18;
const int kPsVGprInputFrontFace = 19;
const int kPsVGprInputAncillary = 20;
const int kPsVGprInputSampleCoverage = 21;
const int kPsVGprInputPosFixed = 22;

const int kCompSwapStd = 0;
const int kCompSwapAlt = 1;
const int kCompSwapStdRev = 2;
const int kCompSwapAltRev = 3;

f32vec4 ps_comp_swap(uint32_t mode, f32vec4 value) {
    switch (mode) {
    case kCompSwapStd:
        return value.rgba;
    case kCompSwapAlt:
        return value.bgra;
    case kCompSwapStdRev:
        return value.abgr;
    case kCompSwapAltRev:
        return value.argb;
    }

    return value;
}

float32_t ps_input_vgpr(int32_t index, f32vec4 fragCoord, bool frontFace) {
    switch (index) {
    case kPsVGprInputIPerspSample:
    case kPsVGprInputJPerspSample:
    case kPsVGprInputIPerspCenter:
    case kPsVGprInputJPerspCenter:
    case kPsVGprInputIPerspCentroid:
    case kPsVGprInputJPerspCentroid:
    case kPsVGprInputILinearSample:
    case kPsVGprInputJLinearSample:
    case kPsVGprInputILinearCenter:
    case kPsVGprInputJLinearCenter:
    case kPsVGprInputILinearCentroid:
    case kPsVGprInputJLinearCentroid:
        return intBitsToFloat(index);

    case kPsVGprInputIW:
        return fragCoord.y / fragCoord.w;
    case kPsVGprInputJW:
        return fragCoord.z / fragCoord.w;
    case kPsVGprInput1W:
        return 1.f / fragCoord.w;
    case kPsVGprInputX:
        return fragCoord.x;
    case kPsVGprInputY:
        return fragCoord.y;
    case kPsVGprInputZ:
        return fragCoord.z;
    case kPsVGprInputW:
        return fragCoord.w;
    case kPsVGprInputFrontFace:
        return intBitsToFloat(frontFace ? 1 : 0);
    case kPsVGprInputAncillary:
        return 0;
    case kPsVGprInputSampleCoverage:
        return 0;
    case kPsVGprInputPosFixed:
        return 0;
    }

    // debugPrintfEXT("ps_input_vgpr: invalid index %d", index);
    return 0;
}

uint32_t cs_input_sgpr(int32_t index, u32vec3 localInvocationId) {
    if (index == 0) {
        return localInvocationId.x;
    }

    if (index == 1) {
        return localInvocationId.y;
    }

    if (index == 2) {
        return localInvocationId.z;
    }

    return 0;
}

void cs_set_initial_exec(u32vec3 localInvocationId, u32vec3 workgroupSize) {
    uint32_t totalWorkgroupSize = workgroupSize.x * workgroupSize.y * workgroupSize.z;

    if (totalWorkgroupSize == 64) {
        exec = ~uint64_t(0);
        return;
    }

    if (totalWorkgroupSize < 64) {
        exec = (uint64_t(1) << totalWorkgroupSize) - 1;
        return;
    }

    uint32_t waveCount = totalWorkgroupSize / 64;

    uint32_t totalInvocationIndex = localInvocationId.x + 
        localInvocationId.y * workgroupSize.x + 
        localInvocationId.z * workgroupSize.x * workgroupSize.y;

    uint32_t waveIndex = (totalInvocationIndex + 63) / 64;

    if (waveIndex + 1 < waveCount) {
        exec = ~uint64_t(0);
        return;
    }

    uint32_t lastWaveLen = totalWorkgroupSize % 64;
    exec = lastWaveLen == 0 ? ~uint64_t(0) : ((uint64_t(1) << lastWaveLen) - 1);
}

void cs_set_thread_id(u32vec3 localInvocationId, u32vec3 workgroupSize) {
    uint32_t totalInvocationIndex = localInvocationId.x + 
        localInvocationId.y * workgroupSize.x + 
        localInvocationId.z * workgroupSize.x * workgroupSize.y;

    thread_id = totalInvocationIndex % 64;
}

const uint32_t kPrimTypeQuadList = 0x13;
const uint32_t kPrimTypeQuadStrip = 0x14;

uint32_t vs_get_index(uint32_t mode, uint32_t index) {
    switch (mode) {
    case kPrimTypeQuadList: {
        const uint32_t indicies[] = {0, 1, 2, 2, 3, 0};
        return index / 6 + indicies[index % 6];
    }

    case kPrimTypeQuadStrip: {
        const uint32_t indicies[] = {0, 1, 3, 0, 3, 2};
        return index / 6 + indicies[index % 6];
    }
    }

    return index;
}

// VINTRP
float32_t v_interp_mov_f32(uint32_t param, float32_t attr[3]) {
    return attr[param == 1 ? 1 : (param == 2 ? 2 : 0)];
}

void v_interp_p1_f32(out float32_t dst, float32_t vI, float32_t attr[3]) {
    dst = attr[0] + vI * attr[1];
}

void v_interp_p2_f32(inout float32_t dst, float32_t vJ, float32_t attr[3]) {
    dst += vJ * attr[2];
}

// VOP
uint64_t vcc;
int32_t v_cvt_i32_f64(float64_t x) { return int32_t(x); }
float64_t v_cvt_f64_i32(int32_t x) { return float64_t(x); }
float32_t v_cvt_f32_i32(int32_t x) { return float32_t(x); }
float32_t v_cvt_f32_u32(uint32_t x) { return float32_t(x); }
uint32_t v_cvt_u32_f32(float32_t x) { return uint32_t(x); }
int32_t v_cvt_i32_f32(float32_t x) { return int32_t(x); }
float16_t v_cvt_f16_f32(float32_t x) { return float16_t(x); }
float32_t v_cvt_f32_f16(float16_t x) { return float32_t(x); }
int32_t v_cvt_rpi_i32_f32(float32_t x) { return int32_t(floor(x + 0.5)); }
int32_t v_cvt_flr_i32_f32(float32_t x) { return int32_t(floor(x)); }
float32_t v_cvt_off_f32_i4(int32_t x) { return float32_t(((x & 0xf) << 28) >> 28); }
float32_t v_cvt_f32_f64(float64_t x) { return float32_t(x); }
float64_t v_cvt_f64_f32(float32_t x) { return float64_t(x); }
float32_t v_cvt_f32_ubyte0(uint32_t x) { return float32_t(x & 0xff); }
float32_t v_cvt_f32_ubyte1(uint32_t x) { return float32_t((x >> 8) & 0xff); }
float32_t v_cvt_f32_ubyte2(uint32_t x) { return float32_t((x >> 16) & 0xff); }
float32_t v_cvt_f32_ubyte3(uint32_t x) { return float32_t((x >> 24) & 0xff); }
float32_t v_cvt_u32_f64(float64_t x) { return float32_t(x); }
float64_t v_cvt_f64_u32(uint32_t x) { return float64_t(x); }
float32_t v_fract_f32(float32_t x) { return fract(x); }
float32_t v_trunc_f32(float32_t x) { return trunc(x); }
float32_t v_ceil_f32(float32_t x) { return ceil(x); }
float32_t v_rndne_f32(float32_t x) {
    float32_t xfract = fract(x);
    float32_t xround = floor(x + 0.5);

    if (xfract == 0.5 && floor(x) * 0.5 == floor(xround * 0.5)) {
        xround -= 1.0;
    }
    return xround;
}
float32_t v_floor_f32(float32_t x) { return floor(x); }
float32_t v_exp_f32(float32_t x) { return exp2(x); }
float32_t v_log_clamp_f32(float32_t x) { return ClampInfToFltMax(log2(x)); }
float32_t v_log_f32(float32_t x) { return log2(x); }
float32_t v_rcp_clamp_f32(float32_t x) { return ClampInfToFltMax(Rcp(x)); }
float32_t v_rcp_legacy_f32(float32_t x) { return ConvertInfToZero(Rcp(x)); }
float32_t v_rcp_f32(float32_t x) { return Rcp(x); }
float32_t v_rcp_iflag_f32(float32_t x) { return Rcp(x); }
float32_t v_rsq_clamp_f32(float32_t x) { return ClampInfToFltMax(Rsqrt(x)); }
float32_t v_rsq_legacy_f32(float32_t x) { return ConvertInfToZero(Rsqrt(x)); }
float32_t v_rsq_f32(float32_t x) { return Rsqrt(x); }
float64_t v_rcp_f64(float64_t x) { return Rcp(x); }
float64_t v_rcp_clamp_f64(float64_t x) { return ClampInfToFltMax(Rcp(x)); }
float64_t v_rsq_f64(float64_t x) { return Rsqrt(x); }
float64_t v_rsq_clamp_f64(float64_t x) { return ClampInfToFltMax(Rsqrt(x)); }
float32_t v_sqrt_f32(float32_t x) { return sqrt(x); }
float64_t v_sqrt_f64(float64_t x) { return sqrt(x); }
float32_t v_sin_f32(float32_t x) { return sin(x * 2 * radians(180)); }
float32_t v_cos_f32(float32_t x) { return cos(x * 2 * radians(180)); }
uint32_t v_not_b32(uint32_t x) { return ~x; }
uint32_t v_bfrev_b32(uint32_t x) { return bitfieldReverse(x); }
uint32_t v_ffbh_u32(uint32_t x) { return findMSB(x); }
uint32_t v_ffbl_b32(uint32_t x) { return findLSB(x); }
int32_t v_ffbh_i32(int32_t x) { return findMSB(x); }
int32_t v_frexp_exp_i32_f64(float64_t x) {
    if (x == 0) {
        return 0;
    }

    if (!isnan(x) && !isinf(x)) {
        int32_t exp;
        frexp(x, exp);
        return exp;
    } else {
        return -1;
    }
}
float64_t v_frexp_mant_f64(float64_t x) {
    if (x == 0) {
        return 0;
    }

    if (!isnan(x) && !isinf(x)) {
        int32_t exp;
        return frexp(x, exp);
    } else {
        return -1;
    }
}
float64_t v_fract_f64(float64_t x) { return fract(x); }
int32_t v_frexp_exp_i32_f32(float32_t x) {
    if (x == 0) {
        return 0;
    }

    if (!isnan(x) && !isinf(x)) {
        int32_t exp;
        frexp(x, exp);
        return exp;
    } else {
        return -1;
    }
}
float32_t v_frexp_mant_f32(float32_t x) {
    if (x == 0) {
        return 0;
    }

    if (!isnan(x) && !isinf(x)) {
        int32_t exp;
        return frexp(x, exp);
    } else {
        return -1;
    }
}

uint32_t v_cndmask_b32(uint32_t x, uint32_t y, uint64_t mask) {
    return (mask & (1 << thread_id)) != 0 ? y : x;
}
float32_t v_add_f32(float32_t x, float32_t y) { return x + y; }
float32_t v_sub_f32(float32_t x, float32_t y) { return x - y; }
float32_t v_subrev_f32(float32_t x, float32_t y) { return y - x; }
void v_mac_legacy_f32(inout float32_t dst, float32_t x, float32_t y) {
    if (!(x == 0 || y == 0)) {
        dst = fma(x, y, dst);
    }
}
float32_t v_mul_legacy_f32(float32_t x, float32_t y) {
    return x == 0 || y == 0 ? 0 : x * y;
}
float32_t v_mul_f32(float32_t x, float32_t y) { return x * y; }
int32_t v_mul_i32_i24(int32_t x, int32_t y) { return mul24lo(x, y); }
int32_t v_mul_hi_i32_i24(int32_t x, int32_t y) { return mul24hi(x, y); }
uint32_t v_mul_u32_u24(uint32_t x, uint32_t y) { return mul24lo(x, y); }
uint32_t v_mul_hi_u32_u24(uint32_t x, uint32_t y) { return mul24hi(x, y); }
float32_t v_min_legacy_f32(float32_t x, float32_t y) {
    return min(x, y);
}
float32_t v_max_legacy_f32(float32_t x, float32_t y) {
    if (isnan(x) || isnan(y)) {
        return y;
    }
    return max(x, y);
}
float32_t v_min_f32(float32_t x, float32_t y) {
    return x < y ? x : y;
}
float32_t v_max_f32(float32_t x, float32_t y) {
    return x >= y ? x : y;
}
int32_t v_min_i32(int32_t x, int32_t y) { return min(x, y); }
int32_t v_max_i32(int32_t x, int32_t y) { return max(x, y); }
uint32_t v_min_u32(uint32_t x, uint32_t y) { return min(x, y); }
uint32_t v_max_u32(uint32_t x, uint32_t y) { return max(x, y); }
uint32_t v_lshr_b32(uint32_t x, uint32_t y) { return x >> (y & 0x1f); }
uint32_t v_lshrrev_b32(uint32_t x, uint32_t y) { return y >> (x & 0x1f); }
int32_t v_ashr_i32(int32_t x, uint32_t y) { return x >> (y & 0x1f); }
int32_t v_ashrrev_i32(uint32_t x, int32_t y) { return y >> (x & 0x1f); }
uint32_t v_lshl_b32(uint32_t x, uint32_t y) { return x << (y & 0x1f); }
uint32_t v_lshlrev_b32(uint32_t x, uint32_t y) { return y << (x & 0x1f); }
uint32_t v_and_b32(uint32_t x, uint32_t y) { return x & y; }
uint32_t v_or_b32(uint32_t x, uint32_t y) { return x | y; }
uint32_t v_xor_b32(uint32_t x, uint32_t y) { return x ^ y; }
uint32_t v_bfm_b32(uint32_t x, uint32_t y) { return ((1 << (x & 0x1f)) - 1) << (y & 0x1f); }
void v_mac_f32(inout float32_t dst, float32_t x, float32_t y) { dst = fma(x, y, dst); }
float32_t v_madmk_f32(float32_t x, float32_t y, float32_t k) { return fma(x, k, y); }
float32_t v_madak_f32(float32_t x, float32_t y, float32_t k) { return fma(x, y, k); }
uint32_t v_bcnt_u32_b32(uint32_t x) { return bitCount(x); }
uint32_t v_mbcnt_lo_u32_b32(uint32_t x, uint32_t y) {
    return bitCount(x & uint32_t((uint64_t(1) << thread_id) - 1)) + y;
}
uint32_t v_mbcnt_hi_u32_b32(uint32_t x, uint32_t y) {
    return (thread_id > 32 ? bitCount(x & ((1 << (thread_id - 32)) - 1)) : 0) + y;
}
uint32_t v_add_i32(inout uint64_t sdst, int32_t x, int32_t y) {
    uint64_t result = uint64_t(x) + uint64_t(y);
    
    if (result > 0xffffffff) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }

    return uint32_t(result);
}
uint32_t v_sub_i32(inout uint64_t sdst, int32_t x, int32_t y) {
    uint32_t result = x - y;

    if (y > x) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }

    return result;
}
uint32_t v_subrev_i32(inout uint64_t sdst, int32_t x, int32_t y) {
    uint32_t result = y - x;

    if (x > y) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }

    return result;
}
uint32_t v_addc_u32(inout uint64_t sdst, uint32_t x, uint32_t y, uint64_t z) {
    uint64_t result = uint64_t(x) + y + ((z & (1 << thread_id)) != 0 ? 1 : 0);
    if (result > 0xffffffff) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }
    return uint32_t(result);
}
uint32_t v_subb_u32(inout uint64_t sdst, uint32_t x, uint32_t y, uint64_t z) {
    uint32_t borrow = ((z & (1 << thread_id)) != 0 ? 1 : 0);
    uint64_t result = uint64_t(x) - y - borrow;
    if (uint64_t(y) + borrow > x) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }
    return uint32_t(result);
}
uint32_t v_subbrev_u32(inout uint64_t sdst, uint32_t x, uint32_t y, uint64_t z) {
    uint32_t borrow = ((z & (1 << thread_id)) != 0 ? 1 : 0);
    uint64_t result = uint64_t(y) - x - borrow;
    if (uint64_t(x) + borrow > y) {
        sdst |= exec & (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }
    return uint32_t(result);
}
float32_t v_ldexp_f32(float32_t x, int32_t y) { return ldexp(x, y); }
uint32_t v_cvt_pkaccum_u8_f32(float32_t x, uint32_t y, uint32_t dst) {
    uint32_t bit = 8 * (y & 3);
    return (dst & ~(0xff << bit)) | (uint32_t(clamp(x, 0, 255)) << bit);
}
uint32_t v_cvt_pknorm_i16_f32(float32_t x, float32_t y) { return packSnorm2x16(vec2(x, y)); }
uint32_t v_cvt_pknorm_u16_f32(float32_t x, float32_t y) { return packUnorm2x16(vec2(x, y)); }
uint32_t v_cvt_pkrtz_f16_f32(float32_t x, float32_t y) { return packHalf2x16(vec2(x, y)); }
uint32_t v_cvt_pk_u16_u32(uint32_t x, uint32_t y) { return packUint2x16(u16vec2(min(x, 0xffff), min(y, 0xffff))); }
uint32_t v_cvt_pk_i16_i32(int32_t x, int32_t y) { return packUint2x16(u16vec2(clamp(x, -0x8000, 0x7fff), clamp(y, -0x8000, 0x7fff))); }

void set_cond_thread_bit(inout uint64_t sdst, bool cond) {
    if (cond) {
        sdst |= (uint64_t(1) << thread_id);
    } else {
        sdst &= ~(uint64_t(1) << thread_id);
    }
}

void set_cond_thread_bit_exec(inout uint64_t sdst, bool cond) {
    uint64_t bit = uint64_t(1) << thread_id;
    if (cond) {
        sdst |= bit;
        exec |= bit;
    } else {
        sdst &= ~bit;
        exec &= ~bit;
    }
}

void v_cmp_f_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_lt_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_eq_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_le_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_gt_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_lg_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_ge_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_o_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !isnan(a) && !isnan(b)); }
void v_cmp_u_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, isnan(a) || isnan(b)); }
void v_cmp_nge_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a >= b)); }
void v_cmp_nlg_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a != b)); }
void v_cmp_ngt_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a > b)); }
void v_cmp_nle_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a <= b)); }
void v_cmp_neq_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a == b)); }
void v_cmp_nlt_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, !(a < b)); }
void v_cmp_tru_f32(inout uint64_t sdst, float32_t a, float32_t b) { set_cond_thread_bit(sdst, true); }

void v_cmp_f_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_lt_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_eq_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_le_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_gt_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_lg_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_ge_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_o_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !isnan(a) && !isnan(b)); }
void v_cmp_u_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, isnan(a) || isnan(b)); }
void v_cmp_nge_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a >= b)); }
void v_cmp_nlg_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a != b)); }
void v_cmp_ngt_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a > b)); }
void v_cmp_nle_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a <= b)); }
void v_cmp_neq_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a == b)); }
void v_cmp_nlt_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, !(a < b)); }
void v_cmp_tru_f64(inout uint64_t sdst, float64_t a, float64_t b) { set_cond_thread_bit(sdst, true); }


void v_cmp_eq_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_f_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_ge_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_gt_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_le_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_lt_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_ne_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_t_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit(sdst, true); }

void v_cmpx_eq_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a == b); }
void v_cmpx_f_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, false); }
void v_cmpx_ge_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a >= b); }
void v_cmpx_gt_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a > b); }
void v_cmpx_le_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a <= b); }
void v_cmpx_lt_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a < b); }
void v_cmpx_ne_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, a != b); }
void v_cmpx_t_u32(inout uint64_t sdst, uint32_t a, uint32_t b) { set_cond_thread_bit_exec(sdst, true); }

void v_cmp_eq_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_f_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_ge_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_gt_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_le_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_lt_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_ne_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_t_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit(sdst, true); }

void v_cmpx_eq_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a == b); }
void v_cmpx_f_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, false); }
void v_cmpx_ge_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a >= b); }
void v_cmpx_gt_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a > b); }
void v_cmpx_le_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a <= b); }
void v_cmpx_lt_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a < b); }
void v_cmpx_ne_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, a != b); }
void v_cmpx_t_i32(inout uint64_t sdst, int32_t a, int32_t b) { set_cond_thread_bit_exec(sdst, true); }

void v_cmp_eq_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_f_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_ge_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_gt_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_le_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_lt_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_ne_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_t_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit(sdst, true); }

void v_cmpx_eq_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a == b); }
void v_cmpx_f_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, false); }
void v_cmpx_ge_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a >= b); }
void v_cmpx_gt_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a > b); }
void v_cmpx_le_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a <= b); }
void v_cmpx_lt_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a < b); }
void v_cmpx_ne_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, a != b); }
void v_cmpx_t_u64(inout uint64_t sdst, uint64_t a, uint64_t b) { set_cond_thread_bit_exec(sdst, true); }

void v_cmp_eq_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a == b); }
void v_cmp_f_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, false); }
void v_cmp_ge_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a >= b); }
void v_cmp_gt_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a > b); }
void v_cmp_le_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a <= b); }
void v_cmp_lt_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a < b); }
void v_cmp_ne_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, a != b); }
void v_cmp_t_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit(sdst, true); }

void v_cmpx_eq_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a == b); }
void v_cmpx_f_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, false); }
void v_cmpx_ge_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a >= b); }
void v_cmpx_gt_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a > b); }
void v_cmpx_le_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a <= b); }
void v_cmpx_lt_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a < b); }
void v_cmpx_ne_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, a != b); }
void v_cmpx_t_i64(inout uint64_t sdst, int64_t a, int64_t b) { set_cond_thread_bit_exec(sdst, true); }


#define CMP_CLASS(x, vftypemask) ( \
    /* snan  */ (((vftypemask) & (3 << 0)) != 0 && isnan(x)) || \
    /* qnan  */ (((vftypemask) & (1 << 1)) != 0 && isnan(x)) || \
    /* -inf  */ (((vftypemask) & (1 << 2)) != 0 && isinf(x) && x < 0) || \
    /* -norm */ (((vftypemask) & (1 << 3)) != 0 && !isinf(x) && !isnan(x) && x < 0) || \
    /* -den  */ (((vftypemask) & (1 << 4)) != 0 && (isnan(x))) || \
    /* -0    */ (((vftypemask) & (1 << 5)) != 0 && x == -0.0) || \
    /* +0    */ (((vftypemask) & (1 << 6)) != 0 && x == +0.0) || \
    /* +den  */ (((vftypemask) & (1 << 7)) != 0 && isnan(x)) || \
    /* +norm */ (((vftypemask) & (1 << 8)) != 0 && !isinf(x) && !isnan(x) && x > 0) || \
    /* +inf  */ (((vftypemask) & (1 << 9)) != 0 && isinf(x) && x > 0) \
)

bool v_cmp_class_f32(float32_t x, uint vftypemask) { return CMP_CLASS(x, vftypemask); }
bool v_cmp_class_f64(float64_t x, uint vftypemask) { return CMP_CLASS(x, vftypemask); }

float32_t v_mad_legacy_f32(float32_t a, float32_t b, float32_t c) { return (a == 0 || b == 0) ? c : fma(a, b, c); }
float32_t v_mad_f32(float32_t a, float32_t b, float32_t c) { return fma(a, b, c); }
uint32_t v_mad_i32_i24(int32_t a, int32_t b, int32_t c) { return mul24lo(a, b) + c; }
uint32_t v_mad_u32_u24(uint32_t a, uint32_t b, uint32_t c) { return mul24lo(a, b) + c; }
float32_t v_cubeid_f32(float32_t a, float32_t b, float32_t c) {
    if (abs(c) >= abs(a) && abs(c) >= abs(b)) {
        return c < 0 ? 5 : 4;
    }

    if (abs(b) >= abs(a)) {
        return b < 0 ? 3 : 2;
    }

    return a < 0 ? 1 : 0;
}
float32_t v_cubesc_f32(float32_t a, float32_t b, float32_t c) {
    if (abs(c) >= abs(a) && abs(c) >= abs(b)) {
        return c < 0 ? -a : a;
    }

    if (abs(b) >= abs(a)) {
        return a;
    }

    return a < 0 ? c : -c;
}
float32_t v_cubetc_f32(float32_t a, float32_t b, float32_t c) {
    if (abs(c) >= abs(a) && abs(c) >= abs(b)) {
        return -b;
    }

    if (abs(b) >= abs(a)) {
        return b < 0 ? -c : c;
    }

    return -b;
}
float32_t v_cubema_f32(float32_t a, float32_t b, float32_t c) {
    if (abs(c) >= abs(a) && abs(c) >= abs(b)) {
        return 2 * c;
    }
    
    if (abs(b) >= abs(a)) {
        return 2 * b;
    }

    return  2 * a;
}
uint32_t v_bfe_u32(uint32_t a, uint32_t b, uint32_t c) {
    return (a >> (b & 0x1f)) & ((1 << (c & 0x1f)) - 1);
}
int32_t v_bfe_i32(int32_t a, uint32_t b, uint32_t c) {
    return (a >> (b & 0x1f)) & ((1 << (c & 0x1f)) - 1);
}
uint32_t v_bfi_b32(uint32_t a, uint32_t b, uint32_t c) { return (a & b) | (~a & c); }
float32_t v_fma_f32(float32_t a, float32_t b, float32_t c) { return fma(a, b, c); }
float64_t v_fma_f64(float64_t a, float64_t b, float64_t c) { return fma(a, b, c); }
uint32_t v_lerp_u8(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t result = (((a >> 24) + (b >> 24) + ((c >> 24) & 1)) >> 1) << 24;
    result += ((((a >> 16) & 0xff) + ((b >> 16) & 0xff) + ((c >> 16) & 1)) >> 1) << 16;
    result += ((((a >> 8) & 0xff) + ((b >> 8) & 0xff) + ((c >> 8) & 1)) >> 1) << 8;
    result += ((a & 0xff) + (b & 0xff) + (c & 1)) >> 1;
    return result;
}
uint32_t v_alignbit_b32(uint32_t a, uint32_t b, uint32_t c) { return uint32_t(((uint64_t(a) << 32) | b) >> (c & 0x1f)); }
uint32_t v_alignbyte_b32(uint32_t a, uint32_t b, uint32_t c) { return uint32_t(((uint64_t(a) << 32) | b) >> (8 * (c & 3)));  }
float32_t v_mullit_f32(float32_t a, float32_t b, float32_t c) {
    return (b > -FLT_MAX && c > 0) ? a * b : -FLT_MAX;
}
float32_t v_min3_f32(float32_t a, float32_t b, float32_t c) { return v_min_f32(v_min_f32(a, b), c); }
int32_t v_min3_i32(int32_t a, int32_t b, int32_t c) { return v_min_i32(v_min_i32(a, b), c); }
uint32_t v_min3_u32(uint32_t a, uint32_t b, uint32_t c) { return v_min_u32(v_min_u32(a, b), c); }
float32_t v_max3_f32(float32_t a, float32_t b, float32_t c) { return v_max_f32(v_max_f32(a, b), c);  }
int32_t v_max3_i32(int32_t a, int32_t b, int32_t c) { return v_max_i32(v_max_i32(a, b), c); }
uint32_t v_max3_u32(uint32_t a, uint32_t b, uint32_t c) { return v_max_u32(v_max_u32(a, b), c);  }
float32_t v_med3_f32(float32_t a, float32_t b, float32_t c) {
    if (isnan(a) || isnan(b) || isnan(c)) {
        return v_min3_f32(a, b, c);
    }

    if (v_max3_f32(a, b, c) == a) {
        return v_max_f32(b, c);
    }

    if (v_max3_f32(a, b, c) == b) {
        return v_max_f32(a, c);
    }

    return v_max_f32(a, b);
}
int32_t v_med3_i32(int32_t a, int32_t b, int32_t c) {
    if (v_max3_i32(a, b, c) == a) {
        return v_max_i32(b, c);
    }
    
    if (v_max3_i32(a, b, c) == b) {
        return v_max_i32(a, c);
    }
    return v_max_i32(a, b);
}
uint32_t v_med3_u32(uint32_t a, uint32_t b, uint32_t c) {
    if (v_max3_u32(a, b, c) == a) {
        return v_max_u32(b, c);
    }
    
    if (v_max3_u32(a, b, c) == b) {
        return v_max_u32(a, c);
    }

    return v_max_u32(a, b);
}
uint32_t v_sad_u8(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t result = z;
    result += absdiff(x >> 24, y >> 24); 
    result += absdiff((x >> 16) & 0xff, (y >> 16) & 0xff);
    result += absdiff((x >> 8) & 0xff, (y >> 8) & 0xff);
    result += absdiff(x & 0xff, y & 0xff);
    return result;
}
uint32_t v_sad_hi_u8(uint32_t x, uint32_t y, uint32_t z) { return (v_sad_u8(x, y, 0) << 16) + z; }
uint32_t v_sad_u16(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t result = z;
    result += absdiff(x & 0xffff, y & 0xffff);
    result += absdiff(x >> 16, y >> 16);
    return result;
}
uint32_t v_sad_u32(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t result = z;
    result += absdiff(x, y);
    return result;
}
uint32_t v_cvt_pk_u8_f32(float32_t x, uint32_t y, uint32_t z) {
    uint32_t byte = 8 * (y & 3);
    uint32_t result = z & ~(0xff << byte);
    result |= (uint8_t(x) & 0xff) << byte;
    return result;
}
// uint32_t v_div_fixup_f32(uint32_t x) { return x; }
// uint32_t v_div_fixup_f64(uint32_t x) { return x; }
uint64_t v_lshl_b64(uint64_t x, uint32_t y) {
    return x << (y & 0x3f);
}
uint64_t v_lshr_b64(uint64_t x, uint32_t y) { return x >> (y & 0x3f); }
int64_t v_ashr_i64(int64_t x, uint32_t y) { return x >> (y & 0x3f); }
float64_t v_add_f64(float64_t x, float64_t y) { return x + y; }
float64_t v_mul_f64(float64_t x, float64_t y) { return x * y; }
float64_t v_min_f64(float64_t x, float64_t y) { return x < y ? x : y; }
float64_t v_max_f64(float64_t x, float64_t y) { return x >= y ? x : y; }
float64_t v_ldexp_f64(float64_t x, int32_t y) { return ldexp(x, y); }
uint32_t v_mul_lo_u32(uint32_t x, uint32_t y) { return x * y; }
uint32_t v_mul_hi_u32(uint32_t x, uint32_t y) {
    uint32_t hi, lo;
    umulExtended(x, y, hi, lo);
    return hi;
}
int32_t v_mul_lo_i32(int32_t x, int32_t y) { return x * y; }
int32_t v_mul_hi_i32(int32_t x, int32_t y) {
    int32_t hi, lo;
    imulExtended(x, y, hi, lo);
    return hi;
}
float32_t v_div_scale_f32(inout uint64_t vcc, float32_t x, float32_t y, float32_t z) {
    int32_t e1 = get_ieee_exponent(y);
    int32_t e2 = get_ieee_exponent(z);
    uint64_t thread_mask = uint64_t(1) << thread_id;
    if (abs(e2 - e1) >= 96) {
        vcc |= thread_mask & exec;
    } else {
        vcc &= ~thread_mask;
    }
    int32_t e_scale = 0;

    if (isnan(y) || isinf(y) || e2 - e1 >= 96) {
        e_scale = 64;
    } else if (e1 >= 126) {
        e_scale = -64;
    } else if (e2 <= -103 || e2 - e1 <= -96) {
        e_scale = 64;
    }

    if (vcc != 0 && x != y) {
        e_scale -= sign(e2 - e1) * 64;
    }

    if (y == 0.0 || z == 0.0) {
        return 0.0 / 0.0;
    }

    return ldexp(x, e_scale);
}
float64_t v_div_scale_f64(inout uint64_t vcc, float64_t x, float64_t y, float64_t z) {
    int32_t e1 = get_ieee_exponent(y);
    int32_t e2 = get_ieee_exponent(z);

    uint64_t thread_mask = uint64_t(1) << thread_id;
    if (abs(e2 - e1) >= 768 && (exec & thread_mask) != 0) {
        vcc |= thread_mask & exec;
    } else {
        vcc &= ~thread_mask;
    }

    int32_t e_scale = 0;

    if (isnan(y) || isinf(y) || e2 - e1 >= 768) {
        e_scale = 128;
    } else if (e1 >= 126) {
        e_scale = -128;
    } else if (e2 <= -970 || e2 - e1 <= -768) {
        e_scale = 128;
    }

    if (vcc != 0 && x != y) {
        e_scale -= sign(e2 - e1) * 128;
    }

    if (y == 0.0 || z == 0.0) {
        return 0.0 / 0.0;
    }

    return ldexp(x, e_scale);
}
float32_t v_div_fmas_f32(float32_t x, float32_t y, float32_t z) {
    float32_t result = fma(x, y, z);
    if (vcc != 0) {
        result *= pow(2.0, z >= 2.0 ? 64 : -64);
    }
    return result;
}
float64_t v_div_fmas_f64(float64_t x, float64_t y, float64_t z) {
    float64_t result = fma(x, y, z);
    if (vcc != 0) {
        result *= pow(2.0, z >= 2.0 ? 128 : -128);
    }
    return result;
}
uint32_t v_msad_u8(uint32_t x, uint32_t y, uint32_t z) {
    uint32_t ybyte0 = y & 0xff;
    uint32_t ybyte1 = (y >> 8) & 0xff;
    uint32_t ybyte2 = (y >> 16) & 0xff;
    uint32_t ybyte3 = y >> 24;

    return z
        + (ybyte0 == 0 ? 0 : absdiff(ybyte0, x & 0xff))
        + (ybyte1 == 0 ? 0 : absdiff(ybyte1, (x >> 8) & 0xff))
        + (ybyte2 == 0 ? 0 : absdiff(ybyte2, (x >> 16) & 0xff))
        + (ybyte3 == 0 ? 0 : absdiff(ybyte3, x >> 24));
}
// float64_t v_trig_preop_f64(float64_t x, uint32_t y) {
//     return x;
// }

// void v_mqsad_u32_u8() {}
// void v_mad_u64_u32() {}
// void v_mad_i64_i32() {}

// SOP

bool scc;

void s_cmp_eq_i32(int32_t a, int32_t b) { scc = a == b; }
void s_cmp_ge_i32(int32_t a, int32_t b) { scc = a >= b; }
void s_cmp_gt_i32(int32_t a, int32_t b) { scc = a > b; }
void s_cmp_le_i32(int32_t a, int32_t b) { scc = a <= b; }
void s_cmp_lt_i32(int32_t a, int32_t b) { scc = a < b; }
void s_cmp_lg_i32(int32_t a, int32_t b) { scc = a != b; }

void s_cmp_eq_u32(uint32_t a, uint32_t b) { scc = a == b; }
void s_cmp_ge_u32(uint32_t a, uint32_t b) { scc = a >= b; }
void s_cmp_gt_u32(uint32_t a, uint32_t b) { scc = a > b; }
void s_cmp_le_u32(uint32_t a, uint32_t b) { scc = a <= b; }
void s_cmp_lt_u32(uint32_t a, uint32_t b) { scc = a < b; }
void s_cmp_lg_u32(uint32_t a, uint32_t b) { scc = a != b; }

void s_cmpk_eq_i32(int32_t a, int32_t b) { scc = a == b; }
void s_cmpk_ge_i32(int32_t a, int32_t b) { scc = a >= b; }
void s_cmpk_gt_i32(int32_t a, int32_t b) { scc = a > b; }
void s_cmpk_le_i32(int32_t a, int32_t b) { scc = a <= b; }
void s_cmpk_lt_i32(int32_t a, int32_t b) { scc = a < b; }
void s_cmpk_lg_i32(int32_t a, int32_t b) { scc = a != b; }

void s_cmpk_eq_u32(uint32_t a, uint32_t b) { scc = a == b; }
void s_cmpk_ge_u32(uint32_t a, uint32_t b) { scc = a >= b; }
void s_cmpk_gt_u32(uint32_t a, uint32_t b) { scc = a > b; }
void s_cmpk_le_u32(uint32_t a, uint32_t b) { scc = a <= b; }
void s_cmpk_lt_u32(uint32_t a, uint32_t b) { scc = a < b; }
void s_cmpk_lg_u32(uint32_t a, uint32_t b) { scc = a != b; }

void s_cmovk_i32(out uint32_t sdst, uint32_t value) { 
    if (scc) {
        sdst = value;
    }
}

void s_cmov_b32(out uint32_t sdst, uint32_t value) { 
    if (scc) {
        sdst = value;
    }
}

void s_cmov_b64(out uint64_t sdst, uint64_t value) { 
    if (scc) {
        sdst = value;
    }
}

uint32_t s_not_b32(uint32_t x) {
    uint32_t result = ~x;
    scc = result != 0;
    return result;
}
uint64_t s_not_b64(uint64_t x) {
    uint64_t result = ~x;
    scc = result != 0;
    return result;
}
uint32_t s_wqm_b32(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= ((x >> (i * 4)) & 0xf) != 0 ? (0xf << (i * 4)) : 0;
    }
    scc = result != 0;
    return result;
}
uint64_t s_wqm_b64(uint64_t x) {
    uint64_t result = 0;
    for (int i = 0; i < 16; ++i) {
        result |= ((x >> (i * 4)) & 0xf) != 0 ? (uint64_t(0xf) << (i * 4)) : 0;
    }
    scc = result != 0;
    return result;
}
uint32_t s_brev_b32(uint32_t x) { return bitfieldReverse(x); }
uint64_t s_brev_b64(uint64_t x) { return (uint64_t(bitfieldReverse(uint32_t(x))) << 32) | bitfieldReverse(uint32_t(x >> 32)); }
int32_t s_bcnt0_i32_b32(uint32_t x) {
    int32_t result = int32_t(bitCount(~x));
    scc = result != 0;
    return result;
}
int32_t s_bcnt0_i32_b64(uint64_t x) {
    int32_t result = int32_t(bitCount(~uint32_t(x)) + bitCount(~uint32_t(x >> 32)));
    scc = result != 0;
    return result;
}
int32_t s_bcnt1_i32_b32(uint32_t x) {
    int32_t result = int32_t(bitCount(x));
    scc = result != 0;
    return result;
}
int32_t s_bcnt1_i32_b64(uint64_t x) {
    int32_t result = int32_t(bitCount(uint32_t(x)) + bitCount(uint32_t(x >> 32)));
    scc = result != 0;
    return result;
}
int32_t s_ff0_i32_b32(uint32_t x) { return int32_t(findLSB(~x)); }
int32_t s_ff0_i32_b64(u32vec2 x) {
    int lo = findLSB(~x.x);
    if (lo >= 0) {
        return lo;
    }
    int hi = findLSB(~x.y);
    return hi < 0 ? -1 : 32 + hi;
}
int32_t s_ff1_i32_b32(uint32_t x) { return int32_t(findLSB(x)); }
int32_t s_ff1_i32_b64(u32vec2 x) {
    int lo = findLSB(x.x);
    if (lo >= 0) {
        return lo;
    }
    int hi = findLSB(x.y);
    return hi < 0 ? -1 : 32 + hi;
}
int32_t s_flbit_i32_b32(uint32_t x) { return findMSB(x); }
int32_t s_flbit_i32_b64(u32vec2 x) {
    int hi = findMSB(x.y);
    if (hi >= 0) {
        return 32 + hi;
    }
    int lo = findMSB(x.x);
    return lo < 0 ? -1 : lo;
}
int32_t s_flbit_i32(int32_t x) { return findMSB(x); }
int32_t s_flbit_i32_i64(i32vec2 x) {
    int hi = findMSB(x.y);
    if (hi >= 0) {
        return 32 + hi;
    }
    int lo = findMSB(x.y < 0 ? ~uint32_t(x.x) : uint32_t(x.x));
    return lo < 0 ? -1 : lo;
}
int32_t s_sext_i32_i8(int8_t x) { return int32_t(x); }
int32_t s_sext_i32_i16(int16_t x) { return int32_t(x); }
uint32_t s_bitset0_b32(uint32_t dest, uint32_t x) { return dest & ~(~0 << (x & 0x1f)); }
uint64_t s_bitset0_b64(uint32_t dest, uint64_t x) { return dest & ~(~uint64_t(0) << (x & 0x3f)); }
uint32_t s_bitset1_b32(uint32_t dest, uint32_t x) { return dest | (~0 << (x & 0x1f)); }
uint64_t s_bitset1_b64(uint64_t dest, uint64_t x) { return dest | (~uint64_t(0) << (x & 0x3f)); }

uint64_t s_and_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = result & x;
    scc = result != 0;
    return result;
}
uint64_t s_or_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = result | x;
    scc = result != 0;
    return result;
}
uint64_t s_xor_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = result ^ x;
    scc = result != 0;
    return result;
}
uint64_t s_andn2_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = result & ~x;
    scc = result != 0;
    return result;
}
uint64_t s_orn2_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = result | ~x;
    scc = result != 0;
    return result;
}
uint64_t s_nand_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = ~(result & x);
    scc = result != 0;
    return result;
}
uint64_t s_nor_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = ~(result | x);
    scc = result != 0;
    return result;
}
uint64_t s_xnor_saveexec_b64(uint64_t x) {
    uint64_t result = exec;
    exec = ~(result ^ x);
    scc = result != 0;
    return result;
}

uint32_t s_quadmask_b32(uint32_t x) {
    uint32_t result = 0;
    for (int i = 0; i < 8; ++i) {
        result |= ((x >> (i * 4)) & 0xf) != 0 ? (1 << i) : 0;
    }
    scc = result != 0;
    return result;
}
uint64_t s_quadmask_b64(uint64_t x) {
    uint64_t result = 0;
    for (int i = 0; i < 16; ++i) {
        result |= ((x >> (i * 4)) & 0xf) != 0 ? (1 << i) : 0;
    }
    scc = result != 0;
    return result;
}

uint32_t s_add_u32(uint32_t x, uint32_t y) {
    uint32_t carry;
    uint32_t result = uaddCarry(x, y, carry);
    scc = carry != 0;
    return result;
}
uint32_t s_sub_u32(uint32_t x, uint32_t y) {
    uint32_t carry;
    uint32_t result = usubBorrow(x, y, carry);
    scc = carry != 0;
    return result;
}
int32_t s_add_i32(int32_t x, int32_t y) {
    int32_t result = x + y;
    scc = sign(x) == sign(y) && sign(result) != sign(x);
    return result;
}
int32_t s_sub_i32(int32_t x, int32_t y) {
    int32_t result = x - y;
    scc = sign(x) != sign(y) && sign(result) != sign(x);
    return result;
}
uint32_t s_addc_u32(uint32_t x, uint32_t y) {
    uint32_t carry0;
    uint32_t carry1 = 0;
    uint32_t result = uaddCarry(x, y, carry0);
    if (scc) {
        result = uaddCarry(result, 1, carry1);
    }
    scc = (carry0 | carry1) != 0;
    return result;
}
uint32_t s_subb_u32(uint32_t x, uint32_t y) {
    uint32_t result = x - y - (scc ? 1 : 0);
    scc = y + (scc ? 1 : 0) > x;
    return result;
}
int32_t s_min_i32(int32_t x, int32_t y) {
    int32_t result = x < y ? x : y;
    scc = x < y;
    return result;
}
uint32_t s_min_u32(uint32_t x, uint32_t y) {
    uint32_t result = x < y ? x : y;
    scc = x < y;
    return result;
}
int32_t s_max_i32(int32_t x, int32_t y) {
    int32_t result = x > y ? x : y;
    scc = x > y;
    return result;
}
uint32_t s_max_u32(uint32_t x, uint32_t y) {
    uint32_t result = x > y ? x : y;
    scc = x > y;
    return result;
}
uint32_t s_cselect_b32(uint32_t x, uint32_t y) { return scc ? x : y; }
uint64_t s_cselect_b64(uint64_t x, uint64_t y) { return scc ? x : y; }
uint32_t s_and_b32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
uint64_t s_and_b64(uint64_t x, uint64_t y) { uint64_t result = x & y; scc = result != 0; return result; }
uint32_t s_or_b32(uint32_t x, uint32_t y) { uint32_t result = x | y; scc = result != 0; return result; }
uint64_t s_or_b64(uint64_t x, uint64_t y) { uint64_t result = x | y; scc = result != 0; return result; }
uint32_t s_xor_b32(uint32_t x, uint32_t y) { uint32_t result = x ^ y; scc = result != 0; return result; }
uint64_t s_xor_b64(uint64_t x, uint64_t y) { uint64_t result = x ^ y; scc = result != 0; return result; }
uint32_t s_andn2_b32(uint32_t x, uint32_t y) { uint32_t result = x & ~y; scc = result != 0; return result; }
uint64_t s_andn2_b64(uint64_t x, uint64_t y) { uint64_t result = x & ~y; scc = result != 0; return result; }
uint32_t s_orn2_b32(uint32_t x, uint32_t y) { uint32_t result = x | ~y; scc = result != 0; return result; }
uint64_t s_orn2_b64(uint64_t x, uint64_t y) { uint64_t result = x | ~y; scc = result != 0; return result; }
uint32_t s_nand_b32(uint32_t x, uint32_t y) { uint32_t result = ~(x & y); scc = result != 0; return result; }
uint64_t s_nand_b64(uint64_t x, uint64_t y) { uint64_t result = ~(x & y); scc = result != 0; return result; }
uint32_t s_nor_b32(uint32_t x, uint32_t y) { uint32_t result = ~(x | y); scc = result != 0; return result; }
uint64_t s_nor_b64(uint64_t x, uint64_t y) { uint64_t result = ~(x | y); scc = result != 0; return result; }
uint32_t s_xnor_b32(uint32_t x, uint32_t y) { uint32_t result = ~(x ^ y); scc = result != 0; return result; }
uint64_t s_xnor_b64(uint64_t x, uint64_t y) { uint64_t result = ~(x ^ y); scc = result != 0; return result; }
uint32_t s_lshl_b32(uint32_t x, uint32_t y) { uint32_t result = x << (y & 0x1f); scc = result != 0; return result; }
uint64_t s_lshl_b64(uint64_t x, uint32_t y) { uint64_t result = x << (y & 0x3f); scc = result != 0; return result; }
uint32_t s_lshr_b32(uint32_t x, uint32_t y) { uint32_t result = x >> (y & 0x1f); scc = result != 0; return result; }
uint64_t s_lshr_b64(uint64_t x, uint32_t y) { uint64_t result = x >> (y & 0x3f); scc = result != 0; return result; }
int32_t s_ashr_i32(int32_t x, uint32_t y) { int32_t result = x >> (y & 0x1f); scc = result != 0; return result; }
int64_t s_ashr_i64(int64_t x, uint32_t y) { int64_t result = x >> (y & 0x3f); scc = result != 0; return result; }
uint32_t s_bfm_b32(uint32_t x, uint32_t y) { uint32_t result = ((1 << (x & 0x1f)) - 1) << (y & 0x1f); scc = result != 0; return result; }
uint64_t s_bfm_b64(uint64_t x, uint64_t y) { uint64_t result = ((uint64_t(1) << (x & 0x1f)) - 1) << (y & 0x1f); scc = result != 0; return result; }
int32_t s_mul_i32(int32_t x, int32_t y) { return x * y; }
int32_t s_mulk_i32(int32_t x, int32_t y) { return x * y; }
int32_t s_abs_i32(int32_t x) {
    int32_t result = abs(x);
    scc = result == 0;
    return result;
}
uint32_t s_bfe_u32(uint32_t x, uint32_t y) {
    uint32_t offset = y & 0x1f;
    uint32_t width = (y >> 16) & 0x7f;
    uint32_t result = width >= 32 ? x >> offset : (x >> offset) & ((1 << width) - 1);
    scc = result != 0;
    return result;
}
int32_t s_bfe_i32(int32_t x, int32_t y) {
    uint32_t offset = y & 0x1f;
    uint32_t width = (y >> 16) & 0x7f;
    if (width == 0) {
        scc = false;
        return 0;
    }

    uint32_t result = width >= 32 ? x >> offset : (x >> offset) & ((1 << width) - 1);
    if ((result & (1 << (width - 1))) != 0) {
        result -= 1 << width;
    }
    scc = result != 0;
    return int32_t(result);
}
uint64_t s_bfe_u64(uint64_t x, uint32_t y) {
    uint32_t offset = y & 0x3f;
    uint32_t width = (y >> 16) & 0x7f;
    uint64_t result = width >= 64 ? x >> offset : (x >> offset) & ((uint64_t(1) << width) - 1);
    scc = result != 0;
    return result;
}
int64_t s_bfe_i64(int64_t x, uint32_t y) {
    uint32_t offset = y & 0x1f;
    uint32_t width = (y >> 16) & 0x7f;
    if (width == 0) {
        scc = false;
        return 0;
    }

    uint64_t result = width >= 64 ? x >> offset : (x >> offset) & ((uint64_t(1) << width) - 1);
    if ((result & (uint64_t(1) << (width - 1))) != 0) {
        result -= uint64_t(1) << width;
    }
    scc = result != 0;
    return int64_t(result);
}
int32_t s_absdiff_i32(int32_t x, int32_t y) { int32_t result = abs(x - y); scc = result != 0; return result; }
// uint32_t s_lshl1_add_u32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_lshl2_add_u32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_lshl3_add_u32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_lshl4_add_u32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_pack_ll_b32_b16(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_pack_lh_b32_b16(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_pack_hh_b32_b16(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// uint32_t s_mul_hi_u32(uint32_t x, uint32_t y) { uint32_t result = x & y; scc = result != 0; return result; }
// int32_t s_mul_hi_i32(int32_t x, int32_t y) { int32_t result = x & y; scc = result != 0; return result; }

void s_bitcmp0_b32(uint32_t x, uint32_t y) { scc = ((x >> (y & 0x1f)) & 1) == 0; }
void s_bitcmp1_b32(uint32_t x, uint32_t y) { scc = ((x >> (y & 0x1f)) & 1) == 1; }
void s_bitcmp0_b64(uint64_t x, uint32_t y) { scc = ((x >> (y & 0x3f)) & 1) == 0; }
void s_bitcmp1_b64(uint64_t x, uint32_t y) { scc = ((x >> (y & 0x3f)) & 1) == 1; }


// MUBUF

const int kBufferFormatInvalid = 0x00000000;
const int kBufferFormat8 = 0x00000001;
const int kBufferFormat16 = 0x00000002;
const int kBufferFormat8_8 = 0x00000003;
const int kBufferFormat32 = 0x00000004;
const int kBufferFormat16_16 = 0x00000005;
const int kBufferFormat10_11_11 = 0x00000006;
const int kBufferFormat11_11_10 = 0x00000007;
const int kBufferFormat10_10_10_2 = 0x00000008;
const int kBufferFormat2_10_10_10 = 0x00000009;
const int kBufferFormat8_8_8_8 = 0x0000000a;
const int kBufferFormat32_32 = 0x0000000b;
const int kBufferFormat16_16_16_16 = 0x0000000c;
const int kBufferFormat32_32_32 = 0x0000000d;
const int kBufferFormat32_32_32_32 = 0x0000000e;

const int kBufferChannelTypeUNorm = 0x00000000;
const int kBufferChannelTypeSNorm = 0x00000001;
const int kBufferChannelTypeUScaled = 0x00000002;
const int kBufferChannelTypeSScaled = 0x00000003;
const int kBufferChannelTypeUInt = 0x00000004;
const int kBufferChannelTypeSInt = 0x00000005;
const int kBufferChannelTypeSNormNoZero = 0x00000006;
const int kBufferChannelTypeFloat = 0x00000007;

uint64_t compute_vbuffer_address(uint size, u32vec4 vbuffer, uint64_t soff, uint64_t OFFSET, bool IDXEN, uint64_t vINDEX, uint64_t vOFFSET) {
    bool addTid = vbuffer_addtid_en(vbuffer);
    uint64_t base = vbuffer_base(vbuffer) + soff;
    uint64_t index = uint64_t(vINDEX) + (addTid ? thread_id : 0);
    uint64_t offset = vOFFSET + OFFSET;
    bool index_en = IDXEN || addTid;
    uint64_t stride = vbuffer_stride(vbuffer);
    uint64_t num_records = vbuffer_num_records(vbuffer);
    uint64_t index_stride = vbuffer_index_stride(vbuffer);
    uint64_t element_size = vbuffer_element_size(vbuffer);
    bool swizzle_en = vbuffer_swizzle_en(vbuffer);

    if ((stride == 0 && offset + size > num_records - soff) || (stride != 0 && (index >= num_records || (index_en && offset + size > stride)))) {
        return 0;
    }
    
    if (!swizzle_en) {
        uint64_t address = base + offset + index * stride;
        return address & ~uint64_t(3);
    }

    uint64_t index_msb = index / index_stride;
    uint64_t index_lsb = index % index_stride;
    uint64_t offset_msb = offset / element_size;
    uint64_t offset_lsb = offset % element_size;
    uint64_t address = base + (index_msb * stride + offset_msb * element_size) * index_stride + index_lsb * element_size + offset_lsb;
    return address & ~uint64_t(3);
}

#define DEFINE_BUFFER_REFERENCE(TYPE) \
    layout(buffer_reference) buffer buffer_reference_##TYPE { \
        TYPE data[]; \
    }; \

#ifdef _8BIT_BUFFER_ACCESS
DEFINE_BUFFER_REFERENCE(int8_t)
DEFINE_BUFFER_REFERENCE(uint8_t)
#else
layout(buffer_reference) buffer buffer_reference_uint8_t {
    uint16_t data[];
};
layout(buffer_reference) buffer buffer_reference_int8_t {
    int16_t data[];
};
#endif

DEFINE_BUFFER_REFERENCE(int16_t)
DEFINE_BUFFER_REFERENCE(uint16_t)
DEFINE_BUFFER_REFERENCE(float16_t)
DEFINE_BUFFER_REFERENCE(int32_t)
DEFINE_BUFFER_REFERENCE(uint32_t)
DEFINE_BUFFER_REFERENCE(float32_t)
DEFINE_BUFFER_REFERENCE(int64_t)
DEFINE_BUFFER_REFERENCE(uint64_t)
DEFINE_BUFFER_REFERENCE(float64_t)

#ifdef _8BIT_BUFFER_ACCESS
#define MEMORY_DATA_REF(TYPE, ADDRESS) buffer_reference_##TYPE(ADDRESS).data[0]
#define MEMORY_DATA_REF8(TYPE, ADDRESS) buffer_reference_##TYPE(ADDRESS).data[0]
#else
#define MEMORY_DATA_REF(TYPE, ADDRESS) buffer_reference_##TYPE(ADDRESS).data[0]
#define MEMORY_DATA_REF8(TYPE, ADDRESS) TYPE(buffer_reference_##TYPE((ADDRESS) & ~uint64_t(1)).data[0] >> (8 *((ADDRESS) & uint64_t(1))))
#endif

uint64_t memory_table;

struct MemoryTableSlot {
    uint64_t address;
    uint64_t sizeAndFlags;
    uint64_t deviceAddress;
};

uint64_t getSlotSize(MemoryTableSlot slot) {
    return slot.sizeAndFlags & ((uint64_t(1) << 40) - 1);
}
uint8_t getSlotFlags(MemoryTableSlot slot) {
    return uint8_t(slot.sizeAndFlags >> 40);
}

layout(buffer_reference) buffer MemoryTable {
    uint32_t count;
    uint32_t pad;
    MemoryTableSlot slots[];
};

const uint64_t kInvalidAddress = ~uint64_t(0);

uint64_t findMemoryAddress(uint64_t address, uint64_t size, int32_t hint, out uint64_t areaSize) {
    MemoryTable mt = MemoryTable(memory_table);

    uint32_t pivot;
    uint32_t slotCount = mt.count;
    if (hint < 0 || hint >= slotCount) {
        pivot = slotCount / 2;
    } else {
        pivot = uint32_t(hint);
    }

    uint32_t begin = 0;
    uint32_t end = slotCount;

    while (begin < end) {
        MemoryTableSlot slot = mt.slots[pivot];
        uint64_t slotSize = getSlotSize(slot);
        if (slot.address >= address + size) {
            end = pivot;
        } else if (address >= slot.address + slotSize) {
            begin = pivot + 1;
        } else {
            uint64_t offset = address - slot.address;
            areaSize = slotSize - offset;
            return slot.deviceAddress + offset;
        }

        pivot = begin + ((end - begin) / 2);
    }

    return kInvalidAddress;
}

#define BUFFER_ATOMIC_OP(TYPE, LOCATION_HINT, OP) \
    TYPE prev = 0; \
    if (vbuffer_dfmt(vbuffer) != kBufferFormatInvalid) { \
        uint64_t address = compute_vbuffer_address(SIZEOF(TYPE), vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET); \
        if (address != 0) { \
            uint64_t deviceAreaSize = 0; \
            uint64_t deviceAddress = findMemoryAddress(address, SIZEOF(TYPE), LOCATION_HINT, deviceAreaSize); \
            if (deviceAddress != kInvalidAddress && deviceAreaSize >= SIZEOF(TYPE)) { \
                OP(prev, TYPE, MEMORY_DATA_REF(TYPE, deviceAddress), vdata); \
            } \
            /* FIXME: handle segmentation fault */ \
        } \
    } \
    if (GLC) vdata.x = prev; \


#define ATOMIC_ADD(RESULT, TYPE, MEM, DATA) RESULT = atomicAdd(MEM, DATA)
#define ATOMIC_AND(RESULT, TYPE, MEM, DATA) RESULT = atomicAnd(MEM, DATA)

#define ATOMIC_CMPSWAP(RESULT, TYPE, MEM, DATA) RESULT = atomicCompSwap(MEM, DATA.y, DATA.x)
#define ATOMIC_INC(RESULT, TYPE, MEM, DATA) \
    RESULT = 0; \
    while (true) {\
        TYPE newValue = RESULT >= DATA ? 0 : RESULT + 1; \
        TYPE updatedValue = atomicCompSwap(MEM, RESULT, newValue, gl_ScopeWorkgroup, gl_StorageSemanticsBuffer, gl_SemanticsRelease, gl_StorageSemanticsBuffer, gl_SemanticsAcquire); \
        if (RESULT == updatedValue) break; \
        RESULT = updatedValue; \
    } \

#define ATOMIC_DEC(RESULT, TYPE, MEM, DATA) \
    RESULT = 1; \
    while (true) {\
        TYPE newValue = (RESULT == 0 || RESULT > DATA) ? DATA : RESULT - 1; \
        TYPE updatedValue = atomicCompSwap(MEM, RESULT, newValue, gl_ScopeWorkgroup, gl_StorageSemanticsBuffer, gl_SemanticsRelease, gl_StorageSemanticsBuffer, gl_SemanticsAcquire); \
        if (RESULT == updatedValue) break; \
        RESULT = updatedValue; \
    } \

void buffer_atomic_add(inout uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_ADD);
}
void buffer_atomic_add_x2(inout uint64_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_ADD);
}

void buffer_atomic_and(inout uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_AND);
}
void buffer_atomic_and_x2(inout uint64_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_AND);
}

void buffer_atomic_cmpswap(inout u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_CMPSWAP);
}
void buffer_atomic_cmpswap_x2(inout u64vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_CMPSWAP);
}

void buffer_atomic_dec(inout uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_DEC);
}
void buffer_atomic_dec_x2(inout uint64_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_DEC);
}

void buffer_atomic_fcmpswap(inout u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_CMPSWAP);
}
void buffer_atomic_fcmpswap_x2(inout u64vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_CMPSWAP);
}

void buffer_atomic_inc(inout uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint32_t, memoryLocationHint, ATOMIC_INC);
}
void buffer_atomic_inc_x2(inout uint64_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_ATOMIC_OP(uint64_t, memoryLocationHint, ATOMIC_INC);
}

uint32_t convert_from_nfmt(uint32_t data, uint bits, uint nfmt) {
    data = zext(data, bits);

    switch (nfmt) {
    case kBufferChannelTypeUNorm:
        return floatBitsToUint(float(uint(data)) / ((1 << bits) - 1));

    case kBufferChannelTypeSNorm:
        return floatBitsToUint(float(sext(int(data), bits)) / ((1 << (bits - 1)) - 1));

    case kBufferChannelTypeUScaled:
        return floatBitsToUint(float(data));

    case kBufferChannelTypeSScaled:
        return floatBitsToUint(float(sext(int(data), bits)));

    case kBufferChannelTypeUInt:
        return data;

    case kBufferChannelTypeSInt:
        return uint32_t(sext(int(data), bits));

    case kBufferChannelTypeSNormNoZero:
        return floatBitsToUint((float(sext(int(data), bits) * 2 + 1)) / ((1 << bits) - 1));

    case kBufferChannelTypeFloat:
        return data;
    }

    return 0;
}

uint32_t convert_to_nfmt(uint32_t data, uint bits, uint nfmt) {
    data = zext(data, bits);

    switch (nfmt) {
    case kBufferChannelTypeUNorm:
        return uint32_t(clamp(uintBitsToFloat(data), 0, 1) * ((1 << bits) - 1));

    case kBufferChannelTypeSNorm:
        return uint32_t(clamp(uintBitsToFloat(data), -1, 1) * ((1 << (bits - 1)) - 1));

    case kBufferChannelTypeUScaled:
        return uint32_t(uintBitsToFloat(data));

    case kBufferChannelTypeUInt:
        return data;

    case kBufferChannelTypeSInt:
        return uint32_t(sext(int32_t(data), bits));

    case kBufferChannelTypeSNormNoZero:
        return uint32_t(clamp(uintBitsToFloat(data), -1, 1) * ((1 << bits) - 1) / 2 - 1);

    case kBufferChannelTypeFloat:
        return data;
    }

    return 0;
}

uint32_t convert_from_format_x(uint32_t data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormatInvalid:
        return 0;

    case kBufferFormat8:
        return convert_from_nfmt(data, 8, nfmt);

    case kBufferFormat16:
        return convert_from_nfmt(data, 16, nfmt);
        
    case kBufferFormat32:
    case kBufferFormat32_32:
    case kBufferFormat32_32_32:
    case kBufferFormat32_32_32_32:
        return convert_from_nfmt(data, 32, nfmt);
    }
    return data;
}

u32vec2 convert_from_format_xy(uint32_t data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormat8_8:
        return u32vec2(
            convert_from_nfmt(data >> 0, 8, nfmt),
            convert_from_nfmt(data >> 8, 8, nfmt)
        );
    case kBufferFormat16_16:
    case kBufferFormat16_16_16_16:
        return u32vec2(
            convert_from_nfmt(data >> 0, 16, nfmt),
            convert_from_nfmt(data >> 16, 16, nfmt)
        );
    }
    return u32vec2(0);
}

u32vec3 convert_from_format_xyz(uint32_t data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormat10_11_11:
        return u32vec3(
            convert_from_nfmt(data >> 0, 10, nfmt),
            convert_from_nfmt(data >> 10, 11, nfmt),
            convert_from_nfmt(data >> 21, 11, nfmt)
        );

    case kBufferFormat11_11_10:
        return u32vec3(
            convert_from_nfmt(data >> 0, 11, nfmt),
            convert_from_nfmt(data >> 11, 11, nfmt),
            convert_from_nfmt(data >> 22, 10, nfmt)
        );
    }

    return u32vec3(0);
}

u32vec4 convert_from_format_xyzw(uint32_t data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormat8_8_8_8:
        return u32vec4(
            convert_from_nfmt(data >> 0, 8, nfmt),
            convert_from_nfmt(data >> 8, 8, nfmt),
            convert_from_nfmt(data >> 16, 8, nfmt),
            convert_from_nfmt(data >> 24, 8, nfmt)
        );

    case kBufferFormat2_10_10_10:
        return u32vec4(
            convert_from_nfmt(data >> 0, 2, nfmt),
            convert_from_nfmt(data >> 2, 10, nfmt),
            convert_from_nfmt(data >> 12, 10, nfmt),
            convert_from_nfmt(data >> 22, 10, nfmt)
        );

    case kBufferFormat10_10_10_2:
        return u32vec4(
            convert_from_nfmt(data >> 0, 10, nfmt),
            convert_from_nfmt(data >> 10, 10, nfmt),
            convert_from_nfmt(data >> 20, 10, nfmt),
            convert_from_nfmt(data >> 30, 2, nfmt)
        );
    }

    return u32vec4(0);
}


u32vec4 convert_from_format(uint32_t data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormat8:
    case kBufferFormat16:
    case kBufferFormat32:
    case kBufferFormat32_32:
    case kBufferFormat32_32_32:
    case kBufferFormat32_32_32_32:
        return u32vec4(convert_from_format_x(data, dfmt, nfmt), 0, 0, 0);

    case kBufferFormat8_8:
    case kBufferFormat16_16:
    case kBufferFormat16_16_16_16:
        return u32vec4(convert_from_format_xy(data, dfmt, nfmt), u32vec2(0));

    case kBufferFormat10_11_11:
    case kBufferFormat11_11_10:
        return u32vec4(convert_from_format_xyz(data, dfmt, nfmt), 0);

    case kBufferFormat10_10_10_2:
    case kBufferFormat2_10_10_10:
    case kBufferFormat8_8_8_8:
        return convert_from_format_xyzw(data, dfmt, nfmt);
    }

    return u32vec4(0);
}

uint32_t convert_to_format(uint element, u32vec4 data, uint dfmt, uint nfmt) {
    switch (dfmt) {
    case kBufferFormat8:
        if (element == 0) {
            return convert_to_nfmt(data[0], 8, nfmt);
        }
        return 0;

    case kBufferFormat16:
        if (element == 0) {
            return convert_to_nfmt(data[0], 16, nfmt);
        }
        return 0;

    case kBufferFormat16_16:
        if (element == 0) {
            return 
                (convert_to_nfmt(data[0], 16, nfmt) << 0) |
                (convert_to_nfmt(data[1], 16, nfmt) << 8);
        }
        return 0;

    case kBufferFormat16_16_16_16:
        if (element == 0) {
            return 
                (convert_to_nfmt(data[0], 16, nfmt) << 0) |
                (convert_to_nfmt(data[1], 16, nfmt) << 8);
        } else if (element == 1) {
            return 
                (convert_to_nfmt(data[2], 16, nfmt) << 0) |
                (convert_to_nfmt(data[3], 16, nfmt) << 8);
        }
        return 0;

    case kBufferFormat32:
        if (element == 0) {
            return convert_to_nfmt(data[0], 32, nfmt);
        }

        return 0;

    case kBufferFormat32_32:
        switch (element) {
        case 0: return convert_to_nfmt(data[0], 32, nfmt);
        case 1: return convert_to_nfmt(data[1], 32, nfmt);
        case 2: return convert_to_nfmt(data[2], 32, nfmt);
        case 3: return convert_to_nfmt(data[3], 32, nfmt);
        }

        return 0;
    case kBufferFormat32_32_32:
        switch (element) {
        case 0: return convert_to_nfmt(data[0], 32, nfmt);
        case 1: return convert_to_nfmt(data[1], 32, nfmt);
        case 2: return convert_to_nfmt(data[2], 32, nfmt);
        case 3: return convert_to_nfmt(data[3], 32, nfmt);
        }

        return 0;

    case kBufferFormat32_32_32_32:
        switch (element) {
        case 0: return convert_to_nfmt(data[0], 32, nfmt);
        case 1: return convert_to_nfmt(data[1], 32, nfmt);
        case 2: return convert_to_nfmt(data[2], 32, nfmt);
        case 3: return convert_to_nfmt(data[3], 32, nfmt);
        }

        return 0;

    case kBufferFormat10_11_11:
        return uint32_t(
            (convert_to_nfmt(data[0], 10, nfmt) << 0) |
            (convert_to_nfmt(data[1], 11, nfmt) << 10) |
            (convert_to_nfmt(data[2], 11, nfmt) << 21)
        );

    case kBufferFormat11_11_10:
        return uint32_t(
            (convert_to_nfmt(data[0], 11, nfmt) << 0) |
            (convert_to_nfmt(data[1], 11, nfmt) << 11) |
            (convert_to_nfmt(data[2], 10, nfmt) << 22)
        );

    case kBufferFormat8_8_8_8:
        if (element == 0) {
            return uint32_t(
                (convert_to_nfmt(data[0], 8, nfmt) << 0) |
                (convert_to_nfmt(data[1], 8, nfmt) << 8) |
                (convert_to_nfmt(data[2], 8, nfmt) << 16) |
                (convert_to_nfmt(data[3], 8, nfmt) << 24)
            );
        }
        return 0;

    case kBufferFormat2_10_10_10:
        return uint32_t(
            (convert_to_nfmt(data[0], 2, nfmt) << 0) |
            (convert_to_nfmt(data[1], 10, nfmt) << 2) |
            (convert_to_nfmt(data[2], 10, nfmt) << 12) |
            (convert_to_nfmt(data[3], 10, nfmt) << 22)
        );

    case kBufferFormat10_10_10_2:
        return uint32_t(
            (convert_to_nfmt(data[0], 10, nfmt) << 0) |
            (convert_to_nfmt(data[1], 10, nfmt) << 10) |
            (convert_to_nfmt(data[2], 10, nfmt) << 20) |
            (convert_to_nfmt(data[3], 2, nfmt) << 30)
        );
    }

    return uint32_t(0);
}

uint size_of_format(uint dfmt) {
    switch (dfmt) {
    case kBufferFormat8: return 1;
    case kBufferFormat8_8: return 2;
    case kBufferFormat8_8_8_8: return 4;
    case kBufferFormat16: return 2;
    case kBufferFormat16_16: return 4;
    case kBufferFormat16_16_16_16: return 8;
    case kBufferFormat32: return 4;
    case kBufferFormat32_32: return 8;
    case kBufferFormat32_32_32: return 12;
    case kBufferFormat32_32_32_32: return 16;
    case kBufferFormat10_11_11: return 4;
    case kBufferFormat11_11_10: return 4;
    case kBufferFormat10_10_10_2: return 4;
    case kBufferFormat2_10_10_10: return 4;
    }
    return 0;
}

uint components_of_format(uint dfmt) {
    switch (dfmt) {
    case kBufferFormat8: return 1;
    case kBufferFormat8_8: return 2;
    case kBufferFormat8_8_8_8: return 4;
    case kBufferFormat16: return 1;
    case kBufferFormat16_16: return 2;
    case kBufferFormat16_16_16_16: return 4;
    case kBufferFormat32: return 1;
    case kBufferFormat32_32: return 2;
    case kBufferFormat32_32_32: return 3;
    case kBufferFormat32_32_32_32: return 4;
    case kBufferFormat10_11_11: return 3;
    case kBufferFormat11_11_10: return 3;
    case kBufferFormat10_10_10_2: return 4;
    case kBufferFormat2_10_10_10: return 4;
    }
    return 0;
}

u32vec4 buffer_load_format(uint dfmt, uint nfmt, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    uint data_size = size_of_format(dfmt);
    uint channel_count = components_of_format(dfmt);
    uint channel_size = data_size / channel_count;
    uint elements_count = (data_size + SIZEOF(uint32_t) - 1) / SIZEOF(uint32_t);
    uint channels_per_element;
 
    if (data_size > SIZEOF(uint32_t)) {
        channels_per_element = SIZEOF(uint32_t) / channel_size;
    } else {
        channels_per_element = channel_count;
    }

    uint64_t address = compute_vbuffer_address(data_size, vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET);

    if (address == 0 || dfmt == kBufferFormatInvalid) {
        return u32vec4(0);
    }

    uint64_t deviceAreaSize = 0;
    uint64_t deviceAddress = findMemoryAddress(address, data_size, memoryLocationHint, deviceAreaSize);

    if (deviceAddress == kInvalidAddress || deviceAreaSize < data_size) {
        return u32vec4(0);
    }

    uint32_t result[4] = {};
    int outIndex = 0;
    for (int element = 0; element < elements_count; element++) {
        uint32_t data = MEMORY_DATA_REF(uint32_t, deviceAddress);
        u32vec4 unpacked = convert_from_format(data, dfmt, nfmt);
        deviceAddress += SIZEOF(uint32_t);
        for (int channel = 0; channel < channels_per_element; channel++) {
            result[outIndex++] = unpacked[channel];
        }
    }

    return u32vec4(result[0], result[1], result[2], result[3]);
}

void buffer_store_format(u32vec4 data, uint dfmt, uint nfmt, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    uint data_size = size_of_format(dfmt);
    uint elements_count = (data_size + SIZEOF(uint32_t) - 1) / SIZEOF(uint32_t);

    uint64_t address = compute_vbuffer_address(data_size, vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET);

    if (address == 0 || dfmt == kBufferFormatInvalid) {
        return;
    }

    uint64_t deviceAreaSize = 0;
    uint64_t deviceAddress = findMemoryAddress(address, data_size, memoryLocationHint, deviceAreaSize);

    if (deviceAddress == kInvalidAddress || deviceAreaSize < data_size) {
        return;
    }

    for (uint element = 0; element < elements_count; element++) {
        uint32_t value = convert_to_format(element, data, dfmt, nfmt);
        MEMORY_DATA_REF(uint32_t, deviceAddress) = value;
        deviceAddress += SIZEOF(uint32_t);
    }
}

uint32_t buffer_load_format_x(uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    return buffer_load_format(vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE).x;
}
u32vec2 buffer_load_format_xy(uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    return buffer_load_format(vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE).xy;
}
u32vec3 buffer_load_format_xyz(uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    return buffer_load_format(vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE).xyz;
}
u32vec4 buffer_load_format_xyzw(uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    return buffer_load_format(vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE);
}
void buffer_store_format_x(uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    buffer_store_format(i32vec4(vdata, 0, 0, 0), vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE);
}
void buffer_store_format_xy(u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    buffer_store_format(i32vec4(vdata, i32vec2(0)), vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE);
}
void buffer_store_format_xyz(u32vec3 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    buffer_store_format(i32vec4(vdata, 0), vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE);
}
void buffer_store_format_xyzw(u32vec4 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    buffer_store_format(vdata, vbuffer_dfmt(vbuffer), vbuffer_nfmt(vbuffer), vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, LDS, SLC, TFE);
}

#define BUFFER_LOAD_IMPL(TYPE, DATA_REF) \
    uint64_t address = compute_vbuffer_address(1, vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET); \
    if (address == 0) { \
        return; \
    } \
    uint64_t deviceAreaSize = 0; \
    uint64_t deviceAddress = findMemoryAddress(address, SIZEOF(TYPE), memoryLocationHint, deviceAreaSize); \
    if (deviceAddress == kInvalidAddress || deviceAreaSize < SIZEOF(TYPE)) { \
        return; \
    } \
    TYPE result = DATA_REF(TYPE, deviceAddress) \

#define BUFFER_LOAD_DWORD_N_IMPL(N) \
    uint64_t address = compute_vbuffer_address(1, vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET); \
    if (address == 0) { \
        return; \
    } \
    uint64_t deviceAreaSize = 0; \
    uint64_t deviceAddress = findMemoryAddress(address, SIZEOF(uint32_t) * N, memoryLocationHint, deviceAreaSize); \
    if (deviceAddress == kInvalidAddress || deviceAreaSize < SIZEOF(uint32_t) * N) { \
        return; \
    } \
    for (int i = 0; i < (N); ++i) { \
        vdata[i] = MEMORY_DATA_REF(uint32_t, deviceAddress); \
        deviceAddress += SIZEOF(uint32_t); \
    } \

void buffer_load_ubyte(out uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_IMPL(uint8_t, MEMORY_DATA_REF8);

    // FIXME: support LDS
    vdata = uint32_t(result);
}
void buffer_load_sbyte(out int32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_IMPL(int8_t, MEMORY_DATA_REF8);

    // FIXME: support LDS
    vdata = int32_t(result);
}
void buffer_load_ushort(out uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_IMPL(uint16_t, MEMORY_DATA_REF);

    // FIXME: support LDS
    vdata = uint32_t(result);
}
void buffer_load_sshort(out int32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_IMPL(int16_t, MEMORY_DATA_REF);

    // FIXME: support LDS
    vdata = int32_t(result);
}
void buffer_load_dword(out uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_IMPL(uint32_t, MEMORY_DATA_REF);

    // FIXME: support LDS
    vdata = result;
}
void buffer_load_dwordx2(out u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_DWORD_N_IMPL(2);
}
void buffer_load_dwordx4(out u32vec4 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_DWORD_N_IMPL(4);
}
void buffer_load_dwordx3(out u32vec3 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_LOAD_DWORD_N_IMPL(3);
}

#define BUFFER_STORE_IMPL(TYPE, N, DATA) \
    uint64_t address = compute_vbuffer_address(1, vbuffer, soff, OFFSET, IDXEN, vINDEX, vOFFSET); \
    if (address == 0) { \
        return; \
    } \
    uint64_t deviceAreaSize = 0; \
    uint64_t deviceAddress = findMemoryAddress(address, SIZEOF(TYPE) * N, memoryLocationHint, deviceAreaSize); \
    if (deviceAddress == kInvalidAddress || deviceAreaSize < SIZEOF(TYPE) * N) { \
        return; \
    } \
    for (int i = 0; i < (N); ++i) { \
        MEMORY_DATA_REF(TYPE, deviceAddress) = (DATA)[i]; \
        deviceAddress += SIZEOF(TYPE); \
    } \

void buffer_store_byte(uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint8_t, 1, uint8_t[1](uint8_t(vdata)));
}
void buffer_store_short(uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint16_t, 1, uint16_t[1](uint16_t(vdata)));
}
void buffer_store_dword(uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint32_t, 1, uint32_t[1](vdata));
}
void buffer_store_dwordx2(u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint32_t, 2, vdata);
}
void buffer_store_dwordx4(u32vec4 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint32_t, 4, vdata);
}
void buffer_store_dwordx3(u32vec3 vdata, uint32_t vOFFSET, uint32_t vINDEX, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool LDS, bool SLC, bool TFE) {
    BUFFER_STORE_IMPL(uint32_t, 3, vdata);
}


uint32_t tbuffer_load_format_x(uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    return buffer_load_format(dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE).x;
}
u32vec2 tbuffer_load_format_xy(uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    return buffer_load_format(dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE).xy;
}
u32vec3 tbuffer_load_format_xyz(uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    return buffer_load_format(dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE).xyz;
}
u32vec4 tbuffer_load_format_xyzw(uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    return buffer_load_format(dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE);
}
void tbuffer_store_format_x(uint32_t vdata, uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    buffer_store_format(u32vec4(vdata, 0, 0, 0), dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE);
}
void tbuffer_store_format_xy(u32vec2 vdata, uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    buffer_store_format(u32vec4(vdata, i32vec2(0)), dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE);
}
void tbuffer_store_format_xyz(u32vec3 vdata, uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    buffer_store_format(u32vec4(vdata, 0), dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE);
}
void tbuffer_store_format_xyzw(u32vec4 vdata, uint32_t vOFFSET, uint32_t vINDEX, uint dfmt, uint nfmt, int32_t memoryLocationHint, u32vec4 vbuffer, uint32_t soff, uint32_t OFFSET, bool IDXEN, bool GLC, bool SLC, bool TFE) {
    buffer_store_format(vdata, dfmt, nfmt, vOFFSET, vINDEX, memoryLocationHint, vbuffer, soff, OFFSET, IDXEN, GLC, false, SLC, TFE);
}

#define S_LOAD_DWORD(dest, memoryLocationHint, sbase, offset, N) \
    int32_t _offset = 0; \
    uint64_t deviceAreaSize = 0; \
    uint64_t deviceAddress = findMemoryAddress((sbase & ~uint64_t(3)) + (offset & ~3), SIZEOF(uint32_t) * N, memoryLocationHint, deviceAreaSize); \
    if (deviceAddress == kInvalidAddress || deviceAreaSize < SIZEOF(uint32_t) * N) { \
        for (int i = 0; i < (N); ++i) { \
            dest[i] = 0; \
        } \
    } else { \
        for (int i = 0; i < (N); ++i) { \
            dest[i] = MEMORY_DATA_REF(uint32_t, deviceAddress + _offset); \
            _offset += SIZEOF(uint32_t); \
        } \
    }\

uint32_t s_load_dword(int32_t memoryLocationHint, uint64_t sbase, int32_t offset) {
    uint32_t sdst[1];
    S_LOAD_DWORD(sdst, memoryLocationHint, sbase, offset, 1);
    return sdst[0];
}

uint32_t[2] s_load_dwordx2(int32_t memoryLocationHint, uint64_t sbase, int32_t offset) {
    uint32_t sdst[2];
    S_LOAD_DWORD(sdst, memoryLocationHint, sbase, offset, 2);
    return sdst;
}
uint32_t[4] s_load_dwordx4(int32_t memoryLocationHint, uint64_t sbase, int32_t offset) {
    uint32_t sdst[4];
    S_LOAD_DWORD(sdst, memoryLocationHint, sbase, offset, 4);
    return sdst;
}
uint32_t[8] s_load_dwordx8(int32_t memoryLocationHint, uint64_t sbase, int32_t offset) {
    uint32_t sdst[8];
    S_LOAD_DWORD(sdst, memoryLocationHint, sbase, offset, 8);
    return sdst;
}
uint32_t[16] s_load_dwordx16(int32_t memoryLocationHint, uint64_t sbase, int32_t offset) {
    uint32_t sdst[16];
    S_LOAD_DWORD(sdst, memoryLocationHint, sbase, offset, 16);
    return sdst;
}

#define S_BUFFER_LOAD_DWORD(dest, memoryLocationHint, vbuffer, offset, N) \
    uint64_t base_address = vbuffer_base(vbuffer) & ~0x3; \
    uint64_t stride = vbuffer_stride(vbuffer); \
    uint64_t num_records = vbuffer_num_records(vbuffer); \
    uint64_t size = (stride == 0 ? 1 : stride) * num_records; \
    uint64_t deviceAreaSize = 0; \
    uint64_t deviceAddress = findMemoryAddress(base_address + offset, size, memoryLocationHint, deviceAreaSize); \
    int32_t _offset = 0; \
    for (int i = 0; i < N; i++) { \
        if (deviceAddress == kInvalidAddress || _offset + SIZEOF(uint32_t) > deviceAreaSize) { \
            sdst[i] = 0; \
        } else { \
            sdst[i] = MEMORY_DATA_REF(uint32_t, deviceAddress + _offset); \
        } \
        _offset += SIZEOF(uint32_t); \
    } \

uint32_t s_buffer_load_dword(int32_t memoryLocationHint, u32vec4 vbuffer, int32_t offset) {
    uint32_t sdst[1];
    S_BUFFER_LOAD_DWORD(sdst, memoryLocationHint, vbuffer, offset, 1);
    return sdst[0];
}
u32vec2 s_buffer_load_dwordx2(int32_t memoryLocationHint, u32vec4 vbuffer, int32_t offset) {
    u32vec2 sdst;
    S_BUFFER_LOAD_DWORD(sdst, memoryLocationHint, vbuffer, offset, 2);
    return sdst;
}
u32vec4 s_buffer_load_dwordx4(int32_t memoryLocationHint, u32vec4 vbuffer, int32_t offset) {
    u32vec4 sdst;
    S_BUFFER_LOAD_DWORD(sdst, memoryLocationHint, vbuffer, offset, 4);
    return sdst;
}
uint32_t[8] s_buffer_load_dwordx8(int32_t memoryLocationHint, u32vec4 vbuffer, int32_t offset) {
    uint32_t sdst[8];
    S_BUFFER_LOAD_DWORD(sdst, memoryLocationHint, vbuffer, offset, 8);
    return sdst;
}
uint32_t[16] s_buffer_load_dwordx16(int32_t memoryLocationHint, u32vec4 vbuffer, int32_t offset) {
    uint32_t sdst[16];
    S_BUFFER_LOAD_DWORD(sdst, memoryLocationHint, vbuffer, offset, 16);
    return sdst;
}

uint64_t s_memtime() {
    // TODO
    return 0;
}
void s_dcache_inv() {
    // TODO
}

bool s_cbranch_scc0() { return scc == false; }
bool s_cbranch_scc1() { return scc == true; }
bool s_cbranch_vccz() { return (vcc & (uint64_t(1) << thread_id)) == 0; }
bool s_cbranch_vccnz() { return (vcc & (uint64_t(1) << thread_id)) != 0; }
bool s_cbranch_execz() { return (exec & (uint64_t(1) << thread_id)) == 0; }
bool s_cbranch_execnz() { return (exec & (uint64_t(1) << thread_id)) != 0; }


// DS
// void ds_add_u32() {
//     // vbindex, vsrc [OFFSET:<0..65535>] [GDS:< 0|1>]
// }
// void ds_sub_u32() {}
// void ds_rsub_u32() {}
// void ds_inc_u32() {}
// void ds_dec_u32() {}
// void ds_min_i32() {}
// void ds_max_i32() {}
// void ds_min_u32() {}
// void ds_max_u32() {}
// void ds_and_b32() {}
// void ds_or_b32() {}
// void ds_xor_b32() {}
// void ds_mskor_b32() {}
// void ds_write_b32() {}
// void ds_write2_b32() {}
// void ds_write2st64_b32() {}
// void ds_cmpst_b32() {}
// void ds_cmpst_f32() {}
// void ds_min_f32() {}
// void ds_max_f32() {}
void ds_nop(bool GDS) {}
// void ds_gws_sema_release_all() {}
// void ds_gws_init() {}
// void ds_gws_sema_v() {}
// void ds_gws_sema_br() {}
// void ds_gws_sema_p() {}
// void ds_gws_barrier() {}
// void ds_write_b8() {}
// void ds_write_b16() {}
// void ds_add_rtn_u32() {}
// void ds_sub_rtn_u32() {}
// void ds_rsub_rtn_u32() {}
// void ds_inc_rtn_u32() {}
// void ds_dec_rtn_u32() {}
// void ds_min_rtn_i32() {}
// void ds_max_rtn_i32() {}
// void ds_min_rtn_u32() {}
// void ds_max_rtn_u32() {}
// void ds_and_rtn_b32() {}
// void ds_or_rtn_b32() {}
// void ds_xor_rtn_b32() {}
// void ds_mskor_rtn_b32() {}
// void ds_wrxchg_rtn_b32() {}
// void ds_wrxchg2_rtn_b32() {}
// void ds_wrxchg2st64_rtn_b32() {}
// void ds_cmpst_rtn_b32() {}
// void ds_cmpst_rtn_f32() {}
// void ds_min_rtn_f32() {}
// void ds_max_rtn_f32() {}
// void ds_wrap_rtn_b32() {}
// void ds_swizzle_b32() {
//     // uses lane, not DS
// }
// void ds_read_b32() {
//     ds_base = (GDS) ? M0[31:16] : LDS_BASE
//     ds_size = (GDS) ? M0[15:0] : min(M0[16:0], LDS_SIZE)
//     valid = (GDS) ? gdsPartitionRangeCheck(ds_base, ds_size) : true
//     alignment = ~(OpDataSize-1)
//     region_addr = (OFFSET + vbindex) & alignment
//     valid = valid && (0 <= region_addr <= ds_size - OpDataSize)

//     if (OpDataSize == 8)
//         vdst.du = valid ? DS[ds_base + region_addr].du : 0
//     else if (OpDataSize == 4)
//         vdst.u = valid ? DS[ds_base + region_addr].u : 0
//     else if (OpDataSize == 2)
//         data = valid ? DS[ds_base + region_addr].h : 0
//         vdst.u = OpDataSigned ? sign_ext16(data) : zero_ext16(data)
//     else if (OpDataSize == 1)
//         data = valid ? DS[ds_base + region_addr].b : 0
//         vdst.u = OpDataSigned ? sign_ext8(data) : zero_ext8(data)
// }
// void ds_read2_b32() {}
// void ds_read2st64_b32() {}
// void ds_read_i8() {}
// void ds_read_u8() {}
// void ds_read_i16() {}
// void ds_read_u16() {}
// void ds_consume() {}
// void ds_append() {}
// void ds_ordered_count() {}
// void ds_add_u64() {}
// void ds_sub_u64() {}
// void ds_rsub_u64() {}
// void ds_inc_u64() {}
// void ds_dec_u64() {}
// void ds_min_i64() {}
// void ds_max_i64() {}
// void ds_min_u64() {}
// void ds_max_u64() {}
// void ds_and_b64() {}
// void ds_or_b64() {}
// void ds_xor_b64() {}
// void ds_mskor_b64() {}
// void ds_write_b64() {}
// void ds_write2_b64() {}
// void ds_write2st64_b64() {}
// void ds_cmpst_b64() {}
// void ds_cmpst_f64() {}
// void ds_min_f64() {}
// void ds_max_f64() {}
// void ds_add_rtn_u64() {}
// void ds_sub_rtn_u64() {}
// void ds_rsub_rtn_u64() {}
// void ds_inc_rtn_u64() {}
// void ds_dec_rtn_u64() {}
// void ds_min_rtn_i64() {}
// void ds_max_rtn_i64() {}
// void ds_min_rtn_u64() {}
// void ds_max_rtn_u64() {}
// void ds_and_rtn_b64() {}
// void ds_or_rtn_b64() {}
// void ds_xor_rtn_b64() {}
// void ds_mskor_rtn_b64() {}
// void ds_wrxchg_rtn_b64() {}
// void ds_wrxchg2_rtn_b64() {}
// void ds_wrxchg2st64_rtn_b64() {}
// void ds_cmpst_rtn_b64() {}
// void ds_cmpst_rtn_f64() {}
// void ds_min_rtn_f64() {}
// void ds_max_rtn_f64() {}
// void ds_read_b64() {}
// void ds_read2_b64() {}
// void ds_read2st64_b64() {}
// void ds_condxchg32_rtn_b64() {}
// void ds_add_src2_u32() {}
// void ds_sub_src2_u32() {}
// void ds_rsub_src2_u32() {}
// void ds_inc_src2_u32() {}
// void ds_dec_src2_u32() {}
// void ds_min_src2_i32() {}
// void ds_max_src2_i32() {}
// void ds_min_src2_u32() {}
// void ds_max_src2_u32() {}
// void ds_and_src2_b32() {}
// void ds_or_src2_b32() {}
// void ds_xor_src2_b32() {}
// void ds_write_src2_b32() {}
// void ds_min_src2_f32() {}
// void ds_max_src2_f32() {}
// void ds_add_src2_u64() {}
// void ds_sub_src2_u64() {}
// void ds_rsub_src2_u64() {}
// void ds_inc_src2_u64() {}
// void ds_dec_src2_u64() {}
// void ds_min_src2_i64() {}
// void ds_max_src2_i64() {}
// void ds_min_src2_u64() {}
// void ds_max_src2_u64() {}
// void ds_and_src2_b64() {}
// void ds_or_src2_b64() {}
// void ds_xor_src2_b64() {}
// void ds_write_src2_b64() {}
// void ds_min_src2_f64() {}
// void ds_max_src2_f64() {}


// void ds_write_b96() {}
// void ds_write_b128() {}
// void ds_condxchg32_rtn_b128() {}
// void ds_read_b96() {}
// void ds_read_b128() {}

layout(binding = 1) uniform sampler samplers[];
layout(binding = 2) uniform texture1D textures1D[];
layout(binding = 3) uniform texture2D textures2D[];
layout(binding = 4) uniform texture3D textures3D[];
layout(binding = 5) uniform textureBuffer textureBuffers[];

// void image_atomic_add() {
//     // imageAtomicAdd
// }
// void image_atomic_and() {
//     // imageAtomicAnd
// }
// void image_atomic_cmpswap() {
//     // imageAtomicCompSwap
// }
// void image_atomic_dec() {}
// void image_atomic_fcmpswap() {
//     // imageAtomicCompSwap
// }
// void image_atomic_fmax() {
//     // imageAtomicMax
// }
// void image_atomic_fmin() {
//     // imageAtomicMin
// }
// void image_atomic_inc() {
//     // imageAtomicMin
// }
// void image_atomic_or() {}
// void image_atomic_smax() {}
// void image_atomic_smin() {}
// void image_atomic_sub() {}
// void image_atomic_swap() {}
// void image_atomic_umax() {}
// void image_atomic_umin() {}
// void image_atomic_xor() {}

// void image_load() {}
// void image_load_pck() {}
// void image_load_pck_sgn() {}
// void image_load_mip() {}
// void image_load_mip_pck() {}
// void image_load_mip_pck_sgn() {}

// void image_store() {}
// void image_store_pck() {}
// void image_store_mip() {}
// void image_store_mip_pck() {}

const uint8_t kTextureType1D = uint8_t(8);
const uint8_t kTextureType2D = uint8_t(9);
const uint8_t kTextureType3D = uint8_t(10);
const uint8_t kTextureTypeCube = uint8_t(11);
const uint8_t kTextureTypeArray1D = uint8_t(12);
const uint8_t kTextureTypeArray2D = uint8_t(13);
const uint8_t kTextureTypeMsaa2D = uint8_t(14);
const uint8_t kTextureTypeMsaaArray2D = uint8_t(15);

uint64_t tbuffer_base256(uint32_t tbuffer[8]) {
    uint64_t baseLo = tbuffer[0];
    uint64_t baseHi = U32ARRAY_FETCH_BITS(tbuffer, 32, 6);
    uint64_t base = baseLo | (baseHi << 32);
    return base;
}
uint64_t tbuffer_base(uint32_t tbuffer[8]) {
    return tbuffer_base256(tbuffer) << 8;
}
uint8_t tbuffer_mtype_L2(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 38, 2));
}
uint16_t tbuffer_min_lod(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 40, 12));
}
uint8_t tbuffer_dfmt(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 52, 6));
}
uint8_t tbuffer_nfmt(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 58, 4));
}
uint8_t tbuffer_mtype(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 62, 2) | (U32ARRAY_FETCH_BITS(tbuffer, 122, 1) << 2));
}
uint16_t tbuffer_width(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 64, 14));
}
uint16_t tbuffer_height(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 78, 14));
}
uint8_t tbuffer_perf_mod(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 92, 3));
}
bool tbuffer_interlaced(uint32_t tbuffer[8]) {
    return U32ARRAY_FETCH_BITS(tbuffer, 95, 1) != 0;
}
uint8_t tbuffer_dst_sel_x(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 96, 3));
}
uint8_t tbuffer_dst_sel_y(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 99, 3));
}
uint8_t tbuffer_dst_sel_z(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 102, 3));
}
uint8_t tbuffer_dst_sel_w(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 105, 3));
}
uint8_t tbuffer_base_level(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 108, 4));
}
uint8_t tbuffer_last_level(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 112, 4));
}
uint8_t tbuffer_tiling_idx(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 116, 5));
}
bool tbuffer_pow2pad(uint32_t tbuffer[8]) {
    return U32ARRAY_FETCH_BITS(tbuffer, 121, 1) != 0;
}
uint8_t tbuffer_type(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 124, 4));
}
uint16_t tbuffer_depth(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 128, 13));
}
uint16_t tbuffer_pitch(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 141, 14));
}
uint16_t tbuffer_base_array(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 160, 13));
}
uint16_t tbuffer_last_array(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 173, 13));
}
uint16_t tbuffer_min_lod_warn(uint32_t tbuffer[8]) {
    return uint16_t(U32ARRAY_FETCH_BITS(tbuffer, 192, 12));
}
uint8_t tbuffer_counter_bank_id(uint32_t tbuffer[8]) {
    return uint8_t(U32ARRAY_FETCH_BITS(tbuffer, 204, 8));
}
bool tbuffer_LOD_hdw_cnt_en(uint32_t tbuffer[8]) {
    return U32ARRAY_FETCH_BITS(tbuffer, 212, 1) != 0;
}
uint8_t ssampler_clamp_x(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 0, 3));
}
uint8_t ssampler_clamp_y(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 3, 3));
}
uint8_t ssampler_clamp_z(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 6, 3));
}
uint8_t ssampler_max_aniso_ratio(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 9, 3));
}
uint8_t ssampler_depth_compare_func(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 12, 3));
}
bool ssampler_force_unorm_coord(u32vec4 ssampler) {
    return U32ARRAY_FETCH_BITS(ssampler, 15, 1) != 0;
}
uint8_t ssampler_aniso_thresholt(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 16, 3));
}
bool ssampler_mc_coord_trunc(u32vec4 ssampler) {
    return U32ARRAY_FETCH_BITS(ssampler, 19, 1) != 0;
}
bool ssampler_force_degamma(u32vec4 ssampler) {
    return U32ARRAY_FETCH_BITS(ssampler, 20, 1) != 0;
}
uint8_t ssampler_aniso_bias(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 21, 6));
}
bool ssampler_trunc_coord(u32vec4 ssampler) {
    return U32ARRAY_FETCH_BITS(ssampler, 27, 1) != 0;
}
bool ssampler_disable_cube_wrap(u32vec4 ssampler) {
    return U32ARRAY_FETCH_BITS(ssampler, 28, 1) != 0;
}
uint8_t ssampler_filter_mode(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 29, 2));
}
uint16_t ssampler_min_lod(u32vec4 ssampler) {
    return uint16_t(U32ARRAY_FETCH_BITS(ssampler, 32, 12));
}
uint16_t ssampler_max_lod(u32vec4 ssampler) {
    return uint16_t(U32ARRAY_FETCH_BITS(ssampler, 44, 12));
}
uint8_t ssampler_perf_mip(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 56, 4));
}
uint8_t ssampler_perf_z(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 60, 4));
}
uint16_t ssampler_lod_bias(u32vec4 ssampler) {
    return uint16_t(U32ARRAY_FETCH_BITS(ssampler, 64, 14));
}
uint8_t ssampler_lod_bias_sec(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 78, 6));
}
uint8_t ssampler_xy_mag_filter(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 84, 2));
}
uint8_t ssampler_xy_min_filter(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 86, 2));
}
uint8_t ssampler_z_filter(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 88, 2));
}
uint8_t ssampler_mip_filter(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 90, 2));
}
uint16_t ssampler_border_color_ptr(u32vec4 ssampler) {
    return uint16_t(U32ARRAY_FETCH_BITS(ssampler, 96, 12));
}
uint8_t ssampler_border_color_type(u32vec4 ssampler) {
    return uint8_t(U32ARRAY_FETCH_BITS(ssampler, 126, 2));
}

// void image_gather4(inout u32vec4 vdata, u32vec4 vaddr, int32_t textureIndexHint, uint32_t tbuffer[8], int32_t samplerIndexHint, u32vec4 samplerDescriptor) {}
// image_gather4_cl
// image_gather4_l
// image_gather4_b
// image_gather4_b_cl
// image_gather4_lz
// image_gather4_c
// image_gather4_c_cl
// image_gather4_c_l
// image_gather4_c_b
// image_gather4_c_b_cl
// image_gather4_c_lz
// image_gather4_o
// image_gather4_cl_o
// image_gather4_l_o
// image_gather4_b_o
// image_gather4_b_cl_o
// image_gather4_lz_o
// image_gather4_c_o
// image_gather4_c_cl_o
// image_gather4_c_l_o
// image_gather4_c_b_o
// image_gather4_c_b_cl_o
// image_gather4_c_lz_o

int findSamplerIndex(int32_t samplerIndexHint, u32vec4 ssampler) {
    return samplerIndexHint;
}
int findTexture1DIndex(int32_t textureIndexHint, uint32_t tbuffer[8]) {
    return textureIndexHint;
}
int findTexture2DIndex(int32_t textureIndexHint, uint32_t tbuffer[8]) {
    return textureIndexHint;
}
int findTexture3DIndex(int32_t textureIndexHint, uint32_t tbuffer[8]) {
    return textureIndexHint;
}

float32_t swizzle(f32vec4 comp, int sel) {
    switch (sel) {
    case 0: return 0;
    case 1: return 1;
    case 4: return comp.x;
    case 5: return comp.y;
    case 6: return comp.z;
    case 7: return comp.w;
    }

    return 1;
}

f32vec4 swizzle(f32vec4 comp, int selX, int selY, int selZ, int selW) {
    return f32vec4(swizzle(comp, selX), swizzle(comp, selY), swizzle(comp, selZ), swizzle(comp, selW));
}

void image_sample(inout f32vec4 vdata, f32vec3 vaddr, int32_t textureIndexHint, uint32_t tbuffer[8], int32_t samplerIndexHint, u32vec4 ssampler, uint32_t dmask) {
    uint8_t textureType = tbuffer_type(tbuffer);
    f32vec4 result;
    switch (uint(textureType)) {
    case kTextureType1D:
    case kTextureTypeArray1D:
        result = texture(
            sampler1D(
                textures1D[findTexture1DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr.x);
        break;

    case kTextureType2D:
    case kTextureTypeCube:
    case kTextureTypeArray2D:
    case kTextureTypeMsaa2D:
    case kTextureTypeMsaaArray2D:
        result = texture(
            sampler2D(
                textures2D[findTexture2DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr.xy);
        break;

    case kTextureType3D:
        result = texture(
            sampler3D(
                textures3D[findTexture3DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr);
        break;

    default:
        return;
    }

    // debugPrintfEXT("image_sample: textureType: %u, coord: %v3f, result: %v4f, dmask: %u", textureType, vaddr, result, dmask);

    
    result = swizzle(result,
        tbuffer_dst_sel_x(tbuffer),
        tbuffer_dst_sel_y(tbuffer),
        tbuffer_dst_sel_z(tbuffer),
        tbuffer_dst_sel_w(tbuffer));


    int vdataIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if ((dmask & (1 << i)) != 0) {
            vdata[vdataIndex++] = result[i];
        }
    }
}

// image_sample_cl
// image_sample_d
// image_sample_d_cl
// image_sample_l
// image_sample_b
// image_sample_b_cl
// image_sample_lz
// image_sample_c
// image_sample_c_cl
// image_sample_c_d
// image_sample_c_d_cl
// image_sample_c_l
// image_sample_c_b
// image_sample_c_b_cl
// image_sample_c_lz
// image_sample_o
// image_sample_cl_o
// image_sample_d_o
// image_sample_d_cl_o
// image_sample_l_o
// image_sample_b_o
// image_sample_b_cl_o
// image_sample_lz_o
// image_sample_c_o
// image_sample_c_cl_o
// image_sample_c_d_o
// image_sample_c_d_cl_o
// image_sample_c_l_o
// image_sample_c_b_o
// image_sample_c_b_cl_o
// image_sample_c_lz_o
// image_sample_cd
// image_sample_cd_cl
// image_sample_c_cd
// image_sample_c_cd_cl
// image_sample_cd_o
// image_sample_cd_cl_o
// image_sample_c_cd_o
// image_sample_c_cd_cl_o

void image_get_lod(inout f32vec2 vdata, u32vec3 vaddr, int32_t textureIndexHint, uint32_t tbuffer[8], int32_t samplerIndexHint, u32vec4 ssampler, uint32_t dmask) {
    f32vec2 result = f32vec2(0);
    switch (uint(tbuffer_type(tbuffer))) {
    case kTextureType1D:
    case kTextureTypeArray1D:
        result = textureQueryLod(
            sampler1D(
                textures1D[findTexture1DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr.x);
        break;

    case kTextureType2D:
    case kTextureTypeCube:
    case kTextureTypeArray2D:
    case kTextureTypeMsaa2D:
    case kTextureTypeMsaaArray2D:
        result = textureQueryLod(
            sampler2D(
                textures2D[findTexture2DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr.xy);
        break;

    case kTextureType3D:
        result = textureQueryLod(
            sampler3D(
                textures3D[findTexture3DIndex(textureIndexHint, tbuffer)],
                samplers[findSamplerIndex(samplerIndexHint, ssampler)]
            ), vaddr);
        break;
    }

    int vdataIndex = 0;
    for (int i = 0; i < 2; ++i) {
        if ((dmask & (1 << i)) != 0) {
            vdata[vdataIndex++] = result[i];
        }
    }
}

void image_get_resinfo(inout u32vec4 vdata, int32_t vmipid, int32_t textureIndexHint, uint32_t tbuffer[8], uint32_t dmask) {
    i32vec4 result = i32vec4(1);

    switch (uint(tbuffer_type(tbuffer))) {
    case kTextureType1D: {
        int texIndex = findTexture1DIndex(textureIndexHint, tbuffer);
        result.x = textureSize(textures1D[texIndex], vmipid);
        result.w = textureQueryLevels(textures1D[texIndex]);
        break;
    }

    case kTextureTypeArray1D:
    case kTextureType2D:
    case kTextureTypeCube:
    case kTextureTypeArray2D: {
        int texIndex = findTexture2DIndex(textureIndexHint, tbuffer);
        result.xy = textureSize(textures2D[texIndex], vmipid);
        result.w = textureQueryLevels(textures2D[texIndex]);
        break;
    }

    case kTextureTypeMsaa2D:
    case kTextureTypeMsaaArray2D:
        result.xy = textureSize(textures2D[findTexture2DIndex(textureIndexHint, tbuffer)], 0);
        break;

    case kTextureType3D: {
        int texIndex = findTexture3DIndex(textureIndexHint, tbuffer);
        result.xyz = textureSize(textures3D[texIndex], vmipid);
        result.w = textureQueryLevels(textures3D[texIndex]);
        break;
    }
    }

    int vdataIndex = 0;
    for (int i = 0; i < 4; ++i) {
        if ((dmask & (1 << i)) != 0) {
            vdata[vdataIndex++] = result[i];
        }
    }
}

