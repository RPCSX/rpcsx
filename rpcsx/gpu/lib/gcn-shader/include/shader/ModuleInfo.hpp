#pragma once

#include "Access.hpp"
#include "ir/Value.hpp"
#include "spv.hpp"
#include <map>
#include <vector>

namespace shader {
struct ModuleInfo {
  struct Param {
    ir::Value type;
    Access access = Access::None;
  };

  struct Function {
    std::map<ir::Value, Access> variables;
    std::vector<Param> parameters;
    ir::Value returnType;
  };

  std::map<ir::Value, Function> functions;
};

ModuleInfo::Function &collectFunctionInfo(ModuleInfo &moduleInfo,
                                          ir::Value function);
void collectModuleInfo(ModuleInfo &moduleInfo, const spv::BinaryLayout &layout);
} // namespace shader
