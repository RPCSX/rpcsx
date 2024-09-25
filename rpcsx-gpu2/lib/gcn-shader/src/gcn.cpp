#include "gcn.hpp"

#include "Evaluator.hpp"
#include "SemanticInfo.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "ir.hpp"

#include <bit>
#include <functional>
#include <iostream>

#include "GcnInstruction.hpp"
#include "dialect.hpp"
#include "ir/Region.hpp"
#include "ir/Value.hpp"

#include "spv.hpp"
#include "transform.hpp"
#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>
#include <map>
#include <optional>
#include <print>
#include <spirv-tools/libspirv.h>
#include <type_traits>
#include <unordered_map>
#include <vector>

using namespace shader;
using namespace shader::spv;

void dump(auto... objects) {
  ir::NameStorage ns;
  ((objects.print(std::cerr, ns), std::cerr << "\n"), ...);
}

inline shader::spv::TypeInfo getRegisterInfo(unsigned id) {
  switch (gcn::RegId(id)) {
  case gcn::RegId::Sgpr:
    return {
        .baseType = ir::spv::OpTypeArray,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 256,
    };
  case gcn::RegId::Vgpr:
    return {
        .baseType = ir::spv::OpTypeArray,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 512,
    };
  case gcn::RegId::M0:
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 1,
    };
  case gcn::RegId::Scc:
    return {
        .baseType = ir::spv::OpTypeBool,
        .componentWidth = 1,
        .componentsCount = 1,
    };
  case gcn::RegId::Vcc:
    return {
        .baseType = ir::spv::OpTypeVector,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 2,
    };
  case gcn::RegId::Exec:
    return {
        .baseType = ir::spv::OpTypeVector,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 2,
    };
  case gcn::RegId::VccZ:
    return {
        .baseType = ir::spv::OpTypeBool,
        .componentWidth = 1,
        .componentsCount = 1,
    };
  case gcn::RegId::ExecZ:
    return {
        .baseType = ir::spv::OpTypeBool,
        .componentWidth = 1,
        .componentsCount = 1,
    };
  case gcn::RegId::LdsDirect:
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 1,
    };
  case gcn::RegId::SgprCount:
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 1,
    };
  case gcn::RegId::VgprCount:
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 1,
    };
  case gcn::RegId::ThreadId:
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 1,
    };

  case gcn::RegId::MemoryTable:
    return {
        .baseType = ir::spv::OpTypeVector,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 2,
    };

  case gcn::RegId::Gds:
    return {
        .baseType = ir::spv::OpTypeVector,
        .componentType = ir::spv::OpTypeInt,
        .componentWidth = 32,
        .componentsCount = 2,
    };
  }

  std::abort();
}

inline const char *getRegisterName(unsigned id) {
  switch (gcn::RegId(id)) {
  case gcn::RegId::Sgpr:
    return "sgpr";
  case gcn::RegId::Vgpr:
    return "vgpr";
  case gcn::RegId::M0:
    return "M0";
  case gcn::RegId::Scc:
    return "scc";
  case gcn::RegId::Vcc:
    return "vcc";
  case gcn::RegId::Exec:
    return "exec";
  case gcn::RegId::ExecZ:
    return "execz";
  case gcn::RegId::VccZ:
    return "vccz";
  case gcn::RegId::LdsDirect:
    return "lds_direct";
  case gcn::RegId::SgprCount:
    return "sgpr_count";
  case gcn::RegId::VgprCount:
    return "vgpr_count";
  case gcn::RegId::ThreadId:
    return "thread_id";
  case gcn::MemoryTable:
    return "memory_table";
  case gcn::Gds:
    return "gds";
  }
  std::abort();
}

static std::optional<gcn::RegId> getRegIdByName(std::string_view variableName) {
  if (variableName == "sgpr")
    return gcn::RegId::Sgpr;
  if (variableName == "vgpr")
    return gcn::RegId::Vgpr;
  if (variableName == "m0")
    return gcn::RegId::M0;
  if (variableName == "scc")
    return gcn::RegId::Scc;
  if (variableName == "vcc")
    return gcn::RegId::Vcc;
  if (variableName == "exec")
    return gcn::RegId::Exec;
  if (variableName == "lds_direct")
    return gcn::RegId::LdsDirect;
  if (variableName == "sgpr_count")
    return gcn::RegId::SgprCount;
  if (variableName == "vgpr_count")
    return gcn::RegId::VgprCount;
  if (variableName == "thread_id")
    return gcn::RegId::ThreadId;
  if (variableName == "memory_table")
    return gcn::RegId::MemoryTable;
  if (variableName == "gds")
    return gcn::RegId::Gds;

  return {};
}

struct AddressLocationBuilder {
  ir::Context *context = nullptr;

  ir::Location getLocation(std::uint64_t ptr) {
    return context->getMemoryLocation(ptr, 4);
  }
};

void gcn::collectSemanticModuleInfo(SemanticModuleInfo &moduleInfo,
                                    const spv::BinaryLayout &layout) {
  shader::collectSemanticModuleInfo(moduleInfo, layout);

  auto debugs = layout.regions[spv::BinaryLayout::kDebugs];
  if (debugs == nullptr) {
    return;
  }

  for (auto inst : debugs.children()) {
    if (inst != ir::spv::OpName) {
      continue;
    }

    auto namedNode = inst.getOperand(0).getAsValue();
    auto name = inst.getOperand(1).getAsString();

    if (namedNode != ir::spv::OpVariable || name == nullptr) {
      continue;
    }

    if (auto storage = namedNode.getOperand(1).getAsInt32();
        storage == nullptr ||
        *storage == int(ir::spv::StorageClass::Function)) {
      continue;
    }

    if (auto regId = getRegIdByName(*name)) {
      if (shader::spv::getTypeInfo(
              namedNode.getOperand(0).getAsValue().getOperand(1).getAsValue())
              .width() != getRegisterInfo(*regId).width()) {
        std::fprintf(stderr,
                     "unexpected type width for register variable "
                     "'%s', expected %u\n",
                     name->c_str(), getRegisterInfo(*regId).width());
        std::abort();
      }

      moduleInfo.registerVariables[*regId] = namedNode;
    }
  }
}

std::pair<ir::Value, bool>
gcn::Context::getOrCreateLabel(ir::Location loc, ir::Region body,
                               std::uint64_t address) {
  auto it = instructions.lower_bound(address);

  bool exists = false;
  if (it != instructions.end() && it->first == address) {
    if (it->second == ir::spv::OpLabel) {
      return {it->second.staticCast<ir::Value>(), false};
    }

    auto injectedLabel =
        Builder::createInsertBefore(*this, it->second).createSpvLabel(loc);
    it->second = injectedLabel;
    return {injectedLabel, false};
  }

  ir::Value newLabel;
  if (it == instructions.end()) {
    newLabel = Builder::createAppend(*this, body).createSpvLabel(loc);
  } else {
    newLabel =
        Builder::createInsertBefore(*this, it->second).createSpvLabel(loc);
  }

  instructions.emplace_hint(it, address, newLabel);
  return {newLabel, true};
}

