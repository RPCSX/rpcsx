#pragma once
#include "Fragment.hpp"
#include "RegisterId.hpp"
#include "spirv/spirv-builder.hpp"
#include <span>

namespace amdgpu::shader {
class ConverterContext;

struct Function {
  ConverterContext *context = nullptr;
  Stage stage = Stage::None;
  std::span<const std::uint32_t> userSgprs;
  std::span<const std::uint32_t> userVgprs;
  Fragment entryFragment;
  Fragment exitFragment;
  std::map<RegisterId, Value> inputs;
  spirv::FunctionBuilder builder;
  std::vector<Fragment *> fragments;

  Value getInput(RegisterId id);
  Value createInput(RegisterId id);
  void createExport(spirv::BlockBuilder &builder, unsigned index, Value value);
  spirv::Type getResultType();
  spirv::FunctionType getFunctionType();

  Fragment *createFragment();

  void insertReturn();
};
} // namespace amdgpu::shader
