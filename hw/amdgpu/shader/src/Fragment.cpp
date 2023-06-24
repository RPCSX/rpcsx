#include "Fragment.hpp"
#include "ConverterContext.hpp"
#include "RegisterId.hpp"
#include "RegisterState.hpp"

#include <spirv/GLSL.std.450.h>
#include <spirv/spirv-instruction.hpp>
#include <util/unreachable.hpp>

#include <bit>

using namespace amdgpu::shader;

namespace {
std::uint32_t getChannelsCount(SurfaceFormat format) {
  switch (format) {
  case kSurfaceFormat8:
    return 1;
  case kSurfaceFormat16:
    return 1;
  case kSurfaceFormat8_8:
    return 2;
  case kSurfaceFormat32:
    return 1;
  case kSurfaceFormat16_16:
    return 2;
  case kSurfaceFormat10_11_11:
    return 3;
  case kSurfaceFormat11_11_10:
    return 3;
  case kSurfaceFormat10_10_10_2:
    return 4;
  case kSurfaceFormat2_10_10_10:
    return 4;
  case kSurfaceFormat8_8_8_8:
    return 4;
  case kSurfaceFormat32_32:
    return 2;
  case kSurfaceFormat16_16_16_16:
    return 4;
  case kSurfaceFormat32_32_32:
    return 3;
  case kSurfaceFormat32_32_32_32:
    return 4;
  default:
    util::unreachable();
  }
}

std::uint32_t sizeOfFormat(SurfaceFormat format) {
  switch (format) {
  case kSurfaceFormat8:
    return 8;
  case kSurfaceFormat16:
    return 16;
  case kSurfaceFormat8_8:
    return 16;
  case kSurfaceFormat32:
    return 32;
  case kSurfaceFormat16_16:
    return 32;
  case kSurfaceFormat10_11_11:
    return 32;
  case kSurfaceFormat11_11_10:
    return 32;
  case kSurfaceFormat10_10_10_2:
    return 32;
  case kSurfaceFormat2_10_10_10:
    return 32;
  case kSurfaceFormat8_8_8_8:
    return 32;
  case kSurfaceFormat32_32:
    return 64;
  case kSurfaceFormat16_16_16_16:
    return 64;
  case kSurfaceFormat32_32_32:
    return 96;
  case kSurfaceFormat32_32_32_32:
    return 128;
  default:
    util::unreachable();
  }
}

TypeId pickBufferType(SurfaceFormat surfaceFormat,
                      TextureChannelType channelType) {
  auto size = sizeOfFormat(surfaceFormat) / getChannelsCount(surfaceFormat);

  if (size == 8) {
    switch (channelType) {
    case kTextureChannelTypeUNorm:
    case kTextureChannelTypeUScaled:
    case kTextureChannelTypeUInt:
      return TypeId::UInt8;

    default:
      return TypeId::SInt8;
    }
  }

  if (size == 16) {
    switch (channelType) {
    case kTextureChannelTypeUNorm:
    case kTextureChannelTypeUScaled:
    case kTextureChannelTypeUInt:
      return TypeId::UInt16;

    case kTextureChannelTypeFloat:
      return TypeId::Float16;

    default:
      return TypeId::SInt16;
    }
  }

  if (size == 32) {
    switch (channelType) {
    case kTextureChannelTypeUNorm:
    case kTextureChannelTypeUScaled:
    case kTextureChannelTypeUInt:
      return TypeId::UInt32;

    case kTextureChannelTypeFloat:
      return TypeId::Float32;

    default:
      return TypeId::SInt32;
    }
  }

  if (size == 64) {
    switch (channelType) {
    case kTextureChannelTypeUNorm:
    case kTextureChannelTypeUScaled:
    case kTextureChannelTypeUInt:
      return TypeId::UInt64;

    case kTextureChannelTypeFloat:
      return TypeId::Float64;

    default:
      return TypeId::SInt64;
    }
  }

  util::unreachable();
}

spirv::Type convertFromFormat(spirv::Value *result, int count,
                              Fragment &fragment, std::uint32_t *vBufferData,
                              spirv::UIntValue offset,
                              SurfaceFormat surfaceFormat,
                              TextureChannelType channelType) {
  auto loadType = pickBufferType(surfaceFormat, channelType);

  auto uniform =
      fragment.context->getOrCreateStorageBuffer(vBufferData, loadType);
  uniform->accessOp |= AccessOp::Load;

  auto storageBufferPointerType = fragment.context->getPointerType(
      spv::StorageClass::StorageBuffer, loadType);

  auto &builder = fragment.builder;

  switch (surfaceFormat) {
  case kSurfaceFormat8:
  case kSurfaceFormat8_8:
  case kSurfaceFormat8_8_8_8:
  case kSurfaceFormat16:
  case kSurfaceFormat16_16:
  case kSurfaceFormat16_16_16_16:
  case kSurfaceFormat32:
  case kSurfaceFormat32_32:
  case kSurfaceFormat32_32_32:
  case kSurfaceFormat32_32_32_32: {
    // format not requires bit fetching
    auto totalChannelsCount = getChannelsCount(surfaceFormat);
    auto channelSize = sizeOfFormat(surfaceFormat) / 8 / totalChannelsCount;
    auto channelsCount = std::min<int>(count, totalChannelsCount);

    if (channelSize != 1) {
      offset = builder.createUDiv(fragment.context->getUInt32Type(), offset,
                                  fragment.context->getUInt32(channelSize));
    }

    int channel = 0;
    auto resultType = fragment.context->getType(loadType);
    for (; channel < channelsCount; ++channel) {
      auto channelOffset = offset;

      if (channel != 0) {
        channelOffset =
            builder.createIAdd(fragment.context->getUInt32Type(), channelOffset,
                               fragment.context->getUInt32(channel));
      }

      auto uniformPointerValue = fragment.builder.createAccessChain(
          storageBufferPointerType, uniform->variable,
          {{fragment.context->getUInt32(0), channelOffset}});

      auto channelValue = fragment.builder.createLoad(
          fragment.context->getType(loadType), uniformPointerValue);
      switch (channelType) {
      case kTextureChannelTypeFloat:
      case kTextureChannelTypeSInt:
      case kTextureChannelTypeUInt:
        result[channel] = channelValue;
        break;

      case kTextureChannelTypeUNorm: {
        auto maxValue =
            (static_cast<std::uint64_t>(1) << (channelSize * 8)) - 1;

        auto uintChannelValue = spirv::cast<spirv::UIntValue>(channelValue);

        if (loadType != TypeId::UInt32) {
          uintChannelValue = builder.createUConvert(
              fragment.context->getUInt32Type(), uintChannelValue);
        }

        auto floatChannelValue = builder.createConvertUToF(
            fragment.context->getFloat32Type(), uintChannelValue);
        floatChannelValue = builder.createFDiv(
            fragment.context->getFloat32Type(), floatChannelValue,
            fragment.context->getFloat32(maxValue));
        result[channel] = floatChannelValue;
        resultType = fragment.context->getFloat32Type();
        break;
      }

      case kTextureChannelTypeSNorm: {
        auto maxValue =
            (static_cast<std::uint64_t>(1) << (channelSize * 8 - 1)) - 1;

        auto uintChannelValue = spirv::cast<spirv::SIntValue>(channelValue);

        if (loadType != TypeId::SInt32) {
          uintChannelValue = builder.createSConvert(
              fragment.context->getSint32Type(), uintChannelValue);
        }

        auto floatChannelValue = builder.createConvertSToF(
            fragment.context->getFloat32Type(), uintChannelValue);

        floatChannelValue = builder.createFDiv(
            fragment.context->getFloat32Type(), floatChannelValue,
            fragment.context->getFloat32(maxValue));

        auto glslStd450 = fragment.context->getGlslStd450();
        floatChannelValue =
            spirv::cast<spirv::FloatValue>(fragment.builder.createExtInst(
                fragment.context->getFloat32Type(), glslStd450,
                GLSLstd450FClamp,
                {{floatChannelValue, fragment.context->getFloat32(-1),
                  fragment.context->getFloat32(1)}}));
        result[channel] = floatChannelValue;
        resultType = fragment.context->getFloat32Type();
        break;
      }

      case kTextureChannelTypeUScaled: {
        auto uintChannelValue = spirv::cast<spirv::UIntValue>(channelValue);

        if (loadType != TypeId::UInt32) {
          uintChannelValue = builder.createUConvert(
              fragment.context->getUInt32Type(), uintChannelValue);
        }

        auto floatChannelValue = builder.createConvertUToF(
            fragment.context->getFloat32Type(), uintChannelValue);

        result[channel] = floatChannelValue;
        resultType = fragment.context->getFloat32Type();
        break;
      }

      case kTextureChannelTypeSScaled: {
        auto uintChannelValue = spirv::cast<spirv::SIntValue>(channelValue);

        if (loadType != TypeId::SInt32) {
          uintChannelValue = builder.createSConvert(
              fragment.context->getSint32Type(), uintChannelValue);
        }

        auto floatChannelValue = builder.createConvertSToF(
            fragment.context->getFloat32Type(), uintChannelValue);

        result[channel] = floatChannelValue;
        resultType = fragment.context->getFloat32Type();
        break;
      }

      case kTextureChannelTypeSNormNoZero: {
        auto maxValue =
            (static_cast<std::uint64_t>(1) << (channelSize * 8)) - 1;

        auto uintChannelValue = spirv::cast<spirv::SIntValue>(channelValue);

        if (loadType != TypeId::SInt32) {
          uintChannelValue = builder.createSConvert(
              fragment.context->getSint32Type(), uintChannelValue);
        }

        auto floatChannelValue = builder.createConvertSToF(
            fragment.context->getFloat32Type(), uintChannelValue);

        floatChannelValue = builder.createFMul(
            fragment.context->getFloat32Type(), floatChannelValue,
            fragment.context->getFloat32(2));
        floatChannelValue = builder.createFAdd(
            fragment.context->getFloat32Type(), floatChannelValue,
            fragment.context->getFloat32(1));

        floatChannelValue = builder.createFDiv(
            fragment.context->getFloat32Type(), floatChannelValue,
            fragment.context->getFloat32(maxValue));

        result[channel] = floatChannelValue;
        resultType = fragment.context->getFloat32Type();
        break;
      }

      default:
        util::unreachable("unimplemented channel type %u", channelType);
      }
    }

    for (; channel < count; ++channel) {
      result[channel] =
          fragment.createBitcast(resultType, fragment.context->getUInt32Type(),
                                 fragment.context->getUInt32(0));
    }
    return resultType;
  }

  default:
    break;
  }

  util::unreachable("unimplemented conversion type. %u.%u", surfaceFormat,
                    channelType);
}

void convertToFormat(RegisterId sourceRegister, int count, Fragment &fragment,
                     std::uint32_t *vBufferData, spirv::UIntValue offset,
                     SurfaceFormat surfaceFormat,
                     TextureChannelType channelType) {

  auto storeType = pickBufferType(surfaceFormat, channelType);

  auto uniform =
      fragment.context->getOrCreateStorageBuffer(vBufferData, storeType);
  uniform->accessOp |= AccessOp::Store;

  auto uniformPointerType = fragment.context->getPointerType(
      spv::StorageClass::StorageBuffer, storeType);

  auto &builder = fragment.builder;
  switch (surfaceFormat) {
  case kSurfaceFormat8:
  case kSurfaceFormat8_8:
  case kSurfaceFormat8_8_8_8:
  case kSurfaceFormat16:
  case kSurfaceFormat16_16:
  case kSurfaceFormat16_16_16_16:
  case kSurfaceFormat32:
  case kSurfaceFormat32_32:
  case kSurfaceFormat32_32_32:
  case kSurfaceFormat32_32_32_32: {
    // format not requires bit fetching
    auto totalChannelsCount = getChannelsCount(surfaceFormat);
    auto channelSize = sizeOfFormat(surfaceFormat) / 8 / totalChannelsCount;
    auto channelsCount = std::min<int>(count, totalChannelsCount);

    if (channelSize != 1) {
      offset = builder.createUDiv(fragment.context->getUInt32Type(), offset,
                                  fragment.context->getUInt32(channelSize));
    }

    int channel = 0;

    for (; channel < channelsCount; ++channel) {
      auto channelOffset = offset;

      if (channel != 0) {
        channelOffset =
            builder.createIAdd(fragment.context->getUInt32Type(), channelOffset,
                               fragment.context->getUInt32(channel));
      }

      auto uniformPointerValue = fragment.builder.createAccessChain(
          uniformPointerType, uniform->variable,
          {{fragment.context->getUInt32(0), channelOffset}});

      switch (channelType) {
      case kTextureChannelTypeFloat:
      case kTextureChannelTypeSInt:
      case kTextureChannelTypeUInt:
        fragment.builder.createStore(
            uniformPointerValue,
            fragment
                .getOperand(RegisterId::Raw(sourceRegister + channel),
                            storeType)
                .value);
        break;

      default:
        util::unreachable("unimplemented channel type %u", channelType);
      }
    }

    for (; channel < count; ++channel) {
      auto channelOffset =
          builder.createIAdd(fragment.context->getUInt32Type(), offset,
                             fragment.context->getUInt32(channel));
      auto uniformPointerValue = fragment.builder.createAccessChain(
          uniformPointerType, uniform->variable,
          {{fragment.context->getUInt32(0), channelOffset}});

      fragment.builder.createStore(
          uniformPointerValue,
          fragment.createBitcast(fragment.context->getType(storeType),
                                 fragment.context->getUInt32Type(),
                                 fragment.context->getUInt32(0)));
    }

    return;
  }

  default:
    break;
  }

  util::unreachable("unimplemented conversion type. %u.%u", surfaceFormat,
                    channelType);
}

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