gcn::Builder gcn::Context::createBuilder(gcn::InstructionRegion &region,
                                         ir::Region bodyRegion,
                                         std::uint64_t address) {
  auto it = instructions.lower_bound(address);

  if (it != instructions.end() && it->first == address) {
    if (it->second == nullptr) {
      region.base = bodyRegion;
      region.firstInstruction = &it->second;

      auto result = Builder::createAppend(*this, &region);
      result.setInsertionPoint(it->second.getPrev());
      return result;
    }

    ++it;

    if (it == instructions.end()) {
      return Builder::createAppend(*this, bodyRegion);
    }

    return Builder::createInsertBefore(*this, it->second);
  }

  auto newNodeIt = instructions.emplace_hint(it, address, ir::Instruction{});
  region.base = bodyRegion;
  region.firstInstruction = &newNodeIt->second;

  if (it != instructions.end()) {
    auto result = Builder::createAppend(*this, &region);
    result.setInsertionPoint(it->second.getPrev());
    return result;
  }

  auto result = Builder::createAppend(*this, &region);
  result.setInsertionPoint(bodyRegion.getLast());
  return result;
}

ir::Value gcn::Context::createCast(ir::Location loc, Builder &builder,
                                   ir::Value targetType, ir::Value value) {
  auto valueType = value.getOperand(0).getAsValue();
  if (targetType == valueType) {
    return value;
  }

  if (targetType == ir::spv::OpTypeArray ||
      targetType == ir::spv::OpTypeRuntimeArray ||
      valueType == ir::spv::OpTypeArray ||
      valueType == ir::spv::OpTypeRuntimeArray) {
    std::abort();
  }

  auto targetTypeInfo = shader::spv::getTypeInfo(targetType);
  auto valueTypeInfo =
      shader::spv::getTypeInfo(value.getOperand(0).getAsValue());

  if (targetTypeInfo.width() == valueTypeInfo.width()) {
    return builder.createSpvBitcast(loc, targetType, value);
  }

  if (targetTypeInfo.baseType == valueTypeInfo.baseType) {
    if (targetTypeInfo.width() == valueTypeInfo.width()) {
      std::abort();
    }

    if (targetTypeInfo.baseType == ir::spv::OpTypeInt) {
      auto sign = *targetType.getOperand(2).getAsInt32();
      if (sign == 0) {
        return builder.createSpvUConvert(loc, targetType, value);
      }

      return builder.createSpvSConvert(loc, targetType, value);
    }

    if (targetTypeInfo.baseType == ir::spv::OpTypeFloat) {
      return builder.createSpvFConvert(loc, targetType, value);
    }
  }

  // TODO

  dump(targetType);
  dump(value.getOperand(0).getAsValue());
  dump(value);
  std::abort();
}

ir::Value gcn::Context::getOrCreateRegisterVariable(gcn::RegId id) {
  auto &entity = registerVariables[id];

  if (entity != nullptr) {
    return entity;
  }

  auto location = rootLocation;

  ir::Value regT;
  auto regInfo = getRegisterInfo(id);

  switch (regInfo.baseType) {
  case ir::spv::OpTypeBool:
    regT = getTypeBool();
    break;

  case ir::spv::OpTypeInt:
    regT = getTypeInt(regInfo.componentWidth, regInfo.isSigned);
    break;

  case ir::spv::OpTypeFloat:
    regT = getTypeFloat(regInfo.componentWidth);
    break;

  case ir::spv::OpTypeArray: {
    auto cLen = getIndex(regInfo.componentsCount);
    regT = getTypeArray(getTypeUInt32(), cLen);
    break;
  }

  case ir::spv::OpTypeVector:
    regT = getTypeVector(getTypeInt(regInfo.componentWidth, 0),
                         regInfo.componentsCount);
    break;

  default:
    std::abort();
  }

  auto storageClass = ir::spv::StorageClass::Private;
  auto pRegTxN = getTypePointer(storageClass, regT);

  auto globals = Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
  auto debugs = Builder::createAppend(*this, layout.getOrCreateDebugs(*this));

  entity = globals.createSpvVariable(location, pRegTxN, storageClass);
  setName(entity, getRegisterName(id));
  return entity;
}

ir::Value gcn::Context::getRegisterRef(ir::Location loc, Builder &builder,
                                       RegId id, const ir::Operand &index,
                                       ir::Value lane) {
  auto variable = getOrCreateRegisterVariable(id);

  if (id == RegId::Vgpr && lane == nullptr) {
    lane = readReg(loc, builder, getTypeUInt32(), RegId::ThreadId, 0);
  }

  auto result = createRegisterAccess(builder, loc, variable, index, lane);

  if (result == variable) {
    setName(result, "&" + std::string(getRegisterName(id)));
  } else if (result == ir::spv::OpAccessChain) {
    if (auto i = index.getAsInt32()) {
      setName(result, std::string(getRegisterName(id)) + "[" +
                          std::to_string(*i) + "]");
    } else {
      setName(result, std::string(getRegisterName(id)) + "[n]");
    }
  }

  return result;
}

ir::Value gcn::Context::readReg(ir::Location loc, Builder &builder,
                                ir::Value typeValue, gcn::RegId id,
                                const ir::Operand &index, ir::Value lane) {
  auto regInfo = getRegisterInfo(id);
  auto valInfo = shader::spv::getTypeInfo(typeValue);

  int valWidth = valInfo.width();
  int regWidth = regInfo.componentWidth;

  if (regWidth == 1) {
    auto ref = getRegisterRef(loc, builder, id, index, lane);
    auto result = builder.createSpvLoad(loc, getTypeBool(), ref);

    if (valInfo.baseType == ir::spv::OpTypeInt) {
      if (valWidth == 32) {
        return builder.createSpvSelect(
            loc, typeValue, result,
            getOrCreateConstant(typeValue, static_cast<std::uint32_t>(1)),
            getOrCreateConstant(typeValue, static_cast<std::uint32_t>(0)));
      }

      if (valWidth == 64) {
        return builder.createSpvSelect(
            loc, typeValue, result,
            getOrCreateConstant(typeValue, static_cast<std::uint64_t>(1)),
            getOrCreateConstant(typeValue, static_cast<std::uint64_t>(0)));
      }
    }

    std::abort();
  }

  if (valWidth == regWidth) {
    auto ref = getRegisterRef(loc, builder, id, index, lane);
    auto regType = ref.getOperand(0).getAsValue().getOperand(1).getAsValue();

    auto result = builder.createSpvLoad(loc, regType, ref);
    if (regType == typeValue) {
      return result;
    }

    return builder.createSpvBitcast(loc, typeValue, result);
  }

  if (valWidth < regWidth || (valWidth % regWidth) != 0) {
    std::abort();
  }

  int regCount = valWidth / regWidth;
  auto sint32 = getTypeSInt32();
  auto channelType = getTypeInt(regWidth, false);
  auto splittedType = regCount > 4 ? getTypeArray(channelType, imm32(regCount))
                                   : getTypeVector(channelType, regCount);

  std::vector<ir::spv::IdRef> compositeValues;

  for (int i = 0; i < regCount; ++i) {
    ir::Value ref;
    auto channel = getIndex(i);

    if (i == 0) {
      ref = getRegisterRef(loc, builder, id, index, lane);
    } else {
      if (auto constIndex = index.getAsInt32()) {
        ref = getRegisterRef(loc, builder, id, *constIndex + i, lane);
      } else {
        auto indexValue = index.getAsValue();
        auto indexType = indexValue.getOperand(0).getAsValue();
        auto channelIndex =
            builder.createSpvIAdd(loc, indexType, channel, indexValue);

        ref = getRegisterRef(loc, builder, id, channelIndex, lane);
      }
    }

    auto regType = ref.getOperand(0).getAsValue().getOperand(1).getAsValue();

    if (regType != channelType) {
      dump(regType, channelType);
      std::abort();
    }

    auto regValue = builder.createSpvLoad(loc, regType, ref);
    compositeValues.push_back(regValue);
  }

  auto result =
      builder.createSpvCompositeConstruct(loc, splittedType, compositeValues);
  if (splittedType == typeValue) {
    return result;
  }
  return builder.createSpvBitcast(loc, typeValue, result);
}

