#include "ConverterContext.hpp"
#include "util/unreachable.hpp"
using namespace amdgpu::shader;

std::optional<TypeId> ConverterContext::getTypeIdOf(spirv::Type type) const {
  for (int i = 0; i < kGenericTypesCount; ++i) {
    if (mTypes[i] == type) {
      return static_cast<TypeId::enum_type>(i);
    }
  }

  return std::nullopt;
}

spirv::StructType
ConverterContext::findStructType(std::span<const spirv::Type> members) {
  for (auto &structType : mStructTypes) {
    if (structType.match(members)) {
      return structType.id;
    }
  }

  return {};
}

spirv::StructType
ConverterContext::getStructType(std::span<const spirv::Type> members) {
  for (auto &structType : mStructTypes) {
    if (structType.match(members)) {
      return structType.id;
    }
  }

  auto &newType = mStructTypes.emplace_back();
  newType.id = mBuilder.createTypeStruct(members);
  newType.members.reserve(members.size());
  for (auto member : members) {
    newType.members.push_back(member);
  }
  return newType.id;
}

spirv::PointerType
ConverterContext::getStructPointerType(spv::StorageClass storageClass,
                                       spirv::StructType structType) {
  StructTypeEntry *entry = nullptr;
  for (auto &structType : mStructTypes) {
    if (structType.id != structType.id) {
      continue;
    }

    entry = &structType;
  }

  if (entry == nullptr) {
    util::unreachable("Struct type not found");
  }

  auto &ptrType = entry->ptrTypes[static_cast<unsigned>(storageClass)];

  if (!ptrType) {
    ptrType = mBuilder.createTypePointer(storageClass, structType);
  }

  return ptrType;
}

spirv::Type ConverterContext::getType(TypeId id) {
  auto &type = mTypes[static_cast<std::uint32_t>(id)];

  if (type) {
    return type;
  }

  switch (id) {
  case TypeId::Void:
    return ((type = mBuilder.createTypeVoid()));
  case TypeId::Bool:
    return ((type = mBuilder.createTypeBool()));
  case TypeId::SInt8:
    return ((type = mBuilder.createTypeSInt(8)));
  case TypeId::UInt8:
    return ((type = mBuilder.createTypeUInt(8)));
  case TypeId::SInt16:
    return ((type = mBuilder.createTypeSInt(16)));
  case TypeId::UInt16:
    return ((type = mBuilder.createTypeUInt(16)));
  case TypeId::SInt32:
    return ((type = mBuilder.createTypeSInt(32)));
  case TypeId::UInt32:
    return ((type = mBuilder.createTypeUInt(32)));
  case TypeId::UInt32x2:
    return ((type = mBuilder.createTypeVector(getType(TypeId::UInt32), 2)));
  case TypeId::UInt32x3:
    return ((type = mBuilder.createTypeVector(getType(TypeId::UInt32), 3)));
  case TypeId::UInt32x4:
    return ((type = mBuilder.createTypeVector(getType(TypeId::UInt32), 4)));
  case TypeId::UInt64:
    return ((type = mBuilder.createTypeUInt(64)));
  case TypeId::SInt64:
    return ((type = mBuilder.createTypeSInt(64)));
  case TypeId::ArrayUInt32x8:
    type = mBuilder.createTypeArray(getType(TypeId::UInt32x4), getUInt32(2));
    getBuilder().createDecorate(type, spv::Decoration::ArrayStride,
                                std::array{static_cast<std::uint32_t>(16)});
  case TypeId::ArrayUInt32x16:
    type = mBuilder.createTypeArray(getType(TypeId::UInt32x4), getUInt32(4));
    getBuilder().createDecorate(type, spv::Decoration::ArrayStride,
                                std::array{static_cast<std::uint32_t>(16)});
    return type;
  case TypeId::Float16:
    return ((type = mBuilder.createTypeFloat(16)));
  case TypeId::Float32:
    return ((type = mBuilder.createTypeFloat(32)));
  case TypeId::Float32x2:
    return ((type = mBuilder.createTypeVector(getType(TypeId::Float32), 2)));
  case TypeId::Float32x3:
    return ((type = mBuilder.createTypeVector(getType(TypeId::Float32), 3)));
  case TypeId::Float32x4:
    return ((type = mBuilder.createTypeVector(getType(TypeId::Float32), 4)));
  case TypeId::Float64:
    return ((type = mBuilder.createTypeFloat(64)));
  case TypeId::ArrayFloat32x8:
    type = mBuilder.createTypeArray(getType(TypeId::Float32x4), getUInt32(2));
    getBuilder().createDecorate(type, spv::Decoration::ArrayStride,
                                std::array{static_cast<std::uint32_t>(16)});
    return type;
  case TypeId::ArrayFloat32x16:
    type = mBuilder.createTypeArray(getType(TypeId::Float32x4), getUInt32(4));
    getBuilder().createDecorate(type, spv::Decoration::ArrayStride,
                                std::array{static_cast<std::uint32_t>(16)});
    return type;

  case TypeId::Image2D:
    return ((type = getBuilder().createTypeImage(getFloat32Type(),
                                                 spv::Dim::Dim2D, 0, 0, 0, 1,
                                                 spv::ImageFormat::Unknown)));
  case TypeId::SampledImage2D:
    return ((type = getBuilder().createTypeSampledImage(getImage2DType())));

  case TypeId::Sampler:
    return ((type = getBuilder().createTypeSampler()));
  }

  util::unreachable();
}

