#include "SpvConverter.hpp"
#include "dialect.hpp"
#include "dialect/spv.hpp"
#include <string>

using namespace shader;

using Builder = ir::Builder<ir::spv::Builder, ir::builtin::Builder>;

static std::string getTypeName(ir::Value type);

static std::string getConstantName(ir::Value constant) {
  if (constant == ir::spv::OpConstant) {
    auto typeValue = constant.getOperand(0).getAsValue();
    auto value = constant.getOperand(1);

    if (typeValue == ir::spv::OpTypeInt) {
      auto width = *typeValue.getOperand(0).getAsInt32();

      if (width <= 32) {
        if (value.getAsInt32() == nullptr) {
          std::abort();
        }
        return "_" + std::to_string(*value.getAsInt32());
      }
      if (value.getAsInt64() == nullptr) {
        std::abort();
      }
      return "c_" + std::to_string(*value.getAsInt64());
    }

    if (typeValue == ir::spv::OpTypeFloat) {
      auto width = *typeValue.getOperand(0).getAsInt32();

      if (width == 32) {
        if (value.getAsFloat() == nullptr) {
          std::abort();
        }
        return "c_" + std::to_string(*value.getAsFloat());
      }
      if (value.getAsDouble() == nullptr) {
        std::abort();
      }
      return "c_" + std::to_string(*value.getAsDouble());
    }

    return {};
  }

  if (constant == ir::spv::OpConstantTrue) {
    return "true";
  }

  if (constant == ir::spv::OpConstantFalse) {
    return "false";
  }

  if (constant == ir::spv::OpConstantNull) {
    return "null_" + getTypeName(constant.getOperand(0).getAsValue());
  }

  return {};
}

static std::string getTypeName(ir::Value type) {
  if (type == ir::spv::OpTypeInt) {
    if (type.getOperand(1) != 0) {
      return "s" + std::to_string(*type.getOperand(0).getAsInt32());
    }
    return "u" + std::to_string(*type.getOperand(0).getAsInt32());
  }

  if (type == ir::spv::OpTypeFloat) {
    return "f" + std::to_string(*type.getOperand(0).getAsInt32());
  }

  if (type == ir::spv::OpTypeBool) {
    return "bool";
  }

  if (type == ir::spv::OpTypeVoid) {
    return "void";
  }

  if (type == ir::spv::OpTypeSampler) {
    return "sampler";
  }

  if (type == ir::spv::OpTypeVector) {
    return getTypeName(type.getOperand(0).getAsValue()) + 'x' +
           std::to_string(*type.getOperand(1).getAsInt32());
  }

  if (type == ir::spv::OpTypeArray) {
    auto count = type.getOperand(1).getAsValue();
    if (count == ir::spv::OpConstant) {
      if (auto n = count.getOperand(1).getAsInt32()) {
        return getTypeName(type.getOperand(0).getAsValue()) + '[' +
               std::to_string(*n) + ']';
      }
    }

    return getTypeName(type.getOperand(0).getAsValue()) + "[N]";
  }

  if (type == ir::spv::OpTypeRuntimeArray) {
    return getTypeName(type.getOperand(0).getAsValue()) + "[]";
  }

  if (type == ir::spv::OpTypeStruct) {
    std::string result = "struct{";
    for (bool first = true; auto &op : type.getOperands()) {
      if (!first) {
        result += ", ";
      } else {
        first = false;
      }
      result += getTypeName(op.getAsValue());
    }

    result += "}";
    return result;
  }

  if (type == ir::spv::OpTypePointer) {
    return getTypeName(type.getOperand(1).getAsValue()) + "*";
  }

  return {};
}

spv::Context::Context() {
  localVariables = create<ir::Region>(getUnknownLocation());
  epilogue = createRegionWithLabel(getUnknownLocation()).getParent();
}