void gcn::Context::writeReg(ir::Location loc, Builder &builder, gcn::RegId id,
                            const ir::Operand &index, ir::Value value,
                            ir::Value lane) {
  auto regInfo = getRegisterInfo(id);
  auto valInfo = shader::spv::getTypeInfo(value.getOperand(0).getAsValue());

  int valWidth = valInfo.width();
  int regWidth = regInfo.componentWidth;

  if (valWidth == regWidth) {
    auto ref = getRegisterRef(loc, builder, id, index, lane);
    auto regType = ref.getOperand(0).getAsValue().getOperand(1).getAsValue();

    if (regType != value.getOperand(0)) {
      value = builder.createSpvBitcast(loc, regType, value);
    }

    builder.createSpvStore(loc, ref, value);
    return;
  }

  if (valWidth < regWidth || (valWidth % regWidth) != 0) {
    std::abort();
  }

  if (valInfo.baseType == ir::spv::OpTypeArray) {
    if (valInfo.componentWidth != regWidth) {
      std::abort();
    }

    auto elementType =
        value.getOperand(0).getAsValue().getOperand(0).getAsValue();

    for (int i = 0; i < valInfo.componentsCount; ++i) {
      ir::Value ref;

      if (i == 0) {
        ref = getRegisterRef(loc, builder, id, index, lane);
      } else {
        if (auto constIndex = index.getAsInt32()) {
          ref = getRegisterRef(loc, builder, id, *constIndex + i, lane);
        } else {
          auto indexValue = index.getAsValue();
          auto indexType = indexValue.getOperand(0).getAsValue();
          auto channelIndex =
              builder.createSpvIAdd(loc, indexType, getIndex(i), indexValue);

          ref = getRegisterRef(loc, builder, id, channelIndex, lane);
        }
      }

      auto regType = ref.getOperand(0).getAsValue().getOperand(1).getAsValue();

      auto element =
          builder.createSpvCompositeExtract(loc, elementType, value, {{i}});

      if (regType != elementType) {
        element = builder.createSpvBitcast(loc, regType, element);
      }

      builder.createSpvStore(loc, ref, element);
    }

    return;
  }

  int regCount = valWidth / regWidth;

  auto sint32 = getTypeSInt32();
  auto channelType = getTypeInt(regWidth, false);
  auto splittedType = regCount > 4 ? getTypeArray(channelType, imm32(regCount))
                                   : getTypeVector(channelType, regCount);
  auto splittedValue = builder.createSpvBitcast(loc, splittedType, value);

  for (int i = 0; i < regCount; ++i) {
    ir::Value ref;

    if (i == 0) {
      ref = getRegisterRef(loc, builder, id, index, lane);
    } else {
      if (auto constIndex = index.getAsInt32()) {
        ref = getRegisterRef(loc, builder, id, *constIndex + i, lane);
      } else {
        auto indexValue = index.getAsValue();
        auto indexType = indexValue.getOperand(0).getAsValue();
        auto channel = getIndex(i);
        auto channelIndex =
            builder.createSpvIAdd(loc, indexType, channel, indexValue);

        ref = getRegisterRef(loc, builder, id, channelIndex, lane);
      }
    }

    auto channelValue = builder.createSpvCompositeExtract(loc, channelType,
                                                          splittedValue, {{i}});
    builder.createSpvStore(loc, ref, channelValue);
  }
}

ir::Value gcn::Context::createRegisterAccess(Builder &builder, ir::Location loc,
                                             ir::Value reg,
                                             const ir::Operand &index,
                                             ir::Value lane) {
  auto regPointerType = reg.getOperand(0).getAsValue();
  if (regPointerType != ir::spv::OpTypePointer) {
    std::abort();
  }

  auto pointerStorageClass = static_cast<ir::spv::StorageClass>(
      *regPointerType.getOperand(0).getAsInt32());
  auto regPointeeType = regPointerType.getOperand(1).getAsValue();

  if (regPointeeType == nullptr) {
    std::abort();
  }

  if (lane != nullptr) {
    regPointeeType = regPointeeType.getOperand(0).getAsValue();

    if (regPointeeType == nullptr) {
      std::abort();
    }
  }

  auto regTypeInfo = shader::spv::getTypeInfo(regPointeeType);

  switch (regTypeInfo.baseType) {
  case ir::spv::OpTypeBool:
  case ir::spv::OpTypeInt:
  case ir::spv::OpTypeFloat:
    if (index != 0) {
      dump(index);
      std::abort();
    }

    return reg;

  case ir::spv::OpTypeVector:
  case ir::spv::OpTypeArray: {
    auto elementType = getType({
        .baseType = regTypeInfo.componentType,
        .componentWidth = regTypeInfo.componentWidth,
        .isSigned = regTypeInfo.isSigned,
    });
    auto indexValue = getOperandValue(index);
    auto pointeeType = getTypePointer(pointerStorageClass, elementType);

    if (lane == nullptr) {
      return builder.createSpvAccessChain(loc, pointeeType, reg,
                                          {{indexValue}});
    }

    return builder.createSpvAccessChain(loc, pointeeType, reg,
                                        {{lane, indexValue}});
  }

  default:
    std::abort();
  }
}