spirv::RuntimeArrayType ConverterContext::getRuntimeArrayType(TypeId id) {
  auto &type = mRuntimeArrayTypes[static_cast<std::uint32_t>(id)];

  if (!type) {
    type = mBuilder.createTypeRuntimeArray(getType(id));
    mBuilder.createDecorate(type, spv::Decoration::ArrayStride,
                            {{(std::uint32_t)id.getSize()}});
  }

  return type;
}

spirv::ConstantUInt ConverterContext::getUInt64(std::uint64_t value) {
  auto &id = mConstantUint64Map[value];
  if (!id) {
    id = mBuilder.createConstant64(getUInt64Type(), value);
  }
  return id;
}

spirv::ConstantUInt ConverterContext::getUInt32(std::uint32_t value) {
  auto &id = mConstantUint32Map[value];
  if (!id) {
    id = mBuilder.createConstant32(getUInt32Type(), value);
  }
  return id;
}

spirv::ConstantSInt ConverterContext::getSInt32(std::uint32_t value) {
  auto &id = mConstantSint32Map[value];
  if (!id) {
    id = mBuilder.createConstant32(getSint32Type(), value);
  }
  return id;
}

spirv::ConstantFloat ConverterContext::getFloat32Raw(std::uint32_t value) {
  auto &id = mConstantFloat32Map[value];
  if (!id) {
    id = mBuilder.createConstant32(getFloat32Type(), value);
  }
  return id;
}

UniformInfo *ConverterContext::createStorageBuffer(TypeId type) {
  std::array<spirv::Type, 1> uniformStructMembers{getRuntimeArrayType(type)};
  auto uniformStruct = findStructType(uniformStructMembers);

  if (!uniformStruct) {
    uniformStruct = getStructType(uniformStructMembers);

    getBuilder().createDecorate(uniformStruct, spv::Decoration::Block, {});

    getBuilder().createMemberDecorate(
        uniformStruct, 0, spv::Decoration::Offset,
        std::array{static_cast<std::uint32_t>(0)});
  }

  auto uniformType =
      getStructPointerType(spv::StorageClass::StorageBuffer, uniformStruct);
  auto uniformVariable = getBuilder().createVariable(
      uniformType, spv::StorageClass::StorageBuffer);

  mInterfaces.push_back(uniformVariable);

  auto &newUniform = mUniforms.emplace_back();
  newUniform.index = mUniforms.size() - 1;
  newUniform.typeId = type;
  newUniform.type = uniformType;
  newUniform.variable = uniformVariable;
  newUniform.isBuffer = true;
  std::printf("new storage buffer %u of type %u\n", newUniform.index,
               newUniform.typeId.raw);
  return &newUniform;
}