ir::Node spv::Import::getOrCloneImpl(ir::Context &context, ir::Node node,
                                     bool isOperand) {
  auto inst = node.cast<ir::Instruction>();

  if (inst == nullptr) {
    return CloneMap::getOrCloneImpl(context, node, isOperand);
  }

  auto &spvContext = static_cast<spv::Context &>(context);

  auto redefine = [&](ir::Node newNode) {
    setOverride(node, newNode);
    return newNode;
  };

  auto cloneDecorationsAndDebugs = [&](ir::Node inst = nullptr) {
    if (inst == nullptr) {
      inst = node;
    }

    auto annotations = spvContext.layout.getOrCreateAnnotations(context);
    auto debugs = spvContext.layout.getOrCreateDebugs(context);
    auto value = inst.cast<ir::Value>();
    if (value == nullptr) {
      return;
    }

    for (auto &use : value.getUseList()) {
      if (use.user == ir::spv::OpDecorate ||
          use.user == ir::spv::OpMemberDecorate ||
          use.user == ir::spv::OpDecorationGroup ||
          use.user == ir::spv::OpGroupDecorate ||
          use.user == ir::spv::OpGroupMemberDecorate ||
          use.user == ir::spv::OpDecorateId) {

        annotations.addChild(ir::clone(use.user, context, *this));
      }

      if (use.user == ir::spv::OpName || use.user == ir::spv::OpMemberName) {
        auto cloned = ir::clone(use.user, context, *this);
        debugs.addChild(cloned);
        if (use.user == ir::spv::OpName) {
          auto demangled =
              std::string_view(*cloned.getOperand(1).getAsString());
          if (auto pos = demangled.find('('); pos != std::string::npos) {
            demangled = demangled.substr(0, pos);
          }
          spvContext.setName(cloned.getOperand(0).getAsValue(),
                             std::string(demangled));
        }
      }
    }
  };

  auto hasDecoration = [&] {
    for (auto use : node.staticCast<ir::Value>().getUseList()) {
      if (use.user == ir::spv::OpDecorate ||
          use.user == ir::spv::OpMemberDecorate) {
        return true;
      }
    }

    return false;
  };

  if (inst.getKind() == ir::Kind::Spv) {
    if (inst.getOp() == ir::spv::OpExtInstImport) {
      auto extensions = spvContext.layout.getOrCreateExtInstImports(context);
      auto result = CloneMap::getOrCloneImpl(context, node, isOperand);
      extensions.addChild(result.staticCast<ir::Value>());

      return redefine(result);
    }

    if (ir::spv::isTypeOp(inst.getOp())) {
      std::vector<ir::Operand> operands;

      for (auto &op : inst.getOperands()) {
        operands.push_back(op.clone(context, *this));
      }

      auto typeOp = static_cast<ir::spv::Op>(inst.getOp());

      if ((inst != ir::spv::OpTypeArray || !hasDecoration()) &&
          inst != ir::spv::OpTypeRuntimeArray &&
          inst != ir::spv::OpTypeStruct) {
        if (inst != ir::spv::OpTypePointer ||
            inst.getOperand(0) == ir::spv::StorageClass::Function) {
          if (auto result = spvContext.findGlobal(typeOp, operands)) {
            return redefine(result);
          }
        }
      }

      auto result = spvContext.createGlobal(
          static_cast<ir::spv::Op>(inst.getOp()), operands);
      redefine(result);
      cloneDecorationsAndDebugs();
      return result;
    }
  }

  if (inst == ir::spv::OpConstant || inst == ir::spv::OpConstantComposite ||
      inst == ir::spv::OpConstantTrue || inst == ir::spv::OpConstantFalse ||
      inst == ir::spv::OpConstantNull || inst == ir::spv::OpConstantSampler ||
      inst == ir::spv::OpSpecConstantTrue ||
      inst == ir::spv::OpSpecConstantFalse || inst == ir::spv::OpSpecConstant ||
      inst == ir::spv::OpSpecConstantComposite) {
    std::vector<ir::Operand> operands;

    for (auto &op : inst.getOperands()) {
      operands.push_back(op.clone(context, *this));
    }

    auto result = spvContext.getOrCreateGlobal(
        static_cast<ir::spv::Op>(inst.getOp()), operands);
    return redefine(result);
  }

  if (isOperand && inst == ir::spv::OpVariable) {
    if (inst == ir::spv::OpVariable) {
      auto storage = inst.getOperand(1).getAsInt32();
      if (*storage == int(ir::spv::StorageClass::Function)) {
        return CloneMap::getOrCloneImpl(context, node, isOperand);
      }
    }

    auto globals = spvContext.layout.getOrCreateGlobals(context);
    auto result = CloneMap::getOrCloneImpl(context, node, isOperand);
    globals.addChild(result.staticCast<ir::Instruction>());
    cloneDecorationsAndDebugs();
    return result;
  }

  if (inst == ir::spv::OpConstant) {
    auto type = inst.getOperand(0).clone(context, *this);
    return redefine(
        spvContext.getOrCreateConstant(type.getAsValue(), inst.getOperand(1)));
  }

  if (inst == ir::spv::OpFunction) {
    auto functions = spvContext.layout.getOrCreateFunctions(context);

    auto result = CloneMap::getOrCloneImpl(context, node, isOperand)
                      .staticCast<ir::Value>();
    functions.insertAfter(nullptr, result);
    redefine(result);
    cloneDecorationsAndDebugs();

    ir::Instruction insertPoint = result;

    for (auto child : ir::range(inst.getNext())) {
      auto cloned = ir::clone(child, context, *this);
      functions.insertAfter(insertPoint, cloned);
      insertPoint = cloned;
      cloneDecorationsAndDebugs(child);

      if (child == ir::spv::OpFunctionEnd) {
        break;
      }
    }

    return result;
  }

  return CloneMap::getOrCloneImpl(context, node, isOperand);
}