static ir::Value deserializeGcnRegion(
    gcn::Context &converter, const gcn::Environment &environment,
    const SemanticInfo &semInfo, std::uint64_t address,
    const std::function<std::uint32_t(std::uint64_t)> &readMemory,
    std::vector<ir::Instruction> &branchesToUnknown,
    std::unordered_set<std::uint64_t> &processed) {
  BinaryLayout &resultLayout = converter.layout;
  AddressLocationBuilder locBuilder{&converter};

  ir::Value boolTV = converter.getTypeBool();
  ir::Value float64TV = converter.getTypeFloat64();
  ir::Value float32TV = converter.getTypeFloat32();
  ir::Value uint16TV = converter.getTypeUInt16();
  ir::Value sint16TV = converter.getTypeSInt16();
  ir::Value uint32TV = converter.getTypeUInt32();
  ir::Value sint32TV = converter.getTypeSInt32();
  ir::Value uint64TV = converter.getTypeUInt64();
  ir::Value sint64TV = converter.getTypeSInt64();

  unsigned currentOp = 0;

  auto createOperandReadImpl = [&](ir::Location loc, gcn::Builder &builder,
                                   ir::Value type,
                                   const GcnOperand &op) -> ir::Value {
    switch (op.kind) {
    case GcnOperand::Kind::Constant: {
      auto createConstant = [&](auto value) {
        return converter.getOrCreateConstant(type, value);
        // return value;
      };

      if (type == float32TV) {
        return createConstant(std::bit_cast<float>(op.value));
      }

      if (type == sint64TV) {
        return createConstant(
            static_cast<std::uint64_t>(std::bit_cast<std::int32_t>(op.value)));
      }

      if (type == uint64TV) {
        return createConstant(
            static_cast<std::uint64_t>(std::bit_cast<std::uint32_t>(op.value)));
      }

      if (type == sint32TV) {
        return createConstant(std::bit_cast<std::int32_t>(op.value));
      }

      if (type == uint32TV) {
        return createConstant(std::bit_cast<std::uint32_t>(op.value));
      }

      if (type == sint16TV) {
        return createConstant(
            static_cast<std::int32_t>(static_cast<std::int16_t>(op.value)));
      }

      if (type == uint16TV) {
        return createConstant(
            static_cast<std::uint32_t>(static_cast<std::uint16_t>(op.value)));
      }
      if (type == boolTV) {
        return createConstant(op.value ? true : false);
      }
      break;
    }

    case GcnOperand::Kind::Immediate: {
      auto loc = locBuilder.getLocation(op.address);
      return builder.createValue(loc, ir::amdgpu::IMM, type, op.address);
    }
    case GcnOperand::Kind::VccLo:
      return converter.readReg(loc, builder, type, gcn::RegId::Vcc, 0);
    case GcnOperand::Kind::VccHi:
      return converter.readReg(loc, builder, type, gcn::RegId::Vcc, 1);
    case GcnOperand::Kind::M0:
      return converter.readReg(loc, builder, type, gcn::RegId::M0, 0);
    case GcnOperand::Kind::ExecLo:
      return converter.readReg(loc, builder, type, gcn::RegId::Exec, 0);
    case GcnOperand::Kind::ExecHi:
      return converter.readReg(loc, builder, type, gcn::RegId::Exec, 1);
    case GcnOperand::Kind::Scc:
      return converter.readReg(loc, builder, type, gcn::RegId::Scc, 0);
    case GcnOperand::Kind::VccZ:
      return converter.readReg(loc, builder, type, gcn::RegId::VccZ, 0);
    case GcnOperand::Kind::ExecZ:
      return converter.readReg(loc, builder, type, gcn::RegId::ExecZ, 0);
    case GcnOperand::Kind::LdsDirect:
      return converter.readReg(loc, builder, type, gcn::RegId::LdsDirect, 0);
    case GcnOperand::Kind::Vgpr:
      return converter.readReg(loc, builder, type, gcn::RegId::Vgpr, op.value);
    case GcnOperand::Kind::Sgpr:
      return converter.readReg(loc, builder, type, gcn::RegId::Sgpr, op.value);
    case GcnOperand::Kind::Attr: {
      auto f32 = converter.getTypeFloat32();
      auto attrChannelPtrType =
          converter.getTypePointer(ir::spv::StorageClass::Input, f32);
      auto resultType = converter.getTypeArray(f32, converter.simm32(3));

      auto attr =
          converter.createAttr(loc, op.attrId, environment.supportsBarycentric,
                               currentOp == ir::vintrp::MOV_F32);

      if (environment.supportsBarycentric) {
        ir::spv::IdRef compositeValues[3];
        for (int vertex = 0; vertex < 3; ++vertex) {
          auto ptr = builder.createSpvAccessChain(
              loc, attrChannelPtrType, attr,
              {{converter.imm32(vertex), converter.imm32(op.attrChannel)}});

          compositeValues[vertex] = builder.createSpvLoad(loc, f32, ptr);
        }

        return builder.createSpvCompositeConstruct(loc, type, compositeValues);
      } else {
        auto attrValue = builder.createSpvLoad(
            loc, converter.getTypeVector(float32TV, 4), attr);
        auto result = builder.createSpvCompositeExtract(
            loc, float32TV, attrValue, {{op.attrChannel}});

        return builder.createSpvCompositeConstruct(loc, type,
                                                   {{result, result, result}});
      }
    }
    case GcnOperand::Kind::Invalid:
      break;
    case GcnOperand::Kind::Buffer:
    case GcnOperand::Kind::Texture128:
    case GcnOperand::Kind::Texture256:
    case GcnOperand::Kind::Sampler:
    case GcnOperand::Kind::Pointer:
      break;
    }

    dump(type);
    std::abort();
  };

  auto createOperandRead = [&](ir::Location loc, gcn::Builder &builder,
                               ir::Value type,
                               const GcnOperand &op) -> ir::Value {
    switch (op.kind) {
    case GcnOperand::Kind::Buffer:
      return builder.createValue(
          loc, ir::amdgpu::VBUFFER, type, op.access,
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(0)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(1)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(2)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(3)));

    case GcnOperand::Kind::Texture128:
      return builder.createValue(
          loc, ir::amdgpu::TBUFFER, type, op.access,
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(0)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(1)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(2)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(3)));

    case GcnOperand::Kind::Texture256:
      return builder.createValue(
          loc, ir::amdgpu::TBUFFER, type, op.access,
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(0)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(1)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(2)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(3)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(4)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(5)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(6)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(7)));

    case GcnOperand::Kind::Sampler:
      return builder.createValue(
          loc, ir::amdgpu::SAMPLER, type,
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(0)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(1)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(2)),
          createOperandReadImpl(loc, builder, uint32TV,
                                op.getUnderlyingOperand(3)),
          op.samplerUnorm);

    case GcnOperand::Kind::Pointer:
      return builder.createValue(
          loc, ir::amdgpu::POINTER, type, op.pointeeSize,
          createOperandReadImpl(loc, builder, uint64TV,
                                op.getUnderlyingOperand()),
          createOperandReadImpl(loc, builder, sint32TV,
                                op.getPointerOffsetOperand()));

    default: {
      auto result = createOperandReadImpl(loc, builder, type, op);
      if (!op.abs && !op.neg) {
        return result;
      }

      return builder.createValue(loc, ir::amdgpu::NEG_ABS, type, op.neg, op.abs,
                                 result);
    }
    }
  };

  auto createOperandWrite = [&](ir::Location loc, gcn::Builder &builder,
                                const GcnOperand &op, ir::Value value,
                                ir::Value lane = nullptr) {
    if (op.clamp || op.omod != 0) {
      value = builder.createValue(loc, ir::amdgpu::OMOD, value.getOperand(0),
                                  op.clamp, op.omod, value);
    }

    switch (op.kind) {
    case GcnOperand::Kind::Constant:
    case GcnOperand::Kind::Immediate:
      break;
    case GcnOperand::Kind::VccLo:
      return converter.writeReg(loc, builder, gcn::RegId::Vcc, 0, value);
    case GcnOperand::Kind::VccHi:
      return converter.writeReg(loc, builder, gcn::RegId::Vcc, 1, value);
    case GcnOperand::Kind::M0:
      return converter.writeReg(loc, builder, gcn::RegId::M0, 0, value);
    case GcnOperand::Kind::ExecLo:
      return converter.writeReg(loc, builder, gcn::RegId::Exec, 0, value);
    case GcnOperand::Kind::ExecHi:
      return converter.writeReg(loc, builder, gcn::RegId::Exec, 1, value);
    case GcnOperand::Kind::Scc:
      return converter.writeReg(loc, builder, gcn::RegId::Scc, 0, value);
    case GcnOperand::Kind::VccZ:
    case GcnOperand::Kind::ExecZ:
      break;
    case GcnOperand::Kind::LdsDirect:
      return converter.writeReg(loc, builder, gcn::RegId::LdsDirect, 0, value);
    case GcnOperand::Kind::Vgpr:
      return converter.writeReg(loc, builder, gcn::RegId::Vgpr, op.value,
                                value);
    case GcnOperand::Kind::Sgpr:
      return converter.writeReg(loc, builder, gcn::RegId::Sgpr, op.value,
                                value);

    case GcnOperand::Kind::Attr:
    case GcnOperand::Kind::Buffer:
    case GcnOperand::Kind::Texture128:
    case GcnOperand::Kind::Texture256:
    case GcnOperand::Kind::Sampler:
    case GcnOperand::Kind::Pointer:
    case GcnOperand::Kind::Invalid:
      break;
    }

    std::abort();
  };

  if (converter.body == nullptr) {
    converter.body =
        converter.create<ir::Region>(locBuilder.getLocation(address));
  }
  auto bodyRegion = converter.body;
  gcn::InstructionRegion instRegion;

  auto regionEntry = converter
                         .getOrCreateLabel(locBuilder.getLocation(address),
                                           bodyRegion, address)
                         .first;

  auto execTestSem =
      semInfo.findSemantic(ir::getInstructionId(ir::amdgpu::EXEC_TEST));

  if (execTestSem == nullptr) {
    std::fprintf(stderr, "Failed to find semantic of EXEC_TEST\n");
    std::abort();
  }

  auto injectExecTest = [&](ir::Location loc, gcn::Builder &builder,
                            ir::Instruction point) {
    return;
    auto mergeBlock = builder.createSpvLabel(loc);
    gcn::Builder::createInsertBefore(converter, mergeBlock)
        .createSpvBranch(loc, mergeBlock);

    auto instBlock =
        gcn::Builder::createInsertAfter(converter, point).createSpvLabel(loc);
    auto prependInstBuilder =
        gcn::Builder::createInsertBefore(converter, instBlock);

    auto exec = prependInstBuilder.createValue(
        loc, ir::amdgpu::EXEC_TEST, converter.getType(execTestSem->returnType));

    prependInstBuilder.createSpvSelectionMerge(loc, mergeBlock,
                                               ir::spv::SelectionControl::None);
    prependInstBuilder.createSpvBranchConditional(loc, exec, instBlock,
                                                  mergeBlock);
  };

  std::vector<std::uint64_t> workList;
  workList.push_back(address);

  while (!workList.empty()) {
    auto instAddress = workList.back();
    workList.pop_back();
    if (!processed.insert(instAddress).second) {
      continue;
    }

    auto instStart = instAddress;
    auto loc = locBuilder.getLocation(instAddress);
    shader::GcnInstruction isaInst;
    readGcnInst(isaInst, instAddress, readMemory);
    isaInst.dump();
    currentOp = isaInst.op;

    if (isaInst == ir::sopp::ENDPGM) {
      auto builder = converter.createBuilder(instRegion, bodyRegion, instStart);

      builder.createSpvBranch(loc,
                              converter.epilogue.getFirst().cast<ir::Value>());
      continue;
    }

    bool isBranch = isaInst == ir::sopp::BRANCH ||
                    isaInst == ir::sop1::SETPC_B64 ||
                    isaInst == ir::sop1::SWAPPC_B64;

    // isaInst == ir::sopk::CBRANCH_I_FORK ||
    // isaInst == ir::sop2::CBRANCH_G_FORK

    if (!isBranch) {
      workList.push_back(instAddress);
    }

    if (isaInst == ir::sopp::WAITCNT) {
      continue;
    }

    auto builder = converter.createBuilder(instRegion, bodyRegion, instStart);
    auto instrBegin = builder.getInsertionPoint();

    auto variablesBuilder =
        gcn::Builder::createAppend(converter, converter.localVariables);

    auto operands = isaInst.getOperands();
    auto instSem =
        semInfo.findSemantic(ir::getInstructionId(isaInst.kind, isaInst.op));

    if (instSem == nullptr) {
      if (isaInst == ir::sopp::BRANCH) {
        auto target =
            instAddress +
            static_cast<std::int32_t>(isaInst.getOperand(0).value) / 4;
        workList.push_back(target);
        auto [label, inserted] =
            converter.getOrCreateLabel(loc, bodyRegion, target);

        if (inserted) {
          workList.push_back(target);
        }

        builder.createSpvBranch(loc, label);
        continue;
      }

      if (isaInst == ir::sop1::SETPC_B64) {
        auto target =
            createOperandRead(loc, builder, uint64TV, isaInst.getOperand(1));
        branchesToUnknown.push_back(builder.createInstruction(
            loc, ir::Kind::AmdGpu, ir::amdgpu::BRANCH, target));
        continue;
      }

      if (isaInst == ir::sop1::SWAPPC_B64) {
        auto target =
            createOperandRead(loc, builder, uint64TV, isaInst.getOperand(1));
        createOperandWrite(loc, builder, isaInst.getOperand(0),
                           converter.imm64(instAddress));
        branchesToUnknown.push_back(builder.createInstruction(
            loc, ir::Kind::AmdGpu, ir::amdgpu::BRANCH, target));
        continue;
      }

      if (isaInst == ir::sop1::GETPC_B64) {
        createOperandWrite(loc, builder, isaInst.getOperand(0),
                           converter.imm64(instAddress));
        continue;
      }

      if (isaInst == ir::vop1::MOVRELD_B32 ||
          isaInst == ir::vop3::MOVRELD_B32 ||
          isaInst == ir::sop1::MOVRELD_B32 ||
          isaInst == ir::sop1::MOVRELD_B64) {
        auto m0 = converter.readReg(loc, builder, uint32TV, gcn::RegId::M0, 0);
        auto gprCount = converter.readReg(loc, builder, uint32TV,
                                          (isaInst.kind == ir::Kind::Sop1
                                               ? gcn::RegId::SgprCount
                                               : gcn::RegId::VgprCount),
                                          0);

        auto dstIndex = converter.imm32(isaInst.getOperand(0).value);
        dstIndex = builder.createSpvIAdd(loc, uint32TV, dstIndex, m0);

        auto dstInBounds =
            builder.createSpvSLessThan(loc, boolTV, dstIndex, gprCount);

        auto moveBodyBlock = builder.createSpvLabel(loc);
        auto mergeBlock = builder.createSpvLabel(loc);

        {
          builder = gcn::Builder::createInsertBefore(converter, moveBodyBlock);
          builder.createSpvSelectionMerge(loc, mergeBlock,
                                          ir::spv::SelectionControl::None);
          builder.createSpvBranchConditional(loc, dstInBounds, moveBodyBlock,
                                             mergeBlock);
        }

        {
          builder = gcn::Builder::createInsertAfter(converter, moveBodyBlock);
          converter.writeReg(
              loc, builder,
              (isaInst.kind == ir::Kind::Sop1 ? gcn::RegId::Sgpr
                                              : gcn::RegId::Vgpr),
              dstIndex,
              createOperandRead(
                  loc, builder,
                  (isaInst == ir::sop1::MOVRELD_B64 ? uint64TV : uint32TV),
                  isaInst.getOperand(1)));

          builder.createSpvBranch(loc, mergeBlock);
        }

        builder = gcn::Builder::createInsertAfter(converter, mergeBlock);
        continue;
      }

      if (isaInst == ir::vop1::MOVRELS_B32 ||
          isaInst == ir::vop3::MOVRELS_B32 ||
          isaInst == ir::sop1::MOVRELS_B32 ||
          isaInst == ir::sop1::MOVRELS_B64) {
        auto srcIndex = converter.imm32(isaInst.getOperand(1).value);
        srcIndex = builder.createSpvIAdd(
            loc, uint32TV, srcIndex,
            converter.readReg(loc, builder, uint32TV, gcn::RegId::M0, 0));
        auto srcInBounds = builder.createSpvSLessThan(
            loc, uint32TV, srcIndex,
            converter.readReg(loc, builder, boolTV,
                              (isaInst.kind == ir::Kind::Sop1
                                   ? gcn::RegId::SgprCount
                                   : gcn::RegId::VgprCount),
                              0));

        srcIndex = builder.createSpvSelect(loc, uint32TV, srcInBounds, srcIndex,
                                           converter.imm32(0));
        createOperandWrite(
            loc, builder, isaInst.getOperand(0),
            converter.readReg(
                loc, builder,
                (isaInst == ir::sop1::MOVRELS_B64 ? uint64TV : uint32TV),
                (isaInst.kind == ir::Kind::Sop1 ? gcn::RegId::Sgpr
                                                : gcn::RegId::Vgpr),
                srcIndex));
        continue;
      }

      if (isaInst == ir::vop1::MOVRELSD_B32 ||
          isaInst == ir::vop3::MOVRELSD_B32) {
        auto m0 = converter.readReg(loc, builder, uint32TV, gcn::RegId::M0, 0);
        auto vgprCount =
            converter.readReg(loc, builder, uint32TV, gcn::RegId::VgprCount, 0);

        auto dstIndex = converter.imm32(isaInst.getOperand(0).value);
        dstIndex = builder.createSpvIAdd(loc, uint32TV, dstIndex, m0);

        auto dstInBounds =
            builder.createSpvSLessThan(loc, boolTV, dstIndex, vgprCount);

        auto moveBodyBlock = builder.createSpvLabel(loc);
        auto mergeBlock = builder.createSpvLabel(loc);

        {
          builder = gcn::Builder::createInsertBefore(converter, moveBodyBlock);
          builder.createSpvSelectionMerge(loc, mergeBlock,
                                          ir::spv::SelectionControl::None);
          builder.createSpvBranchConditional(loc, dstInBounds, moveBodyBlock,
                                             mergeBlock);
        }

        {
          builder = gcn::Builder::createInsertAfter(converter, moveBodyBlock);
          auto srcIndex = converter.imm32(isaInst.getOperand(1).value);
          srcIndex = builder.createSpvIAdd(loc, uint32TV, srcIndex, m0);
          auto srcInBounds =
              builder.createSpvSLessThan(loc, uint32TV, srcIndex, vgprCount);

          srcIndex = builder.createSpvSelect(loc, uint32TV, srcInBounds,
                                             srcIndex, converter.imm32(0));

          converter.writeReg(loc, builder, gcn::RegId::Vgpr, dstIndex,
                             converter.readReg(loc, builder, uint32TV,
                                               gcn::RegId::Vgpr, srcIndex));
          builder.createSpvBranch(loc, mergeBlock);
        }

        builder = gcn::Builder::createInsertAfter(converter, mergeBlock);
      }

      if (isaInst == ir::vop1::MOV_B32 || isaInst == ir::vop3::MOV_B32 ||
          isaInst == ir::sop1::MOV_B32 || isaInst == ir::sop1::MOV_B64 ||
          isaInst == ir::sopk::MOVK_I32) {
        if (operands.size() == 2) {
          bool is64Bit = isaInst == ir::sop1::MOV_B64;
          auto regTypeValue = is64Bit ? uint64TV : uint32TV;
          auto value =
              createOperandRead(loc, builder, regTypeValue, operands[1]);
          createOperandWrite(loc, builder, operands[0], value);

          if (isaInst.kind == ir::Kind::Vop1 ||
              isaInst.kind == ir::Kind::Vop3) {
            injectExecTest(loc, builder, instrBegin);
          }
          continue;
        }

        std::fprintf(stderr,
                     "Unexpected operand count for move instruction %s: %zu\n",
                     ir::getInstructionName(isaInst.kind, isaInst.op).c_str(),
                     operands.size());
      } else if (isaInst != ir::exp::EXP) {
        std::fprintf(stderr, "failed to find semantic of %s\n",
                     ir::getInstructionName(isaInst.kind, isaInst.op).c_str());
      }

      auto inst = builder.createInstruction(loc, isaInst.kind, isaInst.op);
      auto paramBuilder = gcn::Builder::createInsertBefore(converter, inst);

      for (std::size_t index = 0; auto &op : operands) {
        inst.addOperand(createOperandRead(loc, paramBuilder, uint32TV, op));
      }

      injectExecTest(loc, builder, instrBegin);
      continue;
    }

    if (isaInst == ir::sopp::CBRANCH_SCC0 ||
        isaInst == ir::sopp::CBRANCH_SCC1 ||
        isaInst == ir::sopp::CBRANCH_VCCZ ||
        isaInst == ir::sopp::CBRANCH_VCCNZ ||
        isaInst == ir::sopp::CBRANCH_EXECZ ||
        isaInst == ir::sopp::CBRANCH_EXECNZ) {
      if (!instSem->parameters.empty()) {
        std::fprintf(
            stderr,
            "Unexpected count of parameters for branch instruction %s: %zu\n",
            ir::getInstructionName(isaInst.kind, isaInst.op).c_str(),
            instSem->parameters.size());
        continue;
      }

      auto inst = builder.createValue(loc, isaInst.kind, isaInst.op,
                                      converter.getTypeBool());

      if (isaInst.getOperand(0).kind != GcnOperand::Kind::Constant) {
        std::abort();
      }

      auto target =
          instAddress + static_cast<std::int32_t>(isaInst.operands[0].value);
      workList.push_back(target);
      auto [ifTrueLabel, ifTrueInserted] =
          converter.getOrCreateLabel(loc, bodyRegion, target);
      auto [ifFalseLabel, _] =
          converter.getOrCreateLabel(loc, bodyRegion, instAddress);

      if (ifTrueInserted) {
        workList.push_back(target);
      }

      builder.createSpvBranchConditional(loc, inst, ifTrueLabel, ifFalseLabel);
      continue;
    }

    if (isaInst == ir::vintrp::MOV_F32) {
      if (!environment.supportsBarycentric) {
        auto rawValue = builder.createSpvLoad(
            loc, float32TV,
            createOperandRead(loc, builder, float32TV, isaInst.getOperand(2)));

        createOperandWrite(loc, builder, isaInst.getOperand(0), rawValue);
      }

      continue;
    }

    auto params = std::span(instSem->parameters);

    const GcnOperand *resultOperand = nullptr;

    if (instSem->returnType.baseType != ir::spv::OpTypeVoid) {
      if (!operands.empty()) {
        resultOperand = &operands[0];
        operands = operands.subspan(1);
      } else {
        std::fprintf(stderr, "unexpected return type of %s: expected void\n",
                     ir::getInstructionName(isaInst.kind, isaInst.op).c_str());
        continue;
      }
    }

    if (operands.size() != params.size()) {
      std::fprintf(stderr,
                   "count of arguments mismatch %s: expected %zu, got %zu\n",
                   ir::getInstructionName(isaInst.kind, isaInst.op).c_str(),
                   operands.size(), params.size());
      std::abort();
      continue;
    }

    if (resultOperand && (~resultOperand->access & GcnOperand::W)) {
      std::fprintf(stderr, "%s: missed write access for destination register\n",
                   ir::getInstructionName(isaInst.kind, isaInst.op).c_str());
      std::abort();
    }

    std::vector<ir::spv::IdRef> callArgs;

    for (std::size_t index = 0; auto &op : operands) {
      auto &paramInfo = params[index++];
      auto paramType = converter.getType(paramInfo.type);

      auto arg = variablesBuilder.createSpvVariable(
          loc,
          converter.getTypePointer(ir::spv::StorageClass::Function, paramType),
          ir::spv::StorageClass::Function);

      if ((paramInfo.access & Access::Read) == Access::Read) {
        auto result = createOperandRead(loc, builder, paramType, op);
        builder.createSpvStore(loc, arg, result);
      }

      callArgs.push_back(arg);
    }

    auto inst = builder.createValue(loc, isaInst.kind, isaInst.op);
    inst.addOperand(converter.getType(instSem->returnType));
    for (auto arg : callArgs) {
      inst.addOperand(arg);
    }

    if (resultOperand) {
      createOperandWrite(loc, builder, *resultOperand, inst);
    }

    for (std::size_t index = 0; auto &op : operands) {
      auto opIndex = index++;

      if ((op.access & GcnOperand::W) != GcnOperand::W) {
        continue;
      }

      auto arg = callArgs[opIndex];
      auto paramType = converter.getType(params[opIndex].type);

      auto value = builder.createSpvLoad(loc, paramType, arg);
      createOperandWrite(loc, builder, op, value);
    }

    if (isaInst.kind == ir::Kind::Sop2 || isaInst.kind == ir::Kind::Sopk ||
        isaInst.kind == ir::Kind::Smrd || isaInst.kind == ir::Kind::Sop1 ||
        isaInst.kind == ir::Kind::Sopc || isaInst.kind == ir::Kind::Sopp) {
      continue;
    }

    if (isaInst == ir::vop1::READFIRSTLANE_B32 ||
        isaInst == ir::vop2::READLANE_B32 ||
        isaInst == ir::vop2::WRITELANE_B32 ||
        isaInst == ir::vop3::READFIRSTLANE_B32 ||
        isaInst == ir::vop3::READLANE_B32 ||
        isaInst == ir::vop3::WRITELANE_B32) {
      continue;
    }

    injectExecTest(loc, builder, instrBegin);
  }

  converter.analysis.invalidateAll();

  return regionEntry;
}

