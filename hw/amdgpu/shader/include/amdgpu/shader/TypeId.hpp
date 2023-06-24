#pragma once

#include <cstddef>

namespace amdgpu::shader {
struct TypeId {
  enum {
    Bool,
    SInt8,
    UInt8,
    SInt16,
    UInt16,
    SInt32,
    UInt32,
    UInt32x2,
    UInt32x3,
    UInt32x4,
    UInt64,
    SInt64,
    ArrayUInt32x8,
    ArrayUInt32x16,
    Float16,
    Float32,
    Float32x2,
    Float32x3,
    Float32x4,
    Float64,
    ArrayFloat32x8,
    ArrayFloat32x16,
    Sampler,
    Image2D,
    SampledImage2D,

    Void // should be last
  } raw = Void;

  using enum_type = decltype(raw);

  TypeId() = default;
  TypeId(enum_type value) : raw(value) {}
  operator enum_type() const { return raw; }

  TypeId getBaseType() const;
  std::size_t getSize() const;
  std::size_t getElementsCount() const;

  bool isSignedInt() const {
    return raw == TypeId::SInt8 || raw == TypeId::SInt16 ||
           raw == TypeId::SInt32 || raw == TypeId::SInt64;
  }

  bool isFloatPoint() const {
    return raw == TypeId::Float16 || raw == TypeId::Float32 ||
           raw == TypeId::Float64;
  }
};
} // namespace amdgpu::shader
