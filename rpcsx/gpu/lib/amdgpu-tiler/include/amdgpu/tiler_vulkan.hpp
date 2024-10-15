#pragma once
#include "gnm/constants.hpp"
#include "tiler.hpp"
#include <Scheduler.hpp>
#include <memory>

namespace amdgpu {
struct GpuTiler {
  struct Impl;
  GpuTiler();
  ~GpuTiler();

  void detile(Scheduler &scheduler, const amdgpu::SurfaceInfo &info,
              amdgpu::TileMode tileMode, std::uint64_t srcTiledAddress,
              std::uint64_t srcSize, std::uint64_t dstLinearAddress,
              std::uint64_t dstSize, int mipLevel, int baseArray,
              int arrayCount);
  void tile(Scheduler &scheduler, const amdgpu::SurfaceInfo &info,
            amdgpu::TileMode tileMode, std::uint64_t srcLinearAddress,
            std::uint64_t srcSize, std::uint64_t dstTiledAddress,
            std::uint64_t dstSize, int mipLevel, int baseArray, int arrayCount);

private:
  std::unique_ptr<Impl> mImpl;
};
} // namespace amdgpu