static void canonicalizeRegisterVariableType(ir::Context &context,
                                             const BinaryLayout &layout,
                                             gcn::RegId regId,
                                             ir::Value variable) {
  auto varPointerType = variable.getOperand(0).getAsValue();
  auto varType = varPointerType.getOperand(1).getAsValue();

  auto varInfo = shader::spv::getTypeInfo(varType);
  auto regInfo = getRegisterInfo(regId);

  if (varInfo == regInfo) {
    return;
  }

  if (varInfo.width() != regInfo.width()) {
    std::cerr << "Unexpected width of register " << getRegisterName(regId)
              << ". expected " << regInfo.width() << ", actual "
              << varInfo.width() << "\n";
    std::abort();
  }

  auto globals = gcn::Builder::createInsertBefore(context, variable);

  ir::Value regType;
  switch (regInfo.baseType) {
  case ir::spv::OpTypeVector:
    regType = globals.createSpvTypeVector(
        variable.getLocation(),
        globals.createSpvTypeInt(variable.getLocation(), regInfo.componentWidth,
                                 0),
        regInfo.componentsCount);
    break;

  default:
    std::abort();
  }

  auto regPointerType = globals.createSpvTypePointer(
      variable.getLocation(),
      static_cast<ir::spv::StorageClass>(
          *varPointerType.getOperand(0).getAsInt32()),
      regType);
  variable.replaceOperand(0, regPointerType);

  for (auto user : variable.getUserList()) {
    auto instUser = user.cast<ir::Instruction>();
    if (instUser == nullptr) {
      continue;
    }

    if (instUser == ir::spv::OpName) {
      continue;
    }

    if (instUser == ir::spv::OpLoad) {
      auto builder = gcn::Builder::createInsertAfter(context, instUser);
      auto tmpInst =
          builder.createSpvUndef(context.getUnknownLocation(), varType);
      instUser.staticCast<ir::Value>().replaceAllUsesWith(tmpInst);

      instUser.replaceOperand(0, regType);
      auto castedLoadValue = builder.createSpvBitcast(
          instUser.getLocation(), varType, instUser.staticCast<ir::Value>());

      tmpInst.replaceAllUsesWith(castedLoadValue);
      tmpInst.remove();
      continue;
    }

    if (instUser == ir::spv::OpStore) {
      auto builder = gcn::Builder::createInsertBefore(context, instUser);
      auto value = instUser.getOperand(1).getAsValue();
      auto castedValue =
          builder.createSpvBitcast(instUser.getLocation(), regType, value);

      instUser.replaceOperand(1, castedValue);
      continue;
    }

    std::cerr << "Unexpected register user: ";
    dump(user);
    std::abort();
  }
}

