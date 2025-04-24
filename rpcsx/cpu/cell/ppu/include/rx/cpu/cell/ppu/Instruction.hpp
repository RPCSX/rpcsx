#pragma once
#include <cstdint>
#include <rx/BitField.h>

namespace rx::cell::ppu {
union Instruction {
  template <typename T, std::uint32_t I, std::uint32_t N>
  using bf = BitField<T, sizeof(T) * 8 - N - I, N>;

  std::uint32_t raw;

  bf<std::uint32_t, 0, 6> main; // 0..5
  BitFieldPack<bf<std::uint32_t, 30, 1>, bf<std::uint32_t, 16, 5>>
      sh64; // 30 + 16..20
  BitFieldPack<bf<std::uint32_t, 26, 1>, bf<std::uint32_t, 21, 5>>
      mbe64;                        // 26 + 21..25
  bf<std::uint32_t, 11, 5> vuimm;   // 11..15
  bf<std::uint32_t, 6, 5> vs;       // 6..10
  bf<std::uint32_t, 22, 4> vsh;     // 22..25
  bf<std::uint32_t, 21, 1> oe;      // 21
  bf<std::uint32_t, 11, 10> spr;    // 11..20
  bf<std::uint32_t, 21, 5> vc;      // 21..25
  bf<std::uint32_t, 16, 5> vb;      // 16..20
  bf<std::uint32_t, 11, 5> va;      // 11..15
  bf<std::uint32_t, 6, 5> vd;       // 6..10
  bf<std::uint32_t, 31, 1> lk;      // 31
  bf<std::uint32_t, 30, 1> aa;      // 30
  bf<std::uint32_t, 16, 5> rb;      // 16..20
  bf<std::uint32_t, 11, 5> ra;      // 11..15
  bf<std::uint32_t, 6, 5> rd;       // 6..10
  bf<std::uint32_t, 16, 16> uimm16; // 16..31
  bf<std::uint32_t, 11, 1> l11;     // 11
  bf<std::uint32_t, 6, 5> rs;       // 6..10
  bf<std::int32_t, 16, 16> simm16;  // 16..31, signed
  bf<std::int32_t, 16, 14> ds;      // 16..29, signed
  bf<std::int32_t, 11, 5> vsimm;    // 11..15, signed
  bf<std::int32_t, 6, 26> ll;       // 6..31, signed
  bf<std::int32_t, 6, 24> li;       // 6..29, signed
  bf<std::uint32_t, 20, 7> lev;     // 20..26
  bf<std::uint32_t, 16, 4> i;       // 16..19
  bf<std::uint32_t, 11, 3> crfs;    // 11..13
  bf<std::uint32_t, 10, 1> l10;     // 10
  bf<std::uint32_t, 6, 3> crfd;     // 6..8
  bf<std::uint32_t, 16, 5> crbb;    // 16..20
  bf<std::uint32_t, 11, 5> crba;    // 11..15
  bf<std::uint32_t, 6, 5> crbd;     // 6..10
  bf<std::uint32_t, 31, 1> rc;      // 31
  bf<std::uint32_t, 26, 5> me32;    // 26..30
  bf<std::uint32_t, 21, 5> mb32;    // 21..25
  bf<std::uint32_t, 16, 5> sh32;    // 16..20
  bf<std::uint32_t, 11, 5> bi;      // 11..15
  bf<std::uint32_t, 6, 5> bo;       // 6..10
  bf<std::uint32_t, 19, 2> bh;      // 19..20
  bf<std::uint32_t, 21, 5> frc;     // 21..25
  bf<std::uint32_t, 16, 5> frb;     // 16..20
  bf<std::uint32_t, 11, 5> fra;     // 11..15
  bf<std::uint32_t, 6, 5> frd;      // 6..10
  bf<std::uint32_t, 12, 8> crm;     // 12..19
  bf<std::uint32_t, 6, 5> frs;      // 6..10
  bf<std::uint32_t, 7, 8> flm;      // 7..14
  bf<std::uint32_t, 6, 1> l6;       // 6
  bf<std::uint32_t, 15, 1> l15;     // 15

  BitFieldPack<bf<std::int32_t, 16, 14>, BitFieldFixed<std::uint32_t, 0, 2>>
      bt14;

  BitFieldPack<bf<std::int32_t, 6, 24>, BitFieldFixed<std::uint32_t, 0, 2>>
      bt24;
};

static_assert(sizeof(Instruction) == sizeof(std::uint32_t));
} // namespace rx::cell::ppu
