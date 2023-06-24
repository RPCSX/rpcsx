#pragma once
#include "spirv.hpp"
#include <cstdint>
#include <cstdio>
#include <span>

namespace spirv {
enum class OperandKind {
  Invalid,
  ValueId,
  TypeId,
  Word,
  String,
  VariadicId,
  VariadicWord,
};

enum class OperandDirection {
  In,
  Out,
};

enum class InstructionFlags {
  None = 0,
  HasResult = 1 << 0,
  HasResultType = 1 << 1,
};

inline InstructionFlags operator|(InstructionFlags lhs, InstructionFlags rhs) {
  return static_cast<InstructionFlags>(static_cast<int>(lhs) |
                                       static_cast<int>(rhs));
}
inline InstructionFlags operator&(InstructionFlags lhs, InstructionFlags rhs) {
  return static_cast<InstructionFlags>(static_cast<int>(lhs) &
                                       static_cast<int>(rhs));
}

struct InstructionInfo {
  const char *name;
  InstructionFlags flags;
  OperandKind operands[16];
};

inline const InstructionInfo *getInstructionInfo(spv::Op opcode) {
  switch (opcode) {
  default: /* unknown opcode */
    break;
  case spv::Op::OpNop: {
    static InstructionInfo result = {"OpNop", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpUndef: {
    static InstructionInfo result = {
        "OpUndef",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSourceContinued: {
    static InstructionInfo result = {
        "OpSourceContinued", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpSource: {
    static InstructionInfo result = {"OpSource", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpSourceExtension: {
    static InstructionInfo result = {
        "OpSourceExtension", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpName: {
    static InstructionInfo result = {"OpName", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpMemberName: {
    static InstructionInfo result = {
        "OpMemberName", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpString: {
    static InstructionInfo result = {
        "OpString", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpLine: {
    static InstructionInfo result = {"OpLine", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpExtension: {
    static InstructionInfo result = {"OpExtension", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpExtInstImport: {
    static InstructionInfo result = {
        "OpExtInstImport", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpExtInst: {
    static InstructionInfo result = {
        "OpExtInst",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpMemoryModel: {
    static InstructionInfo result = {
        "OpMemoryModel", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpEntryPoint: {
    static InstructionInfo result = {
        "OpEntryPoint", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpExecutionMode: {
    static InstructionInfo result = {
        "OpExecutionMode", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCapability: {
    static InstructionInfo result = {
        "OpCapability", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpTypeVoid: {
    static InstructionInfo result = {
        "OpTypeVoid", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeBool: {
    static InstructionInfo result = {
        "OpTypeBool", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeInt: {
    static InstructionInfo result = {
        "OpTypeInt", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeFloat: {
    static InstructionInfo result = {
        "OpTypeFloat", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeVector: {
    static InstructionInfo result = {
        "OpTypeVector", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeMatrix: {
    static InstructionInfo result = {
        "OpTypeMatrix", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeImage: {
    static InstructionInfo result = {
        "OpTypeImage", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeSampler: {
    static InstructionInfo result = {
        "OpTypeSampler", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeSampledImage: {
    static InstructionInfo result = {"OpTypeSampledImage",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpTypeArray: {
    static InstructionInfo result = {
        "OpTypeArray", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeRuntimeArray: {
    static InstructionInfo result = {"OpTypeRuntimeArray",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpTypeStruct: {
    static InstructionInfo result = {
        "OpTypeStruct", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeOpaque: {
    static InstructionInfo result = {
        "OpTypeOpaque", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypePointer: {
    static InstructionInfo result = {
        "OpTypePointer", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeFunction: {
    static InstructionInfo result = {
        "OpTypeFunction", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeEvent: {
    static InstructionInfo result = {
        "OpTypeEvent", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeDeviceEvent: {
    static InstructionInfo result = {"OpTypeDeviceEvent",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpTypeReserveId: {
    static InstructionInfo result = {
        "OpTypeReserveId", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeQueue: {
    static InstructionInfo result = {
        "OpTypeQueue", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypePipe: {
    static InstructionInfo result = {
        "OpTypePipe", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpTypeForwardPointer: {
    static InstructionInfo result = {
        "OpTypeForwardPointer", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpConstantTrue: {
    static InstructionInfo result = {
        "OpConstantTrue",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConstantFalse: {
    static InstructionInfo result = {
        "OpConstantFalse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConstant: {
    static InstructionInfo result = {
        "OpConstant",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConstantComposite: {
    static InstructionInfo result = {
        "OpConstantComposite",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConstantSampler: {
    static InstructionInfo result = {
        "OpConstantSampler",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConstantNull: {
    static InstructionInfo result = {
        "OpConstantNull",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSpecConstantTrue: {
    static InstructionInfo result = {
        "OpSpecConstantTrue",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSpecConstantFalse: {
    static InstructionInfo result = {
        "OpSpecConstantFalse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSpecConstant: {
    static InstructionInfo result = {
        "OpSpecConstant",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSpecConstantComposite: {
    static InstructionInfo result = {
        "OpSpecConstantComposite",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSpecConstantOp: {
    static InstructionInfo result = {
        "OpSpecConstantOp",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFunction: {
    static InstructionInfo result = {
        "OpFunction",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFunctionParameter: {
    static InstructionInfo result = {
        "OpFunctionParameter",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFunctionEnd: {
    static InstructionInfo result = {
        "OpFunctionEnd", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpFunctionCall: {
    static InstructionInfo result = {
        "OpFunctionCall",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpVariable: {
    static InstructionInfo result = {
        "OpVariable",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageTexelPointer: {
    static InstructionInfo result = {
        "OpImageTexelPointer",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLoad: {
    static InstructionInfo result = {
        "OpLoad",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpStore: {
    static InstructionInfo result = {"OpStore", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCopyMemory: {
    static InstructionInfo result = {
        "OpCopyMemory", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCopyMemorySized: {
    static InstructionInfo result = {
        "OpCopyMemorySized", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpAccessChain: {
    static InstructionInfo result = {
        "OpAccessChain",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpInBoundsAccessChain: {
    static InstructionInfo result = {
        "OpInBoundsAccessChain",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPtrAccessChain: {
    static InstructionInfo result = {
        "OpPtrAccessChain",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpArrayLength: {
    static InstructionInfo result = {
        "OpArrayLength",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGenericPtrMemSemantics: {
    static InstructionInfo result = {
        "OpGenericPtrMemSemantics",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpInBoundsPtrAccessChain: {
    static InstructionInfo result = {
        "OpInBoundsPtrAccessChain",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDecorate: {
    static InstructionInfo result = {"OpDecorate", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpMemberDecorate: {
    static InstructionInfo result = {
        "OpMemberDecorate", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpDecorationGroup: {
    static InstructionInfo result = {"OpDecorationGroup",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpGroupDecorate: {
    static InstructionInfo result = {
        "OpGroupDecorate", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGroupMemberDecorate: {
    static InstructionInfo result = {
        "OpGroupMemberDecorate", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpVectorExtractDynamic: {
    static InstructionInfo result = {
        "OpVectorExtractDynamic",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpVectorInsertDynamic: {
    static InstructionInfo result = {
        "OpVectorInsertDynamic",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpVectorShuffle: {
    static InstructionInfo result = {
        "OpVectorShuffle",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCompositeConstruct: {
    static InstructionInfo result = {
        "OpCompositeConstruct",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCompositeExtract: {
    static InstructionInfo result = {
        "OpCompositeExtract",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCompositeInsert: {
    static InstructionInfo result = {
        "OpCompositeInsert",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCopyObject: {
    static InstructionInfo result = {
        "OpCopyObject",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpTranspose: {
    static InstructionInfo result = {
        "OpTranspose",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSampledImage: {
    static InstructionInfo result = {
        "OpSampledImage",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleImplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleExplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleDrefImplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleDrefImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleDrefExplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleDrefExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleProjImplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleProjImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleProjExplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleProjExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleProjDrefImplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleProjDrefImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSampleProjDrefExplicitLod: {
    static InstructionInfo result = {
        "OpImageSampleProjDrefExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageFetch: {
    static InstructionInfo result = {
        "OpImageFetch",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageGather: {
    static InstructionInfo result = {
        "OpImageGather",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageDrefGather: {
    static InstructionInfo result = {
        "OpImageDrefGather",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageRead: {
    static InstructionInfo result = {
        "OpImageRead",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageWrite: {
    static InstructionInfo result = {
        "OpImageWrite", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpImage: {
    static InstructionInfo result = {
        "OpImage",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQueryFormat: {
    static InstructionInfo result = {
        "OpImageQueryFormat",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQueryOrder: {
    static InstructionInfo result = {
        "OpImageQueryOrder",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQuerySizeLod: {
    static InstructionInfo result = {
        "OpImageQuerySizeLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQuerySize: {
    static InstructionInfo result = {
        "OpImageQuerySize",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQueryLod: {
    static InstructionInfo result = {
        "OpImageQueryLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQueryLevels: {
    static InstructionInfo result = {
        "OpImageQueryLevels",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageQuerySamples: {
    static InstructionInfo result = {
        "OpImageQuerySamples",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertFToU: {
    static InstructionInfo result = {
        "OpConvertFToU",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertFToS: {
    static InstructionInfo result = {
        "OpConvertFToS",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertSToF: {
    static InstructionInfo result = {
        "OpConvertSToF",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertUToF: {
    static InstructionInfo result = {
        "OpConvertUToF",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUConvert: {
    static InstructionInfo result = {
        "OpUConvert",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSConvert: {
    static InstructionInfo result = {
        "OpSConvert",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFConvert: {
    static InstructionInfo result = {
        "OpFConvert",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpQuantizeToF16: {
    static InstructionInfo result = {
        "OpQuantizeToF16",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertPtrToU: {
    static InstructionInfo result = {
        "OpConvertPtrToU",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSatConvertSToU: {
    static InstructionInfo result = {
        "OpSatConvertSToU",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSatConvertUToS: {
    static InstructionInfo result = {
        "OpSatConvertUToS",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpConvertUToPtr: {
    static InstructionInfo result = {
        "OpConvertUToPtr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPtrCastToGeneric: {
    static InstructionInfo result = {
        "OpPtrCastToGeneric",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGenericCastToPtr: {
    static InstructionInfo result = {
        "OpGenericCastToPtr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGenericCastToPtrExplicit: {
    static InstructionInfo result = {
        "OpGenericCastToPtrExplicit",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitcast: {
    static InstructionInfo result = {
        "OpBitcast",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSNegate: {
    static InstructionInfo result = {
        "OpSNegate",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFNegate: {
    static InstructionInfo result = {
        "OpFNegate",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIAdd: {
    static InstructionInfo result = {
        "OpIAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFAdd: {
    static InstructionInfo result = {
        "OpFAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpISub: {
    static InstructionInfo result = {
        "OpISub",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFSub: {
    static InstructionInfo result = {
        "OpFSub",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIMul: {
    static InstructionInfo result = {
        "OpIMul",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFMul: {
    static InstructionInfo result = {
        "OpFMul",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUDiv: {
    static InstructionInfo result = {
        "OpUDiv",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSDiv: {
    static InstructionInfo result = {
        "OpSDiv",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFDiv: {
    static InstructionInfo result = {
        "OpFDiv",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUMod: {
    static InstructionInfo result = {
        "OpUMod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSRem: {
    static InstructionInfo result = {
        "OpSRem",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSMod: {
    static InstructionInfo result = {
        "OpSMod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFRem: {
    static InstructionInfo result = {
        "OpFRem",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFMod: {
    static InstructionInfo result = {
        "OpFMod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpVectorTimesScalar: {
    static InstructionInfo result = {
        "OpVectorTimesScalar",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpMatrixTimesScalar: {
    static InstructionInfo result = {
        "OpMatrixTimesScalar",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpVectorTimesMatrix: {
    static InstructionInfo result = {
        "OpVectorTimesMatrix",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpMatrixTimesVector: {
    static InstructionInfo result = {
        "OpMatrixTimesVector",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpMatrixTimesMatrix: {
    static InstructionInfo result = {
        "OpMatrixTimesMatrix",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpOuterProduct: {
    static InstructionInfo result = {
        "OpOuterProduct",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDot: {
    static InstructionInfo result = {
        "OpDot",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIAddCarry: {
    static InstructionInfo result = {
        "OpIAddCarry",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpISubBorrow: {
    static InstructionInfo result = {
        "OpISubBorrow",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUMulExtended: {
    static InstructionInfo result = {
        "OpUMulExtended",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSMulExtended: {
    static InstructionInfo result = {
        "OpSMulExtended",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAny: {
    static InstructionInfo result = {
        "OpAny",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAll: {
    static InstructionInfo result = {
        "OpAll",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIsNan: {
    static InstructionInfo result = {
        "OpIsNan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIsInf: {
    static InstructionInfo result = {
        "OpIsInf",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIsFinite: {
    static InstructionInfo result = {
        "OpIsFinite",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIsNormal: {
    static InstructionInfo result = {
        "OpIsNormal",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSignBitSet: {
    static InstructionInfo result = {
        "OpSignBitSet",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLessOrGreater: {
    static InstructionInfo result = {
        "OpLessOrGreater",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpOrdered: {
    static InstructionInfo result = {
        "OpOrdered",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUnordered: {
    static InstructionInfo result = {
        "OpUnordered",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLogicalEqual: {
    static InstructionInfo result = {
        "OpLogicalEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLogicalNotEqual: {
    static InstructionInfo result = {
        "OpLogicalNotEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLogicalOr: {
    static InstructionInfo result = {
        "OpLogicalOr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLogicalAnd: {
    static InstructionInfo result = {
        "OpLogicalAnd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLogicalNot: {
    static InstructionInfo result = {
        "OpLogicalNot",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSelect: {
    static InstructionInfo result = {
        "OpSelect",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIEqual: {
    static InstructionInfo result = {
        "OpIEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpINotEqual: {
    static InstructionInfo result = {
        "OpINotEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUGreaterThan: {
    static InstructionInfo result = {
        "OpUGreaterThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSGreaterThan: {
    static InstructionInfo result = {
        "OpSGreaterThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpUGreaterThanEqual: {
    static InstructionInfo result = {
        "OpUGreaterThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSGreaterThanEqual: {
    static InstructionInfo result = {
        "OpSGreaterThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpULessThan: {
    static InstructionInfo result = {
        "OpULessThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSLessThan: {
    static InstructionInfo result = {
        "OpSLessThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpULessThanEqual: {
    static InstructionInfo result = {
        "OpULessThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSLessThanEqual: {
    static InstructionInfo result = {
        "OpSLessThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdEqual: {
    static InstructionInfo result = {
        "OpFOrdEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordEqual: {
    static InstructionInfo result = {
        "OpFUnordEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdNotEqual: {
    static InstructionInfo result = {
        "OpFOrdNotEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordNotEqual: {
    static InstructionInfo result = {
        "OpFUnordNotEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdLessThan: {
    static InstructionInfo result = {
        "OpFOrdLessThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordLessThan: {
    static InstructionInfo result = {
        "OpFUnordLessThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdGreaterThan: {
    static InstructionInfo result = {
        "OpFOrdGreaterThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordGreaterThan: {
    static InstructionInfo result = {
        "OpFUnordGreaterThan",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdLessThanEqual: {
    static InstructionInfo result = {
        "OpFOrdLessThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordLessThanEqual: {
    static InstructionInfo result = {
        "OpFUnordLessThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFOrdGreaterThanEqual: {
    static InstructionInfo result = {
        "OpFOrdGreaterThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFUnordGreaterThanEqual: {
    static InstructionInfo result = {
        "OpFUnordGreaterThanEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpShiftRightLogical: {
    static InstructionInfo result = {
        "OpShiftRightLogical",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpShiftRightArithmetic: {
    static InstructionInfo result = {
        "OpShiftRightArithmetic",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpShiftLeftLogical: {
    static InstructionInfo result = {
        "OpShiftLeftLogical",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitwiseOr: {
    static InstructionInfo result = {
        "OpBitwiseOr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitwiseXor: {
    static InstructionInfo result = {
        "OpBitwiseXor",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitwiseAnd: {
    static InstructionInfo result = {
        "OpBitwiseAnd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpNot: {
    static InstructionInfo result = {
        "OpNot",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitFieldInsert: {
    static InstructionInfo result = {
        "OpBitFieldInsert",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitFieldSExtract: {
    static InstructionInfo result = {
        "OpBitFieldSExtract",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitFieldUExtract: {
    static InstructionInfo result = {
        "OpBitFieldUExtract",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitReverse: {
    static InstructionInfo result = {
        "OpBitReverse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBitCount: {
    static InstructionInfo result = {
        "OpBitCount",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdx: {
    static InstructionInfo result = {
        "OpDPdx",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdy: {
    static InstructionInfo result = {
        "OpDPdy",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFwidth: {
    static InstructionInfo result = {
        "OpFwidth",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdxFine: {
    static InstructionInfo result = {
        "OpDPdxFine",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdyFine: {
    static InstructionInfo result = {
        "OpDPdyFine",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFwidthFine: {
    static InstructionInfo result = {
        "OpFwidthFine",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdxCoarse: {
    static InstructionInfo result = {
        "OpDPdxCoarse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpDPdyCoarse: {
    static InstructionInfo result = {
        "OpDPdyCoarse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpFwidthCoarse: {
    static InstructionInfo result = {
        "OpFwidthCoarse",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpEmitVertex: {
    static InstructionInfo result = {
        "OpEmitVertex", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpEndPrimitive: {
    static InstructionInfo result = {
        "OpEndPrimitive", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpEmitStreamVertex: {
    static InstructionInfo result = {
        "OpEmitStreamVertex", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpEndStreamPrimitive: {
    static InstructionInfo result = {
        "OpEndStreamPrimitive", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpControlBarrier: {
    static InstructionInfo result = {
        "OpControlBarrier", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpMemoryBarrier: {
    static InstructionInfo result = {
        "OpMemoryBarrier", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpAtomicLoad: {
    static InstructionInfo result = {
        "OpAtomicLoad",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicStore: {
    static InstructionInfo result = {
        "OpAtomicStore", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpAtomicExchange: {
    static InstructionInfo result = {
        "OpAtomicExchange",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicCompareExchange: {
    static InstructionInfo result = {
        "OpAtomicCompareExchange",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicCompareExchangeWeak: {
    static InstructionInfo result = {
        "OpAtomicCompareExchangeWeak",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicIIncrement: {
    static InstructionInfo result = {
        "OpAtomicIIncrement",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicIDecrement: {
    static InstructionInfo result = {
        "OpAtomicIDecrement",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicIAdd: {
    static InstructionInfo result = {
        "OpAtomicIAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicISub: {
    static InstructionInfo result = {
        "OpAtomicISub",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicSMin: {
    static InstructionInfo result = {
        "OpAtomicSMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicUMin: {
    static InstructionInfo result = {
        "OpAtomicUMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicSMax: {
    static InstructionInfo result = {
        "OpAtomicSMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicUMax: {
    static InstructionInfo result = {
        "OpAtomicUMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicAnd: {
    static InstructionInfo result = {
        "OpAtomicAnd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicOr: {
    static InstructionInfo result = {
        "OpAtomicOr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicXor: {
    static InstructionInfo result = {
        "OpAtomicXor",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPhi: {
    static InstructionInfo result = {
        "OpPhi",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpLoopMerge: {
    static InstructionInfo result = {"OpLoopMerge", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpSelectionMerge: {
    static InstructionInfo result = {
        "OpSelectionMerge", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpLabel: {
    static InstructionInfo result = {
        "OpLabel", InstructionFlags::HasResult, {}};
    return &result;
  }
  case spv::Op::OpBranch: {
    static InstructionInfo result = {"OpBranch", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpBranchConditional: {
    static InstructionInfo result = {
        "OpBranchConditional", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpSwitch: {
    static InstructionInfo result = {"OpSwitch", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpKill: {
    static InstructionInfo result = {"OpKill", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpReturn: {
    static InstructionInfo result = {"OpReturn", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpReturnValue: {
    static InstructionInfo result = {
        "OpReturnValue", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpUnreachable: {
    static InstructionInfo result = {
        "OpUnreachable", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpLifetimeStart: {
    static InstructionInfo result = {
        "OpLifetimeStart", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpLifetimeStop: {
    static InstructionInfo result = {
        "OpLifetimeStop", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGroupAsyncCopy: {
    static InstructionInfo result = {
        "OpGroupAsyncCopy",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupWaitEvents: {
    static InstructionInfo result = {
        "OpGroupWaitEvents", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGroupAll: {
    static InstructionInfo result = {
        "OpGroupAll",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupAny: {
    static InstructionInfo result = {
        "OpGroupAny",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupBroadcast: {
    static InstructionInfo result = {
        "OpGroupBroadcast",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupIAdd: {
    static InstructionInfo result = {
        "OpGroupIAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupFAdd: {
    static InstructionInfo result = {
        "OpGroupFAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupFMin: {
    static InstructionInfo result = {
        "OpGroupFMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupUMin: {
    static InstructionInfo result = {
        "OpGroupUMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupSMin: {
    static InstructionInfo result = {
        "OpGroupSMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupFMax: {
    static InstructionInfo result = {
        "OpGroupFMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupUMax: {
    static InstructionInfo result = {
        "OpGroupUMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupSMax: {
    static InstructionInfo result = {
        "OpGroupSMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpReadPipe: {
    static InstructionInfo result = {
        "OpReadPipe",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpWritePipe: {
    static InstructionInfo result = {
        "OpWritePipe",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpReservedReadPipe: {
    static InstructionInfo result = {
        "OpReservedReadPipe",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpReservedWritePipe: {
    static InstructionInfo result = {
        "OpReservedWritePipe",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpReserveReadPipePackets: {
    static InstructionInfo result = {
        "OpReserveReadPipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpReserveWritePipePackets: {
    static InstructionInfo result = {
        "OpReserveWritePipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCommitReadPipe: {
    static InstructionInfo result = {
        "OpCommitReadPipe", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCommitWritePipe: {
    static InstructionInfo result = {
        "OpCommitWritePipe", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpIsValidReserveId: {
    static InstructionInfo result = {
        "OpIsValidReserveId",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetNumPipePackets: {
    static InstructionInfo result = {
        "OpGetNumPipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetMaxPipePackets: {
    static InstructionInfo result = {
        "OpGetMaxPipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupReserveReadPipePackets: {
    static InstructionInfo result = {
        "OpGroupReserveReadPipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupReserveWritePipePackets: {
    static InstructionInfo result = {
        "OpGroupReserveWritePipePackets",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupCommitReadPipe: {
    static InstructionInfo result = {
        "OpGroupCommitReadPipe", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGroupCommitWritePipe: {
    static InstructionInfo result = {
        "OpGroupCommitWritePipe", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpEnqueueMarker: {
    static InstructionInfo result = {
        "OpEnqueueMarker",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpEnqueueKernel: {
    static InstructionInfo result = {
        "OpEnqueueKernel",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelNDrangeSubGroupCount: {
    static InstructionInfo result = {
        "OpGetKernelNDrangeSubGroupCount",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelNDrangeMaxSubGroupSize: {
    static InstructionInfo result = {
        "OpGetKernelNDrangeMaxSubGroupSize",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelWorkGroupSize: {
    static InstructionInfo result = {
        "OpGetKernelWorkGroupSize",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelPreferredWorkGroupSizeMultiple: {
    static InstructionInfo result = {
        "OpGetKernelPreferredWorkGroupSizeMultiple",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpRetainEvent: {
    static InstructionInfo result = {
        "OpRetainEvent", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpReleaseEvent: {
    static InstructionInfo result = {
        "OpReleaseEvent", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCreateUserEvent: {
    static InstructionInfo result = {
        "OpCreateUserEvent",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpIsValidEvent: {
    static InstructionInfo result = {
        "OpIsValidEvent",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSetUserEventStatus: {
    static InstructionInfo result = {
        "OpSetUserEventStatus", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpCaptureEventProfilingInfo: {
    static InstructionInfo result = {
        "OpCaptureEventProfilingInfo", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGetDefaultQueue: {
    static InstructionInfo result = {
        "OpGetDefaultQueue",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpBuildNDRange: {
    static InstructionInfo result = {
        "OpBuildNDRange",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleImplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleExplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleDrefImplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleDrefImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleDrefExplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleDrefExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleProjImplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleProjImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleProjExplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleProjExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleProjDrefImplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleProjDrefImplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseSampleProjDrefExplicitLod: {
    static InstructionInfo result = {
        "OpImageSparseSampleProjDrefExplicitLod",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseFetch: {
    static InstructionInfo result = {
        "OpImageSparseFetch",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseGather: {
    static InstructionInfo result = {
        "OpImageSparseGather",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseDrefGather: {
    static InstructionInfo result = {
        "OpImageSparseDrefGather",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpImageSparseTexelsResident: {
    static InstructionInfo result = {
        "OpImageSparseTexelsResident",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpNoLine: {
    static InstructionInfo result = {"OpNoLine", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpAtomicFlagTestAndSet: {
    static InstructionInfo result = {
        "OpAtomicFlagTestAndSet",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpAtomicFlagClear: {
    static InstructionInfo result = {
        "OpAtomicFlagClear", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpImageSparseRead: {
    static InstructionInfo result = {
        "OpImageSparseRead",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpSizeOf: {
    static InstructionInfo result = {
        "OpSizeOf",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpTypePipeStorage: {
    static InstructionInfo result = {"OpTypePipeStorage",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpConstantPipeStorage: {
    static InstructionInfo result = {
        "OpConstantPipeStorage",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCreatePipeFromPipeStorage: {
    static InstructionInfo result = {
        "OpCreatePipeFromPipeStorage",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelLocalSizeForSubgroupCount: {
    static InstructionInfo result = {
        "OpGetKernelLocalSizeForSubgroupCount",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGetKernelMaxNumSubgroups: {
    static InstructionInfo result = {
        "OpGetKernelMaxNumSubgroups",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpTypeNamedBarrier: {
    static InstructionInfo result = {"OpTypeNamedBarrier",
                                     InstructionFlags::HasResult,
                                     {}};
    return &result;
  }
  case spv::Op::OpNamedBarrierInitialize: {
    static InstructionInfo result = {
        "OpNamedBarrierInitialize",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpMemoryNamedBarrier: {
    static InstructionInfo result = {
        "OpMemoryNamedBarrier", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpModuleProcessed: {
    static InstructionInfo result = {
        "OpModuleProcessed", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpExecutionModeId: {
    static InstructionInfo result = {
        "OpExecutionModeId", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpDecorateId: {
    static InstructionInfo result = {
        "OpDecorateId", InstructionFlags::None, {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformElect: {
    static InstructionInfo result = {
        "OpGroupNonUniformElect",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformAll: {
    static InstructionInfo result = {
        "OpGroupNonUniformAll",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformAny: {
    static InstructionInfo result = {
        "OpGroupNonUniformAny",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformAllEqual: {
    static InstructionInfo result = {
        "OpGroupNonUniformAllEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBroadcast: {
    static InstructionInfo result = {
        "OpGroupNonUniformBroadcast",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBroadcastFirst: {
    static InstructionInfo result = {
        "OpGroupNonUniformBroadcastFirst",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBallot: {
    static InstructionInfo result = {
        "OpGroupNonUniformBallot",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformInverseBallot: {
    static InstructionInfo result = {
        "OpGroupNonUniformInverseBallot",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBallotBitExtract: {
    static InstructionInfo result = {
        "OpGroupNonUniformBallotBitExtract",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBallotBitCount: {
    static InstructionInfo result = {
        "OpGroupNonUniformBallotBitCount",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBallotFindLSB: {
    static InstructionInfo result = {
        "OpGroupNonUniformBallotFindLSB",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBallotFindMSB: {
    static InstructionInfo result = {
        "OpGroupNonUniformBallotFindMSB",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformShuffle: {
    static InstructionInfo result = {
        "OpGroupNonUniformShuffle",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformShuffleXor: {
    static InstructionInfo result = {
        "OpGroupNonUniformShuffleXor",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformShuffleUp: {
    static InstructionInfo result = {
        "OpGroupNonUniformShuffleUp",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformShuffleDown: {
    static InstructionInfo result = {
        "OpGroupNonUniformShuffleDown",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformIAdd: {
    static InstructionInfo result = {
        "OpGroupNonUniformIAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformFAdd: {
    static InstructionInfo result = {
        "OpGroupNonUniformFAdd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformIMul: {
    static InstructionInfo result = {
        "OpGroupNonUniformIMul",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformFMul: {
    static InstructionInfo result = {
        "OpGroupNonUniformFMul",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformSMin: {
    static InstructionInfo result = {
        "OpGroupNonUniformSMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformUMin: {
    static InstructionInfo result = {
        "OpGroupNonUniformUMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformFMin: {
    static InstructionInfo result = {
        "OpGroupNonUniformFMin",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformSMax: {
    static InstructionInfo result = {
        "OpGroupNonUniformSMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformUMax: {
    static InstructionInfo result = {
        "OpGroupNonUniformUMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformFMax: {
    static InstructionInfo result = {
        "OpGroupNonUniformFMax",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBitwiseAnd: {
    static InstructionInfo result = {
        "OpGroupNonUniformBitwiseAnd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBitwiseOr: {
    static InstructionInfo result = {
        "OpGroupNonUniformBitwiseOr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformBitwiseXor: {
    static InstructionInfo result = {
        "OpGroupNonUniformBitwiseXor",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformLogicalAnd: {
    static InstructionInfo result = {
        "OpGroupNonUniformLogicalAnd",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformLogicalOr: {
    static InstructionInfo result = {
        "OpGroupNonUniformLogicalOr",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformLogicalXor: {
    static InstructionInfo result = {
        "OpGroupNonUniformLogicalXor",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformQuadBroadcast: {
    static InstructionInfo result = {
        "OpGroupNonUniformQuadBroadcast",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpGroupNonUniformQuadSwap: {
    static InstructionInfo result = {
        "OpGroupNonUniformQuadSwap",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpCopyLogical: {
    static InstructionInfo result = {
        "OpCopyLogical",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPtrEqual: {
    static InstructionInfo result = {
        "OpPtrEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPtrNotEqual: {
    static InstructionInfo result = {
        "OpPtrNotEqual",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  case spv::Op::OpPtrDiff: {
    static InstructionInfo result = {
        "OpPtrDiff",
        InstructionFlags::HasResult | InstructionFlags::HasResultType,
        {}};
    return &result;
  }
  }

  return nullptr;
}

inline void dump(std::span<const std::uint32_t> range,
                 void (*printId)(std::uint32_t id) = nullptr) {
  if (printId == nullptr) {
    printId = [](uint32_t id) { std::printf("%%%u", id); };
  }

  while (!range.empty()) {
    auto opWordCount = range[0];
    auto op = static_cast<spv::Op>(opWordCount & spv::OpCodeMask);
    auto wordCount = opWordCount >> spv::WordCountShift;

    if (range.size() < wordCount || wordCount == 0) {
      std::printf("<corrupted data>\n");

      for (auto word : range) {
        std::printf("%08x ", (unsigned)word);
      }

      std::printf("\n");

      break;
    }

    auto info = getInstructionInfo(op);

    if (info == nullptr) {
      std::printf("unknown instruction\n");
      range = range.subspan(wordCount);
      continue;
    }

    auto word = range.data() + 1;
    auto wordEnd = range.data() + wordCount;
    bool isFirst = true;

    if ((info->flags & InstructionFlags::HasResult) ==
        InstructionFlags::HasResult) {
      std::uint32_t outputTypeId = 0;

      if ((info->flags & InstructionFlags::HasResultType) ==
          InstructionFlags::HasResultType) {
        if (word < wordEnd) {
          outputTypeId = *word++;
        }
      }

      std::uint32_t outputId = word < wordEnd ? *word++ : 0;

      printId(outputId);
      if ((info->flags & InstructionFlags::HasResultType) ==
          InstructionFlags::HasResultType) {
        std::printf(": ");
        printId(outputTypeId);
      }

      std::printf(" = ");
    }

    std::printf("%s(", info->name);

    for (auto &op : std::span(info->operands)) {
      if (op == OperandKind::Invalid) {
        break;
      }

      if (word >= wordEnd) {
        if (op == OperandKind::VariadicWord ||
            op == OperandKind::VariadicId) {
          break;
        }

        std::printf("<corrupted>\n");
        break;
      }

      auto currentWord = *word++;

      if (isFirst) {
        isFirst = false;
      } else {
        std::printf(", ");
      }

      if (op == OperandKind::VariadicId ||
          op == OperandKind::TypeId || op == OperandKind::ValueId) {
        printId(currentWord);
      } else if (op == OperandKind::Word ||
                 op == OperandKind::VariadicWord) {
        std::printf("%u", currentWord);
      } else if (op == OperandKind::String) {
        bool foundEnd = false;
        while (true) {
          if (reinterpret_cast<const char *>(currentWord)[3] == '\0') {
            foundEnd = true;
            break;
          }

          if (word >= wordEnd) {
            break;
          }

          currentWord = *word++;
        }

        if (foundEnd) {
          std::printf("'%s'", reinterpret_cast<const char *>(word - 1));
        } else {
          std::printf("<corrupted string>");
        }
      } else {
        std::printf("<invalid>");
      }
    }

    while (word < wordEnd) {
      if (isFirst) {
        isFirst = false;
      } else {
        std::printf(", ");
      }

      auto currentWord = *word++;

      std::printf("%u", currentWord);
    }

    std::printf(")\n");
    range = range.subspan(wordCount);
  }
}
} // namespace spirv