void gcn::canonicalizeSemantic(ir::Context &context,
                               const BinaryLayout &layout) {
  auto debugs = layout.regions[BinaryLayout::kDebugs];
  if (debugs == nullptr) {
    return;
  }

  for (auto entry : debugs.children<ir::Instruction>()) {
    if (entry != ir::spv::OpName) {
      continue;
    }

    auto node = entry.getOperand(0).getAsValue();
    if (node != ir::spv::OpVariable) {
      continue;
    }

    auto &name = *entry.getOperand(1).getAsString();

    if (auto regId = getRegIdByName(name)) {
      canonicalizeRegisterVariableType(context, layout, *regId, node);
    }
  }
}

const char *accessToString(Access access) {
  switch (access) {
  case Access::Read:
    return "read";
  case Access::Write:
    return "write";
  case Access::Write | Access::Read:
    return "read/write";
  default:
    std::abort();
  }
}

SemanticInfo
gcn::collectSemanticInfo(const gcn::SemanticModuleInfo &moduleInfo) {
  std::map<ir::Value, int> registerToId;

  for (auto [regId, variable] : moduleInfo.registerVariables) {
    registerToId[variable] = regId;
  }

  std::map<ir::Value, ModuleInfo::Function> functions;
  SemanticInfo result;

  for (auto [instId, semFn] : moduleInfo.semantics) {
    auto &modInfo = moduleInfo.functions.at(semFn);
    auto &semInfo = result.semantics[instId];
    for (auto param : modInfo.parameters) {
      auto typeInfo =
          shader::spv::getTypeInfo(param.type.getOperand(1).getAsValue());
      semInfo.parameters.push_back({
          .type = typeInfo,
          .access = param.access,
      });
    }

    for (auto [pointer, access] : modInfo.variables) {
      auto storagePtr = pointer.getOperand(1).getAsInt32();
      if (!storagePtr) {
        continue;
      }

      auto storage = ir::spv::StorageClass(*storagePtr);

      if (storage == ir::spv::StorageClass::StorageBuffer) {
        semInfo.bufferAccess |= access;
        continue;
      }

      if (auto it = registerToId.find(pointer); it != registerToId.end()) {
        semInfo.registerAccesses[it->second] = access;
        continue;
      }
    }

    semInfo.returnType = shader::spv::getTypeInfo(modInfo.returnType);
  }

  return result;
}

