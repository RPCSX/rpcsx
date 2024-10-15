#pragma once

#include "gnm/constants.hpp"
#include "tiler.hpp"
#include <cstdint>

namespace amdgpu {
std::uint64_t getTiledOffset(gnm::TextureType texType, bool isPow2Padded,
                             int numFragments, gnm::DataFormat dfmt,
                             amdgpu::TileMode tileMode,
                             amdgpu::MacroTileMode macroTileMode, int mipLevel,
                             int arraySlice, int width, int height, int depth,
                             int pitch, int x, int y, int z, int fragmentIndex);
}
