#pragma once

#include "dialect.hpp"
#include "ir/Kind.hpp"

#include <functional>
#include <ostream>
#include <span>
#include <type_traits>

namespace shader {
struct GcnOperand {
  enum class Kind : std::uint8_t {
    Invalid,
    Constant,
    Immediate,
    VccLo,
    VccHi,
    M0,
    ExecLo,
    ExecHi,
    Scc,
    VccZ,
    ExecZ,
    LdsDirect,
    Vgpr,
    Sgpr,
    Attr,
    Buffer,
    Texture128,
    Texture256,
    ImageBuffer128,
    ImageBuffer256,
    Sampler,
    Pointer,
  };

  static constexpr auto R = 1 << 0;
  static constexpr auto W = 1 << 1;

  union {
    std::uint32_t value;
    std::uint64_t address = 0;

    struct {
      std::uint16_t attrId;
      std::uint16_t attrChannel;
    };

    struct {
      Kind firstRegisterKind;
      union {
        struct {
          Kind pointerOffsetKind;
          std::uint16_t pointeeSize;
        };
        bool samplerUnorm;
      };
      std::uint32_t firstRegisterIndex;

      union {
        std::uint32_t pointerOffsetValue;
        std::uint64_t pointerOffsetAddress;
      };
    };
  };

  Kind kind = Kind::Invalid;
  std::uint8_t access = 0;
  std::uint8_t omod : 4 = 0;
  bool abs : 1 = false;
  bool clamp : 1 = false;
  bool neg : 1 = false;

  constexpr GcnOperand getUnderlyingOperand(int offset = 0) const {
    return {
        .value = firstRegisterIndex + offset,
        .kind = firstRegisterKind,
    };
  }

  constexpr GcnOperand getPointerOffsetOperand() const {
    return {
        .address = pointerOffsetAddress,
        .kind = pointerOffsetKind,
    };
  }

  static constexpr GcnOperand createImmediateConstant(std::uint64_t address) {
    return GcnOperand{
        .address = address,
        .kind = Kind::Immediate,
        .access = R,
    };
  }

  static constexpr GcnOperand createConstant(std::uint32_t value) {
    return GcnOperand{
        .value = value,
        .kind = Kind::Constant,
        .access = R,
    };
  }

  static constexpr GcnOperand createConstant(bool value) {
    return createConstant(std::uint32_t(value ? 1 : 0));
  }

  static constexpr GcnOperand createConstant(float value) {
    return createConstant(std::bit_cast<std::uint32_t>(value));
  }

  static constexpr GcnOperand createVgpr(std::uint32_t index) {
    return {
        .value = index,
        .kind = Kind::Vgpr,
    };
  }

  static constexpr GcnOperand createSgpr(std::uint32_t index) {
    return {
        .value = index,
        .kind = Kind::Sgpr,
    };
  }

  static constexpr GcnOperand createSampler(GcnOperand firstReg, bool unorm) {
    return {
        .firstRegisterKind = firstReg.kind,
        .samplerUnorm = unorm,
        .firstRegisterIndex = static_cast<std::uint8_t>(firstReg.value),
        .kind = Kind::Sampler,
    };
  }
  static constexpr GcnOperand createTexture(GcnOperand firstReg, bool is128) {
    return {
        .firstRegisterKind = firstReg.kind,
        .firstRegisterIndex = static_cast<std::uint8_t>(firstReg.value),
        .kind = (is128 ? Kind::Texture128 : Kind::Texture256),
    };
  }
  static constexpr GcnOperand createImageBuffer(GcnOperand firstReg,
                                                bool is128) {
    return {
        .firstRegisterKind = firstReg.kind,
        .firstRegisterIndex = static_cast<std::uint8_t>(firstReg.value),
        .kind = (is128 ? Kind::ImageBuffer128 : Kind::ImageBuffer256),
    };
  }
  static constexpr GcnOperand createBuffer(GcnOperand firstReg) {
    return {
        .firstRegisterKind = firstReg.kind,
        .firstRegisterIndex = static_cast<std::uint8_t>(firstReg.value),
        .kind = Kind::Buffer,
    };
  }
  static constexpr GcnOperand
  createPointer(GcnOperand firstReg, std::uint16_t size, GcnOperand offset) {
    return {
        .firstRegisterKind = firstReg.kind,
        .pointerOffsetKind = offset.kind,
        .pointeeSize = size,
        .firstRegisterIndex = static_cast<std::uint8_t>(firstReg.value),
        .pointerOffsetAddress = offset.address,
        .kind = Kind::Pointer,
    };
  }

  static constexpr GcnOperand createAttr(std::uint16_t id,
                                         std::uint16_t channel) {
    return {
        .attrId = id,
        .attrChannel = channel,
        .kind = Kind::Attr,
    };
  }

  constexpr GcnOperand withRW() const { return withAccess(R | W); }
  constexpr GcnOperand withR() const { return withAccess(R); }
  constexpr GcnOperand withW() const { return withAccess(W); }

  constexpr GcnOperand withAccess(std::uint8_t access) const {
    GcnOperand result = *this;
    result.access = access;
    return result;
  }

  constexpr GcnOperand withNeg(bool value) const {
    GcnOperand result = *this;
    result.neg = value;
    return result;
  }

  constexpr GcnOperand withAbs(bool value) const {
    GcnOperand result = *this;
    result.abs = value;
    return result;
  }

  constexpr GcnOperand withClamp(bool value) const {
    GcnOperand result = *this;
    result.clamp = value;
    return result;
  }

  constexpr GcnOperand withOutputModifier(std::uint8_t value) const {
    GcnOperand result = *this;
    result.omod = value;
    return result;
  }

  static constexpr GcnOperand createVccLo() { return {.kind = Kind::VccLo}; }
  static constexpr GcnOperand createVccHi() { return {.kind = Kind::VccHi}; }
  static constexpr GcnOperand createM0() { return {.kind = Kind::M0}; }
  static constexpr GcnOperand createExecLo() { return {.kind = Kind::ExecLo}; }
  static constexpr GcnOperand createExecHi() { return {.kind = Kind::ExecHi}; }
  static constexpr GcnOperand createVccZ() { return {.kind = Kind::VccZ}; }
  static constexpr GcnOperand createExecZ() { return {.kind = Kind::ExecZ}; }
  static constexpr GcnOperand createScc() { return {.kind = Kind::Scc}; }
  static constexpr GcnOperand createLdsDirect() {
    return {.kind = Kind::LdsDirect};
  }

  void print(std::ostream &os) const;
  void dump() const;
};

struct GcnInstruction {
  ir::Kind kind = ir::Kind::Builtin;
  unsigned op = ir::builtin::INVALID_INSTRUCTION;
  GcnOperand operands[16];
  std::size_t operandCount{};

  std::span<const GcnOperand> getOperands() const {
    return {operands, operandCount};
  }

  const GcnOperand &getOperand(std::size_t index) const {
    if (index >= operandCount) {
      std::abort();
    }
    return operands[index];
  }

  void addOperand(GcnOperand op) {
    if (operandCount >= std::size(operands)) {
      std::abort();
    }

    operands[operandCount++] = op;
  }

  template <typename T>
  bool operator==(T testOp)
    requires(ir::kOpToKind<std::remove_cvref_t<T>> != ir::Kind::Count)
  {
    return ir::kOpToKind<std::remove_cvref_t<T>> == kind && op == testOp;
  }

  void print(std::ostream &os) const;
  void dump() const;
};

void readGcnInst(GcnInstruction &isaInst, std::uint64_t &address,
                 const std::function<std::uint32_t(std::uint64_t)> &readMemory);
} // namespace shader