ir::Value spv::Context::createRegionWithLabel(ir::Location loc) {
  return Builder::createAppend(*this, create<ir::Region>(loc))
      .createSpvLabel(loc);
}

void spv::Context::setName(ir::spv::IdRef inst, std::string name) {
  ns.setNameOf(inst, name);
  auto debugs = Builder::createAppend(*this, layout.getOrCreateDebugs(*this));
  debugs.createSpvName(getUnknownLocation(), inst, std::move(name));
}

void spv::Context::setConstantName(ir::Value constant) {
  auto name = getConstantName(constant);
  if (!name.empty()) {
    ns.setNameOf(constant, std::move(name));
  }
}

ir::Value spv::Context::getOrCreateConstant(ir::Value typeValue,
                                                     const ir::Operand &value) {
  if (typeValue == getTypeBool()) {
    return *value.getAsBool() ? getTrue() : getFalse();
  }
  return getOrCreateGlobal(ir::spv::OpConstant, {{typeValue, value}});
}

ir::Value spv::Context::getType(ir::spv::Op baseType, int width,
                                         bool isSigned) {
  switch (baseType) {
  case ir::spv::OpTypeInt:
    return getTypeInt(width, isSigned);
  case ir::spv::OpTypeFloat:
    return getTypeFloat(width);
  case ir::spv::OpTypeBool:
    return getTypeBool();
  case ir::spv::OpTypeVoid:
    return getTypeVoid();

  default:
    std::abort();
  }
}

ir::Value spv::Context::getType(const TypeInfo &info) {
  switch (info.baseType) {
  case ir::spv::OpTypeInt:
  case ir::spv::OpTypeFloat:
  case ir::spv::OpTypeBool:
  case ir::spv::OpTypeVoid:
    return getType(info.baseType, info.componentWidth, info.isSigned);

  case ir::spv::OpTypeVector:
    return getTypeVector(
        getType(info.componentType, info.componentWidth, info.isSigned),
        info.componentsCount);

  case ir::spv::OpTypeArray:
    return getTypeArray(
        getType(info.componentType, info.componentWidth, info.isSigned),
        imm32(info.componentsCount));

  default:
    std::abort();
  }
}

void spv::Context::setTypeName(ir::Value type) {
  auto name = getTypeName(type);
  if (!name.empty()) {
    ns.setNameOf(type, std::move(name));
  }
}

