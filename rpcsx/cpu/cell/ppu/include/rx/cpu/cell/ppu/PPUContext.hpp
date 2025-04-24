#pragma once

#include "rx/v128.hpp"
#include <cstdint>

struct alignas(4) CrField {
  std::uint8_t bits[4];

  constexpr void set(bool lt, bool gt, bool eq, bool so) {
    bits[0] = lt;
    bits[1] = gt;
    bits[2] = eq;
    bits[3] = so;
  }

  template <typename T>
  constexpr void update(const T &lhs, const T &rhs, bool so) {
    bits[0] = lhs < rhs;
    bits[1] = lhs > rhs;
    bits[2] = lhs == rhs;
    bits[3] = so;
  }

  static constexpr CrField From(bool lt, bool gt, bool eq, bool so) {
    CrField result;
    result.set(lt, gt, eq, so);
    return result;
  }

  [[nodiscard]] constexpr bool isLt() const { return bits[0] != 0; }
  [[nodiscard]] constexpr bool isGt() const { return bits[1] != 0; }
  [[nodiscard]] constexpr bool isEq() const { return bits[2] != 0; }
  [[nodiscard]] constexpr bool isSo() const { return bits[3] != 0; }
};

struct PPUContext {
  std::uint64_t gpr[32] = {}; // General-Purpose Registers
  double fpr[32] = {};        // Floating Point Registers
  rx::v128 vr[32] = {};       // Vector Registers

  union alignas(16) cr_bits {
    std::uint8_t bits[32];
    CrField fields[8];

    std::uint8_t &operator[](std::size_t i) { return bits[i]; }

    // Pack CR bits
    [[nodiscard]] std::uint32_t pack() const {
      std::uint32_t result{};

      for (u32 bit : bits) {
        result <<= 1;
        result |= bit;
      }

      return result;
    }

    // Unpack CR bits
    void unpack(std::uint32_t value) {
      for (u8 &b : bits) {
        b = !!(value & (1u << 31));
        value <<= 1;
      }
    }
  };

  cr_bits cr{}; // Condition Registers (unpacked)

  // Floating-Point Status and Control Register (unpacked)
  union alignas(16) {
    struct {
      // TODO
      bool _start[16];
      bool fl; // FPCC.FL
      bool fg; // FPCC.FG
      bool fe; // FPCC.FE
      bool fu; // FPCC.FU
      bool _end[12];
    };

    CrField fields[8];
    cr_bits bits;
  } fpscr{};

  std::uint64_t lr{};               // Link Register
  std::uint64_t ctr{};              // Counter Register
  std::uint32_t vrsave{0xffffffff}; // vr Save Register
  std::uint32_t cia{};              // Current Instruction Address

  // Fixed-Point Exception Register (abstract representation)
  bool xer_so{};          // Summary Overflow
  bool xer_ov{};          // Overflow
  bool xer_ca{};          // Carry
  std::uint8_t xer_cnt{}; // 0..6

  /*
      Non-Java. A mode control bit that determines whether vector floating-point
     operations will be performed in a Java-IEEE-C9X-compliant mode or a
     possibly faster non-Java/non-IEEE mode. 0	The Java-IEEE-C9X-compliant mode
     is selected. Denormalized values are handled as specified by Java, IEEE,
     and C9X standard. 1	The non-Java/non-IEEE-compliant mode is
     selected. If an element in a source vector register contains a denormalized
     value, the value '0' is used instead. If an instruction causes an underflow
          exception, the corresponding element in the target vr is cleared to
     '0'. In both cases, the '0' has the same sign as the denormalized or
     underflowing value.
  */
  bool nj = true;

  // Sticky saturation bit
  rx::v128 sat{};

  // Optimization: precomputed java-mode mask for handling denormals
  std::uint32_t jm_mask = 0x7f80'0000;

  std::uint32_t raddr{0}; // Reservation addr
  std::uint64_t rtime{0};
  alignas(64) std::byte rdata[128]{}; // Reservation data
  bool use_full_rdata{};
  std::uint32_t res_cached{0}; // Reservation "cached" addresss
  std::uint32_t res_notify{0};
  std::uint64_t res_notify_time{0};

  inline void setOV(bool bit) {
    xer_ov = bit;
    xer_so |= bit;
  }
};
