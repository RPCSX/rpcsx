#pragma once

#include "ModuleInfo.hpp"
#include "SpvTypeInfo.hpp"

namespace shader {
struct SemanticModuleInfo : ModuleInfo {
  std::unordered_map<ir::InstructionId, ir::Value> semantics;

  ir::Value findSemanticOf(ir::InstructionId sem) const {
    auto semIt = semantics.find(sem);
    if (semIt == semantics.end()) {
      return nullptr;
    }

    return semIt->second;
  }
};

struct SemanticInfo {
  struct Param {
    spv::TypeInfo type;
    Access access = Access::None;
  };

  struct Function {
    std::unordered_map<int, Access> registerAccesses;
    std::vector<Param> parameters;
    spv::TypeInfo returnType;
    Access bufferAccess = Access::None;
  };

  std::unordered_map<ir::InstructionId, Function> semantics;

  const Function *findSemantic(ir::InstructionId sem) const {
    if (auto it = semantics.find(sem); it != semantics.end()) {
      return &it->second;
    }

    return nullptr;
  }
};

void collectSemanticModuleInfo(SemanticModuleInfo &moduleInfo,
                               const spv::BinaryLayout &layout);
} // namespace shader
