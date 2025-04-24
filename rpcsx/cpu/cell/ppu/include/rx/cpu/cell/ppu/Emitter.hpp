#pragma once

#include "Instruction.hpp"
#include <cstdint>

namespace rx::cell::ppu {
inline namespace registers {
enum {
  r0,
  r1,
  r2,
  r3,
  r4,
  r5,
  r6,
  r7,
  r8,
  r9,
  r10,
  r11,
  r12,
  r13,
  r14,
  r15,
  r16,
  r17,
  r18,
  r19,
  r20,
  r21,
  r22,
  r23,
  r24,
  r25,
  r26,
  r27,
  r28,
  r29,
  r30,
  r31,
};

enum {
  f0,
  f1,
  f2,
  f3,
  f4,
  f5,
  f6,
  f7,
  f8,
  f9,
  f10,
  f11,
  f12,
  f13,
  f14,
  f15,
  F16,
  f17,
  f18,
  f19,
  f20,
  f21,
  f22,
  f23,
  f24,
  f25,
  f26,
  f27,
  f28,
  f29,
  f30,
  f31,
};

enum {
  v0,
  v1,
  v2,
  v3,
  v4,
  v5,
  v6,
  v7,
  v8,
  v9,
  v10,
  v11,
  v12,
  v13,
  v14,
  v15,
  v16,
  v17,
  v18,
  v19,
  v20,
  v21,
  v22,
  v23,
  v24,
  v25,
  v26,
  v27,
  v28,
  v29,
  v30,
  v31,
};

enum {
  cr0,
  cr1,
  cr2,
  cr3,
  cr4,
  cr5,
  cr6,
  cr7,
};
} // namespace registers

inline std::uint32_t ADDI(std::uint32_t rt, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x0eu << 26};
  op.rd = rt;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t ADDIS(std::uint32_t rt, std::uint32_t ra,
                           std::int32_t si) {
  Instruction op{0x0fu << 26};
  op.rd = rt;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t XORIS(std::uint32_t rt, std::uint32_t ra,
                           std::int32_t si) {
  Instruction op{0x1bu << 26};
  op.rd = rt;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t ORI(std::uint32_t rt, std::uint32_t ra, std::uint32_t ui) {
  Instruction op{0x18u << 26};
  op.rd = rt;
  op.ra = ra;
  op.uimm16 = ui;
  return op.raw;
}
inline std::uint32_t ORIS(std::uint32_t rt, std::uint32_t ra,
                          std::uint32_t ui) {
  Instruction op{0x19u << 26};
  op.rd = rt;
  op.ra = ra;
  op.uimm16 = ui;
  return op.raw;
}
inline std::uint32_t OR(std::uint32_t ra, std::uint32_t rs, std::uint32_t rb,
                        bool rc = false) {
  Instruction op{0x1fu << 26 | 0x1bcu << 1};
  op.rs = rs;
  op.ra = ra;
  op.rb = rb;
  op.rc = rc;
  return op.raw;
}
inline std::uint32_t SC(std::uint32_t lev) {
  Instruction op{0x11u << 26 | 1 << 1};
  op.lev = lev;
  return op.raw;
}
inline std::uint32_t B(std::int32_t li, bool aa = false, bool lk = false) {
  Instruction op{0x12u << 26};
  op.ll = li;
  op.aa = aa;
  op.lk = lk;
  return op.raw;
}
inline std::uint32_t BC(std::uint32_t bo, std::uint32_t bi, std::int32_t bd,
                        bool aa = false, bool lk = false) {
  Instruction op{0x10u << 26};
  op.bo = bo;
  op.bi = bi;
  op.ds = bd / 4;
  op.aa = aa;
  op.lk = lk;
  return op.raw;
}
inline std::uint32_t BCLR(std::uint32_t bo, std::uint32_t bi, std::uint32_t bh,
                          bool lk = false) {
  Instruction op{0x13u << 26 | 0x10u << 1};
  op.bo = bo;
  op.bi = bi;
  op.bh = bh;
  op.lk = lk;
  return op.raw;
}
inline std::uint32_t BCCTR(std::uint32_t bo, std::uint32_t bi, std::uint32_t bh,
                           bool lk = false) {
  Instruction op{0x13u << 26 | 0x210u << 1};
  op.bo = bo;
  op.bi = bi;
  op.bh = bh;
  op.lk = lk;
  return op.raw;
}
inline std::uint32_t MFSPR(std::uint32_t rt, std::uint32_t spr) {
  Instruction op{0x1fu << 26 | 0x153u << 1};
  op.rd = rt;
  op.spr = spr;
  return op.raw;
}
inline std::uint32_t MTSPR(std::uint32_t spr, std::uint32_t rs) {
  Instruction op{0x1fu << 26 | 0x1d3u << 1};
  op.rs = rs;
  op.spr = spr;
  return op.raw;
}
inline std::uint32_t LWZ(std::uint32_t rt, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x20u << 26};
  op.rd = rt;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t STW(std::uint32_t rt, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x24u << 26};
  op.rd = rt;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t STD(std::uint32_t rs, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x3eu << 26};
  op.rs = rs;
  op.ra = ra;
  op.ds = si / 4;
  return op.raw;
}
inline std::uint32_t STDU(std::uint32_t rs, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x3eu << 26 | 1};
  op.rs = rs;
  op.ra = ra;
  op.ds = si / 4;
  return op.raw;
}
inline std::uint32_t LD(std::uint32_t rt, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x3au << 26};
  op.rd = rt;
  op.ra = ra;
  op.ds = si / 4;
  return op.raw;
}
inline std::uint32_t LDU(std::uint32_t rt, std::uint32_t ra, std::int32_t si) {
  Instruction op{0x3au << 26 | 1};
  op.rd = rt;
  op.ra = ra;
  op.ds = si / 4;
  return op.raw;
}
inline std::uint32_t CMPI(std::uint32_t bf, std::uint32_t l, std::uint32_t ra,
                          std::uint32_t ui) {
  Instruction op{0xbu << 26};
  op.crfd = bf;
  op.l10 = l;
  op.ra = ra;
  op.uimm16 = ui;
  return op.raw;
}
inline std::uint32_t CMPLI(std::uint32_t bf, std::uint32_t l, std::uint32_t ra,
                           std::uint32_t ui) {
  Instruction op{0xau << 26};
  op.crfd = bf;
  op.l10 = l;
  op.ra = ra;
  op.uimm16 = ui;
  return op.raw;
}
inline std::uint32_t RLDICL(std::uint32_t ra, std::uint32_t rs,
                            std::uint32_t sh, std::uint32_t mb,
                            bool rc = false) {
  Instruction op{30 << 26};
  op.ra = ra;
  op.rs = rs;
  op.sh64 = sh;
  op.mbe64 = mb;
  op.rc = rc;
  return op.raw;
}
inline std::uint32_t RLDICR(std::uint32_t ra, std::uint32_t rs,
                            std::uint32_t sh, std::uint32_t mb,
                            bool rc = false) {
  return RLDICL(ra, rs, sh, mb, rc) | 1 << 2;
}
inline std::uint32_t STFD(std::uint32_t frs, std::uint32_t ra,
                          std::int32_t si) {
  Instruction op{54u << 26};
  op.frs = frs;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t STVX(std::uint32_t vs, std::uint32_t ra,
                          std::uint32_t rb) {
  Instruction op{31 << 26 | 231 << 1};
  op.vs = vs;
  op.ra = ra;
  op.rb = rb;
  return op.raw;
}
inline std::uint32_t LFD(std::uint32_t frd, std::uint32_t ra, std::int32_t si) {
  Instruction op{50u << 26};
  op.frd = frd;
  op.ra = ra;
  op.simm16 = si;
  return op.raw;
}
inline std::uint32_t LVX(std::uint32_t vd, std::uint32_t ra, std::uint32_t rb) {
  Instruction op{31 << 26 | 103 << 1};
  op.vd = vd;
  op.ra = ra;
  op.rb = rb;
  return op.raw;
}
inline constexpr std::uint32_t EIEIO() { return 0x7c0006ac; }

inline namespace implicts {
inline std::uint32_t NOP() { return ORI(r0, r0, 0); }
inline std::uint32_t MR(std::uint32_t rt, std::uint32_t ra) {
  return OR(rt, ra, ra, false);
}
inline std::uint32_t LI(std::uint32_t rt, std::uint32_t imm) {
  return ADDI(rt, r0, imm);
}
inline std::uint32_t LIS(std::uint32_t rt, std::uint32_t imm) {
  return ADDIS(rt, r0, imm);
}

inline std::uint32_t BLR() { return BCLR(0x10 | 0x04, 0, 0); }
inline std::uint32_t BCTR() { return BCCTR(0x10 | 0x04, 0, 0); }
inline std::uint32_t BCTRL() { return BCCTR(0x10 | 0x04, 0, 0, true); }
inline std::uint32_t MFCTR(std::uint32_t reg) { return MFSPR(reg, 9 << 5); }
inline std::uint32_t MTCTR(std::uint32_t reg) { return MTSPR(9 << 5, reg); }
inline std::uint32_t MFLR(std::uint32_t reg) { return MFSPR(reg, 8 << 5); }
inline std::uint32_t MTLR(std::uint32_t reg) { return MTSPR(8 << 5, reg); }

inline std::uint32_t BNE(std::uint32_t cr, std::int32_t imm) {
  return BC(4, 2 | cr << 2, imm);
}
inline std::uint32_t BEQ(std::uint32_t cr, std::int32_t imm) {
  return BC(12, 2 | cr << 2, imm);
}
inline std::uint32_t BGT(std::uint32_t cr, std::int32_t imm) {
  return BC(12, 1 | cr << 2, imm);
}
inline std::uint32_t BNE(std::int32_t imm) { return BNE(cr0, imm); }
inline std::uint32_t BEQ(std::int32_t imm) { return BEQ(cr0, imm); }
inline std::uint32_t BGT(std::int32_t imm) { return BGT(cr0, imm); }

inline std::uint32_t CMPDI(std::uint32_t cr, std::uint32_t reg,
                           std::uint32_t imm) {
  return CMPI(cr, 1, reg, imm);
}
inline std::uint32_t CMPDI(std::uint32_t reg, std::uint32_t imm) {
  return CMPDI(cr0, reg, imm);
}
inline std::uint32_t CMPWI(std::uint32_t cr, std::uint32_t reg,
                           std::uint32_t imm) {
  return CMPI(cr, 0, reg, imm);
}
inline std::uint32_t CMPWI(std::uint32_t reg, std::uint32_t imm) {
  return CMPWI(cr0, reg, imm);
}
inline std::uint32_t CMPLDI(std::uint32_t cr, std::uint32_t reg,
                            std::uint32_t imm) {
  return CMPLI(cr, 1, reg, imm);
}
inline std::uint32_t CMPLDI(std::uint32_t reg, std::uint32_t imm) {
  return CMPLDI(cr0, reg, imm);
}
inline std::uint32_t CMPLWI(std::uint32_t cr, std::uint32_t reg,
                            std::uint32_t imm) {
  return CMPLI(cr, 0, reg, imm);
}
inline std::uint32_t CMPLWI(std::uint32_t reg, std::uint32_t imm) {
  return CMPLWI(cr0, reg, imm);
}

inline std::uint32_t EXTRDI(std::uint32_t x, std::uint32_t y, std::uint32_t n,
                            std::uint32_t b) {
  return RLDICL(x, y, b + n, 64 - b, false);
}
inline std::uint32_t SRDI(std::uint32_t x, std::uint32_t y, std::uint32_t n) {
  return RLDICL(x, y, 64 - n, n, false);
}
inline std::uint32_t CLRLDI(std::uint32_t x, std::uint32_t y, std::uint32_t n) {
  return RLDICL(x, y, 0, n, false);
}
inline std::uint32_t CLRRDI(std::uint32_t x, std::uint32_t y, std::uint32_t n) {
  return RLDICR(x, y, 0, 63 - n, false);
}

inline constexpr std::uint32_t TRAP() { return 0x7FE00008; } // tw 31,r0,r0
} // namespace implicts
} // namespace rx::cell::ppu