UniformInfo *ConverterContext::getOrCreateStorageBuffer(std::uint32_t *vbuffer,
                                                        TypeId type) {
  for (auto &uniform : mUniforms) {
    if (std::memcmp(uniform.buffer, vbuffer, sizeof(std::uint32_t) * 4)) {
      continue;
    }

    if (uniform.typeId != type) {
      util::unreachable("getOrCreateStorageBuffer: access to the uniform with "
                        "different type");
    }

    if (!uniform.isBuffer) {
      util::unreachable("getOrCreateStorageBuffer: uniform was constant");
    }

    // std::printf("reuse storage buffer %u of type %u\n", uniform.index,
    //             uniform.typeId.raw);
    return &uniform;
  }

  auto newUniform = createStorageBuffer(type);
  std::memcpy(newUniform->buffer, vbuffer, sizeof(std::uint32_t) * 4);
  return newUniform;
}

UniformInfo *ConverterContext::getOrCreateUniformConstant(std::uint32_t *buffer,
                                                          std::size_t size,
                                                          TypeId type) {
  for (auto &uniform : mUniforms) {
    if (std::memcmp(uniform.buffer, buffer, sizeof(std::uint32_t) * size)) {
      continue;
    }

    if (uniform.typeId != type) {
      util::unreachable(
          "getOrCreateUniformConstant: access to the uniform with "
          "different type");
    }

    if (uniform.isBuffer) {
      util::unreachable("getOrCreateUniformConstant: uniform was buffer");
    }

    return &uniform;
  }

  auto uniformType = getPointerType(spv::StorageClass::UniformConstant, type);
  auto uniformVariable = getBuilder().createVariable(
      uniformType, spv::StorageClass::UniformConstant);
  mInterfaces.push_back(uniformVariable);

  auto &newUniform = mUniforms.emplace_back();
  newUniform.index = mUniforms.size() - 1;
  newUniform.typeId = type;
  newUniform.type = uniformType;
  newUniform.variable = uniformVariable;
  newUniform.isBuffer = false;
  std::memcpy(newUniform.buffer, buffer, sizeof(std::uint32_t) * size);

  return &newUniform;
}

spirv::VariableValue ConverterContext::getThreadId() {
  if (mThreadId) {
    return mThreadId;
  }

  auto inputType = getPointerType(spv::StorageClass::Input, TypeId::UInt32);
  mThreadId = mBuilder.createVariable(inputType, spv::StorageClass::Input);

  if (mStage == Stage::Vertex) {
    mBuilder.createDecorate(
        mThreadId, spv::Decoration::BuiltIn,
        std::array{static_cast<std::uint32_t>(spv::BuiltIn::VertexIndex)});
  } else {
    util::unreachable();
  }

  mInterfaces.push_back(mThreadId);

  return mThreadId;
}

spirv::VariableValue ConverterContext::getWorkgroupId() {
  if (mWorkgroupId) {
    return mWorkgroupId;
  }

  if (mStage != Stage::Compute) {
    util::unreachable();
  }

  auto workgroupIdType =
      getPointerType(spv::StorageClass::Input, TypeId::UInt32x3);
  mWorkgroupId =
      mBuilder.createVariable(workgroupIdType, spv::StorageClass::Input);

  mBuilder.createDecorate(
      mWorkgroupId, spv::Decoration::BuiltIn,
      {{static_cast<std::uint32_t>(spv::BuiltIn::WorkgroupId)}});
  mInterfaces.push_back(mWorkgroupId);

  return mWorkgroupId;
}

