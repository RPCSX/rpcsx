#pragma once

#include "gcn.hpp"
#include "rx/MemoryTable.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace shader::gcn {
enum class VsSGprInput {
  State,
  StreamOutWriteIndex,
  StreamOutBaseOffset0,
  StreamOutBaseOffset1,
  StreamOutBaseOffset2,
  StreamOutBaseOffset3,
  OffchipLds,
  WaveId,
  Scratch,

  Count,
};

enum class PsSGprInput {
  State,
  WaveCount,
  Scratch,

  Count,
};

enum class GsSGprInput {
  GsVsOffset,
  GsWaveId,
  Scratch,

  Count,
};

enum class EsSGprInput {
  OffchipLds,
  IsOffchip,
  EsGsOffset,
  Scratch,

  Count,
};

enum class HsSGprInput {
  OffchipLds,
  ThreadGroupSize,
  TesselationFactorBase,
  Scratch,

  Count,
};

enum class LsSGprInput {
  Scratch,

  Count,
};

enum class CsSGprInput {
  ThreadGroupIdX,
  ThreadGroupIdY,
  ThreadGroupIdZ,
  ThreadGroupSize,
  Scratch,

  Count,
};

enum class PsVGprInput {
  IPerspSample,
  JPerspSample,
  IPerspCenter,
  JPerspCenter,
  IPerspCentroid,
  JPerspCentroid,
  IW,
  JW,
  _1W,
  ILinearSample,
  JLinearSample,
  ILinearCenter,
  JLinearCenter,
  ILinearCentroid,
  JLinearCentroid,
  X,
  Y,
  Z,
  W,
  FrontFace,
  Ancillary,
  SampleCoverage,
  PosFixed,

  Count
};

enum class ConfigType {
  Imm,
  UserSgpr,
  ResourceSlot,
  MemoryTable,
  Gds,
  PsInputVGpr,
  VsInputSGpr,
  PsInputSGpr,
  GsInputSGpr,
  EsInputSGpr,
  HsInputSGpr,
  LsInputSGpr,
  CsInputSGpr,
  GsPrimType,
  GsInstanceEn,
  InstanceEn,
  VsPrimType,
  VsIndexOffset,
  PsPrimType,
  CsTgIdCompCnt,
  VsInputVgprCount,
  CbCompSwap,
  ViewPortOffsetX,
  ViewPortOffsetY,
  ViewPortOffsetZ,
  ViewPortScaleX,
  ViewPortScaleY,
  ViewPortScaleZ,
};

struct ConfigSlot {
  ConfigType type;
  std::uint64_t data;
};

struct Resources {
  struct Resource {
    std::uint32_t resourceSlot;
  };

  struct Pointer : Resource {
    std::uint32_t size;
    ir::Value base;
    ir::Value offset;
  };

  struct Texture : Resource {
    Access access;
    ir::Value words[8];
  };

  struct Buffer : Resource {
    Access access;
    ir::Value words[4];
  };

  struct Sampler : Resource {
    bool unorm;
    ir::Value words[4];
  };

  spv::Context context;
  bool hasUnknown = false;
  std::uint32_t slots = 0;
  std::vector<Pointer> pointers;
  std::vector<Texture> textures;
  std::vector<Buffer> buffers;
  std::vector<Sampler> samplers;

  void print(std::ostream &os, ir::NameStorage &ns) const;
  void dump();
};

struct ShaderInfo {
  std::vector<ConfigSlot> configSlots;
  rx::MemoryAreaTable<> memoryMap;
  std::vector<std::pair<int, std::uint32_t>> requiredSgprs;
  Resources resources;

  std::uint32_t create(ConfigType type, std::uint64_t data) {
    for (std::size_t slotIndex = 0; auto &slotInfo : configSlots) {
      if (slotInfo.type == type && slotInfo.data == data) {
        return slotIndex;
      }

      slotIndex++;
    }

    configSlots.push_back({
        .type = type,
        .data = data,
    });

    return configSlots.size() - 1;
  }
};

struct ConvertedShader {
  std::vector<std::uint32_t> spv;
  ShaderInfo info;
};

std::optional<ConvertedShader>
convertToSpv(Context &context, ir::Region body,
             const SemanticModuleInfo &semanticModule, Stage stage,
             const Environment &state);

} // namespace shader::gcn
