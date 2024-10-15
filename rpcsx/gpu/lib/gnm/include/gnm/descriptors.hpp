#pragma once

#include "constants.hpp"
#include <compare>
#include <cstdint>

namespace gnm {
#pragma pack(push, 1)
struct VBuffer {
  std::uint64_t base : 44;
  std::uint64_t mtype_L1s : 2;
  std::uint64_t mtype_L2 : 2;
  std::uint64_t stride : 14;
  std::uint64_t cache_swizzle : 1;
  std::uint64_t swizzle_en : 1;

  std::uint32_t num_records;

  Swizzle dst_sel_x : 3;
  Swizzle dst_sel_y : 3;
  Swizzle dst_sel_z : 3;
  Swizzle dst_sel_w : 3;

  NumericFormat nfmt : 3;
  DataFormat dfmt : 4;
  std::uint32_t element_size : 2;
  std::uint32_t index_stride : 2;
  std::uint32_t addtid_en : 1;
  std::uint32_t reserved0 : 1;
  std::uint32_t hash_en : 1;
  std::uint32_t reserved1 : 1;
  std::uint32_t mtype : 3;
  std::uint32_t type : 2;

  std::uint64_t address() const { return base; }
  std::uint64_t size() const {
    return stride ? num_records * stride : num_records;
  }

  auto operator<=>(const VBuffer &) const = default;
};

static_assert(sizeof(VBuffer) == sizeof(std::uint64_t) * 2);

struct TBuffer {
  uint64_t baseaddr256 : 38;
  uint64_t mtype_L2 : 2;
  uint64_t min_lod : 12;
  DataFormat dfmt : 6;
  NumericFormat nfmt : 4;
  uint64_t mtype01 : 2;

  uint64_t width : 14;
  uint64_t height : 14;
  uint64_t perfMod : 3;
  uint64_t interlaced : 1;
  Swizzle dst_sel_x : 3;
  Swizzle dst_sel_y : 3;
  Swizzle dst_sel_z : 3;
  Swizzle dst_sel_w : 3;
  uint64_t base_level : 4;
  uint64_t last_level : 4;
  uint64_t tiling_idx : 5;
  uint64_t pow2pad : 1;
  uint64_t mtype2 : 1;
  uint64_t : 1; // reserved
  TextureType type : 4;

  uint64_t depth : 13;
  uint64_t pitch : 14;
  uint64_t : 5; // reserved
  uint64_t base_array : 13;
  uint64_t last_array : 13;
  uint64_t : 6; // reserved

  uint64_t min_lod_warn : 12; // fixed point 4.8
  uint64_t counter_bank_id : 8;
  uint64_t LOD_hdw_cnt_en : 1;
  uint64_t : 42; // reserved

  std::uint64_t address() const {
    return static_cast<std::uint64_t>(static_cast<std::uint32_t>(baseaddr256))
           << 8;
  }

  auto operator<=>(const TBuffer &) const = default;
};

static_assert(sizeof(TBuffer) == sizeof(std::uint64_t) * 4);

struct SSampler {
  ClampMode clamp_x : 3;
  ClampMode clamp_y : 3;
  ClampMode clamp_z : 3;
  AnisoRatio max_aniso_ratio : 3;
  CompareFunc depth_compare_func : 3;
  int32_t force_unorm_coords : 1;
  int32_t aniso_threshold : 3;
  int32_t mc_coord_trunc : 1;
  int32_t force_degamma : 1;
  int32_t aniso_bias : 6;
  int32_t trunc_coord : 1;
  int32_t disable_cube_wrap : 1;
  FilterMode filter_mode : 2;
  int32_t : 1;
  uint32_t min_lod : 12;
  uint32_t max_lod : 12;
  int32_t perf_mip : 4;
  int32_t perf_z : 4;
  int32_t lod_bias : 14;
  int32_t lod_bias_sec : 6;
  Filter xy_mag_filter : 2;
  Filter xy_min_filter : 2;
  Filter z_filter : 2;
  MipFilter mip_filter : 2;
  int32_t : 4;
  int32_t border_color_ptr : 12;
  int32_t : 18;
  BorderColor border_color_type : 2;

  auto operator<=>(const SSampler &) const = default;
};

static_assert(sizeof(SSampler) == sizeof(std::uint32_t) * 4);
#pragma pack(pop)
} // namespace gnm