ir::Value
spv::Context::findGlobal(ir::spv::Op op,
                                  std::span<const ir::Operand> operands) const {
  auto it = globals.find(ir::getInstructionId(ir::Kind::Spv, op));

  if (it == globals.end()) {
    return nullptr;
  }

  auto &types = it->second;

  for (auto type : types) {
    if (type.getOperandCount() != operands.size()) {
      continue;
    }

    bool matches = true;
    for (std::size_t i = 0; auto &operand : type.getOperands()) {
      if (operands[i++] != operand) {
        matches = false;
        break;
      }
    }

    if (matches) {
      return type;
    }
  }

  return nullptr;
}

ir::Value
spv::Context::createGlobal(ir::spv::Op op,
                                    std::span<const ir::Operand> operands) {
  auto builder = Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
  auto result =
      builder.createValue(getUnknownLocation(), ir::Kind::Spv, op, operands);

  globals[ir::getInstructionId(op)].push_back(result);
  if (ir::spv::isTypeOp(op)) {
    setTypeName(result);
  } else {
    setConstantName(result);
  }
  return result;
}

ir::Value spv::Context::getOrCreateGlobal(
    ir::spv::Op op, std::span<const ir::Operand> operands) {
  if (auto result = findGlobal(op, operands)) {
    return result;
  }

  return createGlobal(op, operands);
}

ir::Value spv::Context::getOperandValue(const ir::Operand &op,
                                                 ir::Value type) {
  if (auto result = op.getAsValue()) {
    return result;
  }

  auto createConstant = [&](auto value, ir::Value expType) {
    return getOrCreateConstant(type ? type : expType, value);
  };

  if (auto result = op.getAsInt32()) {
    return createConstant(*result, getTypeSInt32());
  }

  if (auto result = op.getAsInt64()) {
    return createConstant(*result, getTypeSInt64());
  }

  if (auto result = op.getAsFloat()) {
    return createConstant(*result, getTypeFloat32());
  }

  if (auto result = op.getAsDouble()) {
    return createConstant(*result, getTypeFloat64());
  }

  if (auto result = op.getAsBool()) {
    return createConstant(*result, getTypeBool());
  }

  std::abort();
}

void spv::Context::createPerVertex() {
  if (perVertex != nullptr) {
    return;
  }

  auto loc = rootLocation;

  auto float32 = getTypeFloat32();
  auto arr1Float = getTypeArray(float32, getIndex(1));
  auto float32x4 = getTypeVector(float32, 4);

  auto gl_PerVertexStructT =
      getTypeStruct(float32x4, float32, arr1Float, arr1Float);
  auto gl_PerVertexPtrT =
      getTypePointer(ir::spv::StorageClass::Output, gl_PerVertexStructT);
  auto annotations =
      Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));

  annotations.createSpvDecorate(loc, gl_PerVertexStructT,
                                ir::spv::Decoration::Block());
  annotations.createSpvMemberDecorate(
      loc, gl_PerVertexStructT, 0,
      ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::Position));
  annotations.createSpvMemberDecorate(
      loc, gl_PerVertexStructT, 1,
      ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::PointSize));
  annotations.createSpvMemberDecorate(
      loc, gl_PerVertexStructT, 2,
      ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::ClipDistance));
  annotations.createSpvMemberDecorate(
      loc, gl_PerVertexStructT, 3,
      ir::spv::Decoration::BuiltIn(ir::spv::BuiltIn::CullDistance));

  auto globals = Builder::createAppend(*this, layout.getOrCreateGlobals(*this));

  perVertex = globals.createSpvVariable(loc, gl_PerVertexPtrT,
                                        ir::spv::StorageClass::Output);
}

ir::Value spv::Context::createUniformBuffer(int descriptorSet,
                                                     int binding,
                                                     ir::Value structType) {
  auto globals = Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
  auto annotations =
      Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));
  auto loc = getUnknownLocation();

  auto storageClass = ir::spv::StorageClass::StorageBuffer;
  auto blockType = globals.createSpvTypePointer(loc, storageClass, structType);

  auto blockVariable = globals.createSpvVariable(loc, blockType, storageClass);

  annotations.createSpvDecorate(
      loc, blockVariable, ir::spv::Decoration::DescriptorSet(descriptorSet));
  annotations.createSpvDecorate(loc, blockVariable,
                                ir::spv::Decoration::Binding(binding));
  annotations.createSpvDecorate(loc, blockVariable,
                                ir::spv::Decoration::Uniform());
  return blockVariable;
}

