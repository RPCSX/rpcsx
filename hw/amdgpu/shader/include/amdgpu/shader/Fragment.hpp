#pragma once

#include "FragmentTerminator.hpp"
#include "Instruction.hpp"
#include "RegisterId.hpp"
#include "RegisterState.hpp"
#include "Stage.hpp"
#include "TypeId.hpp"
#include "Uniform.hpp"
#include "scf.hpp"

#include <map>
#include <optional>
#include <spirv/spirv-builder.hpp>

namespace amdgpu::shader {
enum class OperandGetFlags {
  None,
  PreserveType = 1 << 0
};

struct Function;
class ConverterContext;

struct Fragment {
  ConverterContext *context = nullptr;
  Function *function = nullptr;
  spirv::Block entryBlockId;
  spirv::BlockBuilder builder;
  RegisterState *registers = nullptr;

  std::set<RegisterId> values;
  std::set<RegisterId> outputs;

  std::vector<Fragment *> predecessors;
  std::uint64_t jumpAddress = 0;
  spirv::BoolValue branchCondition;

  void appendBranch(Fragment &other) {
    other.predecessors.push_back(this);
  }

  void injectValuesFromPreds();

  // std::optional<RegisterId> findInput(spirv::Value value);
  // Value addInput(RegisterId id, spirv::Type type);
  spirv::SamplerValue createSampler(RegisterId base);
  spirv::ImageValue createImage(RegisterId base, bool r128); // TODO: params
  Value createCompositeExtract(Value composite, std::uint32_t member);
  Value getOperand(RegisterId id, TypeId type, OperandGetFlags flags = OperandGetFlags::None);
  void setOperand(RegisterId id, Value value);
  void setVcc(Value value);
  void setScc(Value value);
  spirv::BoolValue getScc();
  spirv::Value createBitcast(spirv::Type to, spirv::Type from, spirv::Value value);

  Value getScalarOperand(int id, TypeId type, OperandGetFlags flags = OperandGetFlags::None) {
    return getOperand(RegisterId::Scalar(id), type, flags);
  }
  Value getVectorOperand(int id, TypeId type, OperandGetFlags flags = OperandGetFlags::None) {
    return getOperand(RegisterId::Vector(id), type, flags);
  }
  Value getAttrOperand(int id, TypeId type, OperandGetFlags flags = OperandGetFlags::None) {
    return getOperand(RegisterId::Attr(id), type, flags);
  }
  Value getVccLo() {
    return getOperand(RegisterId::VccLo, TypeId::UInt32);
  }
  Value getVccHi() {
    return getOperand(RegisterId::VccHi, TypeId::UInt32);
  }
  Value getExecLo() {
    return getOperand(RegisterId::ExecLo, TypeId::UInt32);
  }
  Value getExecHi() {
    return getOperand(RegisterId::ExecHi, TypeId::UInt32);
  }
  void setScalarOperand(int id, Value value) {
    setOperand(RegisterId::Scalar(id), value);
  }
  void setVectorOperand(int id, Value value) {
    setOperand(RegisterId::Vector(id), value);
  }
  void setExportTarget(int id, Value value) {
    setOperand(RegisterId::Export(id), value);
  }
  // void createCallTo(MaterializedFunction *other);
  void convert(std::uint64_t size);

private:
  Value getRegister(RegisterId id);
  Value getRegister(RegisterId id, spirv::Type type);
  void setRegister(RegisterId id, Value value);
};
} // namespace amdgpu::shader
