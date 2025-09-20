#pragma once
#include "SpvTypeInfo.hpp"
#include "dialect/spv.hpp"
#include "spv.hpp"

namespace shader::spv {
struct Import : ir::CloneMap {
  ir::Node getOrCloneImpl(ir::Context &context, ir::Node node,
                          bool isOperand) override;
};

struct Context : ir::Context {
  BinaryLayout layout;
  ir::Location rootLocation;

  ir::NameStorage ns;
  ir::Value perVertex;
  std::map<int, ir::Value> outputs;
  std::map<int, ir::Value> inputs;
  ir::Value fragDepth;

  ir::RegionLike localVariables;
  ir::RegionLike epilogue;
  ir::Value entryPoint;

  std::map<ir::InstructionId, std::vector<ir::Value>> globals;
  std::map<ir::InstructionId, std::vector<ir::Value>> constants;

  Context();

  ir::Region createRegion(ir::Location loc);
  ir::Value createRegionWithLabel(ir::Location loc);

  void setName(ir::spv::IdRef inst, std::string name);
  void setConstantName(ir::Value constant);

  ir::Value getOrCreateConstant(ir::Value typeValue, const ir::Operand &value);
  ir::Value getNull(ir::Value typeValue);
  ir::Value getUndef(ir::Value typeValue);

  ir::Value getType(ir::spv::Op baseType, int width, bool isSigned);
  ir::Value getType(const TypeInfo &info);

  ir::Value imm64(std::uint64_t value) {
    return getOrCreateConstant(getTypeUInt64(), value);
  }
  ir::Value imm32(std::uint32_t value) {
    return getOrCreateConstant(getTypeUInt32(), value);
  }

  ir::Value simm64(std::int64_t value) {
    return getOrCreateConstant(getTypeSInt64(), value);
  }
  ir::Value simm32(std::int32_t value) {
    return getOrCreateConstant(getTypeSInt32(), value);
  }
  ir::Value fimm64(double value) {
    return getOrCreateConstant(getTypeFloat(64), value);
  }
  ir::Value fimm32(float value) {
    return getOrCreateConstant(getTypeFloat(32), value);
  }
  ir::Value getBool(bool value) { return value ? getTrue() : getFalse(); }
  ir::Value getTrue() {
    return getOrCreateGlobal(ir::spv::OpConstantTrue, {{getTypeBool()}});
  }
  ir::Value getFalse() {
    return getOrCreateGlobal(ir::spv::OpConstantFalse, {{getTypeBool()}});
  }

  ir::Value getIndex(std::int32_t index) { return simm32(index); }

  void setTypeName(ir::Value type);

  void addGlobal(ir::Value type) {
    globals[type.getInstId()].push_back(type);
    setTypeName(type);
  }

  ir::Value findGlobal(ir::spv::Op op,
                       std::span<const ir::Operand> operands = {}) const;
  ir::Value createGlobal(ir::spv::Op op, std::span<const ir::Operand> operands);
  ir::Value getOrCreateGlobal(ir::spv::Op op,
                              std::span<const ir::Operand> operands = {});

  ir::Value getTypeInt(int width, bool sign) {
    return getOrCreateGlobal(ir::spv::OpTypeInt, {{width, sign ? 1 : 0}});
  }
  ir::Value getTypeFloat(int width) {
    return getOrCreateGlobal(ir::spv::OpTypeFloat, {{width}});
  }
  ir::Value getTypeVoid() { return getOrCreateGlobal(ir::spv::OpTypeVoid); }
  ir::Value getTypeBool() { return getOrCreateGlobal(ir::spv::OpTypeBool); }
  ir::Value getTypeSampler() {
    return getOrCreateGlobal(ir::spv::OpTypeSampler);
  }
  ir::Value getTypeArray(ir::Value elementType, ir::Value count) {
    return getOrCreateGlobal(ir::spv::OpTypeArray, {{elementType, count}});
  }
  ir::Value getTypeVector(ir::Value elementType, int count) {
    return getOrCreateGlobal(ir::spv::OpTypeVector, {{elementType, count}});
  }

  ir::Value getTypeStruct(auto... elements) {
    return getOrCreateGlobal(ir::spv::OpTypeStruct, {{elements...}});
  }
  ir::Value getTypeSInt8() { return getTypeInt(8, true); }
  ir::Value getTypeUInt8() { return getTypeInt(8, false); }
  ir::Value getTypeSInt16() { return getTypeInt(16, true); }
  ir::Value getTypeUInt16() { return getTypeInt(16, false); }
  ir::Value getTypeSInt32() { return getTypeInt(32, true); }
  ir::Value getTypeUInt32() { return getTypeInt(32, false); }
  ir::Value getTypeSInt64() { return getTypeInt(64, true); }
  ir::Value getTypeUInt64() { return getTypeInt(64, false); }
  ir::Value getTypeFloat16() { return getTypeFloat(16); }
  ir::Value getTypeFloat32() { return getTypeFloat(32); }
  ir::Value getTypeFloat64() { return getTypeFloat(64); }

  ir::Value getTypeFunction(ir::Value returnType,
                            std::span<const ir::Value> params) {
    std::vector<ir::Operand> operands;
    operands.reserve(1 + params.size());
    operands.push_back(returnType);
    for (auto param : params) {
      operands.push_back(param);
    }
    return getOrCreateGlobal(ir::spv::OpTypeFunction, operands);
  }

  ir::Value getTypePointer(ir::spv::StorageClass storageClass,
                           ir::spv::IdRef pointeeType) {
    return getOrCreateGlobal(ir::spv::OpTypePointer,
                             {{storageClass, pointeeType}});
  }

  ir::Value getTypeImage(ir::spv::IdRef sampledType, ir::spv::Dim dim,
                         std::int32_t depth, bool arrayed, bool multisampled,
                         std::int32_t sampled, ir::spv::ImageFormat format) {
    return getOrCreateGlobal(
        ir::spv::OpTypeImage,
        {{sampledType, dim, depth, arrayed, multisampled, sampled, format}});
  }

  ir::Value getOperandValue(const ir::Operand &op, ir::Value type = {});

  void createPerVertex();

  ir::Value createUniformBuffer(int descriptorSet, int binding,
                                ir::Value structType);

  ir::Value createRuntimeArrayUniformBuffer(int descriptorSet, int binding,
                                            ir::Value elementType);

  ir::Value createOutput(ir::Location loc, int index);
  ir::Value createInput(ir::Location loc, int index);
  ir::Value createAttr(ir::Location loc, int attrId, bool perVertex, bool flat);

  ir::Value createFragDepth(ir::Location loc);
};
} // namespace shader::spv