spirv::VariableValue ConverterContext::getLocalInvocationId() {
  if (mLocalInvocationId) {
    return mLocalInvocationId;
  }

  if (mStage != Stage::Compute) {
    util::unreachable();
  }

  auto localInvocationIdType =
      getPointerType(spv::StorageClass::Input, TypeId::UInt32x3);
  mLocalInvocationId =
      mBuilder.createVariable(localInvocationIdType, spv::StorageClass::Input);

  mBuilder.createDecorate(
      mLocalInvocationId, spv::Decoration::BuiltIn,
      std::array{static_cast<std::uint32_t>(spv::BuiltIn::LocalInvocationId)});

  mInterfaces.push_back(mLocalInvocationId);

  return mLocalInvocationId;
}

spirv::VariableValue ConverterContext::getPerVertex() {
  if (mPerVertex) {
    return mPerVertex;
  }

  auto floatT = getFloat32Type();
  auto float4T = getFloat32x4Type();

  auto uintConst1 = getUInt32(1);
  auto arr1Float = mBuilder.createTypeArray(floatT, uintConst1);

  auto gl_PerVertexStructT = mBuilder.createTypeStruct(std::array{
      static_cast<spirv::Type>(float4T),
      static_cast<spirv::Type>(floatT),
      static_cast<spirv::Type>(arr1Float),
      static_cast<spirv::Type>(arr1Float),
  });

  mBuilder.createDecorate(gl_PerVertexStructT, spv::Decoration::Block, {});
  mBuilder.createMemberDecorate(
      gl_PerVertexStructT, 0, spv::Decoration::BuiltIn,
      std::array{static_cast<std::uint32_t>(spv::BuiltIn::Position)});
  mBuilder.createMemberDecorate(
      gl_PerVertexStructT, 1, spv::Decoration::BuiltIn,
      std::array{static_cast<std::uint32_t>(spv::BuiltIn::PointSize)});
  mBuilder.createMemberDecorate(
      gl_PerVertexStructT, 2, spv::Decoration::BuiltIn,
      std::array{static_cast<std::uint32_t>(spv::BuiltIn::ClipDistance)});
  mBuilder.createMemberDecorate(
      gl_PerVertexStructT, 3, spv::Decoration::BuiltIn,
      std::array{static_cast<std::uint32_t>(spv::BuiltIn::CullDistance)});

  auto gl_PerVertexPtrT = mBuilder.createTypePointer(spv::StorageClass::Output,
                                                     gl_PerVertexStructT);
  mPerVertex =
      mBuilder.createVariable(gl_PerVertexPtrT, spv::StorageClass::Output);

  mInterfaces.push_back(mPerVertex);
  return mPerVertex;
}

spirv::VariableValue ConverterContext::getFragCoord() {
  if (mFragCoord) {
    return mFragCoord;
  }

  auto inputType = getPointerType(spv::StorageClass::Input, TypeId::Float32x4);
  mFragCoord =
      mBuilder.createVariable(inputType, spv::StorageClass::Input);

  mBuilder.createDecorate(mFragCoord, spv::Decoration::BuiltIn,
                          {{static_cast<std::uint32_t>(spv::BuiltIn::FragCoord)}});

  mInterfaces.push_back(mFragCoord);
  return mFragCoord;
}

spirv::VariableValue ConverterContext::getIn(unsigned location) {
  auto [it, inserted] = mIns.try_emplace(location);
  if (!inserted) {
    return it->second;
  }

  auto inputType = getPointerType(spv::StorageClass::Input, TypeId::Float32x4);
  auto inputVariable =
      mBuilder.createVariable(inputType, spv::StorageClass::Input);

  mBuilder.createDecorate(inputVariable, spv::Decoration::Location,
                          {{location}});

  mInterfaces.push_back(inputVariable);
  it->second = inputVariable;
  return inputVariable;
}

