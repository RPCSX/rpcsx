#pragma once

#include "Stage.hpp"
#include "util/unreachable.hpp"

namespace amdgpu::shader {
struct UniformBindings {
  static constexpr auto kBufferSlots = 16;
  static constexpr auto kImageSlots = 16;
  static constexpr auto kSamplerSlots = 16;

  static constexpr auto kBufferOffset = 0;
  static constexpr auto kImageOffset = kBufferOffset + kBufferSlots;
  static constexpr auto kSamplerOffset = kImageOffset + kImageSlots;

  static constexpr auto kStageSize = kSamplerOffset + kSamplerSlots;

  static constexpr auto kVertexOffset = 0;
  static constexpr auto kFragmentOffset = kStageSize;

  static unsigned getBufferBinding(Stage stage, unsigned index) {
    if (index >= kBufferSlots) {
      util::unreachable();
    }

    return index + getStageOffset(stage) + kBufferOffset;
  }

  static unsigned getImageBinding(Stage stage, unsigned index) {
    if (index >= kImageSlots) {
      util::unreachable();
    }

    return index + getStageOffset(stage) + kImageOffset;
  }

  static unsigned getSamplerBinding(Stage stage, unsigned index) {
    if (index >= kSamplerSlots) {
      util::unreachable();
    }

    return index + getStageOffset(stage) + kSamplerOffset;
  }

private:
  static unsigned getStageOffset(Stage stage) {
    switch (stage) {
    case Stage::Fragment:
      return kFragmentOffset;

    case Stage::Vertex:
      return kVertexOffset;

    case Stage::Compute:
      return kVertexOffset;

    default:
      util::unreachable();
    }
  }
};
} // namespace amdgpu::shader