ir::Value spv::Context::createRuntimeArrayUniformBuffer(
    int descriptorSet, int binding, ir::Value elementType) {
  auto globals = Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
  auto annotations =
      Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));
  auto loc = getUnknownLocation();

  auto element = globals.createSpvTypeRuntimeArray(loc, elementType);
  annotations.createSpvDecorate(
      loc, element,
      ir::spv::Decoration::ArrayStride(
          shader::spv::getTypeInfo(elementType).width() / 8));

  auto blockStruct = globals.createSpvTypeStruct(loc, {{element}});
  annotations.createSpvDecorate(loc, blockStruct, ir::spv::Decoration::Block());
  annotations.createSpvMemberDecorate(loc, blockStruct, 0,
                                      ir::spv::Decoration::Offset(0));
  return createUniformBuffer(descriptorSet, binding, blockStruct);
}

ir::Value spv::Context::createOutput(ir::Location loc, int index) {
  auto &result = outputs[index];

  if (result == nullptr) {
    auto floatType = getTypeFloat32();
    auto float32x4Type = getTypeVector(floatType, 4);
    auto variableType =
        getTypePointer(ir::spv::StorageClass::Output, float32x4Type);

    auto globals =
        Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
    auto annotations =
        Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));
    auto debugs = Builder::createAppend(*this, layout.getOrCreateDebugs(*this));

    auto variable = globals.createSpvVariable(loc, variableType,
                                              ir::spv::StorageClass::Output);

    annotations.createSpvDecorate(loc, variable,
                                  ir::spv::Decoration::Location(index));

    setName(variable, "output" + std::to_string(index));
    result = variable;
  }

  return result;
}

ir::Value spv::Context::createInput(ir::Location loc, int index) {
  auto &result = inputs[index];

  if (result == nullptr) {
    auto floatType = getTypeFloat32();
    auto float32x4Type = getTypeVector(floatType, 4);
    auto variableType =
        getTypePointer(ir::spv::StorageClass::Input, float32x4Type);

    auto globals =
        Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
    auto annotations =
        Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));
    auto debugs = Builder::createAppend(*this, layout.getOrCreateDebugs(*this));

    auto variable = globals.createSpvVariable(loc, variableType,
                                              ir::spv::StorageClass::Input);

    annotations.createSpvDecorate(loc, variable,
                                  ir::spv::Decoration::Location(index));

    setName(variable, "input" + std::to_string(index));
    result = variable;
  }

  return result;
}

ir::Value spv::Context::createAttr(ir::Location loc, int attrId,
                                            bool perVertex, bool flat) {
  auto &result = inputs[attrId];

  if (result == nullptr) {
    auto floatType = getTypeFloat32();
    auto float32x4Type = getTypeVector(floatType, 4);

    auto attrArrayType = getTypeArray(float32x4Type, imm32(3));
    auto variableType =
        getTypePointer(ir::spv::StorageClass::Input,
                       perVertex ? attrArrayType : float32x4Type);

    auto globals =
        Builder::createAppend(*this, layout.getOrCreateGlobals(*this));
    auto annotations =
        Builder::createAppend(*this, layout.getOrCreateAnnotations(*this));
    auto debugs = Builder::createAppend(*this, layout.getOrCreateDebugs(*this));

    auto variable = globals.createSpvVariable(loc, variableType,
                                              ir::spv::StorageClass::Input);

    annotations.createSpvDecorate(loc, variable,
                                  ir::spv::Decoration::Location(attrId));

    if (perVertex) {
      annotations.createSpvDecorate(loc, variable,
                                    ir::spv::Decoration::PerVertexKHR());
    } else if (flat) {
      annotations.createSpvDecorate(loc, variable, ir::spv::Decoration::Flat());
    }
    setName(variable, "attr" + std::to_string(attrId));
    result = variable;
  }

  return result;
}