spirv::VariableValue ConverterContext::getOut(unsigned location) {
  auto [it, inserted] = mOuts.try_emplace(location);
  if (!inserted) {
    return it->second;
  }
  auto outputType =
      getPointerType(spv::StorageClass::Output, TypeId::Float32x4);
  auto outputVariable =
      mBuilder.createVariable(outputType, spv::StorageClass::Output);

  mBuilder.createDecorate(outputVariable, spv::Decoration::Location,
                          {{location}});

  mInterfaces.push_back(outputVariable);
  it->second = outputVariable;
  return outputVariable;
}

spirv::Function ConverterContext::getDiscardFn() {
  if (mDiscardFn) {
    return mDiscardFn;
  }

  if (mStage != Stage::Fragment) {
    util::unreachable();
  }

  auto fn = mBuilder.createFunctionBuilder(5);
  mDiscardFn = fn.id;
  auto entry = fn.createBlockBuilder(5);
  entry.createKill();

  fn.insertBlock(entry);
  mBuilder.insertFunction(fn, getVoidType(), {},
                          getFunctionType(getVoidType(), {}));

  return mDiscardFn;
}

std::optional<std::uint32_t>
ConverterContext::findUint32Value(spirv::Value id) const {
  for (auto [value, constId] : mConstantUint32Map) {
    if (constId == id) {
      return value;
    }
  }

  return std::nullopt;
}

std::optional<std::int32_t>
ConverterContext::findSint32Value(spirv::Value id) const {
  for (auto [value, constId] : mConstantSint32Map) {
    if (constId == id) {
      return value;
    }
  }

  return std::nullopt;
}

std::optional<float> ConverterContext::findFloat32Value(spirv::Value id) const {
  for (auto [value, constId] : mConstantFloat32Map) {
    if (constId == id) {
      return std::bit_cast<float>(value);
    }
  }

  return std::nullopt;
}

spirv::FunctionType
ConverterContext::getFunctionType(spirv::Type resultType,
                                  std::span<const spirv::Type> params) {
  for (auto fnType : mFunctionTypes) {
    if (fnType.resultType != resultType) {
      continue;
    }

    if (fnType.params.size() != params.size()) {
      continue;
    }

    bool match = true;
    for (std::size_t i = 0, end = params.size(); i < end; ++i) {
      if (fnType.params[i] != params[i]) {
        match = false;
        break;
      }
    }
    if (!match) {
      continue;
    }

    return fnType.id;
  }

  auto id = mBuilder.createTypeFunction(resultType, params);

  std::vector<spirv::Type> paramsVec;
  paramsVec.reserve(params.size());

  for (auto param : params) {
    paramsVec.push_back(param);
  }

  mFunctionTypes.push_back(FunctionType{
      .resultType = resultType, .params = std::move(paramsVec), .id = id});

  return id;
}

Function *ConverterContext::createFunction(std::size_t expectedSize) {
  auto result = &mFunctions.emplace_front();

  result->context = this;
  result->entryFragment.context = this;
  result->entryFragment.function = result;
  result->entryFragment.builder = mBuilder.createBlockBuilder(expectedSize);
  result->entryFragment.entryBlockId = result->entryFragment.builder.id;
  result->fragments.push_back(&result->entryFragment);

  result->exitFragment.context = this;
  result->exitFragment.function = result;
  result->exitFragment.builder = mBuilder.createBlockBuilder(0);
  result->exitFragment.entryBlockId = result->exitFragment.builder.id;
  result->builder = mBuilder.createFunctionBuilder(expectedSize);

  return result;
}

Fragment *ConverterContext::createFragment(std::size_t expectedSize) {
  auto result = &mFragments.emplace_front();

  result->context = this;
  result->builder = mBuilder.createBlockBuilder(expectedSize);
  result->entryBlockId = result->builder.id;

  return result;
}
