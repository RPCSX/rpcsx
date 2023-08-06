#pragma once

#include "AccessOp.hpp"
#include "Stage.hpp"

#include <amdgpu/RemoteMemory.hpp>
#include <util/area.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace amdgpu::shader {
struct Shader {
  enum class UniformKind { Buffer, Sampler, StorageImage, Image };

  struct UniformInfo {
    std::uint32_t binding;
    std::uint32_t buffer[8];
    UniformKind kind;
    AccessOp accessOp;
  };

  std::vector<UniformInfo> uniforms;
  std::vector<std::uint32_t> spirv;
};

Shader convert(RemoteMemory memory, Stage stage, std::uint64_t entry,
               std::span<const std::uint32_t> userSpgrs, std::uint32_t dimX,
               std::uint32_t dimY, std::uint32_t dimZ,
               util::MemoryAreaTable<> &dependencies);
} // namespace amdgpu::shader
