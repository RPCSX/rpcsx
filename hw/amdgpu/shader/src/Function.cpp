#include "Function.hpp"
#include "ConverterContext.hpp"
#include "RegisterId.hpp"

using namespace amdgpu::shader;

Value Function::createInput(RegisterId id) {
  auto [it, inserted] = inputs.try_emplace(id);

  if (!inserted) {
    assert(it->second);
    return it->second;
  }

  auto offset = id.getOffset();

  if (id.isScalar()) {
    auto uint32T = context->getUInt32Type();

    if (userSgprs.size() > offset) {
      return ((it->second = {uint32T, context->getUInt32(userSgprs[offset])}));
    }

    if (stage == Stage::None) {
      return ((it->second =
                   Value{uint32T, builder.createFunctionParameter(uint32T)}));
    }

    switch (id.raw) {
    case RegisterId::ExecLo:
      return ((it->second = {uint32T, context->getUInt32(1)}));
    case RegisterId::ExecHi:
      return ((it->second = {uint32T, context->getUInt32(0)}));

    case RegisterId::Scc:
      return ((it->second = {context->getBoolType(), context->getFalse()}));

    default:
      break;
    }

    if (stage == Stage::Vertex) {
      return ((it->second = {uint32T, context->getUInt32(0)}));
    } else if (stage == Stage::Fragment) {
      return ((it->second = {uint32T, context->getUInt32(0)}));
    } else if (stage == Stage::Compute) {
      std::uint32_t offsetAfterSgprs = offset - userSgprs.size();
      if (offsetAfterSgprs < 3) {
        auto workgroupIdVar = context->getWorkgroupId();
        auto workgroupId = entryFragment.builder.createLoad(
            context->getUint32x3Type(), workgroupIdVar);
        for (uint32_t i = 0; i < 3; ++i) {
          auto input = entryFragment.builder.createCompositeExtract(
              uint32T, workgroupId, {{i}});

          inputs[RegisterId::Scalar(userSgprs.size() + i)] = {uint32T, input};
        }

        return inputs[id];
      }

      return ((it->second = {uint32T, context->getUInt32(0)}));
    }

    util::unreachable();
  }

  if (stage == Stage::None) {
    auto float32T = context->getFloat32Type();
    return (
        (it->second = {float32T, builder.createFunctionParameter(float32T)}));
  }

  if (stage == Stage::Vertex) {
    if (id.isVector()) {
      auto uint32T = context->getUInt32Type();

      if (id.getOffset() == 0) {
        auto input =
            entryFragment.builder.createLoad(uint32T, context->getThreadId());

        return ((it->second = {uint32T, input}));
      }

      return ((it->second = {uint32T, context->getUInt32(0)}));
    }

    util::unreachable("Unexpected vertex input %u. user sgprs count=%zu",
                      id.raw, userSgprs.size());
  }

  if (stage == Stage::Fragment) {
    if (id.isAttr()) {
      auto float4T = context->getFloat32x4Type();
      auto input = entryFragment.builder.createLoad(
          float4T, context->getIn(id.getOffset()));
      return ((it->second = {float4T, input}));
    }

    if (id.isVector()) {
      switch (offset) {
      case 2:
      case 3:
      case 4:
      case 5: {
        auto float4T = context->getFloat32x4Type();
        auto floatT = context->getFloat32Type();
        auto fragCoord =
            entryFragment.builder.createLoad(float4T, context->getFragCoord());
        return (
            (it->second = {floatT, entryFragment.builder.createCompositeExtract(
                                       floatT, fragCoord, {{offset - 2}})}));
      }
      }
    }

    return ((it->second = {context->getUInt32Type(), context->getUInt32(0)}));
  }

  if (stage == Stage::Compute) {
    if (id.isVector() && offset < 3) {
      auto uint32T = context->getUInt32Type();
      auto localInvocationIdVar = context->getLocalInvocationId();
      auto localInvocationId = entryFragment.builder.createLoad(
          context->getUint32x3Type(), localInvocationIdVar);

      for (uint32_t i = 0; i < 3; ++i) {
        auto input = entryFragment.builder.createCompositeExtract(
            uint32T, localInvocationId, {{i}});

        inputs[RegisterId::Vector(i)] = {uint32T, input};
      }

      return inputs[id];
    }

    return ((it->second = {context->getUInt32Type(), context->getUInt32(0)}));
  }

  util::unreachable();
}