ir::Node gcn::Import::getOrCloneImpl(ir::Context &context, ir::Node node,
                                     bool isOperand) {
  auto inst = node.cast<ir::Instruction>();

  if (inst == nullptr) {
    return CloneMap::getOrCloneImpl(context, node, isOperand);
  }

  auto &gcnContext = static_cast<Context &>(context);

  auto redefine = [&](ir::Node newNode) {
    setOverride(node, newNode);
    return newNode;
  };

  if (inst == ir::spv::OpVariable) {
    if (auto storage = inst.getOperand(1).getAsInt32();
        !storage || *storage == int(ir::spv::StorageClass::Function)) {
      return spv::Import::getOrCloneImpl(context, node, isOperand);
    }

    for (auto use : inst.staticCast<ir::Value>().getUseList()) {
      if (use.user != ir::spv::OpName) {
        continue;
      }

      auto name = use.user.getOperand(1).getAsString();

      if (name == nullptr) {
        continue;
      }

      if (auto regId = getRegIdByName(*name)) {
        if (shader::spv::getTypeInfo(
                inst.getOperand(0).getAsValue().getOperand(1).getAsValue()) !=
            getRegisterInfo(*regId)) {
          std::fprintf(stderr,
                       "unexpected type for register variable "
                       "'%s', expected %u\n",
                       name->c_str(), getRegisterInfo(*regId).width());
          std::abort();
        }

        return redefine(gcnContext.getOrCreateRegisterVariable(*regId));
      }

      break;
    }
  }

  return spv::Import::getOrCloneImpl(context, node, isOperand);
}

