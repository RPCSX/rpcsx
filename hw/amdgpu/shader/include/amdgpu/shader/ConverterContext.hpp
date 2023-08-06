#pragma once

#include "Fragment.hpp"
#include "Function.hpp"
#include "Stage.hpp"
#include "TypeId.hpp"
#include "Uniform.hpp"
#include "util/area.hpp"

#include <amdgpu/RemoteMemory.hpp>
#include <forward_list>
#include <spirv/spirv-builder.hpp>
#include <util/unreachable.hpp>

#include <bit>
#include <cassert>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace amdgpu::shader {
/*
struct MaterializedFunction {
  spirv::Function function;
  spirv::FunctionType type;
  spirv::Type returnType;

  std::vector<std::pair<RegisterId, TypeId>> args;
  std::vector<std::pair<RegisterId, TypeId>> results;
};
*/

class ConverterContext {
  Stage mStage;
  RemoteMemory mMemory;
  spirv::IdGenerator mGenerator;
  spirv::SpirvBuilder mBuilder{mGenerator, 1024};
  static constexpr auto kGenericTypesCount =
      static_cast<std::size_t>(TypeId::Void) + 1;
  spirv::Type mTypes[kGenericTypesCount];
  spirv::PointerType mPtrTypes[13][kGenericTypesCount];
  spirv::RuntimeArrayType mRuntimeArrayTypes[kGenericTypesCount];
  spirv::VariableValue mThreadId;
  spirv::VariableValue mWorkgroupId;
  spirv::VariableValue mLocalInvocationId;
  spirv::VariableValue mPerVertex;
  spirv::VariableValue mFragCoord;
  std::vector<spirv::VariableValue> mInterfaces;
  std::map<unsigned, spirv::VariableValue> mIns;
  std::map<unsigned, spirv::VariableValue> mOuts;

  std::map<std::uint32_t, spirv::ConstantFloat> mConstantFloat32Map;
  std::map<std::uint32_t, spirv::ConstantUInt> mConstantUint32Map;
  std::map<std::uint32_t, spirv::ConstantSInt> mConstantSint32Map;
  std::map<std::uint64_t, spirv::ConstantUInt> mConstantUint64Map;

  struct FunctionType {
    spirv::Type resultType;
    std::vector<spirv::Type> params;
    spirv::FunctionType id;
  };

  std::vector<FunctionType> mFunctionTypes;

  struct StructTypeEntry {
    spirv::StructType id;
    std::vector<spirv::Type> members;
    spirv::PointerType ptrTypes[13];

    bool match(std::span<const spirv::Type> other) {
      if (members.size() != other.size()) {
        return false;
      }

      for (std::size_t i = 0; i < other.size(); ++i) {
        if (members[i] != other[i]) {
          return false;
        }
      }

      return true;
    }
  };

  std::vector<StructTypeEntry> mStructTypes;

  std::forward_list<Fragment> mFragments;
  std::forward_list<Function> mFunctions;

  spirv::ConstantBool mTrue;
  spirv::ConstantBool mFalse;

  std::vector<UniformInfo> mUniforms;
  spirv::ExtInstSet mGlslStd450;
  spirv::Function mDiscardFn;

public:
  util::MemoryAreaTable<> *dependencies = nullptr;

  ConverterContext(RemoteMemory memory, Stage stage,
                   util::MemoryAreaTable<> *dependencies)
      : mStage(stage), mMemory(memory), dependencies(dependencies) {
    mGlslStd450 = mBuilder.createExtInstImport("GLSL.std.450");
  }

  const decltype(mInterfaces) &getInterfaces() const { return mInterfaces; }

  spirv::SpirvBuilder &getBuilder() { return mBuilder; }
  RemoteMemory getMemory() const { return mMemory; }
  spirv::ExtInstSet getGlslStd450() const { return mGlslStd450; }
  std::optional<TypeId> getTypeIdOf(spirv::Type type) const;

  spirv::StructType findStructType(std::span<const spirv::Type> members);
  spirv::StructType getStructType(std::span<const spirv::Type> members);
  spirv::PointerType getStructPointerType(spv::StorageClass storageClass,
                                          spirv::StructType structType);
  spirv::Type getType(TypeId id);

  spirv::PointerType getPointerType(spv::StorageClass storageClass, TypeId id) {
    assert(static_cast<unsigned>(storageClass) < 13);
    auto &type = mPtrTypes[static_cast<unsigned>(storageClass)]
                          [static_cast<std::uint32_t>(id)];

    if (!type) {
      type = mBuilder.createTypePointer(storageClass, getType(id));
    }

    return type;
  }

  spirv::RuntimeArrayType getRuntimeArrayType(TypeId id);

  spirv::UIntType getUInt32Type() {
    return spirv::cast<spirv::UIntType>(getType(TypeId::UInt32));
  }
  spirv::UIntType getUInt64Type() {
    return spirv::cast<spirv::UIntType>(getType(TypeId::UInt64));
  }
  spirv::UIntType getUInt8Type() {
    return spirv::cast<spirv::UIntType>(getType(TypeId::UInt8));
  }

