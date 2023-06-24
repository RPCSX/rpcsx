#pragma once

#include <cstdint>

namespace amdgpu::shader {
class RegisterId {
  static constexpr std::uint32_t kScalarOperandsOffset = 0;
  static constexpr std::uint32_t kScalarOperandsCount = 256;
  static constexpr std::uint32_t kVectorOperandsOffset =
      kScalarOperandsOffset + kScalarOperandsCount;
  static constexpr std::uint32_t kVectorOperandsCount = 512;
  static constexpr std::uint32_t kExportOperandsOffset =
      kVectorOperandsOffset + kVectorOperandsCount;
  static constexpr std::uint32_t kExportOperandsCount = 64;
  static constexpr std::uint32_t kAttrOperandsOffset =
      kExportOperandsOffset + kExportOperandsCount;
  static constexpr std::uint32_t kAttrOperandsCount = 32;
  static constexpr std::uint32_t kOperandsCount =
      kAttrOperandsOffset + kAttrOperandsCount;

  static constexpr std::uint32_t kRegisterVccLoId = kScalarOperandsOffset + 106;
  static constexpr std::uint32_t kRegisterVccHiId = kScalarOperandsOffset + 107;
  static constexpr std::uint32_t kRegisterM0Id = kScalarOperandsOffset + 124;
  static constexpr std::uint32_t kRegisterExecLoId =
      kScalarOperandsOffset + 126;
  static constexpr std::uint32_t kRegisterExecHiId =
      kScalarOperandsOffset + 127;
  static constexpr std::uint32_t kRegisterSccId = kScalarOperandsOffset + 253;
  static constexpr std::uint32_t kRegisterLdsDirect =
      kScalarOperandsOffset + 254;

public:
  enum enum_type : std::uint32_t {
    Invalid = ~static_cast<std::uint32_t>(0),

    VccLo = kRegisterVccLoId,
    VccHi = kRegisterVccHiId,
    M0 = kRegisterM0Id,
    ExecLo = kRegisterExecLoId,
    ExecHi = kRegisterExecHiId,
    Scc = kRegisterSccId,
    LdsDirect = kRegisterLdsDirect,
  } raw = Invalid;

  RegisterId(enum_type value) : raw(value) {}

  operator enum_type() const { return raw; }

  static RegisterId Raw(std::uint32_t index) {
    return static_cast<enum_type>(index);
  }
  static RegisterId Scalar(std::uint32_t index) {
    return static_cast<enum_type>(index + kScalarOperandsOffset);
  }
  static RegisterId Vector(std::uint32_t index) {
    return static_cast<enum_type>(index + kVectorOperandsOffset);
  }
  static RegisterId Export(std::uint32_t index) {
    return static_cast<enum_type>(index + kExportOperandsOffset);
  }
  static RegisterId Attr(std::uint32_t index) {
    return static_cast<enum_type>(index + kAttrOperandsOffset);
  }

  bool isScalar() const {
    return raw >= kScalarOperandsOffset &&
           raw < kScalarOperandsOffset + kScalarOperandsCount;
  }
  bool isVector() const {
    return raw >= kVectorOperandsOffset &&
           raw < kVectorOperandsOffset + kVectorOperandsCount;
  }
  bool isExport() const {
    return raw >= kExportOperandsOffset &&
           raw < kExportOperandsOffset + kExportOperandsCount;
  }
  bool isAttr() const {
    return raw >= kAttrOperandsOffset &&
           raw < kAttrOperandsOffset + kAttrOperandsCount;
  }

  unsigned getOffset() const {
    if (isScalar()) {
      return raw - kScalarOperandsOffset;
    }

    if (isVector()) {
      return raw - kVectorOperandsOffset;
    }

    if (isExport()) {
      return raw - kExportOperandsOffset;
    }

    if (isAttr()) {
      return raw - kAttrOperandsOffset;
    }

    return raw;
  }
};
} // namespace amdgpu::shader