struct GcnEvaluator : eval::Evaluator {
  std::span<const std::uint32_t> userSGprs;
  std::function<std::uint32_t(std::uint64_t)> readMemory;
  gcn::Context &context;
  const SemanticInfo &semanticInfo;
  ir::Region region;
  std::uint32_t usedUserSgprs = 0;

  GcnEvaluator(gcn::Context &context, const SemanticInfo &semanticInfo,
               ir::Region region)
      : context(context), semanticInfo(semanticInfo), region(region) {}

  using eval::Evaluator::eval;

  eval::Value eval(ir::Value op) override {
    if (op == ir::spv::OpLoad) {
      auto &cfg = context.analysis.get<CFG>(
          [this] { return buildCFG(region.getFirst()); });

      auto &memorySSA = context.analysis.get<MemorySSA>([&, this] {
        return buildMemorySSA(cfg, semanticInfo, [this](int regId) {
          return context.getOrCreateRegisterVariable(
              static_cast<gcn::RegId>(regId));
        });
      });

      auto ptr = op.getOperand(1).getAsValue();
      if (auto defInst = memorySSA.getDefInst(op, ptr)) {
        if (defInst == ir::spv::OpStore) {
          return eval(defInst.getOperand(1).getAsValue());
        }

        if (auto defVal = defInst.cast<ir::Value>()) {
          return eval(defVal);
        }
      }
    }

    return Evaluator::eval(op);
  }

  eval::Value eval(ir::InstructionId instId,
                   std::span<const ir::Operand> operands) override {
    if (instId == ir::amdgpu::USER_SGPR) {
      if (auto optIndex = eval(operands[1]).zExtScalar()) {
        auto index = *optIndex;
        if (index < userSGprs.size()) {
          usedUserSgprs |= static_cast<std::uint32_t>(1) << index;
          return userSGprs[index];
        }
      }

      return {};
    }

    if (instId == ir::amdgpu::IMM) {
      if (!readMemory) {
        return {};
      }

      if (auto optAddress = eval(operands[1]).zExtScalar()) {
        auto address = *optAddress;
        return readMemory(address);
      }

      return {};
    }

    return eval::Evaluator::eval(instId, operands);
  }
};

ir::Region
gcn::deserialize(gcn::Context &context, const gcn::Environment &environment,
                 const SemanticInfo &semanticInfo, std::uint64_t base,
                 std::function<std::uint32_t(std::uint64_t)> readMemory) {
  readMemory = [&context,
                readMemory = std::move(readMemory)](std::uint64_t address) {
    context.memoryMap.map(address, address + sizeof(std::uint32_t));
    return readMemory(address);
  };

  {
    auto vgprType = context.getTypePointer(
        ir::spv::StorageClass::Private,
        context.getTypeArray(
            context.getTypeArray(context.getTypeUInt32(),
                                 context.imm32(environment.vgprCount)),
            context.imm32(64)));
    auto sgprType = context.getTypePointer(
        ir::spv::StorageClass::Private,
        context.getTypeArray(context.getTypeUInt32(),
                             context.imm32(environment.sgprCount)));

    auto globals = Builder::createAppend(
        context, context.layout.getOrCreateGlobals(context));
    auto debugs = Builder::createAppend(
        context, context.layout.getOrCreateDebugs(context));

    auto vgpr = globals.createSpvVariable(
        context.getUnknownLocation(), vgprType, ir::spv::StorageClass::Private);
    auto sgpr = globals.createSpvVariable(
        context.getUnknownLocation(), sgprType, ir::spv::StorageClass::Private);
    debugs.createSpvName(context.getUnknownLocation(), vgpr, "vgpr");
    debugs.createSpvName(context.getUnknownLocation(), sgpr, "sgpr");
    context.setRegisterVariable(gcn::RegId::Vgpr, vgpr);
    context.setRegisterVariable(gcn::RegId::Sgpr, sgpr);
  }

  std::unordered_set<std::uint64_t> processed;
  std::vector<ir::Instruction> branchesToUnknown;
  auto mainEntry =
      deserializeGcnRegion(context, environment, semanticInfo, base, readMemory,
                           branchesToUnknown, processed);
  auto builder = gcn::Builder::createPrepend(context, context.body);

  {
    auto loc = context.getUnknownLocation();
    context.entryPoint = builder.createSpvLabel(loc);

    for (int i = 0; i < environment.userSgprs.size(); ++i) {
      auto value =
          builder.createValue(loc, ir::Kind::AmdGpu, ir::amdgpu::USER_SGPR,
                              context.getTypeUInt32(), i);
      context.writeReg(loc, builder, gcn::RegId::Sgpr, i, value);
    }

    context.writeReg(loc, builder, gcn::RegId::SgprCount, 0,
                     context.imm32(environment.sgprCount));
    context.writeReg(loc, builder, gcn::RegId::VgprCount, 0,
                     context.imm32(environment.vgprCount));
    builder.createSpvBranch(loc, mainEntry);
  }

  while (!branchesToUnknown.empty()) {
    auto child = branchesToUnknown.back();
    branchesToUnknown.pop_back();

    GcnEvaluator evaluator(context, semanticInfo, context.body);
    evaluator.userSGprs = environment.userSgprs;
    evaluator.readMemory = readMemory;

    if (auto target =
            evaluator.eval(child.getOperand(0).getAsValue()).zExtScalar()) {
      auto regionEntry =
          deserializeGcnRegion(context, environment, semanticInfo, *target,
                               readMemory, branchesToUnknown, processed);
      gcn::Builder::createInsertBefore(context, child)
          .createSpvBranch(child.getLocation(), regionEntry);
      child.remove();
    } else {
      std::fprintf(stderr, "failed to evaluate branch!\n");
    }
    context.requiredUserSgprs |= evaluator.usedUserSgprs;
  }

  for (auto [address, label] : context.instructions) {
    if (label != ir::spv::OpLabel) {
      continue;
    }

    if (auto prev = label.getPrev(); !prev || isTerminator(prev)) {
      continue;
    }

    gcn::Builder::createInsertBefore(context, label)
        .createSpvBranch(label.getLocation(), label.staticCast<ir::Value>());
  }

  std::print("\n\n{}\n\n", buildCFG(context.entryPoint).genTest());

  structurizeCfg(context, context.body,
                 context.epilogue.getFirst().cast<ir::Value>());
  return context.body;
}