void Function::createExport(spirv::BlockBuilder &builder, unsigned index,
                            Value value) {
  if (stage == Stage::Vertex) {
    switch (index) {
    case 12: {
      auto float4OutPtrT =
          context->getPointerType(spv::StorageClass::Output, TypeId::Float32x4);

      auto gl_PerVertexPosition = builder.createAccessChain(
          float4OutPtrT, context->getPerVertex(), {{context->getSInt32(0)}});

      if (value.type != context->getFloat32x4Type()) {
        util::unreachable();
      }

      builder.createStore(gl_PerVertexPosition, value.value);
      return;
    }

    case 32 ... 64: { // paramN
      if (value.type != context->getFloat32x4Type()) {
        util::unreachable();
      }

      builder.createStore(context->getOut(index - 32), value.value);
      return;
    }
    }

    util::unreachable("Unexpected vartex export target %u", index);
  }

  if (stage == Stage::Fragment) {
    switch (index) {
    case 0 ... 7: {
      if (value.type != context->getFloat32x4Type()) {
        util::unreachable();
      }

      builder.createStore(context->getOut(index), value.value);
      return;
    }
    }

    util::unreachable("Unexpected fragment export target %u", index);
  }

  util::unreachable();
}

spirv::Type Function::getResultType() {
  if (exitFragment.outputs.empty()) {
    return context->getVoidType();
  }

  if (exitFragment.outputs.size() == 1) {
    return exitFragment.registers->getRegister(*exitFragment.outputs.begin())
        .type;
  }

  std::vector<spirv::Type> members;
  members.reserve(exitFragment.outputs.size());

  for (auto id : exitFragment.outputs) {
    members.push_back(exitFragment.registers->getRegister(id).type);
  }

  return context->getStructType(members);
}

spirv::FunctionType Function::getFunctionType() {
  if (stage != Stage::None) {
    return context->getFunctionType(getResultType(), {});
  }

  std::vector<spirv::Type> params;
  params.reserve(inputs.size());

  for (auto inp : inputs) {
    params.push_back(inp.second.type);
  }

  return context->getFunctionType(getResultType(), params);
}

Fragment *Function::createFragment() {
  auto result = context->createFragment(0);
  result->function = this;
  fragments.push_back(result);
  return result;
}

void Function::insertReturn() {
  if (exitFragment.outputs.empty()) {
    exitFragment.builder.createReturn();
    return;
  }

  if (exitFragment.outputs.size() == 1) {
    auto value =
        exitFragment.registers->getRegister(*exitFragment.outputs.begin())
            .value;
    exitFragment.builder.createReturnValue(value);
    return;
  }

  auto resultType = getResultType();

  auto resultTypePointer = context->getBuilder().createTypePointer(
      spv::StorageClass::Function, resultType);

  auto resultVariable = entryFragment.builder.createVariable(
      resultTypePointer, spv::StorageClass::Function);

  std::uint32_t member = 0;
  for (auto regId : exitFragment.outputs) {
    auto value = exitFragment.registers->getRegister(regId);
    auto valueTypeId = context->getTypeIdOf(value.type);

    auto pointerType =
        context->getPointerType(spv::StorageClass::Function, *valueTypeId);
    auto valuePointer = exitFragment.builder.createAccessChain(
        pointerType, resultVariable,
        {{exitFragment.context->getUInt32(member++)}});

    exitFragment.builder.createStore(valuePointer, value.value);
  }

  auto resultValue =
      exitFragment.builder.createLoad(resultType, resultVariable);

  exitFragment.builder.createReturnValue(resultValue);
}
