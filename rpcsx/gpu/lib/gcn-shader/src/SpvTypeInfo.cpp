#include "SpvTypeInfo.hpp"
#include "dialect.hpp"

using namespace shader;

shader::spv::TypeInfo shader::spv::getTypeInfo(ir::Value type) {
  if (type == ir::spv::OpTypeBool) {
    return {
        .baseType = ir::spv::OpTypeBool,
        .componentWidth = 1,
        .componentsCount = 1,
    };
  }

  if (type == ir::spv::OpTypeInt) {
    return {
        .baseType = ir::spv::OpTypeInt,
        .componentWidth = *type.getOperand(0).getAsInt32(),
        .componentsCount = 1,
        .isSigned = *type.getOperand(1).getAsInt32() ? true : false,
    };
  }

  if (type == ir::spv::OpTypeFloat) {
    return {
        .baseType = ir::spv::OpTypeFloat,
        .componentWidth = *type.getOperand(0).getAsInt32(),
        .componentsCount = 1,
    };
  }

  if (type == ir::spv::OpTypeVector) {
    auto componentInfo = getTypeInfo(type.getOperand(0).getAsValue());

    return {
        .baseType = ir::spv::OpTypeVector,
        .componentType = componentInfo.baseType,
        .componentWidth = componentInfo.width(),
        .componentsCount = *type.getOperand(1).getAsInt32(),
    };
  }

  if (type == ir::spv::OpTypeArray) {
    auto elementInfo = getTypeInfo(type.getOperand(0).getAsValue());
    auto countOfElements = type.getOperand(1).getAsValue();

    return {
        .baseType = ir::spv::OpTypeArray,
        .componentType = elementInfo.baseType,
        .componentWidth = elementInfo.width(),
        .componentsCount = *countOfElements.getOperand(1).getAsInt32(),
    };
  }

  if (type == ir::spv::OpTypeRuntimeArray) {
    auto elementInfo = getTypeInfo(type.getOperand(0).getAsValue());

    return {
        .baseType = ir::spv::OpTypeRuntimeArray,
        .componentType = elementInfo.baseType,
        .componentWidth = elementInfo.width(),
        .componentsCount = 1,
    };
  }

  return {
      .baseType = static_cast<ir::spv::Op>(type.getOp()),
      .componentWidth = 0,
      .componentsCount = 0,
  };
}
