#include "SemanticInfo.hpp"
#include "dialect.hpp"

using namespace shader;

static std::size_t getOpCount(ir::Kind kind) {
  switch (kind) {
  case ir::Kind::Spv:
  case ir::Kind::Builtin:
  case ir::Kind::MemSSA:
    break;

  case ir::Kind::AmdGpu:
    return ir::amdgpu::OpCount;
  case ir::Kind::Vop2:
    return ir::vop2::OpCount;
  case ir::Kind::Sop2:
    return ir::sop2::OpCount;
  case ir::Kind::Sopk:
    return ir::sopk::OpCount;
  case ir::Kind::Smrd:
    return ir::smrd::OpCount;
  case ir::Kind::Vop3:
    return ir::vop3::OpCount;
  case ir::Kind::Mubuf:
    return ir::mubuf::OpCount;
  case ir::Kind::Mtbuf:
    return ir::mtbuf::OpCount;
  case ir::Kind::Mimg:
    return ir::mimg::OpCount;
  case ir::Kind::Ds:
    return ir::ds::OpCount;
  case ir::Kind::Vintrp:
    return ir::vintrp::OpCount;
  case ir::Kind::Exp:
    return 1;
  case ir::Kind::Vop1:
    return ir::vop1::OpCount;
  case ir::Kind::Vopc:
    return ir::vopc::OpCount;
  case ir::Kind::Sop1:
    return ir::sop1::OpCount;
  case ir::Kind::Sopc:
    return ir::sopc::OpCount;
  case ir::Kind::Sopp:
    return ir::sopp::OpCount;
  case ir::Kind::Count:
    break;
  }

  return 0;
}

void shader::collectSemanticModuleInfo(SemanticModuleInfo &moduleInfo,
                                       const spv::BinaryLayout &layout) {
  static auto instNameToIds = [] {
    std::map<std::string, std::vector<ir::InstructionId>, std::less<>> result;
    for (std::size_t kind = 0; kind < std::size_t(ir::Kind::Count); ++kind) {
      auto opCount = getOpCount(ir::Kind(kind));

      for (unsigned op = 0; op < opCount; ++op) {
        auto name = getInstructionShortName(ir::Kind(kind), op);
        if (name == nullptr) {
          continue;
        }

        result[name].push_back(ir::getInstructionId(ir::Kind(kind), op));
      }
    }
    return result;
  }();

  collectModuleInfo(moduleInfo, layout);

  static auto wideInstNameToIds = [] {
    std::map<std::string, std::vector<ir::InstructionId>, std::less<>> result;
    for (std::size_t kind = 0; kind < std::size_t(ir::Kind::Count); ++kind) {
      auto opCount = getOpCount(ir::Kind(kind));
      if (opCount == 0) {
        continue;
      }

      for (unsigned op = 0; op < opCount; ++op) {
        auto name = getInstructionShortName(ir::Kind(kind), op);
        if (name == nullptr) {
          continue;
        }

        std::string wideName = getKindName(ir::Kind(kind));
        wideName += '_';
        wideName += name;

        result[std::move(wideName)].push_back(
            ir::getInstructionId(ir::Kind(kind), op));
      }
    }
    return result;
  }();

  for (auto &[fn, info] : moduleInfo.functions) {
    for (auto &use : fn.getUseList()) {
      if (use.user != ir::spv::OpName) {
        continue;
      }

      auto mangledNameString = use.user.getOperand(1).getAsString();

      if (mangledNameString == nullptr) {
        break;
      }

      auto mangledName = std::string_view(*mangledNameString);
      std::string_view name;
      if (auto pos = mangledName.find('('); pos != std::string_view::npos) {
        name = mangledName.substr(0, pos);
      } else {
        break;
      }

      std::vector<ir::InstructionId> *ids = nullptr;
      std::vector<ir::InstructionId> *wideIds = nullptr;

      if (auto it = wideInstNameToIds.find(name);
          it != wideInstNameToIds.end()) {
        wideIds = &it->second;
      }

      if (auto it = instNameToIds.find(name); it != instNameToIds.end()) {
        ids = &it->second;
      }

      if (ids == nullptr && wideIds == nullptr) {
        break;
      }

      if (wideIds != nullptr) {
        for (auto id : *wideIds) {
          moduleInfo.semantics[id] = fn;
        }
      } else {
        for (auto id : *ids) {
          moduleInfo.semantics.emplace(id, fn);
        }
      }

      break;
    }
  }
}