  TextureChannelType nfmt : 3;
  SurfaceFormat dfmt : 4;
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

enum class TextureType {
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

enum class CmpKind {
  F,
  LT,
  EQ,
  LE,
  GT,
  LG,
  GE,
  O,
  U,
  NGE,
  NLG,
  NGT,
  NLE,
  NEQ,
  NLT,
  NE,
  TRU,
  T = TRU
};

enum class CmpFlags { None = 0, X = 1 << 0, S = 1 << 1, SX = S | X };
inline CmpFlags operator&(CmpFlags a, CmpFlags b) {
  return static_cast<CmpFlags>(static_cast<int>(a) & static_cast<int>(b));
}

Value doCmpOp(Fragment &fragment, TypeId type, spirv::Value src0,
              spirv::Value src1, CmpKind kind, CmpFlags flags) {
  spirv::BoolValue cmp;
  auto boolT = fragment.context->getBoolType();

  switch (kind) {
  case CmpKind::F:
    cmp = fragment.context->getFalse();
    break;
  case CmpKind::LT:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFOrdLessThan(boolT, src0, src1);
    } else if (type.isSignedInt()) {
      cmp = fragment.builder.createSLessThan(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createULessThan(boolT, src0, src1);
    }
    break;
  case CmpKind::EQ:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFOrdEqual(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createIEqual(boolT, src0, src1);
    }
    break;
  case CmpKind::LE:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFOrdLessThanEqual(boolT, src0, src1);
    } else if (type.isSignedInt()) {
      cmp = fragment.builder.createSLessThanEqual(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createULessThanEqual(boolT, src0, src1);
    }
    break;
  case CmpKind::GT:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFOrdGreaterThan(boolT, src0, src1);
    } else if (type.isSignedInt()) {
      cmp = fragment.builder.createSGreaterThan(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createUGreaterThan(boolT, src0, src1);
    }
    break;
  case CmpKind::LG:
    cmp = fragment.builder.createFOrdNotEqual(boolT, src0, src1);
    break;
  case CmpKind::GE:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFOrdGreaterThanEqual(boolT, src0, src1);
    } else if (type.isSignedInt()) {
      cmp = fragment.builder.createSGreaterThanEqual(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createUGreaterThanEqual(boolT, src0, src1);
    }
    break;
  case CmpKind::O:
    cmp = fragment.builder.createLogicalAnd(
        boolT, fragment.builder.createFOrdEqual(boolT, src0, src0),
        fragment.builder.createFOrdEqual(boolT, src1, src1));
    break;
  case CmpKind::U:
    cmp = fragment.builder.createLogicalAnd(
        boolT, fragment.builder.createFUnordNotEqual(boolT, src0, src0),
        fragment.builder.createFUnordNotEqual(boolT, src1, src1));
    break;
  case CmpKind::NGE:
    cmp = fragment.builder.createFUnordLessThan(boolT, src0, src1);
    break;
  case CmpKind::NLG:
    cmp = fragment.builder.createFUnordGreaterThanEqual(boolT, src0, src1);
    break;
  case CmpKind::NGT:
    cmp = fragment.builder.createFUnordLessThanEqual(boolT, src0, src1);
    break;
  case CmpKind::NLE:
    cmp = fragment.builder.createFUnordGreaterThan(boolT, src0, src1);
    break;
  case CmpKind::NE:
  case CmpKind::NEQ:
    if (type.isFloatPoint()) {
      cmp = fragment.builder.createFUnordNotEqual(boolT, src0, src1);
    } else {
      cmp = fragment.builder.createINotEqual(boolT, src0, src1);
    }
    break;
  case CmpKind::NLT:
    cmp = fragment.builder.createFUnordGreaterThanEqual(boolT, src0, src1);
    break;
  case CmpKind::TRU:
    cmp = fragment.context->getTrue();
    break;
  }

  if (!cmp) {
    util::unreachable();
  }

  auto uint32T = fragment.context->getUInt32Type();
  auto uint32_0 = fragment.context->getUInt32(0);
  auto result = fragment.builder.createSelect(
      uint32T, cmp, fragment.context->getUInt32(1), uint32_0);

  if ((flags & CmpFlags::X) == CmpFlags::X) {
    fragment.setOperand(RegisterId::ExecLo, {uint32T, result});
    fragment.setOperand(RegisterId::ExecHi, {uint32T, uint32_0});
  }

  // TODO: handle flags
  return {uint32T, result};
};

void convertVop2(Fragment &fragment, Vop2 inst) {
  fragment.registers->pc += Vop2::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Vop2::Op::V_CVT_PKRTZ_F16_F32: {
    auto float2T = fragment.context->getType(TypeId::Float32x2);
    auto uintT = fragment.context->getType(TypeId::UInt32);
    auto glslStd450 = fragment.context->getGlslStd450();

    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::Float32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value;

    auto src = fragment.builder.createCompositeConstruct(
        float2T, std::array{src0, src1});
    auto dst = fragment.builder.createExtInst(
        uintT, glslStd450, GLSLstd450PackHalf2x16, std::array{src});

    fragment.setVectorOperand(inst.vdst, {uintT, dst});
    break;
  }
  case Vop2::Op::V_AND_B32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createBitwiseAnd(uintT, src0, src1)});
    break;
  }

  case Vop2::Op::V_OR_B32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createBitwiseOr(uintT, src0, src1)});
    break;
  }

  case Vop2::Op::V_ADD_I32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;
    auto uintT = fragment.context->getType(TypeId::UInt32);
    auto resultStruct =
        fragment.context->getStructType(std::array{uintT, uintT});
    auto result = fragment.builder.createIAddCarry(resultStruct, src0, src1);
    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(0)})});
    fragment.setVcc(
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(1)})});
    // TODO: update vcc hi
    break;
  }

  case Vop2::Op::V_SUB_I32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;
    auto uintT = fragment.context->getType(TypeId::UInt32);
    auto resultStruct =
        fragment.context->getStructType(std::array{uintT, uintT});
    auto result = fragment.builder.createISubBorrow(resultStruct, src0, src1);
    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(0)})});
    fragment.setVcc(
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(1)})});
    // TODO: update vcc hi
    break;
  }

  case Vop2::Op::V_MAC_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto dst = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vdst, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFAdd(
        floatT, fragment.builder.createFMul(floatT, src0, src1), dst);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MAC_LEGACY_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto dst = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vdst, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto boolT = fragment.context->getBoolType();
    auto float0 = fragment.context->getFloat32(0);

    auto src0IsZero = fragment.builder.createFOrdEqual(boolT, src0, float0);
    auto src1IsZero = fragment.builder.createFOrdEqual(boolT, src1, float0);
    auto anySrcIsZero =
        fragment.builder.createLogicalOr(boolT, src0IsZero, src1IsZero);

    auto result = fragment.builder.createFAdd(
        floatT,
        fragment.builder.createSelect(
            floatT, anySrcIsZero, float0,
            fragment.builder.createFMul(floatT, src0, src1)),
        dst);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MUL_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFMul(floatT, src0, src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_ADD_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFAdd(floatT, src0, src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_SUB_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFSub(floatT, src0, src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }
  case Vop2::Op::V_SUBREV_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFSub(floatT, src1, src0);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }
  case Vop2::Op::V_SUBREV_I32: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::SIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::SInt32).value);
    auto floatT = fragment.context->getSint32Type();

    auto result = fragment.builder.createISub(floatT, src1, src0);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MIN_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto boolT = fragment.context->getBoolType();

    auto result = fragment.builder.createSelect(
        floatT, fragment.builder.createFOrdLessThan(boolT, src0, src1), src0,
        src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MAX_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto boolT = fragment.context->getBoolType();

    auto result = fragment.builder.createSelect(
        floatT, fragment.builder.createFOrdGreaterThanEqual(boolT, src0, src1),
        src0, src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MUL_LEGACY_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto boolT = fragment.context->getBoolType();
    auto float0 = fragment.context->getFloat32(0);

    auto src0IsZero = fragment.builder.createFOrdEqual(boolT, src0, float0);
    auto src1IsZero = fragment.builder.createFOrdEqual(boolT, src1, float0);
    auto anySrcIsZero =
        fragment.builder.createLogicalOr(boolT, src0IsZero, src1IsZero);

    auto result = fragment.builder.createSelect(
        floatT, anySrcIsZero, float0,
        fragment.builder.createFMul(floatT, src0, src1));

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MADAK_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto constant = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(255, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFAdd(
        floatT, fragment.builder.createFMul(floatT, src0, src1), constant);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_MADMK_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value);
    auto constant = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(255, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFAdd(
        floatT, fragment.builder.createFMul(floatT, src0, constant), src1);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop2::Op::V_LSHL_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value);
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createShiftLeftLogical(uintT, src0, src1)});
    break;
  }

  case Vop2::Op::V_LSHLREV_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value);
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createShiftLeftLogical(uintT, src1, src0)});
    break;
  }

  case Vop2::Op::V_LSHR_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value);
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createShiftRightLogical(uintT, src0, src1)});
    break;
  }

  case Vop2::Op::V_LSHRREV_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value);
    auto uintT = fragment.context->getType(TypeId::UInt32);

    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createShiftRightLogical(uintT, src1, src0)});
    break;
  }

  case Vop2::Op::V_ASHR_I32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::SInt32).value);
    auto sintT = fragment.context->getType(TypeId::SInt32);

    fragment.setVectorOperand(
        inst.vdst, {sintT, fragment.builder.createShiftRightArithmetic(
                               sintT, src0, src1)});
    break;
  }

  case Vop2::Op::V_ASHRREV_I32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getVectorOperand(inst.vsrc1, TypeId::SInt32).value);
    auto sintT = fragment.context->getType(TypeId::SInt32);

    fragment.setVectorOperand(
        inst.vdst, {sintT, fragment.builder.createShiftRightArithmetic(
                               sintT, src1, src0)});
    break;
  }

  case Vop2::Op::V_CNDMASK_B32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;
    auto vcc = fragment.getVccLo();

    auto cmp = fragment.builder.createINotEqual(fragment.context->getBoolType(),
                                                vcc.value,
                                                fragment.context->getUInt32(0));

    auto uint32T = fragment.context->getUInt32Type();
    auto result = fragment.builder.createSelect(uint32T, cmp, src1, src0);
    fragment.setVectorOperand(inst.vdst, {uint32T, result});
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertSop2(Fragment &fragment, Sop2 inst) {
  fragment.registers->pc += Sop2::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Sop2::Op::S_ADD_U32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createIAdd(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_ADD_I32: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::SInt32).value);
    auto resultT = fragment.context->getSint32Type();
    auto result = fragment.builder.createIAdd(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_ASHR_I32: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getSint32Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x3f)));

    auto result =
        fragment.builder.createShiftRightArithmetic(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_ASHR_I64: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::SInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getSint64Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x3f)));

    auto result =
        fragment.builder.createShiftRightArithmetic(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_LSHR_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getUInt32Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x1f)));

    auto result = fragment.builder.createShiftRightLogical(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_LSHR_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getUInt64Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x3f)));

    auto result = fragment.builder.createShiftRightLogical(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_LSHL_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getUInt32Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x1f)));

    auto result = fragment.builder.createShiftLeftLogical(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_LSHL_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);

    auto resultT = fragment.context->getUInt64Type();
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        resultT, src1, fragment.context->getUInt32(0x3f)));

    auto result = fragment.builder.createShiftLeftLogical(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_CSELECT_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto resultT = fragment.context->getUInt32Type();
    auto result =
        fragment.builder.createSelect(resultT, fragment.getScc(), src0, src1);
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_CSELECT_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);

    auto resultT = fragment.context->getUInt64Type();
    auto result =
        fragment.builder.createSelect(resultT, fragment.getScc(), src0, src1);
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_MUL_I32: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::SInt32).value);
    auto resultT = fragment.context->getSint32Type();
    auto result = fragment.builder.createIMul(resultT, src0, src1);
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_AND_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createBitwiseAnd(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_ANDN2_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createBitwiseAnd(
        resultT, src0, fragment.builder.createNot(resultT, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_AND_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);
    auto resultT = fragment.context->getUInt64Type();
    auto result = fragment.builder.createBitwiseAnd(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_ANDN2_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);
    auto resultT = fragment.context->getUInt64Type();
    auto result = fragment.builder.createBitwiseAnd(
        resultT, src0, fragment.builder.createNot(resultT, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_OR_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createBitwiseOr(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_OR_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);
    auto resultT = fragment.context->getUInt64Type();
    auto result = fragment.builder.createBitwiseOr(resultT, src0, src1);
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_NAND_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createNot(
        resultT, fragment.builder.createBitwiseAnd(resultT, src0, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_NAND_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);
    auto resultT = fragment.context->getUInt64Type();
    auto result = fragment.builder.createNot(
        resultT, fragment.builder.createBitwiseAnd(resultT, src0, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_NOR_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createNot(
        resultT, fragment.builder.createBitwiseOr(resultT, src0, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }
  case Sop2::Op::S_NOR_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt64).value);
    auto resultT = fragment.context->getUInt64Type();
    auto result = fragment.builder.createNot(
        resultT, fragment.builder.createBitwiseOr(resultT, src0, src1));
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  case Sop2::Op::S_BFE_U32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);

    auto operandT = fragment.context->getUInt32Type();

    auto offset =
        spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
            operandT, src1, fragment.context->getUInt32(0x1f)));
    auto size = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT,
        fragment.builder.createShiftRightLogical(
            operandT, src1, fragment.context->getUInt32(16)),
        fragment.context->getUInt32(0x7f)));

    auto field =
        fragment.builder.createShiftRightLogical(operandT, src0, offset);
    auto mask = fragment.builder.createISub(
        operandT,
        fragment.builder.createShiftLeftLogical(
            operandT, fragment.context->getUInt32(1), size),
        fragment.context->getUInt32(1));

    auto result = fragment.builder.createBitwiseAnd(operandT, field, mask);
    auto resultT = fragment.context->getUInt32Type();
    fragment.setScc({resultT, result});
    fragment.setScalarOperand(inst.sdst, {resultT, result});
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertSopk(Fragment &fragment, Sopk inst) {
  fragment.registers->pc += Sopk::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Sopk::Op::S_MOVK_I32:
    fragment.setScalarOperand(inst.sdst,
                              {fragment.context->getSint32Type(),
                               fragment.context->getSInt32(inst.simm)});
    break;
  default:
    inst.dump();
    util::unreachable();
  }
}
void convertSmrd(Fragment &fragment, Smrd inst) {
  fragment.registers->pc += Smrd::kMinInstSize * sizeof(std::uint32_t);

  auto getOffset = [&](std::int32_t adv = 0) -> spirv::IntValue {
    if (inst.imm) {
      return fragment.context->getUInt32(inst.offset + adv);
    }

    auto resultT = fragment.context->getUInt32Type();
    auto resultV = fragment.getScalarOperand(inst.offset, TypeId::UInt32).value;

    if (auto constVal = fragment.context->findUint32Value(resultV)) {
      return fragment.context->getUInt32(*constVal / 4 + adv);
    }

    auto result = fragment.builder.createUDiv(
        resultT, spirv::cast<spirv::UIntValue>(resultV),
        fragment.context->getUInt32(4));

    if (adv != 0) {
      result = fragment.builder.createIAdd(resultT, result,
                                           fragment.context->getUInt32(adv));
    }
    return result;
  };

  switch (inst.op) {
  case Smrd::Op::S_BUFFER_LOAD_DWORD:
  case Smrd::Op::S_BUFFER_LOAD_DWORDX2:
  case Smrd::Op::S_BUFFER_LOAD_DWORDX4:
  case Smrd::Op::S_BUFFER_LOAD_DWORDX8:
  case Smrd::Op::S_BUFFER_LOAD_DWORDX16: {
    std::uint32_t count = 1
                          << (static_cast<int>(inst.op) -
                              static_cast<int>(Smrd::Op::S_BUFFER_LOAD_DWORD));
    auto vBuffer0 =
        fragment.getScalarOperand((inst.sbase << 1) + 0, TypeId::UInt32);
    auto vBuffer1 =
        fragment.getScalarOperand((inst.sbase << 1) + 1, TypeId::UInt32);
    auto vBuffer2 =
        fragment.getScalarOperand((inst.sbase << 1) + 2, TypeId::UInt32);
    auto vBuffer3 =
        fragment.getScalarOperand((inst.sbase << 1) + 3, TypeId::UInt32);

    auto optVBuffer0Value = fragment.context->findUint32Value(vBuffer0.value);
    auto optVBuffer1Value = fragment.context->findUint32Value(vBuffer1.value);
    auto optVBuffer2Value = fragment.context->findUint32Value(vBuffer2.value);
    auto optVBuffer3Value = fragment.context->findUint32Value(vBuffer3.value);

    if (optVBuffer0Value && optVBuffer1Value && optVBuffer2Value &&
        optVBuffer3Value) {
      std::uint32_t vBufferData[] = {*optVBuffer0Value, *optVBuffer1Value,
                                     *optVBuffer2Value, *optVBuffer3Value};
      auto vbuffer = reinterpret_cast<GnmVBuffer *>(vBufferData);
      // std::printf("vBuffer address = %lx\n", vbuffer->getAddress());

      auto valueT = fragment.context->getFloat32Type();
      auto uniform = fragment.context->getOrCreateStorageBuffer(
          vBufferData, TypeId::Float32);
      uniform->accessOp |= AccessOp::Load;
      auto storageBufferPointerType = fragment.context->getPointerType(
          spv::StorageClass::StorageBuffer, TypeId::Float32);

      for (std::uint32_t i = 0; i < count; ++i) {
        auto storageBufferPointerValue = fragment.builder.createAccessChain(
            storageBufferPointerType, uniform->variable,
            {{fragment.context->getUInt32(0), getOffset(i)}});

        auto value =
            fragment.builder.createLoad(valueT, storageBufferPointerValue);
        fragment.setScalarOperand(inst.sdst + i, {valueT, value});
      }
    } else {
      // FIXME: implement runtime V# buffer fetching
      util::unreachable();
    }
    break;
  }

  case Smrd::Op::S_LOAD_DWORD:
  case Smrd::Op::S_LOAD_DWORDX2:
  case Smrd::Op::S_LOAD_DWORDX4:
  case Smrd::Op::S_LOAD_DWORDX8:
  case Smrd::Op::S_LOAD_DWORDX16: {
    std::uint32_t count = 1 << (static_cast<int>(inst.op) -
                                static_cast<int>(Smrd::Op::S_LOAD_DWORD));

    auto uint32T = fragment.context->getUInt32Type();
    auto sgprLo = fragment.getScalarOperand(inst.sbase << 1, TypeId::UInt32);
    auto sgprHi =
        fragment.getScalarOperand((inst.sbase << 1) + 1, TypeId::UInt32);
    auto optLoAddress = fragment.context->findUint32Value(sgprLo.value);
    auto optHiAddress = fragment.context->findUint32Value(sgprHi.value);

    if (inst.imm && optLoAddress && optHiAddress) {
      // if it is imm and address is known, read the values now
      auto memory = fragment.context->getMemory();
      auto address =
          *optLoAddress | (static_cast<std::uint64_t>(*optHiAddress) << 32);

      auto data =
          memory.getPointer<std::uint32_t>(address + (inst.offset << 2));
      for (std::uint32_t i = 0; i < count; ++i) {
        fragment.setScalarOperand(
            inst.sdst + i, {uint32T, fragment.context->getUInt32(data[i])});
      }
    } else {
      // FIXME: implement
      // TODO: create uniform and do load from it
      util::unreachable();
    }

    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertVop3(Fragment &fragment, Vop3 inst) {
  fragment.registers->pc += Vop3::kMinInstSize * sizeof(std::uint32_t);

  auto applyOmod = [&](Value result) -> Value {
    switch (inst.omod) {
    case 1:
      return {result.type, fragment.builder.createFMul(
                               spirv::cast<spirv::FloatType>(result.type),
                               spirv::cast<spirv::FloatValue>(result.value),
                               fragment.context->getFloat32(2))};

    case 2:
      return {result.type, fragment.builder.createFMul(
                               spirv::cast<spirv::FloatType>(result.type),
                               spirv::cast<spirv::FloatValue>(result.value),
                               fragment.context->getFloat32(4))};
    case 3:
      return {result.type, fragment.builder.createFDiv(
                               spirv::cast<spirv::FloatType>(result.type),
                               spirv::cast<spirv::FloatValue>(result.value),
                               fragment.context->getFloat32(2))};

    default:
    case 0:
      return result;
    }
  };

  auto applyClamp = [&](Value result) -> Value {
    if (inst.clmp) {
      auto glslStd450 = fragment.context->getGlslStd450();
      result.value = fragment.builder.createExtInst(
          result.type, glslStd450, GLSLstd450FClamp,
          {{result.value, fragment.context->getFloat32(0),
            fragment.context->getFloat32(1)}});
    }

    return result;
  };

  auto getSrc = [&](int index, TypeId type) -> Value {
    std::uint32_t src =
        index == 0 ? inst.src0 : (index == 1 ? inst.src1 : inst.src2);

    auto result = fragment.getScalarOperand(src, type);

    if (inst.abs & (1 << index)) {
      auto glslStd450 = fragment.context->getGlslStd450();
      result.value = fragment.builder.createExtInst(
          result.type, glslStd450, GLSLstd450FAbs, {{result.value}});
    }

    if (inst.neg & (1 << index)) {
      result.value = fragment.builder.createFNegate(
          spirv::cast<spirv::FloatType>(result.type),
          spirv::cast<spirv::FloatValue>(result.value));
    }

    return result;
  };

  auto getSdstSrc = [&](int index, TypeId type) -> Value {
    std::uint32_t src =
        index == 0 ? inst.src0 : (index == 1 ? inst.src1 : inst.src2);

    auto result = fragment.getScalarOperand(src, type);

    if (inst.neg & (1 << index)) {
      result.value = fragment.builder.createFNegate(
          spirv::cast<spirv::FloatType>(result.type),
          spirv::cast<spirv::FloatValue>(result.value));
    }

    return result;
  };

  auto cmpOp = [&](TypeId type, CmpKind kind, CmpFlags flags = CmpFlags::None) {
    auto src0 = fragment.getScalarOperand(inst.src0, type).value;
    auto src1 = fragment.getScalarOperand(inst.src1, type).value;

    auto result = doCmpOp(fragment, type, src0, src1, kind, flags);
    fragment.setScalarOperand(inst.vdst, result);
    fragment.setScalarOperand(inst.vdst + 1, {fragment.context->getUInt32Type(),
                                              fragment.context->getUInt32(0)});
  };

  switch (inst.op) {
  case Vop3::Op::V3_CMP_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG);
    break;
  case Vop3::Op::V3_CMP_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O);
    break;
  case Vop3::Op::V3_CMP_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U);
    break;
  case Vop3::Op::V3_CMP_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE);
    break;
  case Vop3::Op::V3_CMP_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG);
    break;
  case Vop3::Op::V3_CMP_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT);
    break;
  case Vop3::Op::V3_CMP_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE);
    break;
  case Vop3::Op::V3_CMP_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ);
    break;
  case Vop3::Op::V3_CMP_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT);
    break;
  case Vop3::Op::V3_CMP_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU);
    break;
  case Vop3::Op::V3_CMPX_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMP_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG);
    break;
  case Vop3::Op::V3_CMP_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O);
    break;
  case Vop3::Op::V3_CMP_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U);
    break;
  case Vop3::Op::V3_CMP_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE);
    break;
  case Vop3::Op::V3_CMP_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG);
    break;
  case Vop3::Op::V3_CMP_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT);
    break;
  case Vop3::Op::V3_CMP_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE);
    break;
  case Vop3::Op::V3_CMP_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ);
    break;
  case Vop3::Op::V3_CMP_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT);
    break;
  case Vop3::Op::V3_CMP_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU);
    break;
  case Vop3::Op::V3_CMPX_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPS_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPSX_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPS_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPS_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::S);
    break;
  case Vop3::Op::V3_CMPSX_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMPSX_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::SX);
    break;
  case Vop3::Op::V3_CMP_F_I32:
    cmpOp(TypeId::SInt32, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_I32:
    cmpOp(TypeId::SInt32, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_I32:
    cmpOp(TypeId::SInt32, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_I32:
    cmpOp(TypeId::SInt32, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_I32:
    cmpOp(TypeId::SInt32, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_I32:
    cmpOp(TypeId::SInt32, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_I32:
    cmpOp(TypeId::SInt32, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_T_I32:
    cmpOp(TypeId::SInt32, CmpKind::T);
    break;
  // case Vop3::Op::V3_CMP_CLASS_F32: cmpOp(TypeId::Float32, CmpKind::CLASS);
  // break;
  case Vop3::Op::V3_CMP_LT_I16:
    cmpOp(TypeId::SInt16, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_I16:
    cmpOp(TypeId::SInt16, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_I16:
    cmpOp(TypeId::SInt16, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_I16:
    cmpOp(TypeId::SInt16, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_I16:
    cmpOp(TypeId::SInt16, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_I16:
    cmpOp(TypeId::SInt16, CmpKind::GE);
    break;
  // case Vop3::Op::V3_CMP_CLASS_F16: cmpOp(TypeId::Float16, CmpKind::CLASS);
  // break;
  case Vop3::Op::V3_CMPX_F_I32:
    cmpOp(TypeId::SInt32, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_I32:
    cmpOp(TypeId::SInt32, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_I32:
    cmpOp(TypeId::SInt32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_I32:
    cmpOp(TypeId::SInt32, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_I32:
    cmpOp(TypeId::SInt32, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_I32:
    cmpOp(TypeId::SInt32, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_I32:
    cmpOp(TypeId::SInt32, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_T_I32:
    cmpOp(TypeId::SInt32, CmpKind::T, CmpFlags::X);
    break;
  // case Vop3::Op::V3_CMPX_CLASS_F32: cmpOp(TypeId::Float32, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vop3::Op::V3_CMPX_LT_I16:
    cmpOp(TypeId::SInt16, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_I16:
    cmpOp(TypeId::SInt16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_I16:
    cmpOp(TypeId::SInt16, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_I16:
    cmpOp(TypeId::SInt16, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_I16:
    cmpOp(TypeId::SInt16, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_I16:
    cmpOp(TypeId::SInt16, CmpKind::GE, CmpFlags::X);
    break;
  // case Vop3::Op::V3_CMPX_CLASS_F16: cmpOp(TypeId::Float16, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vop3::Op::V3_CMP_F_I64:
    cmpOp(TypeId::SInt64, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_I64:
    cmpOp(TypeId::SInt64, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_I64:
    cmpOp(TypeId::SInt64, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_I64:
    cmpOp(TypeId::SInt64, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_I64:
    cmpOp(TypeId::SInt64, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_I64:
    cmpOp(TypeId::SInt64, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_I64:
    cmpOp(TypeId::SInt64, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_T_I64:
    cmpOp(TypeId::SInt64, CmpKind::T);
    break;
  // case Vop3::Op::V3_CMP_CLASS_F64: cmpOp(TypeId::Float64, CmpKind::CLASS);
  // break;
  case Vop3::Op::V3_CMP_LT_U16:
    cmpOp(TypeId::UInt16, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_U16:
    cmpOp(TypeId::UInt16, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_U16:
    cmpOp(TypeId::UInt16, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_U16:
    cmpOp(TypeId::UInt16, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_U16:
    cmpOp(TypeId::UInt16, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_U16:
    cmpOp(TypeId::UInt16, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMPX_F_I64:
    cmpOp(TypeId::SInt64, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_I64:
    cmpOp(TypeId::SInt64, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_I64:
    cmpOp(TypeId::SInt64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_I64:
    cmpOp(TypeId::SInt64, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_I64:
    cmpOp(TypeId::SInt64, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_I64:
    cmpOp(TypeId::SInt64, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_I64:
    cmpOp(TypeId::SInt64, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_T_I64:
    cmpOp(TypeId::SInt64, CmpKind::T, CmpFlags::X);
    break;
  // case Vop3::Op::V3_CMPX_CLASS_F64: cmpOp(TypeId::Float64, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vop3::Op::V3_CMPX_LT_U16:
    cmpOp(TypeId::UInt16, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_U16:
    cmpOp(TypeId::UInt16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_U16:
    cmpOp(TypeId::UInt16, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_U16:
    cmpOp(TypeId::UInt16, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_U16:
    cmpOp(TypeId::UInt16, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_U16:
    cmpOp(TypeId::UInt16, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMP_F_U32:
    cmpOp(TypeId::UInt32, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_U32:
    cmpOp(TypeId::UInt32, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_U32:
    cmpOp(TypeId::UInt32, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_U32:
    cmpOp(TypeId::UInt32, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_U32:
    cmpOp(TypeId::UInt32, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_U32:
    cmpOp(TypeId::UInt32, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_U32:
    cmpOp(TypeId::UInt32, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_T_U32:
    cmpOp(TypeId::UInt32, CmpKind::T);
    break;
  case Vop3::Op::V3_CMP_F_F16:
    cmpOp(TypeId::Float16, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_F16:
    cmpOp(TypeId::Float16, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_F16:
    cmpOp(TypeId::Float16, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_F16:
    cmpOp(TypeId::Float16, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_F16:
    cmpOp(TypeId::Float16, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_LG_F16:
    cmpOp(TypeId::Float16, CmpKind::LG);
    break;
  case Vop3::Op::V3_CMP_GE_F16:
    cmpOp(TypeId::Float16, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_O_F16:
    cmpOp(TypeId::Float16, CmpKind::O);
    break;
  case Vop3::Op::V3_CMPX_F_U32:
    cmpOp(TypeId::UInt32, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_U32:
    cmpOp(TypeId::UInt32, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_U32:
    cmpOp(TypeId::UInt32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_U32:
    cmpOp(TypeId::UInt32, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_U32:
    cmpOp(TypeId::UInt32, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_U32:
    cmpOp(TypeId::UInt32, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_U32:
    cmpOp(TypeId::UInt32, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_T_U32:
    cmpOp(TypeId::UInt32, CmpKind::T, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_F_F16:
    cmpOp(TypeId::Float16, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_F16:
    cmpOp(TypeId::Float16, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_F16:
    cmpOp(TypeId::Float16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_F16:
    cmpOp(TypeId::Float16, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_F16:
    cmpOp(TypeId::Float16, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LG_F16:
    cmpOp(TypeId::Float16, CmpKind::LG, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_F16:
    cmpOp(TypeId::Float16, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_O_F16:
    cmpOp(TypeId::Float16, CmpKind::O, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMP_F_U64:
    cmpOp(TypeId::UInt64, CmpKind::F);
    break;
  case Vop3::Op::V3_CMP_LT_U64:
    cmpOp(TypeId::UInt64, CmpKind::LT);
    break;
  case Vop3::Op::V3_CMP_EQ_U64:
    cmpOp(TypeId::UInt64, CmpKind::EQ);
    break;
  case Vop3::Op::V3_CMP_LE_U64:
    cmpOp(TypeId::UInt64, CmpKind::LE);
    break;
  case Vop3::Op::V3_CMP_GT_U64:
    cmpOp(TypeId::UInt64, CmpKind::GT);
    break;
  case Vop3::Op::V3_CMP_NE_U64:
    cmpOp(TypeId::UInt64, CmpKind::NE);
    break;
  case Vop3::Op::V3_CMP_GE_U64:
    cmpOp(TypeId::UInt64, CmpKind::GE);
    break;
  case Vop3::Op::V3_CMP_T_U64:
    cmpOp(TypeId::UInt64, CmpKind::T);
    break;
  case Vop3::Op::V3_CMP_U_F16:
    cmpOp(TypeId::Float16, CmpKind::U);
    break;
  case Vop3::Op::V3_CMP_NGE_F16:
    cmpOp(TypeId::Float16, CmpKind::NGE);
    break;
  case Vop3::Op::V3_CMP_NLG_F16:
    cmpOp(TypeId::Float16, CmpKind::NLG);
    break;
  case Vop3::Op::V3_CMP_NGT_F16:
    cmpOp(TypeId::Float16, CmpKind::NGT);
    break;
  case Vop3::Op::V3_CMP_NLE_F16:
    cmpOp(TypeId::Float16, CmpKind::NLE);
    break;
  case Vop3::Op::V3_CMP_NEQ_F16:
    cmpOp(TypeId::Float16, CmpKind::NEQ);
    break;
  case Vop3::Op::V3_CMP_NLT_F16:
    cmpOp(TypeId::Float16, CmpKind::NLT);
    break;
  case Vop3::Op::V3_CMP_TRU_F16:
    cmpOp(TypeId::Float16, CmpKind::TRU);
    break;
  case Vop3::Op::V3_CMPX_F_U64:
    cmpOp(TypeId::UInt64, CmpKind::F, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LT_U64:
    cmpOp(TypeId::UInt64, CmpKind::LT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_EQ_U64:
    cmpOp(TypeId::UInt64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_LE_U64:
    cmpOp(TypeId::UInt64, CmpKind::LE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GT_U64:
    cmpOp(TypeId::UInt64, CmpKind::GT, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_NE_U64:
    cmpOp(TypeId::UInt64, CmpKind::NE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_GE_U64:
    cmpOp(TypeId::UInt64, CmpKind::GE, CmpFlags::X);
    break;
  case Vop3::Op::V3_CMPX_T_U64:
    cmpOp(TypeId::UInt64, CmpKind::T, CmpFlags::X);
    break;

  case Vop3::Op::V3_RCP_F32: {
    auto src = getSrc(0, TypeId::Float32);
    auto floatT = fragment.context->getFloat32Type();
    auto float1 = fragment.context->getFloat32(1);
    auto resultValue = fragment.builder.createFDiv(
        floatT, float1, spirv::cast<spirv::FloatValue>(src.value));
    auto result = applyClamp(applyOmod({floatT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }

  case Vop3::Op::V3_ADD_I32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getScalarOperand(inst.src1, TypeId::UInt32).value;
    auto uintT = fragment.context->getType(TypeId::UInt32);
    auto resultStruct =
        fragment.context->getStructType(std::array{uintT, uintT});
    auto result = fragment.builder.createIAddCarry(resultStruct, src0, src1);
    fragment.setVectorOperand(
        inst.vdst,
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(0)})});
    fragment.setScalarOperand(
        inst.sdst,
        {uintT, fragment.builder.createCompositeExtract(
                    uintT, result, std::array{static_cast<std::uint32_t>(1)})});
    // TODO: update sdst + 1
    break;
  }

  case Vop3::Op::V3_SUB_F32: {
    auto floatT = fragment.context->getFloat32Type();
    auto src0 = getSrc(0, TypeId::Float32);
    auto src1 = getSrc(1, TypeId::Float32);
    auto resultValue = fragment.builder.createFSub(
        floatT, spirv::cast<spirv::FloatValue>(src0.value),
        spirv::cast<spirv::FloatValue>(src1.value));
    auto result = applyClamp(applyOmod({floatT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }

  case Vop3::Op::V3_MUL_F32: {
    auto floatT = fragment.context->getFloat32Type();
    auto src0 = getSrc(0, TypeId::Float32);
    auto src1 = getSrc(1, TypeId::Float32);
    auto resultValue = fragment.builder.createFMul(
        floatT, spirv::cast<spirv::FloatValue>(src0.value),
        spirv::cast<spirv::FloatValue>(src1.value));
    auto result = applyClamp(applyOmod({floatT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }
  case Vop3::Op::V3_MUL_LO_I32: {
    auto resultT = fragment.context->getSint32Type();
    auto src0 = getSrc(0, TypeId::SInt32);
    auto src1 = getSrc(1, TypeId::SInt32);
    auto resultValue = fragment.builder.createIMul(
        resultT, spirv::cast<spirv::SIntValue>(src0.value),
        spirv::cast<spirv::SIntValue>(src1.value));
    auto result = applyClamp(applyOmod({resultT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }
  case Vop3::Op::V3_MUL_HI_I32: {
    auto resultT = fragment.context->getSint32Type();
    auto src0 = getSrc(0, TypeId::SInt32);
    auto src1 = getSrc(1, TypeId::SInt32);

    auto sint64T = fragment.context->getSint64Type();

    auto src0_64 = fragment.builder.createSConvert(
        sint64T, spirv::cast<spirv::SIntValue>(src0.value));
    auto src1_64 = fragment.builder.createSConvert(
        sint64T, spirv::cast<spirv::SIntValue>(src1.value));

    auto resultValue64 = fragment.builder.createIMul(
        sint64T, spirv::cast<spirv::SIntValue>(src0_64),
        spirv::cast<spirv::SIntValue>(src1_64));

    resultValue64 = fragment.builder.createShiftRightLogical(
        sint64T, resultValue64, fragment.context->getUInt32(32));
    auto resultValue = fragment.builder.createSConvert(resultT, resultValue64);
    auto result = applyClamp(applyOmod({resultT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }
  case Vop3::Op::V3_MUL_HI_U32: {
    auto resultT = fragment.context->getUInt32Type();
    auto src0 = spirv::cast<spirv::UIntValue>(getSrc(0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(getSrc(1, TypeId::UInt32).value);

    auto uint64T = fragment.context->getUInt64Type();

    auto src0_64 = fragment.builder.createUConvert(uint64T, src0);
    auto src1_64 = fragment.builder.createUConvert(uint64T, src1);

    auto resultValue64 = fragment.builder.createIMul(uint64T, src0_64, src1_64);

    resultValue64 = fragment.builder.createShiftRightLogical(
        uint64T, resultValue64, fragment.context->getUInt32(32));
    auto resultValue = fragment.builder.createUConvert(resultT, resultValue64);
    auto result = applyClamp(applyOmod({resultT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }

  case Vop3::Op::V3_MAC_F32: {
    auto floatT = fragment.context->getFloat32Type();
    auto src0 = getSrc(0, TypeId::Float32);
    auto src1 = getSrc(1, TypeId::Float32);

    auto dst = spirv::cast<spirv::FloatValue>( // FIXME: should use src2?
        fragment.getVectorOperand(inst.vdst, TypeId::Float32).value);

    auto resultValue = fragment.builder.createFAdd(
        floatT,
        fragment.builder.createFMul(floatT,
                                    spirv::cast<spirv::FloatValue>(src0.value),
                                    spirv::cast<spirv::FloatValue>(src1.value)),
        dst);

    auto result = applyClamp(applyOmod({floatT, resultValue}));

    fragment.setVectorOperand(inst.vdst, result);
    break;
  }
  case Vop3::Op::V3_MAD_U32_U24: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src1, TypeId::UInt32).value);
    auto src2 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src2, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt32Type();

    src0 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src0, fragment.context->getUInt32((1 << 24) - 1)));
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32((1 << 24) - 1)));

    auto result = fragment.builder.createIAdd(
        operandT, fragment.builder.createIMul(operandT, src0, src1), src2);

    fragment.setVectorOperand(inst.vdst, {operandT, result});
    break;
  }
  case Vop3::Op::V3_MAD_I32_I24: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src1, TypeId::SInt32).value);
    auto src2 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src2, TypeId::SInt32).value);
    auto operandT = fragment.context->getSint32Type();

    src0 = fragment.builder.createShiftLeftLogical(
        operandT, src0, fragment.context->getUInt32(8));
    src0 = fragment.builder.createShiftRightArithmetic(
        operandT, src0, fragment.context->getUInt32(8));
    src1 = fragment.builder.createShiftLeftLogical(
        operandT, src1, fragment.context->getUInt32(8));
    src1 = fragment.builder.createShiftRightArithmetic(
        operandT, src1, fragment.context->getUInt32(8));

    auto result = fragment.builder.createIAdd(
        operandT, fragment.builder.createIMul(operandT, src0, src1), src2);

    fragment.setVectorOperand(inst.vdst, {operandT, result});
    break;
  }
  case Vop3::Op::V3_MUL_U32_U24: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src1, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt32Type();

    src0 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src0, fragment.context->getUInt32((1 << 24) - 1)));
    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32((1 << 24) - 1)));

    auto result = fragment.builder.createIMul(operandT, src0, src1);

    fragment.setVectorOperand(inst.vdst, {operandT, result});
    break;
  }
  case Vop3::Op::V3_MUL_I32_I24: {
    auto src0 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto src1 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src1, TypeId::SInt32).value);
    auto src2 = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src2, TypeId::SInt32).value);
    auto operandT = fragment.context->getSint32Type();

    src0 = fragment.builder.createShiftLeftLogical(
        operandT, src0, fragment.context->getUInt32(8));
    src0 = fragment.builder.createShiftRightArithmetic(
        operandT, src0, fragment.context->getUInt32(8));
    src1 = fragment.builder.createShiftLeftLogical(
        operandT, src1, fragment.context->getUInt32(8));
    src1 = fragment.builder.createShiftRightArithmetic(
        operandT, src1, fragment.context->getUInt32(8));

    auto result = fragment.builder.createIMul(operandT, src0, src1);

    fragment.setVectorOperand(inst.vdst, {operandT, result});
    break;
  }
  case Vop3::Op::V3_MAD_F32: {
    auto src0 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto src1 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src1, TypeId::Float32).value);
    auto src2 = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src2, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto result = fragment.builder.createFAdd(
        floatT, fragment.builder.createFMul(floatT, src0, src1), src2);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }
  case Vop3::Op::V3_CNDMASK_B32: {
    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto src1 = fragment.getScalarOperand(inst.src1, TypeId::UInt32).value;
    auto src2 = fragment.getScalarOperand(inst.src2, TypeId::UInt32).value;

    auto cmp = fragment.builder.createINotEqual(
        fragment.context->getBoolType(), src2, fragment.context->getUInt32(0));

    auto uint32T = fragment.context->getUInt32Type();
    auto result = fragment.builder.createSelect(uint32T, cmp, src1, src0);
    fragment.setVectorOperand(inst.vdst, {uint32T, result});
    break;
  }

  case Vop3::Op::V3_BFE_U32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src1, TypeId::UInt32).value);
    auto src2 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.src2, TypeId::UInt32).value);

    auto operandT = fragment.context->getUInt32Type();

    auto voffset =
        spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
            operandT, src1, fragment.context->getUInt32(0x1f)));
    auto vsize =
        spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
            operandT, src2, fragment.context->getUInt32(0x1f)));
    auto field =
        fragment.builder.createShiftRightLogical(operandT, src0, voffset);
    auto mask = fragment.builder.createISub(
        operandT,
        fragment.builder.createShiftLeftLogical(
            operandT, fragment.context->getUInt32(1), vsize),
        fragment.context->getUInt32(1));

    auto resultT = fragment.context->getUInt32Type();
    auto result = fragment.builder.createSelect(
        operandT,
        fragment.builder.createIEqual(fragment.context->getBoolType(), vsize,
                                      fragment.context->getUInt32(0)),
        fragment.context->getUInt32(0),
        fragment.builder.createBitwiseAnd(operandT, field, mask));
    fragment.setVectorOperand(inst.vdst, {resultT, result});
    break;
  }

  case Vop3::Op::V3_CVT_PKRTZ_F16_F32: {
    auto float2T = fragment.context->getType(TypeId::Float32x2);
    auto uintT = fragment.context->getType(TypeId::UInt32);
    auto glslStd450 = fragment.context->getGlslStd450();

    auto src0 = fragment.getScalarOperand(inst.src0, TypeId::Float32).value;
    auto src1 = fragment.getScalarOperand(inst.src1, TypeId::Float32).value;

    auto src = fragment.builder.createCompositeConstruct(
        float2T, std::array{src0, src1});
    auto dst = fragment.builder.createExtInst(
        uintT, glslStd450, GLSLstd450PackHalf2x16, std::array{src});

    fragment.setVectorOperand(inst.vdst, {uintT, dst});
    break;
  }

  case Vop3::Op::V3_SAD_U32: {
    auto src0 = spirv::cast<spirv::UIntValue>(getSrc(0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(getSrc(1, TypeId::UInt32).value);
    auto src2 = spirv::cast<spirv::UIntValue>(getSrc(2, TypeId::UInt32).value);

    auto uint32T = fragment.context->getUInt32Type();
    auto sint32T = fragment.context->getSint32Type();

    auto diff = fragment.builder.createISub(uint32T, src0, src1);
    auto sdiff = fragment.builder.createBitcast(sint32T, diff);

    auto glslStd450 = fragment.context->getGlslStd450();
    auto sabsdiff = fragment.builder.createExtInst(sint32T, glslStd450,
                                                   GLSLstd450SAbs, {{sdiff}});

    auto absdiff = fragment.builder.createBitcast(uint32T, sabsdiff);
    auto result = fragment.builder.createIAdd(uint32T, absdiff, src2);
    fragment.setVectorOperand(inst.vdst, {uint32T, result});
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertMubuf(Fragment &fragment, Mubuf inst) {
  fragment.registers->pc += Mubuf::kMinInstSize * sizeof(std::uint32_t);
  /*
  printMubufOpcode(op);
  printf(" ");
  printVectorOperand(vdata, inst + instSize);
  printf(", ");
  printScalarOperand(srsrc << 2, inst + instSize);
  printf(", ");
  printScalarOperand(soffset, inst + instSize);
  */

  auto getSOffset = [&](std::int32_t adv = 0) -> spirv::UIntValue {
    auto resultT = fragment.context->getUInt32Type();
    auto resultV =
        fragment.getScalarOperand(inst.soffset, TypeId::UInt32).value;
    auto result = spirv::cast<spirv::UIntValue>(resultV);

    if (adv != 0) {
      if (auto constVal = fragment.context->findSint32Value(result)) {
        return fragment.context->getUInt32(*constVal + adv);
      }

      result = fragment.builder.createIAdd(resultT, result,
                                           fragment.context->getUInt32(adv));
    }

    return result;
  };

  auto getVBuffer = [&] {
    auto vBuffer0 =
        fragment.getScalarOperand((inst.srsrc << 2) + 0, TypeId::UInt32);
    auto vBuffer1 =
        fragment.getScalarOperand((inst.srsrc << 2) + 1, TypeId::UInt32);
    auto vBuffer2 =
        fragment.getScalarOperand((inst.srsrc << 2) + 2, TypeId::UInt32);
    auto vBuffer3 =
        fragment.getScalarOperand((inst.srsrc << 2) + 3, TypeId::UInt32);

    auto optVBuffer0Value = fragment.context->findUint32Value(vBuffer0.value);
    auto optVBuffer1Value = fragment.context->findUint32Value(vBuffer1.value);
    auto optVBuffer2Value = fragment.context->findUint32Value(vBuffer2.value);
    auto optVBuffer3Value = fragment.context->findUint32Value(vBuffer3.value);

    if (optVBuffer0Value && optVBuffer1Value && optVBuffer2Value &&
        optVBuffer3Value) {
      // V# buffer value is known, read the buffer now
      std::array<std::uint32_t, 4> vBufferData = {
          *optVBuffer0Value, *optVBuffer1Value, *optVBuffer2Value,
          *optVBuffer3Value};

      GnmVBuffer result;
      std::memcpy(&result, vBufferData.data(), sizeof(result));
      return result;
    }

    util::unreachable();
  };

  auto getAddress = [&](GnmVBuffer *vbuffer) {
    auto &builder = fragment.builder;
    auto uint32T = fragment.context->getUInt32Type();

    spirv::UIntValue index;
    if (inst.idxen) {
      index = spirv::cast<spirv::UIntValue>(
          fragment.getVectorOperand(inst.vaddr, TypeId::UInt32).value);
    }

    // std::printf("vBuffer address = %lx\n", vbuffer->getAddress());

    if (vbuffer->addtid_en) {
      spirv::UIntValue threadId =
          builder.createLoad(uint32T, fragment.context->getThreadId());

      if (index) {
        index = builder.createIAdd(uint32T, index, threadId);
      } else {
        index = threadId;
      }
    }

    auto offset = inst.offset ? fragment.context->getUInt32(inst.offset)
                              : spirv::UIntValue{};

    if (inst.offen) {
      auto off = spirv::cast<spirv::UIntValue>(
          fragment
              .getVectorOperand(inst.vaddr + (inst.idxen ? 1 : 0),
                                TypeId::UInt32)
              .value);

      if (offset) {
        offset = builder.createIAdd(uint32T, off, offset);
      } else {
        offset = off;
      }
    }

    spirv::UIntValue address = getSOffset();

    if (vbuffer->swizzle_en == 0) {
      if (vbuffer->stride == 0 || !index) {
        return address;
      }

      auto offset = builder.createIMul(
          uint32T, index, fragment.context->getUInt32(vbuffer->stride));
      if (address == fragment.context->getUInt32(0)) {
        return offset;
      }

      return builder.createIAdd(uint32T, address, offset);
    }

    if (!index && !offset) {
      return address;
    }

    if (index && offset) {
      auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
      auto index_msb = builder.createUDiv(uint32T, index, indexStride);
      auto index_lsb = builder.createUMod(uint32T, index, indexStride);

      auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
      auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
      auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

      auto indexMsb = builder.createIMul(
          uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));
      auto offsetMsb = builder.createIMul(
          uint32T, offset_msb,
          fragment.context->getUInt32(vbuffer->element_size));

      address = builder.createIAdd(
          uint32T, address,
          builder.createIMul(uint32T,
                             builder.createIAdd(uint32T, indexMsb, offsetMsb),
                             indexStride));

      address = builder.createIAdd(
          uint32T, address,
          builder.createIMul(uint32T, index_lsb, elementSize));

      return builder.createIAdd(uint32T, address, offset_lsb);
    }

    if (index) {
      auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
      auto index_msb = builder.createUDiv(uint32T, index, indexStride);
      auto index_lsb = builder.createUMod(uint32T, index, indexStride);

      auto indexMsb = builder.createIMul(
          uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));

      return builder.createIAdd(
          uint32T, address, builder.createIMul(uint32T, indexMsb, indexStride));
    }

    if (!offset) {
      util::unreachable();
    }

    auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
    auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
    auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
    auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

    auto offsetMsb =
        builder.createIMul(uint32T, offset_msb,
                           fragment.context->getUInt32(vbuffer->element_size));

    address = builder.createIAdd(
        uint32T, address, builder.createIMul(uint32T, offsetMsb, indexStride));

    return builder.createIAdd(uint32T, address, offset_lsb);
  };

  switch (inst.op) {
  case Mubuf::Op::BUFFER_LOAD_FORMAT_X:
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XY:
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XYZ:
  case Mubuf::Op::BUFFER_LOAD_FORMAT_XYZW: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mubuf::Op::BUFFER_LOAD_FORMAT_X) + 1;

    auto vbuffer = getVBuffer();
    auto address = getAddress(&vbuffer);

    spirv::Value result[4];
    auto resultType = convertFromFormat(
        result, count, fragment, reinterpret_cast<std::uint32_t *>(&vbuffer),
        address, vbuffer.dfmt, vbuffer.nfmt);

    for (std::uint32_t i = 0; i < count; ++i) {
      fragment.setVectorOperand(inst.vdata + i, {resultType, result[i]});
    }
    break;
  }

  case Mubuf::Op::BUFFER_STORE_FORMAT_X:
  case Mubuf::Op::BUFFER_STORE_FORMAT_XY:
  case Mubuf::Op::BUFFER_STORE_FORMAT_XYZ:
  case Mubuf::Op::BUFFER_STORE_FORMAT_XYZW: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mubuf::Op::BUFFER_STORE_FORMAT_X) +
                          1;

    auto vbuffer = getVBuffer();
    auto address = getAddress(&vbuffer);

    convertToFormat(RegisterId::Vector(inst.vdata), count, fragment,
                    reinterpret_cast<std::uint32_t *>(&vbuffer), address,
                    vbuffer.dfmt, vbuffer.nfmt);
    break;
  }

  case Mubuf::Op::BUFFER_LOAD_UBYTE:
  case Mubuf::Op::BUFFER_LOAD_USHORT:
  case Mubuf::Op::BUFFER_LOAD_SSHORT:
  case Mubuf::Op::BUFFER_LOAD_SBYTE:
    inst.dump();
    util::unreachable();

  case Mubuf::Op::BUFFER_LOAD_DWORD:
  case Mubuf::Op::BUFFER_LOAD_DWORDX2:
  case Mubuf::Op::BUFFER_LOAD_DWORDX4:
  case Mubuf::Op::BUFFER_LOAD_DWORDX3: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mubuf::Op::BUFFER_LOAD_DWORD) + 1;

    auto vbuffer = getVBuffer();
    auto address = getAddress(&vbuffer);
    auto loadType = fragment.context->getType(TypeId::UInt32);
    auto uniform = fragment.context->getOrCreateStorageBuffer(
        reinterpret_cast<std::uint32_t *>(&vbuffer), TypeId::UInt32);
    uniform->accessOp |= AccessOp::Load;

    auto uniformPointerType = fragment.context->getPointerType(
        spv::StorageClass::StorageBuffer, TypeId::UInt32);
    address =
        fragment.builder.createUDiv(fragment.context->getUInt32Type(), address,
                                    fragment.context->getUInt32(4));

    for (int i = 0; i < count; ++i) {
      auto channelOffset = address;

      if (i != 0) {
        channelOffset = fragment.builder.createIAdd(
            fragment.context->getUInt32Type(), channelOffset,
            fragment.context->getUInt32(i));
      }

      auto uniformPointerValue = fragment.builder.createAccessChain(
          uniformPointerType, uniform->variable,
          {{fragment.context->getUInt32(0), channelOffset}});

      auto value = fragment.builder.createLoad(loadType, uniformPointerValue);
      fragment.setVectorOperand(inst.vdata + i, {loadType, value});
    }
    break;
  }

  case Mubuf::Op::BUFFER_STORE_BYTE:
  case Mubuf::Op::BUFFER_STORE_SHORT:
    inst.dump();
    util::unreachable();

  case Mubuf::Op::BUFFER_STORE_DWORD:
  case Mubuf::Op::BUFFER_STORE_DWORDX2:
  case Mubuf::Op::BUFFER_STORE_DWORDX4:
  case Mubuf::Op::BUFFER_STORE_DWORDX3: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mubuf::Op::BUFFER_STORE_DWORD) + 1;

    auto vbuffer = getVBuffer();
    auto address = getAddress(&vbuffer);
    auto storeType = fragment.context->getType(TypeId::UInt32);
    auto uniform = fragment.context->getOrCreateStorageBuffer(
        reinterpret_cast<std::uint32_t *>(&vbuffer), TypeId::UInt32);
    uniform->accessOp |= AccessOp::Store;

    auto uniformPointerType = fragment.context->getPointerType(
        spv::StorageClass::StorageBuffer, TypeId::UInt32);
    address =
        fragment.builder.createUDiv(fragment.context->getUInt32Type(), address,
                                    fragment.context->getUInt32(4));

    for (int i = 0; i < count; ++i) {
      auto channelOffset = address;

      if (i != 0) {
        channelOffset = fragment.builder.createIAdd(
            fragment.context->getUInt32Type(), channelOffset,
            fragment.context->getUInt32(i));
      }

      auto uniformPointerValue = fragment.builder.createAccessChain(
          uniformPointerType, uniform->variable,
          {{fragment.context->getUInt32(0), channelOffset}});

      fragment.builder.createStore(
          uniformPointerValue,
          fragment.getVectorOperand(inst.vdata + i, TypeId::UInt32).value);
    }
  }

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertMtbuf(Fragment &fragment, Mtbuf inst) {
  fragment.registers->pc += Mtbuf::kMinInstSize * sizeof(std::uint32_t);

  switch (inst.op) {
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_X:
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XY:
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XYZ:
  case Mtbuf::Op::TBUFFER_LOAD_FORMAT_XYZW: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mtbuf::Op::TBUFFER_LOAD_FORMAT_X) +
                          1;

    auto &builder = fragment.builder;

    auto vBuffer0 =
        fragment.getScalarOperand((inst.srsrc << 2) + 0, TypeId::UInt32);
    auto vBuffer1 =
        fragment.getScalarOperand((inst.srsrc << 2) + 1, TypeId::UInt32);
    auto vBuffer2 =
        fragment.getScalarOperand((inst.srsrc << 2) + 2, TypeId::UInt32);
    auto vBuffer3 =
        fragment.getScalarOperand((inst.srsrc << 2) + 3, TypeId::UInt32);

    auto optVBuffer0Value = fragment.context->findUint32Value(vBuffer0.value);
    auto optVBuffer1Value = fragment.context->findUint32Value(vBuffer1.value);
    auto optVBuffer2Value = fragment.context->findUint32Value(vBuffer2.value);
    auto optVBuffer3Value = fragment.context->findUint32Value(vBuffer3.value);

    if (optVBuffer0Value && optVBuffer1Value && optVBuffer2Value &&
        optVBuffer3Value) {
      // V# buffer value is known, read the buffer now
      std::uint32_t vBufferData[] = {*optVBuffer0Value, *optVBuffer1Value,
                                     *optVBuffer2Value, *optVBuffer3Value};

      auto vbuffer = reinterpret_cast<GnmVBuffer *>(vBufferData);
      auto base = spirv::cast<spirv::UIntValue>(
          fragment.getScalarOperand(inst.soffset, TypeId::UInt32).value);

      auto uint32T = fragment.context->getUInt32Type();
      auto uint32_0 = fragment.context->getUInt32(0);

      if (inst.dfmt == kSurfaceFormatInvalid) {
        util::unreachable("!! dfmt is invalid !!\n");

        for (std::uint32_t i = 0; i < count; ++i) {
          fragment.setVectorOperand(inst.vdata + i, {uint32T, uint32_0});
        }

        return;
      }

      spirv::UIntValue index;
      if (inst.idxen) {
        index = spirv::cast<spirv::UIntValue>(
            fragment.getVectorOperand(inst.vaddr, TypeId::UInt32).value);
      }

      // std::printf("vBuffer address = %lx\n", vbuffer->getAddress());

      if (vbuffer->addtid_en) {
        spirv::UIntValue threadId =
            builder.createLoad(uint32T, fragment.context->getThreadId());

        if (index) {
          index = builder.createIAdd(uint32T, index, threadId);
        } else {
          index = threadId;
        }
      }

      auto offset = inst.offset ? fragment.context->getUInt32(inst.offset)
                                : spirv::UIntValue{};

      if (inst.offen) {
        auto off = spirv::cast<spirv::UIntValue>(
            fragment
                .getVectorOperand(inst.vaddr + (inst.idxen ? 1 : 0),
                                  TypeId::UInt32)
                .value);

        if (offset) {
          offset = builder.createIAdd(uint32T, off, offset);
        } else {
          offset = off;
        }
      }

      spirv::UIntValue address = base;
      if (vbuffer->swizzle_en == 0) {
        if (vbuffer->stride != 0 && index) {
          auto offset = builder.createIMul(
              uint32T, index, fragment.context->getUInt32(vbuffer->stride));
          if (address == uint32_0) {
            address = offset;
          } else {
            address = builder.createIAdd(uint32T, address, offset);
          }
        }
      } else {
        if (index && offset) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto index_msb = builder.createUDiv(uint32T, index, indexStride);
          auto index_lsb = builder.createUMod(uint32T, index, indexStride);

          auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
          auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
          auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

          auto indexMsb = builder.createIMul(
              uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));
          auto offsetMsb = builder.createIMul(
              uint32T, offset_msb,
              fragment.context->getUInt32(vbuffer->element_size));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(
                  uint32T, builder.createIAdd(uint32T, indexMsb, offsetMsb),
                  indexStride));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, index_lsb, elementSize));

          address = builder.createIAdd(uint32T, address, offset_lsb);
        } else if (index) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto index_msb = builder.createUDiv(uint32T, index, indexStride);
          auto index_lsb = builder.createUMod(uint32T, index, indexStride);

          auto indexMsb = builder.createIMul(
              uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, indexMsb, indexStride));
        } else if (offset) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
          auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
          auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

          auto offsetMsb = builder.createIMul(
              uint32T, offset_msb,
              fragment.context->getUInt32(vbuffer->element_size));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, offsetMsb, indexStride));

          address = builder.createIAdd(uint32T, address, offset_lsb);
        }
      }

      spirv::Value result[4];
      auto resultType = convertFromFormat(result, count, fragment, vBufferData,
                                          address, inst.dfmt, inst.nfmt);

      for (std::uint32_t i = 0; i < count; ++i) {
        fragment.setVectorOperand(inst.vdata + i, {resultType, result[i]});
      }
      break;
    } else {
      util::unreachable();
    }
  }

  case Mtbuf::Op::TBUFFER_STORE_FORMAT_X:
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XY:
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XYZ:
  case Mtbuf::Op::TBUFFER_STORE_FORMAT_XYZW: {
    std::uint32_t count = static_cast<int>(inst.op) -
                          static_cast<int>(Mtbuf::Op::TBUFFER_STORE_FORMAT_X) +
                          1;
    auto &builder = fragment.builder;

    auto vBuffer0 =
        fragment.getScalarOperand((inst.srsrc << 2) + 0, TypeId::UInt32);
    auto vBuffer1 =
        fragment.getScalarOperand((inst.srsrc << 2) + 1, TypeId::UInt32);
    auto vBuffer2 =
        fragment.getScalarOperand((inst.srsrc << 2) + 2, TypeId::UInt32);
    auto vBuffer3 =
        fragment.getScalarOperand((inst.srsrc << 2) + 3, TypeId::UInt32);

    auto optVBuffer0Value = fragment.context->findUint32Value(vBuffer0.value);
    auto optVBuffer1Value = fragment.context->findUint32Value(vBuffer1.value);
    auto optVBuffer2Value = fragment.context->findUint32Value(vBuffer2.value);
    auto optVBuffer3Value = fragment.context->findUint32Value(vBuffer3.value);

    if (optVBuffer0Value && optVBuffer1Value && optVBuffer2Value &&
        optVBuffer3Value) {
      // V# buffer value is known, read the buffer now
      std::uint32_t vBufferData[] = {*optVBuffer0Value, *optVBuffer1Value,
                                     *optVBuffer2Value, *optVBuffer3Value};

      auto vbuffer = reinterpret_cast<GnmVBuffer *>(vBufferData);
      // std::printf("vBuffer address = %lx\n", vbuffer->getAddress());

      auto base = spirv::cast<spirv::UIntValue>(
          fragment.getScalarOperand(inst.soffset, TypeId::UInt32).value);

      auto uint32T = fragment.context->getUInt32Type();
      auto uint32_0 = fragment.context->getUInt32(0);

      if (inst.dfmt == kSurfaceFormatInvalid) {
        util::unreachable("!! dfmt is invalid !!\n");

        for (std::uint32_t i = 0; i < count; ++i) {
          fragment.setVectorOperand(inst.vdata + i, {uint32T, uint32_0});
        }

        return;
      }

      spirv::UIntValue index;
      if (inst.idxen) {
        index = spirv::cast<spirv::UIntValue>(
            fragment.getVectorOperand(inst.vaddr, TypeId::UInt32).value);
      }

      if (vbuffer->addtid_en) {
        spirv::UIntValue threadId =
            builder.createLoad(uint32T, fragment.context->getThreadId());

        if (index) {
          index = builder.createIAdd(uint32T, index, threadId);
        } else {
          index = threadId;
        }
      }

      auto offset = inst.offset ? fragment.context->getUInt32(inst.offset)
                                : spirv::UIntValue{};

      if (inst.offen) {
        auto off = spirv::cast<spirv::UIntValue>(
            fragment
                .getVectorOperand(inst.vaddr + (inst.idxen ? 1 : 0),
                                  TypeId::UInt32)
                .value);

        if (offset) {
          offset = builder.createIAdd(uint32T, off, offset);
        } else {
          offset = off;
        }
      }

      spirv::UIntValue address = base;
      if (vbuffer->swizzle_en == 0) {
        if (vbuffer->stride != 0 && index) {
          auto offset = builder.createIMul(
              uint32T, index, fragment.context->getUInt32(vbuffer->stride));
          if (address == uint32_0) {
            address = offset;
          } else {
            address = builder.createIAdd(uint32T, address, offset);
          }
        }
      } else {
        if (index && offset) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto index_msb = builder.createUDiv(uint32T, index, indexStride);
          auto index_lsb = builder.createUMod(uint32T, index, indexStride);

          auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
          auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
          auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

          auto indexMsb = builder.createIMul(
              uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));
          auto offsetMsb = builder.createIMul(
              uint32T, offset_msb,
              fragment.context->getUInt32(vbuffer->element_size));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(
                  uint32T, builder.createIAdd(uint32T, indexMsb, offsetMsb),
                  indexStride));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, index_lsb, elementSize));

          address = builder.createIAdd(uint32T, address, offset_lsb);
        } else if (index) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto index_msb = builder.createUDiv(uint32T, index, indexStride);
          auto index_lsb = builder.createUMod(uint32T, index, indexStride);

          auto indexMsb = builder.createIMul(
              uint32T, index_msb, fragment.context->getUInt32(vbuffer->stride));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, indexMsb, indexStride));
        } else if (offset) {
          auto indexStride = fragment.context->getUInt32(vbuffer->index_stride);
          auto elementSize = fragment.context->getUInt32(vbuffer->element_size);
          auto offset_msb = builder.createUDiv(uint32T, offset, elementSize);
          auto offset_lsb = builder.createUMod(uint32T, offset, elementSize);

          auto offsetMsb = builder.createIMul(
              uint32T, offset_msb,
              fragment.context->getUInt32(vbuffer->element_size));

          address = builder.createIAdd(
              uint32T, address,
              builder.createIMul(uint32T, offsetMsb, indexStride));

          address = builder.createIAdd(uint32T, address, offset_lsb);
        }
      }

      convertToFormat(RegisterId::Vector(inst.vdata), count, fragment,
                      vBufferData, address, inst.dfmt, inst.nfmt);
    } else {
      util::unreachable();
    }
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertMimg(Fragment &fragment, Mimg inst) {
  fragment.registers->pc += Mimg::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Mimg::Op::IMAGE_GET_RESINFO: {
    auto image = fragment.createImage(RegisterId::Raw(inst.srsrc << 2), inst.r128);
    spirv::Value values[4];
    auto uint32T = fragment.context->getUInt32Type();

    if (inst.dmask & 3) {
      // query whd
      // TODO: support other than 2D textures
      auto uint32x2T = fragment.context->getUint32x2Type();
      auto lod = fragment.getScalarOperand(inst.vaddr, TypeId::UInt32);
      auto sizeResult =
          fragment.builder.createImageQuerySizeLod(uint32x2T, image, lod.value);

      values[0] =
          fragment.builder.createCompositeExtract(uint32T, sizeResult, {{0}});
      values[1] =
          fragment.builder.createCompositeExtract(uint32T, sizeResult, {{1}});
      values[2] = fragment.context->getUInt32(1);
    }

    if (inst.dmask & (1 << 3)) {
      // query total mip count
      values[3] = fragment.builder.createImageQueryLevels(uint32T, image);
    }

    for (std::size_t dstOffset = 0, i = 0; i < 4; ++i) {
      if (inst.dmask & (1 << i)) {
        fragment.setVectorOperand(inst.vdata + dstOffset++, {uint32T, values[i]});
      }
    }
    break;
  }
  case Mimg::Op::IMAGE_SAMPLE: {
    auto image = fragment.createImage(RegisterId::Raw(inst.srsrc << 2), inst.r128);
    auto sampler = fragment.createSampler(RegisterId::Raw(inst.ssamp << 2));
    auto coord0 = fragment.getVectorOperand(inst.vaddr, TypeId::Float32).value;
    auto coord1 =
        fragment.getVectorOperand(inst.vaddr + 1, TypeId::Float32).value;
    auto coord2 =
        fragment.getVectorOperand(inst.vaddr + 2, TypeId::Float32).value;
    auto coords = fragment.builder.createCompositeConstruct(
        fragment.context->getFloat32x3Type(),
        {{coord0, coord1, coord2}}); // TODO

    auto sampledImage2dT = fragment.context->getSampledImage2DType();
    auto float4T = fragment.context->getFloat32x4Type();
    auto floatT = fragment.context->getFloat32Type();
    auto sampledImage =
        fragment.builder.createSampledImage(sampledImage2dT, image, sampler);
    auto value = fragment.builder.createImageSampleImplicitLod(
        float4T, sampledImage, coords);

    for (std::uint32_t dstOffset = 0, i = 0; i < 4; ++i) {
      if (inst.dmask & (1 << i)) {
        fragment.setVectorOperand(
            inst.vdata + dstOffset++, {floatT, fragment.builder.createCompositeExtract(
                                         floatT, value, {{i}})});
      }
    }
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertDs(Fragment &fragment, Ds inst) {
  fragment.registers->pc += Ds::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertVintrp(Fragment &fragment, Vintrp inst) {
  fragment.registers->pc += Vintrp::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Vintrp::Op::V_INTERP_P1_F32:
    // TODO: operation should read from LDS
    // TODO: accurate emulation

    // In current inaccurate emulation we just ignore phase 1 and vsrc argument
    // interpolated value stored in attr#
    break;

  case Vintrp::Op::V_INTERP_P2_F32: {
    // TODO: operation should read from LDS
    // TODO: accurate emulation

    auto attr = fragment.getAttrOperand(inst.attr, TypeId::Float32x4);
    auto channelType = fragment.context->getType(TypeId::Float32);
    auto attrChan = fragment.builder.createCompositeExtract(
        channelType, attr.value,
        std::array{static_cast<std::uint32_t>(inst.attrChan)});
    fragment.setVectorOperand(inst.vdst, {channelType, attrChan});
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertExp(Fragment &fragment, Exp inst) {
  fragment.registers->pc += Exp::kMinInstSize * sizeof(std::uint32_t);

  if (inst.en == 0) {
    fragment.builder.createFunctionCall(fragment.context->getVoidType(),
                                        fragment.context->getDiscardFn(), {});
    return;
  }

  // spirv::Value value;
  std::array<spirv::Value, 4> exports;

  // TODO: handle vm
  if (inst.compr) {
    auto floatT = fragment.context->getType(TypeId::Float32);
    auto float2T = fragment.context->getType(TypeId::Float32x2);
    auto glslStd450 = fragment.context->getGlslStd450();

    auto xyUint = fragment.getVectorOperand(inst.vsrc0, TypeId::UInt32).value;
    auto zwUint = fragment.getVectorOperand(inst.vsrc1, TypeId::UInt32).value;

    auto xy = fragment.builder.createExtInst(
        float2T, glslStd450, GLSLstd450UnpackHalf2x16, std::array{xyUint});
    auto zw = fragment.builder.createExtInst(
        float2T, glslStd450, GLSLstd450UnpackHalf2x16, std::array{zwUint});
    exports[0] = fragment.builder.createCompositeExtract(
        floatT, xy, std::array{static_cast<std::uint32_t>(0)});
    exports[1] = fragment.builder.createCompositeExtract(
        floatT, xy, std::array{static_cast<std::uint32_t>(1)});
    exports[2] = fragment.builder.createCompositeExtract(
        floatT, zw, std::array{static_cast<std::uint32_t>(0)});
    exports[3] = fragment.builder.createCompositeExtract(
        floatT, zw, std::array{static_cast<std::uint32_t>(1)});
    // value = fragment.builder.createCompositeConstruct(type, std::array{x, y,
    // z, w});
  } else {
    exports[0] = fragment.getVectorOperand(inst.vsrc0, TypeId::Float32).value;
    exports[1] = fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value;
    exports[2] = fragment.getVectorOperand(inst.vsrc2, TypeId::Float32).value;
    exports[3] = fragment.getVectorOperand(inst.vsrc3, TypeId::Float32).value;
    /*
    value = fragment.builder.createCompositeConstruct(
        type,
        std::array{
            fragment.getVectorOperand(inst.vsrc0, TypeId::Float32).value,
            fragment.getVectorOperand(inst.vsrc1, TypeId::Float32).value,
            fragment.getVectorOperand(inst.vsrc2, TypeId::Float32).value,
            fragment.getVectorOperand(inst.vsrc3, TypeId::Float32).value});
    */
  }

  auto resultType = fragment.context->getFloat32x4Type();
  auto floatType = fragment.context->getFloat32Type();
/*
  if (inst.en != 0xf) {
    auto prevValue = fragment.getExportTarget(inst.target, TypeId::Float32x4);
    if (prevValue) {
      for (std::uint32_t i = 0; i < 4; ++i) {
        if (~inst.en & (1 << i)) {
          exports[i] = fragment.builder.createCompositeExtract(
              floatType, prevValue.value, {{i}});
        }
      }
    }
  }
*/

  auto value = fragment.builder.createCompositeConstruct(resultType, exports);
  fragment.setExportTarget(inst.target, {resultType, value});
}

void convertVop1(Fragment &fragment, Vop1 inst) {
  fragment.registers->pc += Vop1::kMinInstSize * sizeof(std::uint32_t);
  switch (inst.op) {
  case Vop1::Op::V_MOV_B32:
    fragment.setVectorOperand(
        inst.vdst, fragment.getScalarOperand(inst.src0, TypeId::UInt32,
                                             OperandGetFlags::PreserveType));
    break;

  case Vop1::Op::V_RCP_F32: {
    auto src = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto float1 = fragment.context->getFloat32(1);
    auto result = fragment.builder.createFDiv(floatT, float1, src);

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop1::Op::V_RSQ_F32: {
    auto src = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();
    auto float1 = fragment.context->getFloat32(1);

    auto glslStd450 = fragment.context->getGlslStd450();
    auto result = fragment.builder.createExtInst(
        floatT, glslStd450, GLSLstd450InverseSqrt, {{src}});

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop1::Op::V_SQRT_F32: {
    auto src = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto glslStd450 = fragment.context->getGlslStd450();
    auto result = fragment.builder.createExtInst(floatT, glslStd450,
                                                 GLSLstd450Sqrt, {{src}});

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop1::Op::V_EXP_F32: {
    auto src = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto glslStd450 = fragment.context->getGlslStd450();
    auto result = fragment.builder.createExtInst(floatT, glslStd450,
                                                 GLSLstd450Exp2, {{src}});

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }


  case Vop1::Op::V_FRACT_F32: {
    auto src = spirv::cast<spirv::FloatValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto floatT = fragment.context->getFloat32Type();

    auto glslStd450 = fragment.context->getGlslStd450();
    auto result = fragment.builder.createExtInst(floatT, glslStd450,
                                                 GLSLstd450Fract, {{src}});

    fragment.setVectorOperand(inst.vdst, {floatT, result});
    break;
  }

  case Vop1::Op::V_CVT_I32_F32: {
    auto src = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::Float32).value);
    auto resultType = fragment.context->getType(TypeId::SInt32);
    auto result = fragment.builder.createConvertFToS(resultType, src);

    fragment.setVectorOperand(inst.vdst, {resultType, result});
    break;
  }
  case Vop1::Op::V_CVT_F32_I32: {
    auto src = spirv::cast<spirv::SIntValue>(
        fragment.getScalarOperand(inst.src0, TypeId::SInt32).value);
    auto resultType = fragment.context->getType(TypeId::Float32);
    auto result = fragment.builder.createConvertSToF(resultType, src);

    fragment.setVectorOperand(inst.vdst, {resultType, result});
    break;
  }

  case Vop1::Op::V_CVT_U32_F32: {
    auto src = fragment.getScalarOperand(inst.src0, TypeId::Float32).value;
    auto resultType = fragment.context->getType(TypeId::UInt32);
    auto result = fragment.builder.createConvertFToU(resultType, src);

    fragment.setVectorOperand(inst.vdst, {resultType, result});
    break;
  }
  case Vop1::Op::V_CVT_F32_U32: {
    auto src = fragment.getScalarOperand(inst.src0, TypeId::UInt32).value;
    auto resultType = fragment.context->getFloat32Type();
    auto result = fragment.builder.createConvertUToF(
        resultType, spirv::cast<spirv::UIntValue>(src));

    fragment.setVectorOperand(inst.vdst, {resultType, result});
    break;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertVopc(Fragment &fragment, Vopc inst) {
  fragment.registers->pc += Vopc::kMinInstSize * sizeof(std::uint32_t);

  auto cmpOp = [&](TypeId type, CmpKind kind, CmpFlags flags = CmpFlags::None) {
    auto src0 = fragment.getScalarOperand(inst.src0, type).value;
    auto src1 = fragment.getVectorOperand(inst.vsrc1, type).value;

    auto result = doCmpOp(fragment, type, src0, src1, kind, flags);
    fragment.setVcc(result);
  };

  switch (inst.op) {
  case Vopc::Op::V_CMP_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG);
    break;
  case Vopc::Op::V_CMP_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O);
    break;
  case Vopc::Op::V_CMP_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U);
    break;
  case Vopc::Op::V_CMP_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE);
    break;
  case Vopc::Op::V_CMP_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG);
    break;
  case Vopc::Op::V_CMP_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT);
    break;
  case Vopc::Op::V_CMP_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE);
    break;
  case Vopc::Op::V_CMP_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ);
    break;
  case Vopc::Op::V_CMP_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT);
    break;
  case Vopc::Op::V_CMP_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU);
    break;
  case Vopc::Op::V_CMPX_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::X);
    break;
  case Vopc::Op::V_CMP_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG);
    break;
  case Vopc::Op::V_CMP_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O);
    break;
  case Vopc::Op::V_CMP_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U);
    break;
  case Vopc::Op::V_CMP_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE);
    break;
  case Vopc::Op::V_CMP_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG);
    break;
  case Vopc::Op::V_CMP_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT);
    break;
  case Vopc::Op::V_CMP_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE);
    break;
  case Vopc::Op::V_CMP_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ);
    break;
  case Vopc::Op::V_CMP_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT);
    break;
  case Vopc::Op::V_CMP_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU);
    break;
  case Vopc::Op::V_CMPX_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPS_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPSX_F_F32:
    cmpOp(TypeId::Float32, CmpKind::F, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LT_F32:
    cmpOp(TypeId::Float32, CmpKind::LT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_EQ_F32:
    cmpOp(TypeId::Float32, CmpKind::EQ, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LE_F32:
    cmpOp(TypeId::Float32, CmpKind::LE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_GT_F32:
    cmpOp(TypeId::Float32, CmpKind::GT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LG_F32:
    cmpOp(TypeId::Float32, CmpKind::LG, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_GE_F32:
    cmpOp(TypeId::Float32, CmpKind::GE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_O_F32:
    cmpOp(TypeId::Float32, CmpKind::O, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_U_F32:
    cmpOp(TypeId::Float32, CmpKind::U, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NGE_F32:
    cmpOp(TypeId::Float32, CmpKind::NGE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLG_F32:
    cmpOp(TypeId::Float32, CmpKind::NLG, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NGT_F32:
    cmpOp(TypeId::Float32, CmpKind::NGT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLE_F32:
    cmpOp(TypeId::Float32, CmpKind::NLE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NEQ_F32:
    cmpOp(TypeId::Float32, CmpKind::NEQ, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLT_F32:
    cmpOp(TypeId::Float32, CmpKind::NLT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_TRU_F32:
    cmpOp(TypeId::Float32, CmpKind::TRU, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPS_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPS_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::S);
    break;
  case Vopc::Op::V_CMPSX_F_F64:
    cmpOp(TypeId::Float64, CmpKind::F, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LT_F64:
    cmpOp(TypeId::Float64, CmpKind::LT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_EQ_F64:
    cmpOp(TypeId::Float64, CmpKind::EQ, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LE_F64:
    cmpOp(TypeId::Float64, CmpKind::LE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_GT_F64:
    cmpOp(TypeId::Float64, CmpKind::GT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_LG_F64:
    cmpOp(TypeId::Float64, CmpKind::LG, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_GE_F64:
    cmpOp(TypeId::Float64, CmpKind::GE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_O_F64:
    cmpOp(TypeId::Float64, CmpKind::O, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_U_F64:
    cmpOp(TypeId::Float64, CmpKind::U, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NGE_F64:
    cmpOp(TypeId::Float64, CmpKind::NGE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLG_F64:
    cmpOp(TypeId::Float64, CmpKind::NLG, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NGT_F64:
    cmpOp(TypeId::Float64, CmpKind::NGT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLE_F64:
    cmpOp(TypeId::Float64, CmpKind::NLE, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NEQ_F64:
    cmpOp(TypeId::Float64, CmpKind::NEQ, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_NLT_F64:
    cmpOp(TypeId::Float64, CmpKind::NLT, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMPSX_TRU_F64:
    cmpOp(TypeId::Float64, CmpKind::TRU, CmpFlags::SX);
    break;
  case Vopc::Op::V_CMP_F_I32:
    cmpOp(TypeId::SInt32, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_I32:
    cmpOp(TypeId::SInt32, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_I32:
    cmpOp(TypeId::SInt32, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_I32:
    cmpOp(TypeId::SInt32, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_I32:
    cmpOp(TypeId::SInt32, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_I32:
    cmpOp(TypeId::SInt32, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_I32:
    cmpOp(TypeId::SInt32, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_T_I32:
    cmpOp(TypeId::SInt32, CmpKind::T);
    break;
  // case Vopc::Op::V_CMP_CLASS_F32: cmpOp(TypeId::Float32, CmpKind::CLASS);
  // break;
  case Vopc::Op::V_CMP_LT_I16:
    cmpOp(TypeId::SInt16, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_I16:
    cmpOp(TypeId::SInt16, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_I16:
    cmpOp(TypeId::SInt16, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_I16:
    cmpOp(TypeId::SInt16, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_I16:
    cmpOp(TypeId::SInt16, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_I16:
    cmpOp(TypeId::SInt16, CmpKind::GE);
    break;
  // case Vopc::Op::V_CMP_CLASS_F16: cmpOp(TypeId::Float16, CmpKind::CLASS);
  // break;
  case Vopc::Op::V_CMPX_F_I32:
    cmpOp(TypeId::SInt32, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_I32:
    cmpOp(TypeId::SInt32, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_I32:
    cmpOp(TypeId::SInt32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_I32:
    cmpOp(TypeId::SInt32, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_I32:
    cmpOp(TypeId::SInt32, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_I32:
    cmpOp(TypeId::SInt32, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_I32:
    cmpOp(TypeId::SInt32, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_T_I32:
    cmpOp(TypeId::SInt32, CmpKind::T, CmpFlags::X);
    break;
  // case Vopc::Op::V_CMPX_CLASS_F32: cmpOp(TypeId::Float32, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vopc::Op::V_CMPX_LT_I16:
    cmpOp(TypeId::SInt16, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_I16:
    cmpOp(TypeId::SInt16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_I16:
    cmpOp(TypeId::SInt16, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_I16:
    cmpOp(TypeId::SInt16, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_I16:
    cmpOp(TypeId::SInt16, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_I16:
    cmpOp(TypeId::SInt16, CmpKind::GE, CmpFlags::X);
    break;
  // case Vopc::Op::V_CMPX_CLASS_F16: cmpOp(TypeId::Float16, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vopc::Op::V_CMP_F_I64:
    cmpOp(TypeId::SInt64, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_I64:
    cmpOp(TypeId::SInt64, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_I64:
    cmpOp(TypeId::SInt64, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_I64:
    cmpOp(TypeId::SInt64, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_I64:
    cmpOp(TypeId::SInt64, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_I64:
    cmpOp(TypeId::SInt64, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_I64:
    cmpOp(TypeId::SInt64, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_T_I64:
    cmpOp(TypeId::SInt64, CmpKind::T);
    break;
  // case Vopc::Op::V_CMP_CLASS_F64: cmpOp(TypeId::Float64, CmpKind::CLASS);
  // break;
  case Vopc::Op::V_CMP_LT_U16:
    cmpOp(TypeId::UInt16, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_U16:
    cmpOp(TypeId::UInt16, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_U16:
    cmpOp(TypeId::UInt16, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_U16:
    cmpOp(TypeId::UInt16, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_U16:
    cmpOp(TypeId::UInt16, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_U16:
    cmpOp(TypeId::UInt16, CmpKind::GE);
    break;
  case Vopc::Op::V_CMPX_F_I64:
    cmpOp(TypeId::SInt64, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_I64:
    cmpOp(TypeId::SInt64, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_I64:
    cmpOp(TypeId::SInt64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_I64:
    cmpOp(TypeId::SInt64, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_I64:
    cmpOp(TypeId::SInt64, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_I64:
    cmpOp(TypeId::SInt64, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_I64:
    cmpOp(TypeId::SInt64, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_T_I64:
    cmpOp(TypeId::SInt64, CmpKind::T, CmpFlags::X);
    break;
  // case Vopc::Op::V_CMPX_CLASS_F64: cmpOp(TypeId::Float64, CmpKind::CLASS,
  // CmpFlags::X); break;
  case Vopc::Op::V_CMPX_LT_U16:
    cmpOp(TypeId::UInt16, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_U16:
    cmpOp(TypeId::UInt16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_U16:
    cmpOp(TypeId::UInt16, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_U16:
    cmpOp(TypeId::UInt16, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_U16:
    cmpOp(TypeId::UInt16, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_U16:
    cmpOp(TypeId::UInt16, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMP_F_U32:
    cmpOp(TypeId::UInt32, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_U32:
    cmpOp(TypeId::UInt32, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_U32:
    cmpOp(TypeId::UInt32, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_U32:
    cmpOp(TypeId::UInt32, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_U32:
    cmpOp(TypeId::UInt32, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_U32:
    cmpOp(TypeId::UInt32, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_U32:
    cmpOp(TypeId::UInt32, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_T_U32:
    cmpOp(TypeId::UInt32, CmpKind::T);
    break;
  case Vopc::Op::V_CMP_F_F16:
    cmpOp(TypeId::Float16, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_F16:
    cmpOp(TypeId::Float16, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_F16:
    cmpOp(TypeId::Float16, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_F16:
    cmpOp(TypeId::Float16, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_F16:
    cmpOp(TypeId::Float16, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_LG_F16:
    cmpOp(TypeId::Float16, CmpKind::LG);
    break;
  case Vopc::Op::V_CMP_GE_F16:
    cmpOp(TypeId::Float16, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_O_F16:
    cmpOp(TypeId::Float16, CmpKind::O);
    break;
  case Vopc::Op::V_CMPX_F_U32:
    cmpOp(TypeId::UInt32, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_U32:
    cmpOp(TypeId::UInt32, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_U32:
    cmpOp(TypeId::UInt32, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_U32:
    cmpOp(TypeId::UInt32, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_U32:
    cmpOp(TypeId::UInt32, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_U32:
    cmpOp(TypeId::UInt32, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_U32:
    cmpOp(TypeId::UInt32, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_T_U32:
    cmpOp(TypeId::UInt32, CmpKind::T, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_F_F16:
    cmpOp(TypeId::Float16, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_F16:
    cmpOp(TypeId::Float16, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_F16:
    cmpOp(TypeId::Float16, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_F16:
    cmpOp(TypeId::Float16, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_F16:
    cmpOp(TypeId::Float16, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LG_F16:
    cmpOp(TypeId::Float16, CmpKind::LG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_F16:
    cmpOp(TypeId::Float16, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_O_F16:
    cmpOp(TypeId::Float16, CmpKind::O, CmpFlags::X);
    break;
  case Vopc::Op::V_CMP_F_U64:
    cmpOp(TypeId::UInt64, CmpKind::F);
    break;
  case Vopc::Op::V_CMP_LT_U64:
    cmpOp(TypeId::UInt64, CmpKind::LT);
    break;
  case Vopc::Op::V_CMP_EQ_U64:
    cmpOp(TypeId::UInt64, CmpKind::EQ);
    break;
  case Vopc::Op::V_CMP_LE_U64:
    cmpOp(TypeId::UInt64, CmpKind::LE);
    break;
  case Vopc::Op::V_CMP_GT_U64:
    cmpOp(TypeId::UInt64, CmpKind::GT);
    break;
  case Vopc::Op::V_CMP_NE_U64:
    cmpOp(TypeId::UInt64, CmpKind::NE);
    break;
  case Vopc::Op::V_CMP_GE_U64:
    cmpOp(TypeId::UInt64, CmpKind::GE);
    break;
  case Vopc::Op::V_CMP_T_U64:
    cmpOp(TypeId::UInt64, CmpKind::T);
    break;
  case Vopc::Op::V_CMP_U_F16:
    cmpOp(TypeId::Float16, CmpKind::U);
    break;
  case Vopc::Op::V_CMP_NGE_F16:
    cmpOp(TypeId::Float16, CmpKind::NGE);
    break;
  case Vopc::Op::V_CMP_NLG_F16:
    cmpOp(TypeId::Float16, CmpKind::NLG);
    break;
  case Vopc::Op::V_CMP_NGT_F16:
    cmpOp(TypeId::Float16, CmpKind::NGT);
    break;
  case Vopc::Op::V_CMP_NLE_F16:
    cmpOp(TypeId::Float16, CmpKind::NLE);
    break;
  case Vopc::Op::V_CMP_NEQ_F16:
    cmpOp(TypeId::Float16, CmpKind::NEQ);
    break;
  case Vopc::Op::V_CMP_NLT_F16:
    cmpOp(TypeId::Float16, CmpKind::NLT);
    break;
  case Vopc::Op::V_CMP_TRU_F16:
    cmpOp(TypeId::Float16, CmpKind::TRU);
    break;
  case Vopc::Op::V_CMPX_F_U64:
    cmpOp(TypeId::UInt64, CmpKind::F, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LT_U64:
    cmpOp(TypeId::UInt64, CmpKind::LT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_EQ_U64:
    cmpOp(TypeId::UInt64, CmpKind::EQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_LE_U64:
    cmpOp(TypeId::UInt64, CmpKind::LE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GT_U64:
    cmpOp(TypeId::UInt64, CmpKind::GT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NE_U64:
    cmpOp(TypeId::UInt64, CmpKind::NE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_GE_U64:
    cmpOp(TypeId::UInt64, CmpKind::GE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_T_U64:
    cmpOp(TypeId::UInt64, CmpKind::T, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_U_F16:
    cmpOp(TypeId::Float16, CmpKind::U, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGE_F16:
    cmpOp(TypeId::Float16, CmpKind::NGE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLG_F16:
    cmpOp(TypeId::Float16, CmpKind::NLG, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NGT_F16:
    cmpOp(TypeId::Float16, CmpKind::NGT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLE_F16:
    cmpOp(TypeId::Float16, CmpKind::NLE, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NEQ_F16:
    cmpOp(TypeId::Float16, CmpKind::NEQ, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_NLT_F16:
    cmpOp(TypeId::Float16, CmpKind::NLT, CmpFlags::X);
    break;
  case Vopc::Op::V_CMPX_TRU_F16:
    cmpOp(TypeId::Float16, CmpKind::TRU, CmpFlags::X);
    break;

  default:
    inst.dump();
    util::unreachable();
  }
}
void convertSop1(Fragment &fragment, Sop1 inst) {
  fragment.registers->pc += Sop1::kMinInstSize * sizeof(std::uint32_t);

  switch (inst.op) {
  case Sop1::Op::S_MOV_B32:
    fragment.setScalarOperand(
        inst.sdst, fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32));
    break;

  case Sop1::Op::S_MOV_B64:
    fragment.setScalarOperand(
        inst.sdst, fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32));
    fragment.setScalarOperand(
        inst.sdst + 1,
        fragment.getScalarOperand(inst.ssrc0 + 1, TypeId::UInt32));
    break;

  case Sop1::Op::S_WQM_B32: {
    // TODO: whole quad mode
    break;
  }
  case Sop1::Op::S_WQM_B64: {
    // TODO: whole quad mode
    break;
  }
  case Sop1::Op::S_AND_SAVEEXEC_B64: {
    auto execLo = fragment.getExecLo();
    auto execHi = fragment.getExecHi();

    auto srcLo = fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32);
    auto srcHi = fragment.getScalarOperand(inst.ssrc0 + 1, TypeId::UInt32);

    fragment.setOperand(
        RegisterId::ExecLo,
        {srcLo.type, fragment.builder.createBitwiseAnd(srcLo.type, srcLo.value,
                                                       execLo.value)});
    fragment.setOperand(
        RegisterId::ExecHi,
        {srcHi.type, fragment.builder.createBitwiseAnd(srcHi.type, srcHi.value,
                                                       execHi.value)});
    auto uint32_0 = fragment.context->getUInt32(0);
    auto boolT = fragment.context->getBoolType();
    auto loIsNotZero =
        fragment.builder.createINotEqual(boolT, execLo.value, uint32_0);
    auto hiIsNotZero =
        fragment.builder.createINotEqual(boolT, execHi.value, uint32_0);
    fragment.setScc({boolT, fragment.builder.createLogicalAnd(
                                boolT, loIsNotZero, hiIsNotZero)});
    fragment.setScalarOperand(inst.sdst, execLo);
    fragment.setScalarOperand(inst.sdst + 1, execHi);
    break;
  }

  case Sop1::Op::S_SETPC_B64:
    if (auto ssrc0 = fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32),
        ssrc1 = fragment.getScalarOperand(inst.ssrc0 + 1, TypeId::UInt32);
        ssrc0 && ssrc1) {
      auto ssrc0OptValue = fragment.context->findUint32Value(ssrc0.value);
      auto ssrc1OptValue = fragment.context->findUint32Value(ssrc1.value);

      if (!ssrc0OptValue.has_value() || !ssrc1OptValue.has_value()) {
        util::unreachable();
      }

      fragment.jumpAddress =
          *ssrc0OptValue | (static_cast<std::uint64_t>(*ssrc1OptValue) << 32);
    } else {
      util::unreachable();
    }
    return;

  case Sop1::Op::S_SWAPPC_B64: {
    if (auto ssrc0 = fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32),
        ssrc1 = fragment.getScalarOperand(inst.ssrc0 + 1, TypeId::UInt32);
        ssrc0 && ssrc1) {
      auto ssrc0OptValue = fragment.context->findUint32Value(ssrc0.value);
      auto ssrc1OptValue = fragment.context->findUint32Value(ssrc1.value);

      if (!ssrc0OptValue.has_value() || !ssrc1OptValue.has_value()) {
        util::unreachable();
      }

      auto pc = fragment.registers->pc;
      fragment.setScalarOperand(inst.sdst, {fragment.context->getUInt32Type(),
                                            fragment.context->getUInt32(pc)});
      fragment.setScalarOperand(inst.sdst + 1,
                                {fragment.context->getUInt32Type(),
                                 fragment.context->getUInt32(pc >> 32)});

      fragment.jumpAddress =
          *ssrc0OptValue | (static_cast<std::uint64_t>(*ssrc1OptValue) << 32);
    } else {
      inst.dump();
      util::unreachable();
    }
    return;
  }

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertSopc(Fragment &fragment, Sopc inst) {
  fragment.registers->pc += Sopc::kMinInstSize * sizeof(std::uint32_t);

  auto cmpOp = [&](CmpKind kind, TypeId type) {
    auto src0 = fragment.getScalarOperand(inst.ssrc0, type).value;
    auto src1 = fragment.getScalarOperand(inst.ssrc1, type).value;

    auto result = doCmpOp(fragment, type, src0, src1, kind, CmpFlags::None);
    fragment.setScc(result);
  };

  switch (inst.op) {
  case Sopc::Op::S_CMP_EQ_I32:
    cmpOp(CmpKind::EQ, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_LG_I32:
    cmpOp(CmpKind::LG, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_GT_I32:
    cmpOp(CmpKind::GT, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_GE_I32:
    cmpOp(CmpKind::GE, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_LT_I32:
    cmpOp(CmpKind::LT, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_LE_I32:
    cmpOp(CmpKind::LE, TypeId::SInt32);
    break;
  case Sopc::Op::S_CMP_EQ_U32:
    cmpOp(CmpKind::EQ, TypeId::UInt32);
    break;
  case Sopc::Op::S_CMP_LG_U32:
    cmpOp(CmpKind::LG, TypeId::UInt32);
    break;
  case Sopc::Op::S_CMP_GT_U32:
    cmpOp(CmpKind::GT, TypeId::UInt32);
    break;
  case Sopc::Op::S_CMP_GE_U32:
    cmpOp(CmpKind::GE, TypeId::UInt32);
    break;
  case Sopc::Op::S_CMP_LT_U32:
    cmpOp(CmpKind::LT, TypeId::UInt32);
    break;
  case Sopc::Op::S_CMP_LE_U32:
    cmpOp(CmpKind::LE, TypeId::UInt32);
    break;

  case Sopc::Op::S_BITCMP0_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt32Type();

    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32(0x1f)));
    auto bit = fragment.builder.createBitwiseAnd(
        operandT,
        fragment.builder.createShiftRightLogical(operandT, src0, src1),
        fragment.context->getUInt32(1));

    auto boolT = fragment.context->getBoolType();
    fragment.setScc({boolT, fragment.builder.createIEqual(
                                boolT, bit, fragment.context->getUInt32(0))});
    break;
  }
  case Sopc::Op::S_BITCMP1_B32: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt32).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt32Type();

    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32(0x1f)));
    auto bit = fragment.builder.createBitwiseAnd(
        operandT,
        fragment.builder.createShiftRightLogical(operandT, src0, src1),
        fragment.context->getUInt32(1));

    auto boolT = fragment.context->getBoolType();
    fragment.setScc({boolT, fragment.builder.createIEqual(
                                boolT, bit, fragment.context->getUInt32(1))});
    break;
  }
  case Sopc::Op::S_BITCMP0_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt64Type();

    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32(0x3f)));
    auto bit = fragment.builder.createBitwiseAnd(
        operandT,
        fragment.builder.createShiftRightLogical(operandT, src0, src1),
        fragment.context->getUInt64(1));

    auto boolT = fragment.context->getBoolType();
    fragment.setScc({boolT, fragment.builder.createIEqual(
                                boolT, bit, fragment.context->getUInt64(0))});
    break;
  }
  case Sopc::Op::S_BITCMP1_B64: {
    auto src0 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc0, TypeId::UInt64).value);
    auto src1 = spirv::cast<spirv::UIntValue>(
        fragment.getScalarOperand(inst.ssrc1, TypeId::UInt32).value);
    auto operandT = fragment.context->getUInt64Type();

    src1 = spirv::cast<spirv::UIntValue>(fragment.builder.createBitwiseAnd(
        operandT, src1, fragment.context->getUInt32(0x3f)));
    auto bit = fragment.builder.createBitwiseAnd(
        operandT,
        fragment.builder.createShiftRightLogical(operandT, src0, src1),
        fragment.context->getUInt64(1));

    auto boolT = fragment.context->getBoolType();
    fragment.setScc({boolT, fragment.builder.createIEqual(
                                boolT, bit, fragment.context->getUInt64(1))});
    break;
  }
  default:
    inst.dump();
    util::unreachable();
  }
}

void convertSopp(Fragment &fragment, Sopp inst) {
  fragment.registers->pc += Sopp::kMinInstSize * sizeof(std::uint32_t);

  auto createCondBranch = [&](spirv::BoolValue condition) {
    fragment.branchCondition = condition;
    /*
        auto address = fragment.registers->pc + (inst.simm << 2);

        Fragment *ifTrueTarget =
            fragment.context->getOrCreateFragment(address, 0x100);
        Fragment *ifFalseTarget =
            fragment.context->getOrCreateFragment(fragment.registers->pc,
       0x100);

        fragment.builder.createSelectionMerge(ifTrueTarget->entryBlockId, {});
        fragment.builder.createBranchConditional(condition,
       ifTrueTarget->builder.id, ifFalseTarget->entryBlockId);
    */
  };

  switch (inst.op) {
  case Sopp::Op::S_WAITCNT:
    // TODO
    break;

  case Sopp::Op::S_BRANCH: {
    fragment.jumpAddress = fragment.registers->pc + (inst.simm << 2);
    // auto address = fragment.registers->pc + (inst.simm << 2);
    // Fragment *target = fragment.context->getOrCreateFragment(address, 0x100);

    // fragment.builder.createBranch(target->entryBlockId);
    // fragment.terminator = FragmentTerminator::Branch;
    //  target->predecessors.insert(&fragment);
    //  fragment.successors.insert(target);
    break;
  }

  case Sopp::Op::S_CBRANCH_SCC0: {
    createCondBranch(fragment.builder.createLogicalNot(
        fragment.context->getBoolType(), fragment.getScc()));
    break;
  }

  case Sopp::Op::S_CBRANCH_SCC1: {
    createCondBranch(fragment.getScc());
    break;
  }

  case Sopp::Op::S_CBRANCH_VCCZ: {
    auto loIsZero = fragment.builder.createIEqual(
        fragment.context->getBoolType(), fragment.getVccLo().value,
        fragment.context->getUInt32(0));
    auto hiIsZero = fragment.builder.createIEqual(
        fragment.context->getBoolType(), fragment.getVccHi().value,
        fragment.context->getUInt32(0));
    createCondBranch(fragment.builder.createLogicalAnd(
        fragment.context->getBoolType(), loIsZero, hiIsZero));
    break;
  }

  case Sopp::Op::S_CBRANCH_VCCNZ: {
    auto loIsNotZero = fragment.builder.createINotEqual(
        fragment.context->getBoolType(), fragment.getVccLo().value,
        fragment.context->getUInt32(0));
    auto hiIsNotZero = fragment.builder.createINotEqual(
        fragment.context->getBoolType(), fragment.getVccHi().value,
        fragment.context->getUInt32(0));

    createCondBranch(fragment.builder.createLogicalOr(
        fragment.context->getBoolType(), loIsNotZero, hiIsNotZero));
    break;
  }

  case Sopp::Op::S_CBRANCH_EXECZ: {
    auto loIsZero = fragment.builder.createIEqual(
        fragment.context->getBoolType(), fragment.getExecLo().value,
        fragment.context->getUInt32(0));
    auto hiIsZero = fragment.builder.createIEqual(
        fragment.context->getBoolType(), fragment.getExecHi().value,
        fragment.context->getUInt32(0));
    createCondBranch(fragment.builder.createLogicalAnd(
        fragment.context->getBoolType(), loIsZero, hiIsZero));
    break;
  }

  case Sopp::Op::S_CBRANCH_EXECNZ: {
    auto loIsNotZero = fragment.builder.createINotEqual(
        fragment.context->getBoolType(), fragment.getExecLo().value,
        fragment.context->getUInt32(0));
    auto hiIsNotZero = fragment.builder.createINotEqual(
        fragment.context->getBoolType(), fragment.getExecHi().value,
        fragment.context->getUInt32(0));

    createCondBranch(fragment.builder.createLogicalOr(
        fragment.context->getBoolType(), loIsNotZero, hiIsNotZero));
    break;
  }

  case Sopp::Op::S_ENDPGM:
    // fragment.terminator = FragmentTerminator::EndProgram;
    return;

  case Sopp::Op::S_NOP:
    break;

  default:
    inst.dump();
    util::unreachable();
  }
}

void convertInstruction(Fragment &fragment, Instruction inst) {
  switch (inst.instClass) {
  case InstructionClass::Vop2:
    return convertVop2(fragment, Vop2(inst.inst));
  case InstructionClass::Sop2:
    return convertSop2(fragment, Sop2(inst.inst));
  case InstructionClass::Sopk:
    return convertSopk(fragment, Sopk(inst.inst));
  case InstructionClass::Smrd:
    return convertSmrd(fragment, Smrd(inst.inst));
  case InstructionClass::Vop3:
    return convertVop3(fragment, Vop3(inst.inst));
  case InstructionClass::Mubuf:
    return convertMubuf(fragment, Mubuf(inst.inst));
  case InstructionClass::Mtbuf:
    return convertMtbuf(fragment, Mtbuf(inst.inst));
  case InstructionClass::Mimg:
    return convertMimg(fragment, Mimg(inst.inst));
  case InstructionClass::Ds:
    return convertDs(fragment, Ds(inst.inst));
  case InstructionClass::Vintrp:
    return convertVintrp(fragment, Vintrp(inst.inst));
  case InstructionClass::Exp:
    return convertExp(fragment, Exp(inst.inst));
  case InstructionClass::Vop1:
    return convertVop1(fragment, Vop1(inst.inst));
  case InstructionClass::Vopc:
    return convertVopc(fragment, Vopc(inst.inst));
  case InstructionClass::Sop1:
    return convertSop1(fragment, Sop1(inst.inst));
  case InstructionClass::Sopc:
    return convertSopc(fragment, Sopc(inst.inst));
  case InstructionClass::Sopp:
    return convertSopp(fragment, Sopp(inst.inst));

  case InstructionClass::Invalid:
    break;
  }

  inst.dump();
  util::unreachable();
}

} // namespace

void Fragment::injectValuesFromPreds() {
  for (auto pred : predecessors) {
    for (auto value : pred->values) {
      values.insert(value);
    }

    for (auto output : pred->outputs) {
      outputs.insert(output);
    }
  }

  std::vector<std::pair<spirv::Value, spirv::Block>> predValues;

  // std::printf("injection values for bb%lx\n", registers->pc);

  // auto getRegName = [](RegisterId id) {
  //   if (id.isScalar()) {
  //     return "sgpr";
  //   }

  //   if (id.isVector()) {
  //     return "vgpr";
  //   }

  //   if (id.isExport()) {
  //     return "exp";
  //   }

  //   if (id.isAttr()) {
  //     return "attr";
  //   }

  //   return "<invalid>";
  // };

  auto setupRegisterValue = [&](RegisterId id) {
    bool allSameValues = true;
    predValues.clear();

    spirv::Type type;

    for (auto pred : predecessors) {
      Value value;

      if (type) {
        value = pred->getRegister(id, type);
      } else {
        value = pred->getRegister(id);
        type = value.type;
      }

      if (allSameValues && !predValues.empty()) {
        allSameValues = predValues.back().first == value.value;
      }

      predValues.emplace_back(value.value, pred->builder.id);
    }

    Value value;

    if (allSameValues) {
      value = {type, predValues.back().first};
      // std::printf(" ** %s[%u] is value = %u\n", getRegName(id),
      // id.getOffset(),
      //             predValues.back().first.id);
    } else {
      // std::printf(" ** %s[%u] is phi = { ", getRegName(id), id.getOffset());
      // for (bool isFirst = true; auto value : predValues) {
      //   if (isFirst) {
      //     isFirst = false;
      //   } else {
      //     std::printf(", ");
      //   }
      //   std::printf("%u", value.first.id);
      // }
      // std::printf(" }\n");
      value = {type, builder.createPhi(type, predValues)};
    }

    registers->setRegister(id, value);
  };

  for (auto id : values) {
    setupRegisterValue(id);
  }
  for (auto id : outputs) {
    setupRegisterValue(id);
  }
}

spirv::SamplerValue Fragment::createSampler(RegisterId base) {
  auto sBuffer0 = getOperand(RegisterId::Raw(base + 0), TypeId::UInt32);
  auto sBuffer1 = getOperand(RegisterId::Raw(base + 1), TypeId::UInt32);
  auto sBuffer2 = getOperand(RegisterId::Raw(base + 2), TypeId::UInt32);
  auto sBuffer3 = getOperand(RegisterId::Raw(base + 3), TypeId::UInt32);

  auto optSBuffer0Value = context->findUint32Value(sBuffer0.value);
  auto optSBuffer1Value = context->findUint32Value(sBuffer1.value);
  auto optSBuffer2Value = context->findUint32Value(sBuffer2.value);
  auto optSBuffer3Value = context->findUint32Value(sBuffer3.value);

  if (optSBuffer0Value && optSBuffer1Value && optSBuffer2Value &&
      optSBuffer3Value) {
    std::uint32_t sbuffer[] = {
        *optSBuffer0Value,
        *optSBuffer1Value,
        *optSBuffer2Value,
        *optSBuffer3Value,
    };

    auto uniform = context->getOrCreateUniformConstant(
        sbuffer, std::size(sbuffer), TypeId::Sampler);
    return builder.createLoad(context->getSamplerType(), uniform->variable);
  } else {
    util::unreachable();
  }
}

spirv::ImageValue Fragment::createImage(RegisterId base, bool r128) {
  auto tBuffer0 = getOperand(RegisterId::Raw(base + 0), TypeId::UInt32);
  auto tBuffer1 = getOperand(RegisterId::Raw(base + 1), TypeId::UInt32);
  auto tBuffer2 = getOperand(RegisterId::Raw(base + 2), TypeId::UInt32);
  auto tBuffer3 = getOperand(RegisterId::Raw(base + 3), TypeId::UInt32);

  auto optTBuffer0Value = context->findUint32Value(tBuffer0.value);
  auto optTBuffer1Value = context->findUint32Value(tBuffer1.value);
  auto optTBuffer2Value = context->findUint32Value(tBuffer2.value);
  auto optTBuffer3Value = context->findUint32Value(tBuffer3.value);

  if (!optTBuffer0Value || !optTBuffer1Value || !optTBuffer2Value ||
      !optTBuffer3Value) {
    util::unreachable();
  }

  if (r128) {
    std::uint32_t sbuffer[] = {
        *optTBuffer0Value,
        *optTBuffer1Value,
        *optTBuffer2Value,
        *optTBuffer3Value,
    };

    auto uniform = context->getOrCreateUniformConstant(
        sbuffer, std::size(sbuffer), TypeId::Image2D);
    return builder.createLoad(context->getImage2DType(), uniform->variable);
  }

  auto tBuffer4 = getOperand(RegisterId::Raw(base + 4), TypeId::UInt32);
  auto tBuffer5 = getOperand(RegisterId::Raw(base + 5), TypeId::UInt32);
  auto tBuffer6 = getOperand(RegisterId::Raw(base + 6), TypeId::UInt32);
  auto tBuffer7 = getOperand(RegisterId::Raw(base + 7), TypeId::UInt32);

  auto optTBuffer4Value = context->findUint32Value(tBuffer4.value);
  auto optTBuffer5Value = context->findUint32Value(tBuffer5.value);
  auto optTBuffer6Value = context->findUint32Value(tBuffer6.value);
  auto optTBuffer7Value = context->findUint32Value(tBuffer7.value);

  if (!optTBuffer4Value || !optTBuffer5Value || !optTBuffer6Value ||
      !optTBuffer7Value) {
    util::unreachable();
  }

  std::uint32_t sbuffer[] = {
      *optTBuffer0Value, *optTBuffer1Value, *optTBuffer2Value,
      *optTBuffer3Value, *optTBuffer4Value, *optTBuffer5Value,
      *optTBuffer6Value, *optTBuffer7Value,
  };

  auto uniform = context->getOrCreateUniformConstant(
      sbuffer, std::size(sbuffer), TypeId::Image2D);
  return builder.createLoad(context->getImage2DType(), uniform->variable);
}

Value Fragment::createCompositeExtract(Value composite, std::uint32_t member) {
  auto optCompositeType = context->getTypeIdOf(composite.type);
  if (!optCompositeType.has_value()) {
    util::unreachable();
  }

  auto compositeType = *optCompositeType;

  TypeId baseType = compositeType.getBaseType();
  std::uint32_t memberCount = compositeType.getElementsCount();

  if (member >= memberCount) {
    util::unreachable();
  }

  auto resultType = context->getType(baseType);
  spirv::Value resultValue;

  if (memberCount > 4) {
    // stored in array
    auto row = member / 4;
    auto column = member % 4;

    auto rowType = context->getType(
        static_cast<TypeId::enum_type>(static_cast<int>(baseType) + 3));

    auto rowValue =
        builder.createCompositeExtract(rowType, composite.value, {{row}});
    resultValue =
        builder.createCompositeExtract(resultType, rowValue, {{column}});
  } else {
    resultValue =
        builder.createCompositeExtract(resultType, composite.value, {{member}});
  }

  return {resultType, resultValue};
}

spirv::Value Fragment::createBitcast(spirv::Type to, spirv::Type from,
                                     spirv::Value value) {
  if (from == to) {
    return value;
  }

  if (from == context->getFloat32Type()) {
    if (auto origValue = context->findFloat32Value(value)) {
      if (to == context->getUInt32Type()) {
        return context->getUInt32(std::bit_cast<std::uint32_t>(*origValue));
      }

      if (to == context->getSint32Type()) {
        return context->getSInt32(std::bit_cast<std::uint32_t>(*origValue));
      }
    }
  } else if (from == context->getUInt32Type()) {
    if (auto origValue = context->findUint32Value(value)) {
      if (to == context->getFloat32Type()) {
        return context->getFloat32(std::bit_cast<float>(*origValue));
      }

      if (to == context->getSint32Type()) {
        return context->getSInt32(std::bit_cast<std::uint32_t>(*origValue));
      }
    }
  } else if (from == context->getSint32Type()) {
    if (auto origValue = context->findSint32Value(value)) {
      if (to == context->getFloat32Type()) {
        return context->getFloat32(std::bit_cast<float>(*origValue));
      }

      if (to == context->getUInt32Type()) {
        return context->getUInt32(std::bit_cast<std::uint32_t>(*origValue));
      }
    }
  }

  if (from == context->getUInt64Type() && to == context->getUInt32Type()) {
    util::unreachable();
  }
  return builder.createBitcast(to, value);
}

Value Fragment::getOperand(RegisterId id, TypeId type, OperandGetFlags flags) {
  if (id == RegisterId::Scc) {
    if (type != TypeId::Bool) {
      util::unreachable();
    }

    return getRegister(id);
  }

  auto elementsCount = type.getElementsCount();

  if (elementsCount == 0) {
    util::unreachable();
  }

  auto resultType = context->getType(type);

  auto baseTypeId = type.getBaseType();
  auto baseTypeSize = baseTypeId.getSize();
  auto registerCountPerElement = (baseTypeSize + 3) / 4;
  auto registerElementsCount = elementsCount * registerCountPerElement;

  if (registerElementsCount == 1 || id.isExport() || id.isAttr()) {
    if (flags == OperandGetFlags::PreserveType) {
      return getRegister(id);
    } else {
      return getRegister(id, resultType);
    }
  }

  if (baseTypeSize < 4) {
    util::unreachable();
  }

  auto baseType = context->getType(baseTypeId);

  if (registerCountPerElement == 1) {
    std::vector<spirv::Value> members;
    members.reserve(elementsCount);
    spirv::Type preservedType;

    for (std::uint32_t i = 0; i < elementsCount; ++i) {
      Value member;

      if (flags == OperandGetFlags::PreserveType) {
        if (!preservedType) {
          member = getRegister(RegisterId::Raw(id + i));
          preservedType = member.type;
        } else {
          member = getRegister(RegisterId::Raw(id + i), preservedType);
        }
      } else {
        member = getRegister(RegisterId::Raw(id + i), baseType);
      }

      members.push_back(member.value);
    }

    return {resultType, builder.createCompositeConstruct(resultType, members)};
  }

  if (registerElementsCount != 2) {
    util::unreachable();
  }

  TypeId registerType;

  switch (baseTypeId) {
  case TypeId::UInt64:
    registerType = TypeId::UInt32;
    break;
  case TypeId::SInt64:
    registerType = TypeId::SInt32;
    break;
  case TypeId::Float64:
    registerType = TypeId::Float32;
    break;

  default:
    util::unreachable();
  }

  if (registerCountPerElement != 2) {
    util::unreachable();
  }

  auto uint64T = context->getUInt64Type();
  auto valueLo = builder.createUConvert(
      uint64T,
      spirv::cast<spirv::UIntValue>(getOperand(id, TypeId::UInt32).value));
  auto valueHi = builder.createUConvert(
      uint64T, spirv::cast<spirv::UIntValue>(
                   getOperand(RegisterId::Raw(id + 1), TypeId::UInt32).value));
  valueHi =
      builder.createShiftLeftLogical(uint64T, valueHi, context->getUInt32(32));
  auto value = builder.createBitwiseOr(uint64T, valueLo, valueHi);

  if (baseTypeId != TypeId::UInt64) {
    value = createBitcast(baseType, context->getUInt64Type(), value);
  }

  return {resultType, value};
}

void Fragment::setOperand(RegisterId id, Value value) {
  if (id.isExport()) {
    function->createExport(builder, id.getOffset(), value);
    return;
  }

  auto typeId = *context->getTypeIdOf(value.type);
  auto elementsCount = typeId.getElementsCount();

  if (elementsCount == 0) {
    util::unreachable();
  }

  // if (id.isScalar()) {
  //   std::printf("update sgpr[%u]\n", id.getOffset());
  // }

  // TODO: handle half types
  auto baseTypeId = typeId.getBaseType();
  auto baseTypeSize = baseTypeId.getSize();

  auto registerCountPerElement = (baseTypeSize + 3) / 4;
  auto registerElementsCount = elementsCount * registerCountPerElement;

  if (id == RegisterId::Scc) {
    auto boolT = context->getBoolType();
    if (value.type != boolT) {
      if (value.type == context->getUInt32Type()) {
        value.value =
            builder.createINotEqual(boolT, value.value, context->getUInt32(0));
      } else if (value.type == context->getSint32Type()) {
        value.value =
            builder.createINotEqual(boolT, value.value, context->getSInt32(0));
      } else if (value.type == context->getUInt64Type()) {
        value.value =
            builder.createINotEqual(boolT, value.value, context->getUInt64(0));
      } else {
        util::unreachable();
      }

      value.type = boolT;
    }

    setRegister(id, value);
    return;
  }

  if (registerElementsCount == 1 || id.isExport() || id.isAttr()) {
    setRegister(id, value);
    return;
  }

  if (baseTypeSize < 4) {
    util::unreachable();
  }

  if (registerCountPerElement == 1) {
    for (std::uint32_t i = 0; i < elementsCount; ++i) {
      auto element = createCompositeExtract(value, i);
      auto regId = RegisterId::Raw(id + i);
      setRegister(regId, element);
    }
  } else {
    if (elementsCount != 1 || baseTypeId != typeId) {
      util::unreachable();
    }

    TypeId registerType;

    switch (baseTypeId) {
    case TypeId::UInt64:
      registerType = TypeId::UInt32;
      break;
    case TypeId::SInt64:
      registerType = TypeId::SInt32;
      break;
    case TypeId::Float64:
      registerType = TypeId::Float32;
      break;

    default:
      util::unreachable();
    }

    if (registerCountPerElement != 2) {
      util::unreachable();
    }

    auto uint64T = context->getUInt64Type();
    auto uint64_value = spirv::cast<spirv::UIntValue>(value.value);
    if (baseTypeId != TypeId::UInt64) {
      uint64_value = spirv::cast<spirv::UIntValue>(
          createBitcast(uint64T, context->getType(baseTypeId), value.value));
    }

    auto uint32T = context->getUInt32Type();
    auto valueLo = builder.createUConvert(uint32T, uint64_value);
    auto valueHi = builder.createUConvert(
        uint32T, builder.createShiftRightLogical(uint64T, uint64_value,
                                                 context->getUInt32(32)));

    setOperand(id, {uint32T, valueLo});
    setOperand(RegisterId::Raw(id.raw + 1), {uint32T, valueHi});
  }
}

void Fragment::setVcc(Value value) {
  // TODO: update vcc hi if needed
  // TODO: update vccz

  setOperand(RegisterId::VccLo, value);
  setOperand(RegisterId::VccHi,
             {context->getUInt32Type(), context->getUInt32(0)});
}

void Fragment::setScc(Value value) {
  setOperand(RegisterId::Scc, value);

  if (value.type != context->getBoolType() &&
      value.type != context->getUInt32Type() &&
      value.type != context->getSint32Type() &&
      value.type != context->getUInt64Type()) {
    util::unreachable();
  }
}

spirv::BoolValue Fragment::getScc() {
  auto result =
      getOperand(RegisterId::Scc, TypeId::Bool, OperandGetFlags::PreserveType);

  if (result.type == context->getBoolType()) {
    return spirv::cast<spirv::BoolValue>(result.value);
  }

  if (result.type == context->getUInt32Type()) {
    return builder.createINotEqual(context->getBoolType(), result.value,
                                   context->getUInt32(0));
  }
  if (result.type == context->getSint32Type()) {
    return builder.createINotEqual(context->getBoolType(), result.value,
                                   context->getSInt32(0));
  }
  if (result.type == context->getUInt64Type()) {
    return builder.createINotEqual(context->getBoolType(), result.value,
                                   context->getUInt64(0));
  }

  util::unreachable();
}
/*
void Fragment::createCallTo(MaterializedFunction *materialized) {
  std::vector<spirv::Value> args;
  args.reserve(materialized->args.size());

  for (auto input : materialized->args) {
    auto value = getOperand(input.first, input.second);
    args.push_back(value.value);
  }

  auto callResultType = materialized->returnType;

  auto callResult =
      builder.createFunctionCall(callResultType, materialized->function, args);
  if (materialized->results.empty()) {
    return;
  }

  if (materialized->results.size() == 1) {
    setOperand(materialized->results.begin()->first,
               Value(callResultType, callResult));
    return;
  }

  auto resultTypePointer = context->getBuilder().createTypePointer(
      spv::StorageClass::Function, callResultType);
  auto resultTypeVariable =
      builder.createVariable(resultTypePointer, spv::StorageClass::Function);
  builder.createStore(resultTypeVariable, callResult);

  std::uint32_t member = 0;
  for (auto [output, typeId] : materialized->results) {
    auto pointerType =
        context->getPointerType(spv::StorageClass::Function, typeId);
    auto valuePointer = builder.createAccessChain(
        pointerType, resultTypeVariable, {{context->getUInt32(member++)}});

    auto elementType = context->getType(typeId);
    auto elementValue = builder.createLoad(elementType, valuePointer);
    setOperand(output, Value(elementType, elementValue));
  }
}
*/
void amdgpu::shader::Fragment::convert(std::uint64_t size) {
  auto ptr = context->getMemory().getPointer<std::uint32_t>(registers->pc);
  auto endptr = ptr + size / sizeof(std::uint32_t);

  while (ptr < endptr) {
    Instruction inst(ptr);
    // auto startPoint = builder.bodyRegion.getCurrentPosition();

    // std::printf("===============\n");
    // inst.dump();
    // std::printf("\n");
    convertInstruction(*this, inst);

    // std::printf("-------------->\n");
    // spirv::dump(builder.bodyRegion.getCurrentPosition() - startPoint);

    ptr += inst.size();
  }
}

Value amdgpu::shader::Fragment::getRegister(RegisterId id) {
  if (id.isScalar()) {
    switch (id.getOffset()) {
    case 128 ... 192:
      return {context->getSint32Type(), context->getSInt32(id - 128)};
    case 193 ... 208:
      return {context->getSint32Type(),
              context->getSInt32(-static_cast<std::int32_t>(id - 192))};
    case 240:
      return {context->getFloat32Type(), context->getFloat32(0.5f)};
    case 241:
      return {context->getFloat32Type(), context->getFloat32(-0.5f)};
    case 242:
      return {context->getFloat32Type(), context->getFloat32(1.0f)};
    case 243:
      return {context->getFloat32Type(), context->getFloat32(-1.0f)};
    case 244:
      return {context->getFloat32Type(), context->getFloat32(2.0f)};
    case 245:
      return {context->getFloat32Type(), context->getFloat32(-2.0f)};
    case 246:
      return {context->getFloat32Type(), context->getFloat32(4.0f)};
    case 247:
      return {context->getFloat32Type(), context->getFloat32(-4.0f)};
    case 255: {
      auto ptr = context->getMemory().getPointer<std::uint32_t>(registers->pc);
      registers->pc += sizeof(std::uint32_t);
      return {context->getUInt32Type(), context->getUInt32(*ptr)};
    }
    }
  }

  if (auto result = registers->getRegister(id)) {
    return result;
  }

  if (id.isExport()) {
    util::unreachable();
  }

  // std::printf("creation input %u\n", id.raw);
  auto result = function->createInput(id);
  assert(result);
  values.insert(id);
  registers->setRegister(id, result);
  return result;
}

Value amdgpu::shader::Fragment::getRegister(RegisterId id,
                                               spirv::Type type) {
  auto result = getRegister(id);

  if (!result) {
    return result;
  }

  if (type == context->getUInt64Type()) {
    util::unreachable("%u is ulong\n", id.raw);
  }

  return {type, createBitcast(type, result.type, result.value)};
}

void amdgpu::shader::Fragment::setRegister(RegisterId id, Value value) {
  if (registers->getRegister(id) == value) {
    return;
  }

  assert(value);

  registers->setRegister(id, value);
  outputs.insert(id);
  // std::printf("creation output %u\n", id.raw);
}
