#include "TypeId.hpp"
#include "util/unreachable.hpp"

amdgpu::shader::TypeId amdgpu::shader::TypeId::getBaseType() const {
  switch (raw) {
  case TypeId::Void:
  case TypeId::Bool:
  case TypeId::SInt8:
  case TypeId::UInt8:
  case TypeId::SInt16:
  case TypeId::UInt16:
  case TypeId::SInt32:
  case TypeId::UInt32:
  case TypeId::SInt64:
  case TypeId::UInt64:
  case TypeId::Float16:
  case TypeId::Float32:
  case TypeId::Float64:
  case TypeId::Sampler:
  case TypeId::Image2D:
  case TypeId::StorageImage2D:
  case TypeId::SampledImage2D:
    return raw;

  case TypeId::UInt32x2:
  case TypeId::UInt32x3:
  case TypeId::UInt32x4:
  case TypeId::ArrayUInt32x8:
  case TypeId::ArrayUInt32x16:
    return TypeId::UInt32;

  case TypeId::Float32x2:
  case TypeId::Float32x3:
  case TypeId::Float32x4:
  case TypeId::ArrayFloat32x8:
  case TypeId::ArrayFloat32x16:
    return TypeId::Float32;
  }

  util::unreachable();
}

std::size_t amdgpu::shader::TypeId::getSize() const {
  switch (raw) {
  case TypeId::Void:
  case TypeId::Sampler:
  case TypeId::StorageImage2D:
  case TypeId::Image2D:
  case TypeId::SampledImage2D:
    return 0;
  case TypeId::Bool:
    return 1;
  case TypeId::SInt8:
  case TypeId::UInt8:
    return 1;
  case TypeId::SInt16:
  case TypeId::UInt16:
    return 2;
  case TypeId::SInt32:
  case TypeId::UInt32:
    return 4;
  case TypeId::SInt64:
  case TypeId::UInt64:
    return 8;
  case TypeId::Float16:
    return 2;
  case TypeId::Float32:
    return 4;
  case TypeId::Float64:
    return 8;

  case TypeId::UInt32x2:
  case TypeId::UInt32x3:
  case TypeId::UInt32x4:
  case TypeId::ArrayUInt32x8:
  case TypeId::ArrayUInt32x16:
  case TypeId::Float32x2:
  case TypeId::Float32x3:
  case TypeId::Float32x4:
  case TypeId::ArrayFloat32x8:
  case TypeId::ArrayFloat32x16:
    return getElementsCount() * getBaseType().getSize();
  }

  util::unreachable();
}

std::size_t amdgpu::shader::TypeId::getElementsCount() const {
  switch (raw) {
  case TypeId::Bool:
  case TypeId::SInt8:
  case TypeId::UInt8:
  case TypeId::SInt16:
  case TypeId::UInt16:
  case TypeId::SInt32:
  case TypeId::UInt32:
  case TypeId::SInt64:
  case TypeId::UInt64:
  case TypeId::Float16:
  case TypeId::Float32:
  case TypeId::Float64:
    return 1;

  case TypeId::UInt32x2:
    return 2;
  case TypeId::UInt32x3:
    return 3;
  case TypeId::UInt32x4:
    return 4;
  case TypeId::ArrayUInt32x8:
    return 8;
  case TypeId::ArrayUInt32x16:
    return 16;
  case TypeId::Float32x2:
    return 2;
  case TypeId::Float32x3:
    return 3;
  case TypeId::Float32x4:
    return 4;
  case TypeId::ArrayFloat32x8:
    return 8;
  case TypeId::ArrayFloat32x16:
    return 16;

  case TypeId::Void:
  case TypeId::Sampler:
  case TypeId::Image2D:
  case TypeId::StorageImage2D:
  case TypeId::SampledImage2D:
    return 0;
  }

  util::unreachable();
}
