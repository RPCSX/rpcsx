#pragma once

#include "SemanticInfo.hpp"
#include "SpvConverter.hpp"
#include "analyze.hpp"
#include "rx/MemoryTable.hpp"
#include "spv.hpp"

#include <cstdint>
#include <functional>

namespace shader::gcn {
using Builder = ir::Builder<ir::spv::Builder, ir::builtin::Builder>;

enum class Stage {
  Ps,
  VsVs,
  VsEs,
  VsLs,
  Cs,
  Gs,
  GsVs,
  Hs,
  DsVs,
  DsEs,

  Invalid,
};

enum RegId {
  Sgpr,
  Vgpr,
  M0,
  Scc,
  Vcc,
  Exec,
  VccZ,
  ExecZ,
  LdsDirect,
  SgprCount,
  VgprCount,
  ThreadId,
  MemoryTable,
  Gds,
};

struct Import : spv::Import {
  ir::Node getOrCloneImpl(ir::Context &context, ir::Node node,
                          bool isOperand) override;
};

struct SemanticModuleInfo : shader::SemanticModuleInfo {
  std::map<int, ir::Value> registerVariables;
};

void canonicalizeSemantic(ir::Context &context,
                          const spv::BinaryLayout &semantic);
void collectSemanticModuleInfo(SemanticModuleInfo &moduleInfo,
                               const spv::BinaryLayout &layout);
SemanticInfo collectSemanticInfo(const SemanticModuleInfo &moduleInfo);

struct InstructionRegion : ir::RegionLikeImpl {
  ir::RegionLike base;
  ir::Instruction *firstInstruction;

  void insertAfter(ir::Instruction point, ir::Instruction node) {
    if (!*firstInstruction) {
      *firstInstruction = node;
    }

    base.insertAfter(point, node);
  }
};

struct Context : spv::Context {
  ir::Region body;
  rx::MemoryAreaTable<> memoryMap;
  std::uint32_t requiredUserSgprs = 0;
  std::map<RegId, ir::Value> registerVariables;
  std::map<std::uint64_t, ir::Instruction> instructions;
  AnalysisStorage analysis;

  std::pair<ir::Value, bool> getOrCreateLabel(ir::Location loc, ir::Region body,
                                              std::uint64_t address);
  Builder createBuilder(InstructionRegion &region, ir::Region bodyRegion,
                        std::uint64_t address);

  ir::Value createCast(ir::Location loc, Builder &builder, ir::Value targetType,
                       ir::Value value);

  void setRegisterVariable(RegId id, ir::Value value) {
    registerVariables[id] = value;
  }

  ir::Value getOrCreateRegisterVariable(RegId id);

  ir::Value getRegisterRef(ir::Location loc, Builder &builder, RegId id,
                           const ir::Operand &index, ir::Value lane = nullptr);

  ir::Value readReg(ir::Location loc, Builder &builder, ir::Value typeValue,
                    RegId id, const ir::Operand &index,
                    ir::Value lane = nullptr);

  void writeReg(ir::Location loc, Builder &builder, RegId id,
                const ir::Operand &index, ir::Value value,
                ir::Value lane = nullptr);

  ir::Value createRegisterAccess(Builder &builder, ir::Location loc,
                                 ir::Value reg, const ir::Operand &index,
                                 ir::Value lane = nullptr);
};

struct Environment {
  std::uint8_t vgprCount;
  std::uint8_t sgprCount;
  std::uint8_t numThreadX;
  std::uint8_t numThreadY;
  std::uint8_t numThreadZ;
  bool supportsBarycentric = true;
  bool supportsInt8 = false;
  bool supportsInt64Atomics = false;
  bool supportsNonSemanticInfo = false;
  std::span<const std::uint32_t> userSgprs;
};

ir::Region deserialize(Context &context, const Environment &environment,
                       const SemanticInfo &semanticInfo, std::uint64_t base,
                       std::function<std::uint32_t(std::uint64_t)> readMemory);
} // namespace shader::gcn