  spirv::VectorOfType<spirv::UIntType> getUint32x2Type() {
    return spirv::cast<spirv::VectorOfType<spirv::UIntType>>(
        getType(TypeId::UInt32x2));
  }

  spirv::VectorOfType<spirv::UIntType> getUint32x3Type() {
    return spirv::cast<spirv::VectorOfType<spirv::UIntType>>(
        getType(TypeId::UInt32x3));
  }

  spirv::VectorOfType<spirv::UIntType> getUint32x4Type() {
    return spirv::cast<spirv::VectorOfType<spirv::UIntType>>(
        getType(TypeId::UInt32x4));
  }

  spirv::ArrayOfType<spirv::UIntType> getArrayUint32x8Type() {
    return spirv::cast<spirv::ArrayOfType<spirv::UIntType>>(
        getType(TypeId::ArrayUInt32x8));
  }

  spirv::ArrayOfType<spirv::UIntType> getArrayUint32x16Type() {
    return spirv::cast<spirv::ArrayOfType<spirv::UIntType>>(
        getType(TypeId::ArrayUInt32x16));
  }

  spirv::SIntType getSint32Type() {
    return spirv::cast<spirv::SIntType>(getType(TypeId::SInt32));
  }
  spirv::SIntType getSint64Type() {
    return spirv::cast<spirv::SIntType>(getType(TypeId::SInt64));
  }

  spirv::FloatType getFloat16Type() {
    return spirv::cast<spirv::FloatType>(getType(TypeId::Float16));
  }
  spirv::FloatType getFloat32Type() {
    return spirv::cast<spirv::FloatType>(getType(TypeId::Float32));
  }

  spirv::VectorOfType<spirv::FloatType> getFloat32x4Type() {
    return spirv::cast<spirv::VectorOfType<spirv::FloatType>>(
        getType(TypeId::Float32x4));
  }

  spirv::VectorOfType<spirv::FloatType> getFloat32x3Type() {
    return spirv::cast<spirv::VectorOfType<spirv::FloatType>>(
        getType(TypeId::Float32x3));
  }

  spirv::VectorOfType<spirv::FloatType> getFloat32x2Type() {
    return spirv::cast<spirv::VectorOfType<spirv::FloatType>>(
        getType(TypeId::Float32x2));
  }

  spirv::BoolType getBoolType() {
    return spirv::cast<spirv::BoolType>(getType(TypeId::Bool));
  }

  spirv::VoidType getVoidType() {
    return spirv::cast<spirv::VoidType>(getType(TypeId::Void));
  }

  spirv::ConstantBool getTrue() {
    if (!mTrue) {
      mTrue = mBuilder.createConstantTrue(getBoolType());
    }
    return mTrue;
  }
  spirv::ConstantBool getFalse() {
    if (!mFalse) {
      mFalse = mBuilder.createConstantFalse(getBoolType());
    }
    return mFalse;
  }

  spirv::ConstantUInt getUInt64(std::uint64_t value);
  spirv::ConstantUInt getUInt32(std::uint32_t value);
  spirv::ConstantSInt getSInt32(std::uint32_t value);
  spirv::ConstantFloat getFloat32Raw(std::uint32_t value);

  spirv::ConstantFloat getFloat32(float id) {
    return getFloat32Raw(std::bit_cast<std::uint32_t>(id));
  }

  spirv::SamplerType getSamplerType() {
    return spirv::cast<spirv::SamplerType>(getType(TypeId::Sampler));
  }
  spirv::ImageType getImage2DType() {
    return spirv::cast<spirv::ImageType>(getType(TypeId::Image2D));
  }
  spirv::ImageType getStorageImage2DType() {
    return spirv::cast<spirv::ImageType>(getType(TypeId::StorageImage2D));
  }
  spirv::SampledImageType getSampledImage2DType() {
    return spirv::cast<spirv::SampledImageType>(
        getType(TypeId::SampledImage2D));
  }

  UniformInfo *createStorageBuffer(TypeId type);
  UniformInfo *getOrCreateStorageBuffer(std::uint32_t *vbuffer, TypeId type);
  UniformInfo *getOrCreateUniformConstant(std::uint32_t *buffer,
                                          std::size_t size, TypeId type);
  spirv::VariableValue getThreadId();
  spirv::VariableValue getWorkgroupId();
  spirv::VariableValue getLocalInvocationId();
  spirv::VariableValue getPerVertex();
  spirv::VariableValue getFragCoord();
  spirv::VariableValue getIn(unsigned location);
  spirv::VariableValue getOut(unsigned location);

  spirv::Function getDiscardFn();

  std::optional<std::uint32_t> findUint32Value(spirv::Value id) const;
  std::optional<std::int32_t> findSint32Value(spirv::Value id) const;
  std::optional<float> findFloat32Value(spirv::Value id) const;
  spirv::FunctionType getFunctionType(spirv::Type resultType,
                                      std::span<const spirv::Type> params);

  Function *createFunction(std::size_t expectedSize);
  Fragment *createFragment(std::size_t expectedSize);

  std::vector<UniformInfo> &getUniforms() { return mUniforms; }
};
} // namespace amdgpu::shader
