#include "ModuleInfo.hpp"
#include "analyze.hpp"
#include "dialect.hpp"
#include "ir.hpp"

shader::ModuleInfo::Function &
shader::collectFunctionInfo(ModuleInfo &moduleInfo, ir::Value function) {
  auto [fnIt, fnInserted] =
      moduleInfo.functions.try_emplace(function, ModuleInfo::Function{});
  if (!fnInserted) {
    return fnIt->second;
  }

  auto &result = fnIt->second;
  std::map<ir::Value, int> params;

  result.returnType = function.getOperand(0).getAsValue();

  auto trackAccess = [&](ir::Value pointer, Access access) {
    pointer = unwrapPointer(pointer);

    if (auto it = params.find(pointer); it != params.end()) {
      result.parameters[it->second].access |= access;
      return;
    }

    if (pointer == ir::spv::OpVariable) {
      auto storagePtr = pointer.getOperand(1).getAsInt32();
      if (!storagePtr) {
        return;
      }

      auto storage = ir::spv::StorageClass(*storagePtr);

      if (storage != ir::spv::StorageClass::Function) {
        result.variables[pointer] = access;
      }
    }
  };

  for (auto inst : ir::range(function.getNext())) {
    if (inst == ir::spv::OpFunctionEnd) {
      break;
    }

    if (inst == ir::spv::OpFunctionParameter) {
      auto type = inst.getOperand(0).getAsValue();
      params[inst.staticCast<ir::Value>()] = result.parameters.size();
      result.parameters.push_back({.type = type, .access = Access::None});
      continue;
    }

    if (inst == ir::spv::OpFunctionCall) {
      auto callee = inst.getOperand(1).getAsValue();
      auto &calleeInfo = collectFunctionInfo(moduleInfo, callee);
      auto args = inst.getOperands().subspan(2);

      for (std::size_t index = 0; auto &[_, access] : calleeInfo.parameters) {
        trackAccess(args[index++].getAsValue(), access);
      }
      for (auto &[global, access] : calleeInfo.variables) {
        trackAccess(global, access);
      }
      continue;
    }

    if (inst == ir::spv::OpLoad || inst == ir::spv::OpAtomicLoad) {
      trackAccess(inst.getOperand(1).getAsValue(), Access::Read);
      continue;
    }

    if (inst == ir::spv::OpStore || inst == ir::spv::OpAtomicStore) {
      trackAccess(inst.getOperand(0).getAsValue(), Access::Write);
      continue;
    }

    if (inst == ir::spv::OpAtomicExchange ||
        inst == ir::spv::OpAtomicCompareExchange ||
        inst == ir::spv::OpAtomicCompareExchangeWeak ||
        inst == ir::spv::OpAtomicIIncrement ||
        inst == ir::spv::OpAtomicIDecrement || inst == ir::spv::OpAtomicIAdd ||
        inst == ir::spv::OpAtomicISub || inst == ir::spv::OpAtomicSMin ||
        inst == ir::spv::OpAtomicUMin || inst == ir::spv::OpAtomicSMax ||
        inst == ir::spv::OpAtomicUMax || inst == ir::spv::OpAtomicAnd ||
        inst == ir::spv::OpAtomicOr || inst == ir::spv::OpAtomicXor) {
      trackAccess(inst.getOperand(1).getAsValue(), Access::ReadWrite);
    }
  }

  return result;
}

void shader::collectModuleInfo(ModuleInfo &moduleInfo,
                               const spv::BinaryLayout &layout) {
  auto functions = layout.regions[spv::BinaryLayout::kFunctions];

  if (!functions) {
    return;
  }

  for (auto child : functions.children<ir::Value>()) {
    if (child == ir::spv::OpFunction) {
      collectFunctionInfo(moduleInfo, child);
    }
  }
}
