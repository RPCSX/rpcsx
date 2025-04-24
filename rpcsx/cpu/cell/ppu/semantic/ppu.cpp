#include "Instruction.hpp"
#include "PPUContext.hpp"
#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <rx/simd.hpp>
#include <rx/types.hpp>
#include <rx/v128.hpp>

using namespace rx;
using namespace rx::cell::ppu;

#define EXPORT_SEMANTIC(x)                                                     \
  extern "C" {                                                                 \
  auto ISEL_PPU_##x = x;                                                       \
  auto ISEL_PPU_##x##_DEC = x##_DEC;                                           \
  }

#define SEMANTIC(x) inline x [[gnu::always_inline]]
#define DECODER(x)                                                             \
  inline x##_DEC                                                               \
      [[gnu::always_inline]] ([[maybe_unused]] PPUContext & context,           \
                              [[maybe_unused]] Instruction inst)

template <typename T> struct add_flags_result_t {
  T result;
  bool carry;

  add_flags_result_t() = default;

  // Straighforward ADD with flags
  add_flags_result_t(T a, T b) : result(a + b), carry(result < a) {}

  // Straighforward ADC with flags
  add_flags_result_t(T a, T b, bool c) : add_flags_result_t(a, b) {
    add_flags_result_t r(result, c);
    result = r.result;
    carry |= r.carry;
  }
};

static add_flags_result_t<u64> add64_flags(u64 a, u64 b) { return {a, b}; }

static add_flags_result_t<u64> add64_flags(u64 a, u64 b, bool c) {
  return {a, b, c};
}

extern "C" {
[[noreturn]] void rpcsx_trap();
[[noreturn]] void rpcsx_invalid_instruction();
[[noreturn]] void rpcsx_unimplemented_instruction();

void rpcsx_vm_read(std::uint64_t vaddr, void *dest, std::size_t size);
void rpcsx_vm_write(std::uint64_t vaddr, const void *src, std::size_t size);

std::uint64_t rpcsx_get_tb();
}

namespace {
u32 ppu_fres_mantissas[128] = {
    0x007f0000, 0x007d0800, 0x007b1800, 0x00793000, 0x00775000, 0x00757000,
    0x0073a000, 0x0071e000, 0x00700000, 0x006e4000, 0x006ca000, 0x006ae000,
    0x00694000, 0x00678000, 0x00660000, 0x00646000, 0x0062c000, 0x00614000,
    0x005fc000, 0x005e4000, 0x005cc000, 0x005b4000, 0x0059c000, 0x00584000,
    0x00570000, 0x00558000, 0x00540000, 0x0052c000, 0x00518000, 0x00500000,
    0x004ec000, 0x004d8000, 0x004c0000, 0x004b0000, 0x00498000, 0x00488000,
    0x00474000, 0x00460000, 0x0044c000, 0x00438000, 0x00428000, 0x00418000,
    0x00400000, 0x003f0000, 0x003e0000, 0x003d0000, 0x003bc000, 0x003ac000,
    0x00398000, 0x00388000, 0x00378000, 0x00368000, 0x00358000, 0x00348000,
    0x00338000, 0x00328000, 0x00318000, 0x00308000, 0x002f8000, 0x002ec000,
    0x002e0000, 0x002d0000, 0x002c0000, 0x002b0000, 0x002a0000, 0x00298000,
    0x00288000, 0x00278000, 0x0026c000, 0x00260000, 0x00250000, 0x00244000,
    0x00238000, 0x00228000, 0x00220000, 0x00210000, 0x00200000, 0x001f8000,
    0x001e8000, 0x001e0000, 0x001d0000, 0x001c8000, 0x001b8000, 0x001b0000,
    0x001a0000, 0x00198000, 0x00190000, 0x00180000, 0x00178000, 0x00168000,
    0x00160000, 0x00158000, 0x00148000, 0x00140000, 0x00138000, 0x00128000,
    0x00120000, 0x00118000, 0x00108000, 0x00100000, 0x000f8000, 0x000f0000,
    0x000e0000, 0x000d8000, 0x000d0000, 0x000c8000, 0x000b8000, 0x000b0000,
    0x000a8000, 0x000a0000, 0x00098000, 0x00090000, 0x00080000, 0x00078000,
    0x00070000, 0x00068000, 0x00060000, 0x00058000, 0x00050000, 0x00048000,
    0x00040000, 0x00038000, 0x00030000, 0x00028000, 0x00020000, 0x00018000,
    0x00010000, 0x00000000,
};

u32 ppu_frsqrte_mantissas[16] = {
    0x000f1000u, 0x000d8000u, 0x000c0000u, 0x000a8000u,
    0x00098000u, 0x00088000u, 0x00080000u, 0x00070000u,
    0x00060000u, 0x0004c000u, 0x0003c000u, 0x00030000u,
    0x00020000u, 0x00018000u, 0x00010000u, 0x00008000u,
};

// Large lookup table for FRSQRTE instruction
struct ppu_frsqrte_lut_t {
  // Store only high 32 bits of doubles
  u32 data[0x8000]{};

  constexpr ppu_frsqrte_lut_t() noexcept {
    for (u64 i = 0; i < 0x8000; i++) {
      // Decomposed LUT index
      const u64 sign = i >> 14;
      const u64 expv = (i >> 3) & 0x7ff;

      // (0x3FF - (((EXP_BITS(b) - 0x3FF) >> 1) + 1)) << 52
      const u64 exp = 0x3fe0'0000 - (((expv + 0x1c01) >> 1) << (52 - 32));

      if (expv == 0) // ±INF on zero/denormal, not accurate
      {
        data[i] = static_cast<u32>(0x7ff0'0000 | (sign << 31));
      } else if (expv == 0x7ff) {
        if (i == (0x7ff << 3))
          data[i] = 0; // Zero on +INF, inaccurate
        else
          data[i] = 0x7ff8'0000; // QNaN
      } else if (sign) {
        data[i] = 0x7ff8'0000; // QNaN
      } else {
        // ((MAN_BITS(b) >> 49) & 7ull) + (!(EXP_BITS(b) & 1) << 3)
        const u64 idx = 8 ^ (i & 0xf);

        data[i] = static_cast<u32>(ppu_frsqrte_mantissas[idx] | exp);
      }
    }
  }
} inline ppu_frqrte_lut;
} // namespace

namespace vm {
namespace detail {
template <typename T> struct vm_type_selector {
  using type = be_t<T>;
};
template <typename T> struct vm_type_selector<le_t<T>> {
  using type = le_t<T>;
};
template <typename T> struct vm_type_selector<be_t<T>> {
  using type = be_t<T>;
};

template <typename T>
  requires(sizeof(T) == 1)
struct vm_type_selector<T> {
  using type = T;
};
} // namespace detail

template <typename T> T read(std::uint64_t vaddr) {
  typename detail::vm_type_selector<T>::type result;
  rpcsx_vm_read(vaddr, &result, sizeof(result));
  return T(result);
}

template <typename T> void write(std::uint64_t vaddr, const T &data) {
  typename detail::vm_type_selector<T>::type value = data;
  rpcsx_vm_write(vaddr, &value, sizeof(value));
}

std::uint64_t cast(std::uint64_t address) { return address; }
} // namespace vm

extern void ppu_execute_syscall(PPUContext &context, u64 code);
extern u32 ppu_lwarx(PPUContext &context, u32 addr);
extern u64 ppu_ldarx(PPUContext &context, u32 addr);
extern bool ppu_stwcx(PPUContext &context, u32 addr, u32 reg_value);
extern bool ppu_stdcx(PPUContext &context, u32 addr, u64 reg_value);
extern void ppu_trap(PPUContext &context, u64 addr);

void do_cell_atomic_128_store(u32 addr, const void *to_write);

// NaNs production precedence: NaN from Va, Vb, Vc
// and lastly the result of the operation in case none of the operands is a NaN
// Signaling NaNs are 'quieted' (MSB of fraction is set) with other bits of data
// remain the same
inline v128 ppu_select_vnan(v128 a) { return a; }

inline v128 ppu_select_vnan(v128 a, v128 b) {
  return gv_selectfs(gv_eqfs(a, a), b, a | gv_bcst32(0x7fc00000u));
}

inline v128 ppu_select_vnan(v128 a, v128 b, Vector128 auto... args) {
  return ppu_select_vnan(a, ppu_select_vnan(b, args...));
}

// Flush denormals to zero if NJ is 1
inline v128 ppu_flush_denormal(const v128 &mask, const v128 &a) {
  return gv_andn(gv_shr32(gv_eq32(mask & a, gv_bcst32(0)), 1), a);
}

inline v128 ppu_fix_vnan(v128 r) {
  return gv_selectfs(gv_eqfs(r, r), r, gv_bcst32(0x7fc00000u));
}

inline v128 ppu_set_vnan(v128 r, Vector128 auto... args) {
  return ppu_select_vnan(args..., ppu_fix_vnan(r));
}

template <typename T> auto ppu_feed_data(PPUContext &, u64 addr) {
  static_assert(sizeof(T) <= 128,
                "Incompatible type-size, break down into smaller loads");

  return vm::read<T>(addr);
}

constexpr u64 ppu_rotate_mask(u32 mb, u32 me) {
  const u64 mask = ~0ull << (~(me - mb) & 63);
  return (mask >> (mb & 63)) | (mask << ((64 - mb) & 63));
}
inline u64 dup32(u32 x) { return x | static_cast<u64>(x) << 32; }

void SEMANTIC(MFVSCR)(v128 &d, v128 sat, bool nj) {
  u32 sat_bit = !gv_testz(sat);
  d._u64[0] = 0;
  d._u64[1] = u64(sat_bit | (u32{nj} << 16)) << 32;
}
void DECODER(MFVSCR) { MFVSCR(context.vr[inst.vd], context.sat, context.nj); }
EXPORT_SEMANTIC(MFVSCR);

void SEMANTIC(MTVSCR)(v128 &sat, bool &nj, u32 &jm_mask, v128 b) {
  const u32 vscr = b._u32[3];
  sat._u = vscr & 1;
  jm_mask = (vscr & 0x10000) ? 0x7f80'0000 : 0x7fff'ffff;
  nj = (vscr & 0x10000) != 0;
}
void DECODER(MTVSCR) {
  MTVSCR(context.sat, context.nj, context.jm_mask, context.vr[inst.vb]);
}
EXPORT_SEMANTIC(MTVSCR);

void SEMANTIC(VADDCUW)(v128 &d, v128 a, v128 b) {
  d = gv_sub32(gv_geu32(gv_not32(a), b), gv_bcst32(-1));
}
void DECODER(VADDCUW) {
  VADDCUW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VADDCUW);

void SEMANTIC(VADDFP)(v128 &d, v128 a, v128 b, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  a = ppu_flush_denormal(m, a);
  b = ppu_flush_denormal(m, b);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_addfs(a, b), a, b));
}
void DECODER(VADDFP) {
  VADDFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
         context.jm_mask);
}
EXPORT_SEMANTIC(VADDFP);

void SEMANTIC(VADDSBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_adds_s8(a, b);
  sat = gv_or32(gv_xor32(gv_add8(a, b), r), sat);
  d = r;
}
void DECODER(VADDSBS) {
  VADDSBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDSBS);

void SEMANTIC(VADDSHS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_adds_s16(a, b);
  sat = gv_or32(gv_xor32(gv_add16(a, b), r), sat);
  d = r;
}
void DECODER(VADDSHS) {
  VADDSHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDSHS);

void SEMANTIC(VADDSWS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_adds_s32(a, b);
  sat = gv_or32(gv_xor32(gv_add32(a, b), r), sat);
  d = r;
}
void DECODER(VADDSWS) {
  VADDSWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDSWS);

void SEMANTIC(VADDUBM)(v128 &d, v128 a, v128 b) { d = gv_add8(a, b); }
void DECODER(VADDUBM) {
  VADDUBM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VADDUBM);

void SEMANTIC(VADDUBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_addus_u8(a, b);
  sat = gv_or32(gv_xor32(gv_add8(a, b), r), sat);
  d = r;
}
void DECODER(VADDUBS) {
  VADDUBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDUBS);

void SEMANTIC(VADDUHM)(v128 &d, v128 a, v128 b) { d = gv_add16(a, b); }
void DECODER(VADDUHM) {
  VADDUHM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VADDUHM);

void SEMANTIC(VADDUHS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_addus_u16(a, b);
  sat = gv_or32(gv_xor32(gv_add16(a, b), r), sat);
  d = r;
}
void DECODER(VADDUHS) {
  VADDUHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDUHS);

void SEMANTIC(VADDUWM)(v128 &d, v128 a, v128 b) { d = gv_add32(a, b); }
void DECODER(VADDUWM) {
  VADDUWM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VADDUWM);

void SEMANTIC(VADDUWS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_addus_u32(a, b);
  sat = gv_or32(gv_xor32(gv_add32(a, b), r), sat);
  d = r;
}
void DECODER(VADDUWS) {
  VADDUWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VADDUWS);

void SEMANTIC(VAND)(v128 &d, v128 a, v128 b) { d = gv_andfs(a, b); }
void DECODER(VAND) {
  VAND(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAND);

void SEMANTIC(VANDC)(v128 &d, v128 a, v128 b) { d = gv_andnfs(b, a); }
void DECODER(VANDC) {
  VANDC(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VANDC);

void SEMANTIC(VAVGSB)(v128 &d, v128 a, v128 b) { d = gv_avgs8(a, b); }
void DECODER(VAVGSB) {
  VAVGSB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGSB);

void SEMANTIC(VAVGSH)(v128 &d, v128 a, v128 b) { d = gv_avgs16(a, b); }
void DECODER(VAVGSH) {
  VAVGSH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGSH);

void SEMANTIC(VAVGSW)(v128 &d, v128 a, v128 b) { d = gv_avgs32(a, b); }
void DECODER(VAVGSW) {
  VAVGSW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGSW);

void SEMANTIC(VAVGUB)(v128 &d, v128 a, v128 b) { d = gv_avgu8(a, b); }
void DECODER(VAVGUB) {
  VAVGUB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGUB);

void SEMANTIC(VAVGUH)(v128 &d, v128 a, v128 b) { d = gv_avgu16(a, b); }
void DECODER(VAVGUH) {
  VAVGUH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGUH);

void SEMANTIC(VAVGUW)(v128 &d, v128 &a, v128 &b) { d = gv_avgu32(a, b); }
void DECODER(VAVGUW) {
  VAVGUW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VAVGUW);

void SEMANTIC(VCFSX)(v128 &d, v128 b, u32 i) {
  d = gv_subus_u16(gv_cvts32_tofs(b), gv_bcst32(i));
}
void DECODER(VCFSX) {
  VCFSX(context.vr[inst.vd], context.vr[inst.vb], inst.vuimm << 23);
}
EXPORT_SEMANTIC(VCFSX);

void SEMANTIC(VCFUX)(v128 &d, v128 b, u32 i) {
  d = gv_subus_u16(gv_cvtu32_tofs(b), gv_bcst32(i));
}
void DECODER(VCFUX) {
  VCFUX(context.vr[inst.vd], context.vr[inst.vb], inst.vuimm << 23);
}
EXPORT_SEMANTIC(VCFUX);

void SEMANTIC(VCMPBFP)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto sign = gv_bcstfs(-0.);
  auto cmp1 = gv_nlefs(a, b);
  auto cmp2 = gv_ngefs(a, b ^ sign);
  auto r = (cmp1 & sign) | gv_shr32(cmp2 & sign, 1);
  if (cr6 != nullptr) {
    cr6->set(false, false, gv_testz(r), false);
  }
  d = r;
}
void DECODER(VCMPBFP) {
  VCMPBFP(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
          context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPBFP);

void SEMANTIC(VCMPEQFP)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_eqfs(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPEQFP) {
  VCMPEQFP(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPEQFP);

void SEMANTIC(VCMPEQUB)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_eq8(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPEQUB) {
  VCMPEQUB(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPEQUB);

void SEMANTIC(VCMPEQUH)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_eq16(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPEQUH) {
  VCMPEQUH(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPEQUH);

void SEMANTIC(VCMPEQUW)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_eq32(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPEQUW) {
  VCMPEQUW(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPEQUW);

void SEMANTIC(VCMPGEFP)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gefs(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGEFP) {
  VCMPGEFP(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGEFP);

void SEMANTIC(VCMPGTFP)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gtfs(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTFP) {
  VCMPGTFP(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTFP);

void SEMANTIC(VCMPGTSB)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gts8(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTSB) {
  VCMPGTSB(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTSB);

void SEMANTIC(VCMPGTSH)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gts16(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTSH) {
  VCMPGTSH(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTSH);

void SEMANTIC(VCMPGTSW)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gts32(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTSW) {
  VCMPGTSW(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTSW);

void SEMANTIC(VCMPGTUB)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gtu8(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTUB) {
  VCMPGTUB(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTUB);

void SEMANTIC(VCMPGTUH)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gtu16(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTUH) {
  VCMPGTUH(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTUH);

void SEMANTIC(VCMPGTUW)(CrField *cr6, v128 &d, v128 a, v128 b) {
  auto r = gv_gtu32(a, b);
  if (cr6 != nullptr) {
    cr6->set(gv_testall1(r), false, gv_testall0(r), false);
  }
  d = r;
}
void DECODER(VCMPGTUW) {
  VCMPGTUW(inst.oe ? context.cr.fields + 6 : nullptr, context.vr[inst.vd],
           context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VCMPGTUW);

void SEMANTIC(VCTSXS)(v128 &d, v128 b, v128 &sat, u32 i) {
  auto r = gv_mulfs(b, gv_bcst32(i));
  auto l = gv_ltfs(r, gv_bcstfs(-2147483648.));
  auto h = gv_gefs(r, gv_bcstfs(2147483648.));
#if !defined(ARCH_X64) && !defined(ARCH_ARM64)
  r = gv_selectfs(l, gv_bcstfs(-2147483648.), r);
#endif
  r = gv_cvtfs_tos32(r);
#if !defined(ARCH_ARM64)
  r = gv_select32(h, gv_bcst32(0x7fffffff), r);
#endif
  r = gv_and32(r, gv_eqfs(b, b));
  sat = gv_or32(gv_or32(l, h), sat);
  d = r;
}
void DECODER(VCTSXS) {
  VCTSXS(context.vr[inst.vd], context.vr[inst.vb], context.sat,
         (inst.vuimm + 127) << 23);
}
EXPORT_SEMANTIC(VCTSXS);

void SEMANTIC(VCTUXS)(v128 &d, v128 b, v128 &sat, u32 i) {
  auto r = gv_mulfs(b, gv_bcst32(i));
  auto l = gv_ltfs(r, gv_bcstfs(0.));
  auto h = gv_gefs(r, gv_bcstfs(4294967296.));
  r = gv_cvtfs_tou32(r);
#if !defined(ARCH_ARM64)
  r = gv_andn32(l, r); // saturate to zero
#endif
#if !defined(__AVX512VL__) && !defined(ARCH_ARM64)
  r = gv_or32(r, h); // saturate to 0xffffffff
#endif
  r = gv_and32(r, gv_eqfs(b, b));

  sat = gv_or32(gv_or32(l, h), sat);
  d = r;
}
void DECODER(VCTUXS) {
  VCTUXS(context.vr[inst.vd], context.vr[inst.vb], context.sat,
         (inst.vuimm + 127) << 23);
}
EXPORT_SEMANTIC(VCTUXS);

void SEMANTIC(VEXPTEFP)(v128 &d, v128 b) {
  // for (u32 i = 0; i < 4; i++) d._f[i] = std::exp2f(b._f[i]);
  d = ppu_set_vnan(gv_exp2_approxfs(b));
}
void DECODER(VEXPTEFP) { VEXPTEFP(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VEXPTEFP);

void SEMANTIC(VLOGEFP)(v128 &d, v128 b) {
  // for (u32 i = 0; i < 4; i++) d._f[i] = std::log2f(b._f[i]);
  d = ppu_set_vnan(gv_log2_approxfs(b));
}
void DECODER(VLOGEFP) { VLOGEFP(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VLOGEFP);

void SEMANTIC(VMADDFP)(v128 &d, v128 a_, v128 b_, v128 c_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto a = ppu_flush_denormal(m, a_);
  auto b = ppu_flush_denormal(m, b_);
  auto c = ppu_flush_denormal(m, c_);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_fmafs(a, c, b)));
}
void DECODER(VMADDFP) {
  VMADDFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.vr[inst.vc], context.jm_mask);
}
EXPORT_SEMANTIC(VMADDFP);

void SEMANTIC(VMAXFP)(v128 &d, v128 a, v128 b, u32 jm_mask) {
  d = ppu_flush_denormal(gv_bcst32(jm_mask),
                         ppu_set_vnan(gv_maxfs(a, b), a, b));
}
void DECODER(VMAXFP) {
  VMAXFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
         context.jm_mask);
}
EXPORT_SEMANTIC(VMAXFP);

void SEMANTIC(VMAXSB)(v128 &d, v128 a, v128 b) { d = gv_maxs8(a, b); }
void DECODER(VMAXSB) {
  VMAXSB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXSB);

void SEMANTIC(VMAXSH)(v128 &d, v128 a, v128 b) { d = gv_maxs16(a, b); }
void DECODER(VMAXSH) {
  VMAXSH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXSH);

void SEMANTIC(VMAXSW)(v128 &d, v128 a, v128 b) { d = gv_maxs32(a, b); }
void DECODER(VMAXSW) {
  VMAXSW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXSW);

void SEMANTIC(VMAXUB)(v128 &d, v128 a, v128 b) { d = gv_maxu8(a, b); }
void DECODER(VMAXUB) {
  VMAXUB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXUB);

void SEMANTIC(VMAXUH)(v128 &d, v128 a, v128 b) { d = gv_maxu16(a, b); }
void DECODER(VMAXUH) {
  VMAXUH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXUH);

void SEMANTIC(VMAXUW)(v128 &d, v128 a, v128 b) { d = gv_maxu32(a, b); }
void DECODER(VMAXUW) {
  VMAXUW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMAXUW);

void SEMANTIC(VMHADDSHS)(v128 &d, v128 a, v128 b, v128 c, v128 &sat) {
  auto m = gv_muls_hds16(a, b);
  auto f = gv_gts16(gv_bcst16(0), c);
  auto x = gv_eq16(gv_maxs16(a, b), gv_bcst16(0x8000));
  auto r = gv_sub16(gv_adds_s16(m, c), gv_and32(x, f));
  auto s = gv_add16(m, c);

  sat = gv_or32(gv_or32(gv_andn32(f, x), gv_andn32(x, gv_xor32(s, r))), sat);
  d = r;
}
void DECODER(VMHADDSHS) {
  VMHADDSHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
            context.vr[inst.vc], context.sat);
}
EXPORT_SEMANTIC(VMHADDSHS);

void SEMANTIC(VMHRADDSHS)(v128 &d, v128 a, v128 b, v128 c, v128 &sat) {
  auto m = gv_rmuls_hds16(a, b);
  auto f = gv_gts16(gv_bcst16(0), c);
  auto x = gv_eq16(gv_maxs16(a, b), gv_bcst16(0x8000));
  auto r = gv_sub16(gv_adds_s16(m, c), gv_and32(x, f));
  auto s = gv_add16(m, c);
  sat = gv_or32(gv_or32(gv_andn32(f, x), gv_andn32(x, gv_xor32(s, r))), sat);
  d = r;
}
void DECODER(VMHRADDSHS) {
  VMHRADDSHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
             context.vr[inst.vc], context.sat);
}
EXPORT_SEMANTIC(VMHRADDSHS);

void SEMANTIC(VMINFP)(v128 &d, v128 a, v128 b, u32 jm_mask) {
  d = ppu_flush_denormal(gv_bcst32(jm_mask),
                         ppu_set_vnan(gv_minfs(a, b), a, b));
}
void DECODER(VMINFP) {
  VMINFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
         context.jm_mask);
}
EXPORT_SEMANTIC(VMINFP);

void SEMANTIC(VMINSB)(v128 &d, v128 a, v128 b) { d = gv_mins8(a, b); }
void DECODER(VMINSB) {
  VMINSB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINSB);

void SEMANTIC(VMINSH)(v128 &d, v128 a, v128 b) { d = gv_mins16(a, b); }
void DECODER(VMINSH) {
  VMINSH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINSH);

void SEMANTIC(VMINSW)(v128 &d, v128 a, v128 b) { d = gv_mins32(a, b); }
void DECODER(VMINSW) {
  VMINSW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINSW);

void SEMANTIC(VMINUB)(v128 &d, v128 a, v128 b) { d = gv_minu8(a, b); }
void DECODER(VMINUB) {
  VMINUB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINUB);

void SEMANTIC(VMINUH)(v128 &d, v128 a, v128 b) { d = gv_minu16(a, b); }
void DECODER(VMINUH) {
  VMINUH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINUH);

void SEMANTIC(VMINUW)(v128 &d, v128 a, v128 b) { d = gv_minu32(a, b); }
void DECODER(VMINUW) {
  VMINUW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMINUW);

void SEMANTIC(VMLADDUHM)(v128 &d, v128 a, v128 b, v128 c) {
  d = gv_muladd16(a, b, c);
}
void DECODER(VMLADDUHM) {
  VMLADDUHM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
            context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VMLADDUHM);

void SEMANTIC(VMRGHB)(v128 &d, v128 a, v128 b) { d = gv_unpackhi8(b, a); }
void DECODER(VMRGHB) {
  VMRGHB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGHB);

void SEMANTIC(VMRGHH)(v128 &d, v128 a, v128 &b) { d = gv_unpackhi16(b, a); }
void DECODER(VMRGHH) {
  VMRGHH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGHH);

void SEMANTIC(VMRGHW)(v128 &d, v128 a, v128 b) { d = gv_unpackhi32(b, a); }
void DECODER(VMRGHW) {
  VMRGHW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGHW);

void SEMANTIC(VMRGLB)(v128 &d, v128 a, v128 b) { d = gv_unpacklo8(b, a); }
void DECODER(VMRGLB) {
  VMRGLB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGLB);

void SEMANTIC(VMRGLH)(v128 &d, v128 a, v128 b) { d = gv_unpacklo16(b, a); }
void DECODER(VMRGLH) {
  VMRGLH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGLH);

void SEMANTIC(VMRGLW)(v128 &d, v128 a, v128 b) { d = gv_unpacklo32(b, a); }
void DECODER(VMRGLW) {
  VMRGLW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMRGLW);

void SEMANTIC(VMSUMMBM)(v128 &d, v128 a, v128 b, v128 c) {
  d = gv_dotu8s8x4(b, a, c);
}
void DECODER(VMSUMMBM) {
  VMSUMMBM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VMSUMMBM);

void SEMANTIC(VMSUMSHM)(v128 &d, v128 a, v128 b, v128 c) {
  d = gv_dots16x2(a, b, c);
}
void DECODER(VMSUMSHM) {
  VMSUMSHM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VMSUMSHM);

void SEMANTIC(VMSUMSHS)(v128 &d, v128 a, v128 b, v128 c, v128 &sat) {
  auto r = gv_dots_s16x2(a, b, c);
  auto s = gv_dots16x2(a, b, c);
  sat = gv_or32(gv_xor32(s, r), sat);
  d = r;
}
void DECODER(VMSUMSHS) {
  VMSUMSHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc], context.sat);
}
EXPORT_SEMANTIC(VMSUMSHS);

void SEMANTIC(VMSUMUBM)(v128 &d, v128 a, v128 b, v128 c) {
  d = gv_dotu8x4(a, b, c);
}
void DECODER(VMSUMUBM) {
  VMSUMUBM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VMSUMUBM);

void SEMANTIC(VMSUMUHM)(v128 &d, v128 a, v128 b, v128 c) {
  d = gv_add32(c, gv_dotu16x2(a, b));
}
void DECODER(VMSUMUHM) {
  VMSUMUHM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VMSUMUHM);

void SEMANTIC(VMSUMUHS)(v128 d, v128 a, v128 b, v128 c, v128 &sat) {
  auto m1 = gv_mul_even_u16(a, b);
  auto m2 = gv_mul_odds_u16(a, b);
  auto s1 = gv_add32(m1, m2);
  auto x1 = gv_gtu32(m1, s1);
  auto s2 = gv_or32(gv_add32(s1, c), x1);
  auto x2 = gv_gtu32(s1, s2);
  sat = gv_or32(gv_or32(x1, x2), sat);
  d = gv_or32(s2, x2);
}
void DECODER(VMSUMUHS) {
  VMSUMUHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc], context.sat);
}
EXPORT_SEMANTIC(VMSUMUHS);

void SEMANTIC(VMULESB)(v128 &d, v128 a, v128 b) {
  d = gv_mul16(gv_sar16(a, 8), gv_sar16(b, 8));
}
void DECODER(VMULESB) {
  VMULESB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULESB);

void SEMANTIC(VMULESH)(v128 &d, v128 a, v128 b) { d = gv_mul_odds_s16(a, b); }
void DECODER(VMULESH) {
  VMULESH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULESH);

void SEMANTIC(VMULEUB)(v128 &d, v128 a, v128 b) {
  d = gv_mul16(gv_shr16(a, 8), gv_shr16(b, 8));
}
void DECODER(VMULEUB) {
  VMULEUB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULEUB);

void SEMANTIC(VMULEUH)(v128 &d, v128 a, v128 b) { d = gv_mul_odds_u16(a, b); }
void DECODER(VMULEUH) {
  VMULEUH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULEUH);

void SEMANTIC(VMULOSB)(v128 &d, v128 a, v128 b) {
  d = gv_mul16(gv_sar16(gv_shl16(a, 8), 8), gv_sar16(gv_shl16(b, 8), 8));
}
void DECODER(VMULOSB) {
  VMULOSB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULOSB);

void SEMANTIC(VMULOSH)(v128 &d, v128 a, v128 b) { d = gv_mul_even_s16(a, b); }
void DECODER(VMULOSH) {
  VMULOSH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULOSH);

void SEMANTIC(VMULOUB)(v128 &d, v128 a, v128 b) {
  auto mask = gv_bcst16(0x00ff);
  d = gv_mul16(gv_and32(a, mask), gv_and32(b, mask));
}
void DECODER(VMULOUB) {
  VMULOUB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULOUB);

void SEMANTIC(VMULOUH)(v128 &d, v128 a, v128 b) { d = gv_mul_even_u16(a, b); }
void DECODER(VMULOUH) {
  VMULOUH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VMULOUH);

void SEMANTIC(VNMSUBFP)(v128 &d, v128 a_, v128 b_, v128 c_, u32 jm_mask) {
  // An odd case with (FLT_MIN, FLT_MIN, FLT_MIN) produces FLT_MIN instead of
  // 0
  auto s = gv_bcstfs(-0.0f);
  auto m = gv_bcst32(jm_mask);
  auto a = ppu_flush_denormal(m, a_);
  auto b = ppu_flush_denormal(m, b_);
  auto c = ppu_flush_denormal(m, c_);
  auto r = gv_xorfs(s, gv_fmafs(a, c, gv_xorfs(b, s)));
  d = ppu_flush_denormal(m, ppu_set_vnan(r));
}
void DECODER(VNMSUBFP) {
  VNMSUBFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.vr[inst.vc], context.jm_mask);
}
EXPORT_SEMANTIC(VNMSUBFP);

void SEMANTIC(VNOR)(v128 &d, v128 a, v128 b) { d = gv_notfs(gv_orfs(a, b)); }
void DECODER(VNOR) {
  VNOR(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VNOR);

void SEMANTIC(VOR)(v128 &d, v128 a, v128 b) { d = gv_orfs(a, b); }
void DECODER(VOR) {
  VOR(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VOR);

void SEMANTIC(VPERM)(v128 &d, v128 a, v128 b, v128 c) {
#if defined(ARCH_ARM64)
  uint8x16x2_t ab;
  ab.val[0] = b;
  ab.val[1] = a;
  d = vqtbl2q_u8(ab, vbicq_u8(vdupq_n_u8(0x1f), c));
#else
  u8 ab[32];
  std::memcpy(ab + 0, &b, 16);
  std::memcpy(ab + 16, &a, 16);

  for (u32 i = 0; i < 16; i++) {
    d._u8[i] = ab[~c._u8[i] & 0x1f];
  }
#endif
}
void DECODER(VPERM) {
  VPERM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
        context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VPERM);

void SEMANTIC(VPKPX)(v128 &d, v128 a, v128 b) {
  auto a1 = gv_sar32(gv_shl32(a, 7), 7 + 9);
  auto b1 = gv_sar32(gv_shl32(b, 7), 7 + 9);
  auto a2 = gv_sar32(gv_shl32(a, 16), 16 + 3);
  auto b2 = gv_sar32(gv_shl32(b, 16), 16 + 3);
  auto p1 = gv_packss_s32(b1, a1);
  auto p2 = gv_packss_s32(b2, a2);
  d = gv_or32(gv_or32(gv_and32(p1, gv_bcst16(0xfc00)),
                      gv_shl16(gv_and32(p1, gv_bcst16(0x7c)), 3)),
              gv_and32(p2, gv_bcst16(0x1f)));
}
void DECODER(VPKPX) {
  VPKPX(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VPKPX);

void SEMANTIC(VPKSHSS)(v128 &d, v128 a, v128 b, v128 &sat) {
  sat = gv_or32(
      gv_shr16(gv_add16(a, gv_bcst16(0x80)) | gv_add16(b, gv_bcst16(0x80)), 8),
      sat);
  d = gv_packss_s16(b, a);
}
void DECODER(VPKSHSS) {
  VPKSHSS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKSHSS);

void SEMANTIC(VPKSHUS)(v128 &d, v128 a, v128 b, v128 &sat) {
  sat = gv_or32(gv_shr16(a | b, 8), sat);
  d = gv_packus_s16(b, a);
}
void DECODER(VPKSHUS) {
  VPKSHUS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKSHUS);

void SEMANTIC(VPKSWSS)(v128 &d, v128 a, v128 b, v128 &sat) {
  sat = gv_or32(
      gv_shr32(gv_add32(a, gv_bcst32(0x8000)) | gv_add32(b, gv_bcst32(0x8000)),
               16),
      sat);
  d = gv_packss_s32(b, a);
}
void DECODER(VPKSWSS) {
  VPKSWSS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKSWSS);

void SEMANTIC(VPKSWUS)(v128 &d, v128 a, v128 b, v128 sat) {
  sat = gv_or32(gv_shr32(a | b, 16), sat);
  d = gv_packus_s32(b, a);
}
void DECODER(VPKSWUS) {
  VPKSWUS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKSWUS);

void SEMANTIC(VPKUHUM)(v128 &d, v128 a, v128 b) { d = gv_packtu16(b, a); }
void DECODER(VPKUHUM) {
  VPKUHUM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VPKUHUM);

void SEMANTIC(VPKUHUS)(v128 &d, v128 a, v128 b, v128 &sat) {
  sat = gv_or32(gv_shr16(a | b, 8), sat);
  d = gv_packus_u16(b, a);
}
void DECODER(VPKUHUS) {
  VPKUHUS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKUHUS);

void SEMANTIC(VPKUWUM)(v128 &d, v128 a, v128 b) { d = gv_packtu32(b, a); }
void DECODER(VPKUWUM) {
  VPKUWUM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VPKUWUM);

void SEMANTIC(VPKUWUS)(v128 &d, v128 a, v128 b, v128 &sat) {
  sat = gv_or32(gv_shr32(a | b, 16), sat);
  d = gv_packus_u32(b, a);
}
void DECODER(VPKUWUS) {
  VPKUWUS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VPKUWUS);

void SEMANTIC(VREFP)(v128 &d, v128 b_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto b = ppu_flush_denormal(m, b_);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_divfs(gv_bcstfs(1.0f), b), b));
}
void DECODER(VREFP) {
  VREFP(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VREFP);

void SEMANTIC(VRFIM)(v128 &d, v128 b_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto b = ppu_flush_denormal(m, b_);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_roundfs_floor(b), b));
}
void DECODER(VRFIM) {
  VRFIM(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VRFIM);

void SEMANTIC(VRFIN)(v128 &d, v128 b, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_roundfs_even(b), b));
}
void DECODER(VRFIN) {
  VRFIN(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VRFIN);

void SEMANTIC(VRFIP)(v128 &d, v128 b_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto b = ppu_flush_denormal(m, b_);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_roundfs_ceil(b), b));
}
void DECODER(VRFIP) {
  VRFIP(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VRFIP);

void SEMANTIC(VRFIZ)(v128 &d, v128 b, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_roundfs_trunc(b), b));
}
void DECODER(VRFIZ) {
  VRFIZ(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VRFIZ);

void SEMANTIC(VRLB)(v128 &d, v128 a, v128 b) { d = gv_rol8(a, b); }
void DECODER(VRLB) {
  VRLB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VRLB);

void SEMANTIC(VRLH)(v128 &d, v128 a, v128 b) { d = gv_rol16(a, b); }
void DECODER(VRLH) {
  VRLH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VRLH);

void SEMANTIC(VRLW)(v128 &d, v128 a, v128 b) { d = gv_rol32(a, b); }
void DECODER(VRLW) {
  VRLW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VRLW);

void SEMANTIC(VRSQRTEFP)(v128 &d, v128 b_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto b = ppu_flush_denormal(m, b_);
  d = ppu_flush_denormal(
      m, ppu_set_vnan(gv_divfs(gv_bcstfs(1.0f), gv_sqrtfs(b)), b));
}
void DECODER(VRSQRTEFP) {
  VRSQRTEFP(context.vr[inst.vd], context.vr[inst.vb], context.jm_mask);
}
EXPORT_SEMANTIC(VRSQRTEFP);

void SEMANTIC(VSEL)(v128 &d, v128 a, v128 b, v128 c) {
  auto x = gv_andfs(b, c);
  d = gv_orfs(x, gv_andnfs(c, a));
}
void DECODER(VSEL) {
  VSEL(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
       context.vr[inst.vc]);
}
EXPORT_SEMANTIC(VSEL);

void SEMANTIC(VSL)(v128 &d, v128 a, v128 b) {
  d = gv_fshl8(a, gv_shuffle_left<1>(a), b);
}
void DECODER(VSL) {
  VSL(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSL);

void SEMANTIC(VSLB)(v128 &d, v128 a, v128 b) { d = gv_shl8(a, b); }
void DECODER(VSLB) {
  VSLB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSLB);

template <u32 Count> static void VSLDOI_IMPL(v128 &d, v128 a, v128 b) {
  d = gv_or32(gv_shuffle_left<Count>(a), gv_shuffle_right<16 - Count>(b));
}
void SEMANTIC(VSLDOI)(v128 &d, v128 a, v128 b, u32 vsh) {
  switch (vsh) {
  case 0:
    VSLDOI_IMPL<0>(d, a, b);
    break;
  case 1:
    VSLDOI_IMPL<1>(d, a, b);
    break;
  case 2:
    VSLDOI_IMPL<2>(d, a, b);
    break;
  case 3:
    VSLDOI_IMPL<3>(d, a, b);
    break;
  case 4:
    VSLDOI_IMPL<4>(d, a, b);
    break;
  case 5:
    VSLDOI_IMPL<5>(d, a, b);
    break;
  case 6:
    VSLDOI_IMPL<6>(d, a, b);
    break;
  case 7:
    VSLDOI_IMPL<7>(d, a, b);
    break;
  case 8:
    VSLDOI_IMPL<8>(d, a, b);
    break;
  case 9:
    VSLDOI_IMPL<9>(d, a, b);
    break;
  case 10:
    VSLDOI_IMPL<10>(d, a, b);
    break;
  case 11:
    VSLDOI_IMPL<11>(d, a, b);
    break;
  case 12:
    VSLDOI_IMPL<12>(d, a, b);
    break;
  case 13:
    VSLDOI_IMPL<13>(d, a, b);
    break;
  case 14:
    VSLDOI_IMPL<14>(d, a, b);
    break;
  case 15:
    VSLDOI_IMPL<15>(d, a, b);
    break;
  }
}
void DECODER(VSLDOI) {
  VSLDOI(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
         inst.vsh);
}
EXPORT_SEMANTIC(VSLDOI);

void SEMANTIC(VSLH)(v128 &d, v128 a, v128 b) { d = gv_shl16(a, b); }
void DECODER(VSLH) {
  VSLH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSLH);

void SEMANTIC(VSLO)(v128 &d, v128 a, v128 b) {
  d._u = a._u << (b._u8[0] & 0x78);
}
void DECODER(VSLO) {
  VSLO(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSLO);

void SEMANTIC(VSLW)(v128 &d, v128 a, v128 b) { d = gv_shl32(a, b); }
void DECODER(VSLW) {
  VSLW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSLW);

void SEMANTIC(VSPLTB)(v128 &d, v128 b, std::uint32_t imm) {
  d = gv_bcst8(b.u8r[imm & 15]);
}
void DECODER(VSPLTB) {
  VSPLTB(context.vr[inst.vd], context.vr[inst.vb], inst.vuimm);
}
EXPORT_SEMANTIC(VSPLTB);

void SEMANTIC(VSPLTH)(v128 &d, v128 b, std::uint32_t imm) {
  d = gv_bcst16(b.u16r[imm & 7]);
}
void DECODER(VSPLTH) {
  VSPLTH(context.vr[inst.vd], context.vr[inst.vb], inst.vuimm);
}
EXPORT_SEMANTIC(VSPLTH);

void SEMANTIC(VSPLTISB)(v128 &d, std::int32_t imm) { d = gv_bcst8(imm); }
void DECODER(VSPLTISB) { VSPLTISB(context.vr[inst.vd], inst.vsimm); }
EXPORT_SEMANTIC(VSPLTISB);

void SEMANTIC(VSPLTISH)(v128 &d, std::int32_t imm) { d = gv_bcst16(imm); }
void DECODER(VSPLTISH) { VSPLTISH(context.vr[inst.vd], inst.vsimm); }
EXPORT_SEMANTIC(VSPLTISH);

void SEMANTIC(VSPLTISW)(v128 &d, std::int32_t imm) { d = gv_bcst32(imm); }
void DECODER(VSPLTISW) { VSPLTISW(context.vr[inst.vd], inst.vsimm); }
EXPORT_SEMANTIC(VSPLTISW);

void SEMANTIC(VSPLTW)(v128 &d, v128 b, u32 imm) {
  d = gv_bcst32(b.u32r[imm & 3]);
}
void DECODER(VSPLTW) {
  VSPLTW(context.vr[inst.vd], context.vr[inst.vb], inst.vuimm);
}
EXPORT_SEMANTIC(VSPLTW);

void SEMANTIC(VSR)(v128 &d, v128 a, v128 b) {
  d = gv_fshr8(gv_shuffle_right<1>(a), a, b);
}
void DECODER(VSR) {
  VSR(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSR);

void SEMANTIC(VSRAB)(v128 &d, v128 a, v128 b) { d = gv_sar8(a, b); }
void DECODER(VSRAB) {
  VSRAB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRAB);

void SEMANTIC(VSRAH)(v128 &d, v128 a, v128 b) { d = gv_sar16(a, b); }
void DECODER(VSRAH) {
  VSRAH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRAH);

void SEMANTIC(VSRAW)(v128 &d, v128 a, v128 b) { d = gv_sar32(a, b); }
void DECODER(VSRAW) {
  VSRAW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRAW);

void SEMANTIC(VSRB)(v128 &d, v128 a, v128 b) { d = gv_shr8(a, b); }
void DECODER(VSRB) {
  VSRB(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRB);

void SEMANTIC(VSRH)(v128 &d, v128 a, v128 b) { d = gv_shr16(a, b); }
void DECODER(VSRH) {
  VSRH(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRH);

void SEMANTIC(VSRO)(v128 &d, v128 a, v128 b) {
  d._u = a._u >> (b._u8[0] & 0x78);
}
void DECODER(VSRO) {
  VSRO(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRO);

void SEMANTIC(VSRW)(v128 &d, v128 a, v128 b) { d = gv_shr32(a, b); }
void DECODER(VSRW) {
  VSRW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSRW);

void SEMANTIC(VSUBCUW)(v128 &d, v128 a, v128 b) {
  d = gv_shr32(gv_geu32(a, b), 31);
}
void DECODER(VSUBCUW) {
  VSUBCUW(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSUBCUW);

void SEMANTIC(VSUBFP)(v128 &d, v128 a_, v128 b_, u32 jm_mask) {
  auto m = gv_bcst32(jm_mask);
  auto a = ppu_flush_denormal(m, a_);
  auto b = ppu_flush_denormal(m, b_);
  d = ppu_flush_denormal(m, ppu_set_vnan(gv_subfs(a, b), a, b));
}
void DECODER(VSUBFP) {
  VSUBFP(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
         context.jm_mask);
}
EXPORT_SEMANTIC(VSUBFP);

void SEMANTIC(VSUBSBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subs_s8(a, b);
  sat = gv_or32(gv_xor32(gv_sub8(a, b), r), sat);
  d = r;
}
void DECODER(VSUBSBS) {
  VSUBSBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBSBS);

void SEMANTIC(VSUBSHS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subs_s16(a, b);
  sat = gv_or32(gv_xor32(gv_sub16(a, b), r), sat);
  d = r;
}
void DECODER(VSUBSHS) {
  VSUBSHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBSHS);

void SEMANTIC(VSUBSWS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subs_s32(a, b);
  sat = gv_or32(gv_xor32(gv_sub32(a, b), r), sat);
  d = r;
}
void DECODER(VSUBSWS) {
  VSUBSWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBSWS);

void SEMANTIC(VSUBUBM)(v128 &d, v128 a, v128 b) { d = gv_sub8(a, b); }
void DECODER(VSUBUBM) {
  VSUBUBM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSUBUBM);

void SEMANTIC(VSUBUBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subus_u8(a, b);
  sat = gv_or32(gv_xor32(gv_sub8(a, b), r), sat);
  d = r;
}
void DECODER(VSUBUBS) {
  VSUBUBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBUBS);

void SEMANTIC(VSUBUHM)(v128 &d, v128 a, v128 b) { d = gv_sub16(a, b); }
void DECODER(VSUBUHM) {
  VSUBUHM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSUBUHM);

void SEMANTIC(VSUBUHS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subus_u16(a, b);
  sat = gv_or32(gv_xor32(gv_sub16(a, b), r), sat);
  d = r;
}
void DECODER(VSUBUHS) {
  VSUBUHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBUHS);

void SEMANTIC(VSUBUWM)(v128 &d, v128 a, v128 b) { d = gv_sub32(a, b); }
void DECODER(VSUBUWM) {
  VSUBUWM(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VSUBUWM);

void SEMANTIC(VSUBUWS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_subus_u32(a, b);
  sat = gv_or32(gv_xor32(gv_sub32(a, b), r), sat);
  d = r;
}
void DECODER(VSUBUWS) {
  VSUBUWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUBUWS);

void SEMANTIC(VSUMSWS)(v128 &d, v128 a, v128 b, v128 &sat) {
  s64 sum = s64{b._s32[0]} + a._s32[0] + a._s32[1] + a._s32[2] + a._s32[3];
  if (sum > INT32_MAX) {
    sum = u32(INT32_MAX);
    sat._bytes[0] = 1;
  } else if (sum < INT32_MIN) {
    sum = u32(INT32_MIN);
    sat._bytes[0] = 1;
  } else {
    sum = static_cast<u32>(sum);
  }

  d._u = sum;
}
void DECODER(VSUMSWS) {
  VSUMSWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
          context.sat);
}
EXPORT_SEMANTIC(VSUMSWS);

void SEMANTIC(VSUM2SWS)(v128 &d, v128 a, v128 b, v128 &sat) {
#if defined(__AVX512VL__)
  const auto x = gv_add64(gv_sar64(gv_shl64(a, 32), 32), gv_sar64(a, 32));
  const auto y = gv_add64(x, gv_sar64(gv_shl64(b, 32), 32));
  const auto r =
      _mm_unpacklo_epi32(_mm_cvtsepi64_epi32(y), _mm_setzero_si128());
#elif defined(ARCH_ARM64)
  const auto x =
      vaddl_s32(vget_low_s32(vuzp1q_s32(a, a)), vget_low_s32(vuzp2q_s32(a, a)));
  const auto y = vaddw_s32(x, vget_low_s32(vuzp1q_s32(b, b)));
  const auto r = vmovl_u32(uint32x2_t(vqmovn_s64(y)));
#else
  v128 y{};
  y._s64[0] = s64{a._s32[0]} + a._s32[1] + b._s32[0];
  y._s64[1] = s64{a._s32[2]} + a._s32[3] + b._s32[2];
  v128 r{};
  r._u64[0] = y._s64[0] > INT32_MAX   ? INT32_MAX
              : y._s64[0] < INT32_MIN ? u32(INT32_MIN)
                                      : static_cast<u32>(y._s64[0]);
  r._u64[1] = y._s64[1] > INT32_MAX   ? INT32_MAX
              : y._s64[1] < INT32_MIN ? u32(INT32_MIN)
                                      : static_cast<u32>(y._s64[1]);
#endif
  sat = gv_or32(gv_shr64(gv_add64(y, gv_bcst64(0x80000000u)), 32), sat);
  d = r;
}
void DECODER(VSUM2SWS) {
  VSUM2SWS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.sat);
}
EXPORT_SEMANTIC(VSUM2SWS);

void SEMANTIC(VSUM4SBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_dots_u8s8x4(gv_bcst8(1), a, b);
  sat = gv_or32(gv_xor32(gv_hadds8x4(a, b), r), sat);
  d = r;
}
void DECODER(VSUM4SBS) {
  VSUM4SBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.sat);
}
EXPORT_SEMANTIC(VSUM4SBS);

void SEMANTIC(VSUM4SHS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto r = gv_dots_s16x2(a, gv_bcst16(1), b);
  sat = gv_or32(gv_xor32(gv_hadds16x2(a, b), r), sat);
  d = r;
}
void DECODER(VSUM4SHS) {
  VSUM4SHS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.sat);
}
EXPORT_SEMANTIC(VSUM4SHS);

void SEMANTIC(VSUM4UBS)(v128 &d, v128 a, v128 b, v128 &sat) {
  auto x = gv_haddu8x4(a);
  auto r = gv_addus_u32(x, b);
  sat = gv_or32(gv_xor32(gv_add32(x, b), r), sat);
  d = r;
}
void DECODER(VSUM4UBS) {
  VSUM4UBS(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb],
           context.sat);
}
EXPORT_SEMANTIC(VSUM4UBS);

void SEMANTIC(VUPKHPX)(v128 &d, v128 b) {
  auto x = gv_extend_hi_s16(b);
  auto y = gv_or32(gv_and32(gv_shl32(x, 6), gv_bcst32(0x1f0000)),
                   gv_and32(gv_shl32(x, 3), gv_bcst32(0x1f00)));
  d = gv_or32(y, gv_and32(x, gv_bcst32(0xff00001f)));
}
void DECODER(VUPKHPX) { VUPKHPX(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKHPX);

void SEMANTIC(VUPKHSB)(v128 &d, v128 b) { d = gv_extend_hi_s8(b); }
void DECODER(VUPKHSB) { VUPKHSB(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKHSB);

void SEMANTIC(VUPKHSH)(v128 &d, v128 b) { d = gv_extend_hi_s16(b); }
void DECODER(VUPKHSH) { VUPKHSH(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKHSH);

void SEMANTIC(VUPKLPX)(v128 &d, v128 b) {
  auto x = gv_extend_lo_s16(b);
  auto y = gv_or32(gv_and32(gv_shl32(x, 6), gv_bcst32(0x1f0000)),
                   gv_and32(gv_shl32(x, 3), gv_bcst32(0x1f00)));
  d = gv_or32(y, gv_and32(x, gv_bcst32(0xff00001f)));
}
void DECODER(VUPKLPX) { VUPKLPX(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKLPX);

void SEMANTIC(VUPKLSB)(v128 &d, v128 b) { d = gv_extend_lo_s8(b); }
void DECODER(VUPKLSB) { VUPKLSB(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKLSB);

void SEMANTIC(VUPKLSH)(v128 &d, v128 b) { d = gv_extend_lo_s16(b); }
void DECODER(VUPKLSH) { VUPKLSH(context.vr[inst.vd], context.vr[inst.vb]); }
EXPORT_SEMANTIC(VUPKLSH);

void SEMANTIC(VXOR)(v128 &d, v128 a, v128 b) { d = gv_xorfs(a, b); }
void DECODER(VXOR) {
  VXOR(context.vr[inst.vd], context.vr[inst.va], context.vr[inst.vb]);
}
EXPORT_SEMANTIC(VXOR);

void SEMANTIC(TDI)(s64 ra, u8 bo, s16 simm16) {
  if ((bo & 0x10) && ra < (s64)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x8) && ra > (s64)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x4) && ra == (s64)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x2) && (u64)ra < (u64)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x1) && (u64)ra > (u64)simm16) {
    rpcsx_trap();
  }
}
void DECODER(TDI) { TDI(context.gpr[inst.ra], inst.bo, inst.simm16); }
EXPORT_SEMANTIC(TDI);

void SEMANTIC(TWI)(s32 ra, u8 bo, s16 simm16) {
  if ((bo & 0x10) && ra < (s32)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x8) && ra > (s32)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x4) && ra == (s32)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x2) && (u32)ra < (u32)simm16) {
    rpcsx_trap();
  }

  if ((bo & 0x1) && (u32)ra > (u32)simm16) {
    rpcsx_trap();
  }
}

void DECODER(TWI) { TWI(context.gpr[inst.ra], inst.bo, inst.simm16); }

EXPORT_SEMANTIC(TWI);

void SEMANTIC(MULLI)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] = static_cast<s64>(context.gpr[inst.ra]) * inst.simm16;
}
void DECODER(MULLI) { MULLI(context, inst); }
EXPORT_SEMANTIC(MULLI);

void SEMANTIC(SUBFIC)(PPUContext &context, Instruction inst) {
  const u64 a = context.gpr[inst.ra];
  const s64 i = inst.simm16;
  const auto r = add64_flags(~a, i, 1);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;
}
void DECODER(SUBFIC) { SUBFIC(context, inst); }
EXPORT_SEMANTIC(SUBFIC);

void SEMANTIC(CMPLI)(PPUContext &context, Instruction inst) {
  if (inst.l10) {
    context.cr.fields[inst.crfd].update<u64>(context.gpr[inst.ra], inst.uimm16,
                                             context.xer_so);
  } else {
    context.cr.fields[inst.crfd].update<u32>(
        static_cast<u32>(context.gpr[inst.ra]), inst.uimm16, context.xer_so);
  }
}
void DECODER(CMPLI) { CMPLI(context, inst); }
EXPORT_SEMANTIC(CMPLI);

void SEMANTIC(CMPI)(PPUContext &context, Instruction inst) {
  if (inst.l10) {
    context.cr.fields[inst.crfd].update<s64>(context.gpr[inst.ra], inst.simm16,
                                             context.xer_so);
  } else {
    context.cr.fields[inst.crfd].update<s32>(
        static_cast<s32>(context.gpr[inst.ra]), inst.simm16, context.xer_so);
  }
}
void DECODER(CMPI) { CMPI(context, inst); }
EXPORT_SEMANTIC(CMPI);

void SEMANTIC(ADDIC)(PPUContext &context, Instruction inst) {
  const s64 a = context.gpr[inst.ra];
  const s64 i = inst.simm16;
  const auto r = add64_flags(a, i);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;
  if (inst.main & 1) [[unlikely]]
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
}
void DECODER(ADDIC) { ADDIC(context, inst); }
EXPORT_SEMANTIC(ADDIC);

void SEMANTIC(ADDI)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] =
      inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
}
void DECODER(ADDI) { ADDI(context, inst); }
EXPORT_SEMANTIC(ADDI);

void SEMANTIC(ADDIS)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] = inst.ra ? context.gpr[inst.ra] + (inst.simm16 * 65536)
                                 : (inst.simm16 * 65536);
}
void DECODER(ADDIS) { ADDIS(context, inst); }
EXPORT_SEMANTIC(ADDIS);

void SEMANTIC(BC)(std::uint32_t &cia, std::uint64_t &lr, std::uint64_t &ctr,
                  u8 bo, u8 crBit, bool lk, std::uint32_t target) {
  bool bo0 = (bo & 0x10) != 0;
  bool bo1 = (bo & 0x08) != 0;
  bool bo2 = (bo & 0x04) != 0;
  bool bo3 = (bo & 0x02) != 0;

  ctr -= (bo2 ^ true);

  bool ctr_ok = bo2 | ((ctr != 0) ^ bo3);
  bool cond_ok = bo0 | (!!crBit ^ (bo1 ^ true));

  u32 nextInst = cia + 4;
  if (lk) {
    lr = nextInst;
  }

  if (ctr_ok && cond_ok) {
    cia = target;
  } else {
    cia = nextInst;
  }
}
void DECODER(BC) {
  BC(context.cia, context.lr, context.ctr, inst.bo, context.cr[inst.bi],
     inst.lk, (inst.aa ? 0 : context.cia) + inst.bt14);
}
EXPORT_SEMANTIC(BC);

void SEMANTIC(SC)(PPUContext &context, std::uint64_t sysId) {
  ppu_execute_syscall(context, sysId);
}
void DECODER(SC) { SC(context, context.gpr[11]); }
EXPORT_SEMANTIC(SC);

void SEMANTIC(B)(std::uint32_t &cia, std::uint64_t &lr, bool lk,
                 std::uint32_t target) {
  u32 nextInst = cia + 4;
  if (lk) {
    lr = nextInst;
  }

  cia = target;
}
void DECODER(B) {
  B(context.cia, context.lr, inst.lk, (inst.aa ? 0 : context.cia) + inst.bt24);
}
EXPORT_SEMANTIC(B);

void SEMANTIC(MCRF)(PPUContext &context, Instruction inst) {
  context.cr.fields[inst.crfd] = context.cr.fields[inst.crfs];
}
void DECODER(MCRF) { MCRF(context, inst); }
EXPORT_SEMANTIC(MCRF);

void SEMANTIC(BCLR)(std::uint32_t &cia, std::uint64_t &lr, u64 &ctr, u8 bo,
                    u8 crBit, bool lk) {
  bool bo0 = (bo & 0x10) != 0;
  bool bo1 = (bo & 0x08) != 0;
  bool bo2 = (bo & 0x04) != 0;
  bool bo3 = (bo & 0x02) != 0;

  ctr -= (bo2 ^ true);

  bool ctr_ok = bo2 | ((ctr != 0) ^ bo3);
  bool cond_ok = bo0 | (!!crBit ^ (bo1 ^ true));

  u32 target = static_cast<u32>(lr) & ~3;
  u32 nextInst = cia + 4;
  if (lk) {
    lr = nextInst;
  }

  if (ctr_ok && cond_ok) {
    cia = target;
  } else {
    cia = nextInst;
  }
}
void DECODER(BCLR) {
  BCLR(context.cia, context.lr, context.ctr, inst.bo, context.cr[inst.bi],
       inst.lk);
}
EXPORT_SEMANTIC(BCLR);

void SEMANTIC(CRNOR)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] =
      (context.cr[inst.crba] | context.cr[inst.crbb]) ^ true;
}
void DECODER(CRNOR) { CRNOR(context, inst); }
EXPORT_SEMANTIC(CRNOR);

void SEMANTIC(CRANDC)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] =
      context.cr[inst.crba] & (context.cr[inst.crbb] ^ true);
}
void DECODER(CRANDC) { CRANDC(context, inst); }
EXPORT_SEMANTIC(CRANDC);

void SEMANTIC(ISYNC)() { std::atomic_thread_fence(std::memory_order::acquire); }
void DECODER(ISYNC) { ISYNC(); }
EXPORT_SEMANTIC(ISYNC);

void SEMANTIC(CRXOR)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] = context.cr[inst.crba] ^ context.cr[inst.crbb];
}
void DECODER(CRXOR) { CRXOR(context, inst); }
EXPORT_SEMANTIC(CRXOR);

void SEMANTIC(CRNAND)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] =
      (context.cr[inst.crba] & context.cr[inst.crbb]) ^ true;
}
void DECODER(CRNAND) { CRNAND(context, inst); }
EXPORT_SEMANTIC(CRNAND);

void SEMANTIC(CRAND)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] = context.cr[inst.crba] & context.cr[inst.crbb];
}
void DECODER(CRAND) { CRAND(context, inst); }
EXPORT_SEMANTIC(CRAND);

void SEMANTIC(CREQV)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] =
      (context.cr[inst.crba] ^ context.cr[inst.crbb]) ^ true;
}
void DECODER(CREQV) { CREQV(context, inst); }
EXPORT_SEMANTIC(CREQV);

void SEMANTIC(CRORC)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] =
      context.cr[inst.crba] | (context.cr[inst.crbb] ^ true);
}
void DECODER(CRORC) { CRORC(context, inst); }
EXPORT_SEMANTIC(CRORC);

void SEMANTIC(CROR)(PPUContext &context, Instruction inst) {
  context.cr[inst.crbd] = context.cr[inst.crba] | context.cr[inst.crbb];
}
void DECODER(CROR) { CROR(context, inst); }
EXPORT_SEMANTIC(CROR);

void SEMANTIC(BCCTR)(std::uint32_t &cia, std::uint64_t &lr, std::uint64_t ctr,
                     u8 bo, u8 crBit, bool lk) {
  u32 target = static_cast<u32>(ctr) & ~3;
  u32 nextInst = cia + 4;

  if (lk) {
    lr = nextInst;
  }

  if (bo & 0x10 || crBit == ((bo & 0x8) != 0)) {
    cia = target;
  } else {
    cia = nextInst;
  }
}
void DECODER(BCCTR) {
  BCCTR(context.cia, context.lr, context.ctr, inst.bo, context.cr[inst.bi],
        inst.lk);
}
EXPORT_SEMANTIC(BCCTR);

void SEMANTIC(RLWIMI)(PPUContext &context, Instruction inst) {
  const u64 mask = ppu_rotate_mask(32 + inst.mb32, 32 + inst.me32);
  context.gpr[inst.ra] =
      (context.gpr[inst.ra] & ~mask) |
      (dup32(rol32(static_cast<u32>(context.gpr[inst.rs]), inst.sh32)) & mask);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLWIMI) { RLWIMI(context, inst); }
EXPORT_SEMANTIC(RLWIMI);

void SEMANTIC(RLWINM)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      dup32(rol32(static_cast<u32>(context.gpr[inst.rs]), inst.sh32)) &
      ppu_rotate_mask(32 + inst.mb32, 32 + inst.me32);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLWINM) { RLWINM(context, inst); }
EXPORT_SEMANTIC(RLWINM);

void SEMANTIC(RLWNM)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = dup32(rol32(static_cast<u32>(context.gpr[inst.rs]),
                                     context.gpr[inst.rb] & 0x1f)) &
                         ppu_rotate_mask(32 + inst.mb32, 32 + inst.me32);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLWNM) { RLWNM(context, inst); }
EXPORT_SEMANTIC(RLWNM);

void SEMANTIC(ORI)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] | inst.uimm16;
}
void DECODER(ORI) { ORI(context, inst); }
EXPORT_SEMANTIC(ORI);

void SEMANTIC(ORIS)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] | (u64{inst.uimm16} << 16);
}
void DECODER(ORIS) { ORIS(context, inst); }
EXPORT_SEMANTIC(ORIS);

void SEMANTIC(XORI)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] ^ inst.uimm16;
}
void DECODER(XORI) { XORI(context, inst); }
EXPORT_SEMANTIC(XORI);

void SEMANTIC(XORIS)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] ^ (u64{inst.uimm16} << 16);
}
void DECODER(XORIS) { XORIS(context, inst); }
EXPORT_SEMANTIC(XORIS);

void SEMANTIC(ANDI)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] & inst.uimm16;
  context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
}
void DECODER(ANDI) { ANDI(context, inst); }
EXPORT_SEMANTIC(ANDI);

void SEMANTIC(ANDIS)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] & (u64{inst.uimm16} << 16);
  context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
}
void DECODER(ANDIS) { ANDIS(context, inst); }
EXPORT_SEMANTIC(ANDIS);

void SEMANTIC(RLDICL)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      rol64(context.gpr[inst.rs], inst.sh64) & (~0ull >> inst.mbe64);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDICL) { RLDICL(context, inst); }
EXPORT_SEMANTIC(RLDICL);

void SEMANTIC(RLDICR)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      rol64(context.gpr[inst.rs], inst.sh64) & (~0ull << (inst.mbe64 ^ 63));
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDICR) { RLDICR(context, inst); }
EXPORT_SEMANTIC(RLDICR);

void SEMANTIC(RLDIC)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = rol64(context.gpr[inst.rs], inst.sh64) &
                         ppu_rotate_mask(inst.mbe64, inst.sh64 ^ 63);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDIC) { RLDIC(context, inst); }
EXPORT_SEMANTIC(RLDIC);

void SEMANTIC(RLDIMI)(PPUContext &context, Instruction inst) {
  const u64 mask = ppu_rotate_mask(inst.mbe64, inst.sh64 ^ 63);
  context.gpr[inst.ra] = (context.gpr[inst.ra] & ~mask) |
                         (rol64(context.gpr[inst.rs], inst.sh64) & mask);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDIMI) { RLDIMI(context, inst); }
EXPORT_SEMANTIC(RLDIMI);

void SEMANTIC(RLDCL)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      rol64(context.gpr[inst.rs], context.gpr[inst.rb] & 0x3f) &
      (~0ull >> inst.mbe64);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDCL) { RLDCL(context, inst); }
EXPORT_SEMANTIC(RLDCL);

void SEMANTIC(RLDCR)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      rol64(context.gpr[inst.rs], context.gpr[inst.rb] & 0x3f) &
      (~0ull << (inst.mbe64 ^ 63));
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(RLDCR) { RLDCR(context, inst); }
EXPORT_SEMANTIC(RLDCR);

void SEMANTIC(CMP)(PPUContext &context, Instruction inst) {
  if (inst.l10) {
    context.cr.fields[inst.crfd].update<s64>(
        context.gpr[inst.ra], context.gpr[inst.rb], context.xer_so);
  } else {
    context.cr.fields[inst.crfd].update<s32>(
        context.gpr[inst.ra], static_cast<s32>(context.gpr[inst.rb]),
        static_cast<s32>(context.xer_so));
  }
}
void DECODER(CMP) { CMP(context, inst); }
EXPORT_SEMANTIC(CMP);

void SEMANTIC(TW)(s32 ra, u8 bo, s32 rb) {
  if ((bo & 0x10) && ra < rb) {
    rpcsx_trap();
  }

  if ((bo & 0x8) && ra > rb) {
    rpcsx_trap();
  }

  if ((bo & 0x4) && ra == rb) {
    rpcsx_trap();
  }

  if ((bo & 0x2) && (u32)ra < (u32)rb) {
    rpcsx_trap();
  }

  if ((bo & 0x1) && (u32)ra > (u32)rb) {
    rpcsx_trap();
  }
}

void DECODER(TW) { TW(context.gpr[inst.ra], inst.bo, context.gpr[inst.rb]); }

EXPORT_SEMANTIC(TW);

static const v128 s_lvsl_base =
    v128::from64r(0x0001020304050607, 0x08090a0b0c0d0e0f);

static const v128 s_lvsl_consts[16] = {
    gv_add8(s_lvsl_base, gv_bcst8(0)),  gv_add8(s_lvsl_base, gv_bcst8(1)),
    gv_add8(s_lvsl_base, gv_bcst8(2)),  gv_add8(s_lvsl_base, gv_bcst8(3)),
    gv_add8(s_lvsl_base, gv_bcst8(4)),  gv_add8(s_lvsl_base, gv_bcst8(5)),
    gv_add8(s_lvsl_base, gv_bcst8(6)),  gv_add8(s_lvsl_base, gv_bcst8(7)),
    gv_add8(s_lvsl_base, gv_bcst8(8)),  gv_add8(s_lvsl_base, gv_bcst8(9)),
    gv_add8(s_lvsl_base, gv_bcst8(10)), gv_add8(s_lvsl_base, gv_bcst8(11)),
    gv_add8(s_lvsl_base, gv_bcst8(12)), gv_add8(s_lvsl_base, gv_bcst8(13)),
    gv_add8(s_lvsl_base, gv_bcst8(14)), gv_add8(s_lvsl_base, gv_bcst8(15)),
};

void SEMANTIC(LVSL)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.vr[inst.vd] = s_lvsl_consts[addr % 16];
}
void DECODER(LVSL) { LVSL(context, inst); }
EXPORT_SEMANTIC(LVSL);

void SEMANTIC(LVEBX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  context.vr[inst.vd] = ppu_feed_data<v128>(context, addr);
}
void DECODER(LVEBX) { LVEBX(context, inst); }
EXPORT_SEMANTIC(LVEBX);

void SEMANTIC(SUBFC)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  const auto r = add64_flags(~RA, RB, 1);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((~RA >> 63 == RB >> 63) &&
                  (~RA >> 63 != context.gpr[inst.rd] >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(SUBFC) { SUBFC(context, inst); }
EXPORT_SEMANTIC(SUBFC);

void SEMANTIC(MULHDU)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] = umulh64(context.gpr[inst.ra], context.gpr[inst.rb]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULHDU) { MULHDU(context, inst); }
EXPORT_SEMANTIC(MULHDU);

void SEMANTIC(ADDC)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  const auto r = add64_flags(RA, RB);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((RA >> 63 == RB >> 63) &&
                  (RA >> 63 != context.gpr[inst.rd] >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(ADDC) { ADDC(context, inst); }
EXPORT_SEMANTIC(ADDC);

void SEMANTIC(MULHWU)(PPUContext &context, Instruction inst) {
  u32 a = static_cast<u32>(context.gpr[inst.ra]);
  u32 b = static_cast<u32>(context.gpr[inst.rb]);
  context.gpr[inst.rd] = (u64{a} * b) >> 32;
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULHWU) { MULHWU(context, inst); }
EXPORT_SEMANTIC(MULHWU);

void SEMANTIC(MFCR)(PPUContext &context, std::uint64_t &d) {
#if defined(ARCH_X64)
  be_t<v128> lane0, lane1;
  std::memcpy(&lane0, context.cr.fields, sizeof(v128));
  std::memcpy(&lane1, context.cr.fields + 4, sizeof(v128));
  const u32 mh = _mm_movemask_epi8(_mm_slli_epi64(lane0.value(), 7));
  const u32 ml = _mm_movemask_epi8(_mm_slli_epi64(lane1.value(), 7));

  d = (mh << 16) | ml;
#else
  d = context.cr.pack();
#endif
}
void DECODER(MFCR) { MFCR(context, context.gpr[inst.rd]); }
EXPORT_SEMANTIC(MFCR);

void SEMANTIC(MFOCRF)(u64 &d, u32 crIndex, CrField &cr) {
  const u32 v =
      cr.bits[0] << 3 | cr.bits[1] << 2 | cr.bits[2] << 1 | cr.bits[3] << 0;

  d = v << ((crIndex * 4) ^ 0x1c);
}
void DECODER(MFOCRF) {
  if (inst.l11) {
    auto crIndex = std::countl_zero<u32>(inst.crm) & 7;
    MFOCRF(context.gpr[inst.rd], crIndex, context.cr.fields[crIndex]);
  } else {
    MFCR(context, context.gpr[inst.rd]);
  }
}
EXPORT_SEMANTIC(MFOCRF);

void SEMANTIC(LWARX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_lwarx(context, vm::cast(addr));
}
void DECODER(LWARX) { LWARX(context, inst); }
EXPORT_SEMANTIC(LWARX);

void SEMANTIC(LDX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u64>(context, addr);
}
void DECODER(LDX) { LDX(context, inst); }
EXPORT_SEMANTIC(LDX);

void SEMANTIC(LWZX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u32>(context, addr);
}
void DECODER(LWZX) { LWZX(context, inst); }
EXPORT_SEMANTIC(LWZX);

void SEMANTIC(SLW)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      static_cast<u32>(context.gpr[inst.rs] << (context.gpr[inst.rb] & 0x3f));
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SLW) { SLW(context, inst); }
EXPORT_SEMANTIC(SLW);

void SEMANTIC(CNTLZW)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      std::countl_zero(static_cast<u32>(context.gpr[inst.rs]));
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(CNTLZW) { CNTLZW(context, inst); }
EXPORT_SEMANTIC(CNTLZW);

void SEMANTIC(SLD)(PPUContext &context, Instruction inst) {
  const u32 n = context.gpr[inst.rb] & 0x7f;
  context.gpr[inst.ra] = n & 0x40 ? 0 : context.gpr[inst.rs] << n;
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SLD) { SLD(context, inst); }
EXPORT_SEMANTIC(SLD);

void SEMANTIC(AND)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] & context.gpr[inst.rb];
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(AND) { AND(context, inst); }
EXPORT_SEMANTIC(AND);

void SEMANTIC(CMPL)(PPUContext &context, Instruction inst) {
  if (inst.l10) {
    context.cr.fields[inst.crfd].update<u64>(
        context.gpr[inst.ra], context.gpr[inst.rb], context.xer_so);
  } else {
    context.cr.fields[inst.crfd].update<u32>(
        static_cast<u32>(context.gpr[inst.ra]),
        static_cast<u32>(context.gpr[inst.rb]), context.xer_so);
  }
}
void DECODER(CMPL) { CMPL(context, inst); }
EXPORT_SEMANTIC(CMPL);

static const v128 s_lvsr_consts[16] = {
    gv_add8(s_lvsl_base, gv_bcst8(16)), gv_add8(s_lvsl_base, gv_bcst8(15)),
    gv_add8(s_lvsl_base, gv_bcst8(14)), gv_add8(s_lvsl_base, gv_bcst8(13)),
    gv_add8(s_lvsl_base, gv_bcst8(12)), gv_add8(s_lvsl_base, gv_bcst8(11)),
    gv_add8(s_lvsl_base, gv_bcst8(10)), gv_add8(s_lvsl_base, gv_bcst8(9)),
    gv_add8(s_lvsl_base, gv_bcst8(8)),  gv_add8(s_lvsl_base, gv_bcst8(7)),
    gv_add8(s_lvsl_base, gv_bcst8(6)),  gv_add8(s_lvsl_base, gv_bcst8(5)),
    gv_add8(s_lvsl_base, gv_bcst8(4)),  gv_add8(s_lvsl_base, gv_bcst8(3)),
    gv_add8(s_lvsl_base, gv_bcst8(2)),  gv_add8(s_lvsl_base, gv_bcst8(1)),
};

void SEMANTIC(LVSR)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.vr[inst.vd] = s_lvsr_consts[addr % 16];
}
void DECODER(LVSR) { LVSR(context, inst); }
EXPORT_SEMANTIC(LVSR);

void SEMANTIC(LVEHX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  context.vr[inst.vd] = ppu_feed_data<v128>(context, addr);
}
void DECODER(LVEHX) { LVEHX(context, inst); }
EXPORT_SEMANTIC(LVEHX);

void SEMANTIC(SUBF)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  context.gpr[inst.rd] = RB - RA;

  if (inst.oe) {
    context.setOV((~RA >> 63 == RB >> 63) &&
                  (~RA >> 63 != context.gpr[inst.rd] >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(SUBF) { SUBF(context, inst); }
EXPORT_SEMANTIC(SUBF);

void SEMANTIC(LDUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u64>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LDUX) { LDUX(context, inst); }
EXPORT_SEMANTIC(LDUX);

void SEMANTIC(DCBST)() {}
void DECODER(DCBST) { DCBST(); }
EXPORT_SEMANTIC(DCBST);

void SEMANTIC(LWZUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u32>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LWZUX) { LWZUX(context, inst); }
EXPORT_SEMANTIC(LWZUX);

void SEMANTIC(CNTLZD)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = std::countl_zero(context.gpr[inst.rs]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(CNTLZD) { CNTLZD(context, inst); }
EXPORT_SEMANTIC(CNTLZD);

void SEMANTIC(ANDC)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] & ~context.gpr[inst.rb];
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(ANDC) { ANDC(context, inst); }
EXPORT_SEMANTIC(ANDC);

void SEMANTIC(TD)(s64 ra, u8 bo, s64 rb) {
  if ((bo & 0x10) && ra < rb) {
    rpcsx_trap();
  }

  if ((bo & 0x8) && ra > rb) {
    rpcsx_trap();
  }

  if ((bo & 0x4) && ra == rb) {
    rpcsx_trap();
  }

  if ((bo & 0x2) && (u64)ra < (u64)rb) {
    rpcsx_trap();
  }

  if ((bo & 0x1) && (u64)ra > (u64)rb) {
    rpcsx_trap();
  }
}
void DECODER(TD) { TD(context.gpr[inst.ra], inst.bo, context.gpr[inst.rb]); }
EXPORT_SEMANTIC(TD);

void SEMANTIC(LVEWX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  context.vr[inst.vd] = ppu_feed_data<v128>(context, addr);
}
void DECODER(LVEWX) { LVEWX(context, inst); }
EXPORT_SEMANTIC(LVEWX);

void SEMANTIC(MULHD)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] = mulh64(context.gpr[inst.ra], context.gpr[inst.rb]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULHD) { MULHD(context, inst); }
EXPORT_SEMANTIC(MULHD);

void SEMANTIC(MULHW)(PPUContext &context, Instruction inst) {
  s32 a = static_cast<s32>(context.gpr[inst.ra]);
  s32 b = static_cast<s32>(context.gpr[inst.rb]);
  context.gpr[inst.rd] = (s64{a} * b) >> 32;
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULHW) { MULHW(context, inst); }
EXPORT_SEMANTIC(MULHW);

void SEMANTIC(LDARX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_ldarx(context, vm::cast(addr));
}
void DECODER(LDARX) { LDARX(context, inst); }
EXPORT_SEMANTIC(LDARX);

void SEMANTIC(DCBF)() {}
void DECODER(DCBF) { DCBF(); }
EXPORT_SEMANTIC(DCBF);

void SEMANTIC(LBZX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u8>(context, addr);
}
void DECODER(LBZX) { LBZX(context, inst); }
EXPORT_SEMANTIC(LBZX);

void SEMANTIC(LVX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  context.vr[inst.vd] = ppu_feed_data<v128>(context, addr);
}
void DECODER(LVX) { LVX(context, inst); }
EXPORT_SEMANTIC(LVX);

void SEMANTIC(NEG)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  context.gpr[inst.rd] = 0 - RA;

  if (inst.oe) {
    // FIXME: verify
    context.setOV(RA == (1ull << 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(NEG) { NEG(context, inst); }
EXPORT_SEMANTIC(NEG);

void SEMANTIC(LBZUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u8>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LBZUX) { LBZUX(context, inst); }
EXPORT_SEMANTIC(LBZUX);

void SEMANTIC(NOR)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = ~(context.gpr[inst.rs] | context.gpr[inst.rb]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(NOR) { NOR(context, inst); }
EXPORT_SEMANTIC(NOR);

void SEMANTIC(STVEBX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  const u8 eb = addr & 0xf;
  vm::write(vm::cast(addr), context.vr[inst.vs]._u8[15 - eb]);
}
void DECODER(STVEBX) { STVEBX(context, inst); }
EXPORT_SEMANTIC(STVEBX);

void SEMANTIC(SUBFE)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  const auto r = add64_flags(~RA, RB, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((~RA >> 63 == RB >> 63) &&
                  (~RA >> 63 != context.gpr[inst.rd] >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(SUBFE) { SUBFE(context, inst); }
EXPORT_SEMANTIC(SUBFE);

void SEMANTIC(ADDE)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  const auto r = add64_flags(RA, RB, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((RA >> 63 == RB >> 63) &&
                  (RA >> 63 != context.gpr[inst.rd] >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(ADDE) { ADDE(context, inst); }
EXPORT_SEMANTIC(ADDE);

void SEMANTIC(MTOCRF)(PPUContext &context, Instruction inst) {
  static constexpr CrField s_table[16]{
      CrField::From(false, false, false, false),
      CrField::From(false, false, false, true),
      CrField::From(false, false, true, false),
      CrField::From(false, false, true, true),
      CrField::From(false, true, false, false),
      CrField::From(false, true, false, true),
      CrField::From(false, true, true, false),
      CrField::From(false, true, true, true),
      CrField::From(true, false, false, false),
      CrField::From(true, false, false, true),
      CrField::From(true, false, true, false),
      CrField::From(true, false, true, true),
      CrField::From(true, true, false, false),
      CrField::From(true, true, false, true),
      CrField::From(true, true, true, false),
      CrField::From(true, true, true, true),
  };

  const u64 s = context.gpr[inst.rs];

  if (inst.l11) {
    // MTOCRF

    const u32 n = std::countl_zero<u32>(inst.crm) & 7;
    const u64 v = (s >> ((n * 4) ^ 0x1c)) & 0xf;
    context.cr.fields[n] = s_table[v];
  } else {
    // MTCRF

    for (u32 i = 0; i < 8; i++) {
      if (inst.crm & (128 >> i)) {
        const u64 v = (s >> ((i * 4) ^ 0x1c)) & 0xf;
        context.cr.fields[i] = s_table[v];
      }
    }
  }
}
void DECODER(MTOCRF) { MTOCRF(context, inst); }
EXPORT_SEMANTIC(MTOCRF);

void SEMANTIC(STDX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write(vm::cast(addr), context.gpr[inst.rs]);
}
void DECODER(STDX) { STDX(context, inst); }
EXPORT_SEMANTIC(STDX);

void SEMANTIC(STWCX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.cr.fields[0].set(false, false,
                           ppu_stwcx(context, vm::cast(addr),
                                     static_cast<u32>(context.gpr[inst.rs])),
                           context.xer_so);
}
void DECODER(STWCX) { STWCX(context, inst); }
EXPORT_SEMANTIC(STWCX);

void SEMANTIC(STWX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u32>(context.gpr[inst.rs]));
}
void DECODER(STWX) { STWX(context, inst); }
EXPORT_SEMANTIC(STWX);

void SEMANTIC(STVEHX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~1ULL;
  const u8 eb = (addr & 0xf) >> 1;
  vm::write(vm::cast(addr), context.vr[inst.vs]._u16[7 - eb]);
}
void DECODER(STVEHX) { STVEHX(context, inst); }
EXPORT_SEMANTIC(STVEHX);

void SEMANTIC(STDUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write(vm::cast(addr), context.gpr[inst.rs]);
  context.gpr[inst.ra] = addr;
}
void DECODER(STDUX) { STDUX(context, inst); }
EXPORT_SEMANTIC(STDUX);

void SEMANTIC(STWUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u32>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STWUX) { STWUX(context, inst); }
EXPORT_SEMANTIC(STWUX);

void SEMANTIC(STVEWX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~3ULL;
  const u8 eb = (addr & 0xf) >> 2;
  vm::write(vm::cast(addr), context.vr[inst.vs]._u32[3 - eb]);
}
void DECODER(STVEWX) { STVEWX(context, inst); }
EXPORT_SEMANTIC(STVEWX);

void SEMANTIC(SUBFZE)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const auto r = add64_flags(~RA, 0, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((~RA >> 63 == 0) && (~RA >> 63 != r.result >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(SUBFZE) { SUBFZE(context, inst); }
EXPORT_SEMANTIC(SUBFZE);

void SEMANTIC(ADDZE)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const auto r = add64_flags(RA, 0, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;

  if (inst.oe) {
    context.setOV((RA >> 63 == 0) && (RA >> 63 != r.result >> 63));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(ADDZE) { ADDZE(context, inst); }
EXPORT_SEMANTIC(ADDZE);

void SEMANTIC(STDCX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.cr.fields[0].set(
      false, false, ppu_stdcx(context, vm::cast(addr), context.gpr[inst.rs]),
      context.xer_so);
}
void DECODER(STDCX) { STDCX(context, inst); }
EXPORT_SEMANTIC(STDCX);

void SEMANTIC(STBX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u8>(context.gpr[inst.rs]));
}
void DECODER(STBX) { STBX(context, inst); }
EXPORT_SEMANTIC(STBX);

void SEMANTIC(STVX)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  vm::write(vm::cast(addr), context.vr[inst.vs]);
}
void DECODER(STVX) { STVX(context, inst); }
EXPORT_SEMANTIC(STVX);

void SEMANTIC(MULLD)(PPUContext &context, Instruction inst) {
  const s64 RA = context.gpr[inst.ra];
  const s64 RB = context.gpr[inst.rb];
  context.gpr[inst.rd] = RA * RB;
  if (inst.oe) {
    const s64 high = mulh64(RA, RB);
    // FIXME: verify
    context.setOV(high != s64(context.gpr[inst.rd]) >> 63);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULLD) { MULLD(context, inst); }
EXPORT_SEMANTIC(MULLD);

void SEMANTIC(SUBFME)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const auto r = add64_flags(~RA, ~0ull, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;
  if (inst.oe) {
    context.setOV((~RA >> 63 == 1) && (~RA >> 63 != r.result >> 63));
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(SUBFME) { SUBFME(context, inst); }
EXPORT_SEMANTIC(SUBFME);

void SEMANTIC(ADDME)(PPUContext &context, Instruction inst) {
  const s64 RA = context.gpr[inst.ra];
  const auto r = add64_flags(RA, ~0ull, context.xer_ca);
  context.gpr[inst.rd] = r.result;
  context.xer_ca = r.carry;
  if (inst.oe) {
    context.setOV((u64(RA) >> 63 == 1) && (u64(RA) >> 63 != r.result >> 63));
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(r.result, 0, context.xer_so);
  }
}
void DECODER(ADDME) { ADDME(context, inst); }
EXPORT_SEMANTIC(ADDME);

void SEMANTIC(MULLW)(PPUContext &context, Instruction inst) {
  context.gpr[inst.rd] = s64{static_cast<s32>(context.gpr[inst.ra])} *
                         static_cast<s32>(context.gpr[inst.rb]);

  if (inst.oe) {
    context.setOV(s64(context.gpr[inst.rd]) < INT32_MIN ||
                  s64(context.gpr[inst.rd]) > INT32_MAX);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(MULLW) { MULLW(context, inst); }
EXPORT_SEMANTIC(MULLW);

void SEMANTIC(DCBTST)() {}
void DECODER(DCBTST) { DCBTST(); }
EXPORT_SEMANTIC(DCBTST);

void SEMANTIC(STBUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u8>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STBUX) { STBUX(context, inst); }
EXPORT_SEMANTIC(STBUX);

void SEMANTIC(ADD)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  context.gpr[inst.rd] = RA + RB;

  if (inst.oe) {
    context.setOV((RA >> 63 == RB >> 63) &&
                  (RA >> 63 != context.gpr[inst.rd] >> 63));
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(ADD) { ADD(context, inst); }
EXPORT_SEMANTIC(ADD);

void SEMANTIC(DCBT)() {}
void DECODER(DCBT) { DCBT(); }
EXPORT_SEMANTIC(DCBT);

void SEMANTIC(LHZX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u16>(context, addr);
}
void DECODER(LHZX) { LHZX(context, inst); }
EXPORT_SEMANTIC(LHZX);

void SEMANTIC(EQV)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = ~(context.gpr[inst.rs] ^ context.gpr[inst.rb]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(EQV) { EQV(context, inst); }
EXPORT_SEMANTIC(EQV);

void SEMANTIC(ECIWX)() { rpcsx_unimplemented_instruction(); }
void DECODER(ECIWX) { ECIWX(); }
EXPORT_SEMANTIC(ECIWX);

void SEMANTIC(LHZUX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<u16>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LHZUX) { LHZUX(context, inst); }
EXPORT_SEMANTIC(LHZUX);

void SEMANTIC(XOR)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] ^ context.gpr[inst.rb];
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(XOR) { XOR(context, inst); }
EXPORT_SEMANTIC(XOR);

void SEMANTIC(MFSPR)(PPUContext &context, Instruction inst) {
  const u32 n = (inst.spr >> 5) | ((inst.spr & 0x1f) << 5);

  switch (n) {
  case 0x001:
    context.gpr[inst.rd] = u32{context.xer_so} << 31 | context.xer_ov << 30 |
                           context.xer_ca << 29 | context.xer_cnt;
    break;
  case 0x008:
    context.gpr[inst.rd] = context.lr;
    break;
  case 0x009:
    context.gpr[inst.rd] = context.ctr;
    break;
  case 0x100:
    context.gpr[inst.rd] = context.vrsave;
    break;

  case 0x10C:
    context.gpr[inst.rd] = rpcsx_get_tb();
    break;
  case 0x10D:
    context.gpr[inst.rd] = rpcsx_get_tb() >> 32;
    break;
  default:
    rpcsx_invalid_instruction();
  }
}
void DECODER(MFSPR) { MFSPR(context, inst); }
EXPORT_SEMANTIC(MFSPR);

void SEMANTIC(LWAX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<s32>(context, addr);
}
void DECODER(LWAX) { LWAX(context, inst); }
EXPORT_SEMANTIC(LWAX);

void SEMANTIC(DST)() {}
void DECODER(DST) { DST(); }
EXPORT_SEMANTIC(DST);

void SEMANTIC(LHAX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<s16>(context, addr);
}
void DECODER(LHAX) { LHAX(context, inst); }
EXPORT_SEMANTIC(LHAX);

void SEMANTIC(LVXL)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  context.vr[inst.vd] = ppu_feed_data<v128>(context, addr);
}
void DECODER(LVXL) { LVXL(context, inst); }
EXPORT_SEMANTIC(LVXL);

void SEMANTIC(MFTB)(PPUContext &context, Instruction inst) {
  const u32 n = (inst.spr >> 5) | ((inst.spr & 0x1f) << 5);

  switch (n) {
  case 0x10C:
    context.gpr[inst.rd] = rpcsx_get_tb();
    break;
  case 0x10D:
    context.gpr[inst.rd] = rpcsx_get_tb() >> 32;
    break;
  default:
    rpcsx_invalid_instruction();
  }
}
void DECODER(MFTB) { MFTB(context, inst); }
EXPORT_SEMANTIC(MFTB);

void SEMANTIC(LWAUX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<s32>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LWAUX) { LWAUX(context, inst); }
EXPORT_SEMANTIC(LWAUX);

void SEMANTIC(DSTST)() {}
void DECODER(DSTST) { DSTST(); }
EXPORT_SEMANTIC(DSTST);

void SEMANTIC(LHAUX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<s16>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LHAUX) { LHAUX(context, inst); }
EXPORT_SEMANTIC(LHAUX);

void SEMANTIC(STHX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u16>(context.gpr[inst.rs]));
}
void DECODER(STHX) { STHX(context, inst); }
EXPORT_SEMANTIC(STHX);

void SEMANTIC(ORC)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] | ~context.gpr[inst.rb];
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(ORC) { ORC(context, inst); }
EXPORT_SEMANTIC(ORC);

void SEMANTIC(ECOWX)() { rpcsx_unimplemented_instruction(); }
void DECODER(ECOWX) { ECOWX(); }
EXPORT_SEMANTIC(ECOWX);

void SEMANTIC(STHUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<u16>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STHUX) { STHUX(context, inst); }
EXPORT_SEMANTIC(STHUX);

void SEMANTIC(OR)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = context.gpr[inst.rs] | context.gpr[inst.rb];
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(OR) { OR(context, inst); }
EXPORT_SEMANTIC(OR);

void SEMANTIC(DIVDU)(PPUContext &context, Instruction inst) {
  const u64 RA = context.gpr[inst.ra];
  const u64 RB = context.gpr[inst.rb];
  context.gpr[inst.rd] = RB == 0 ? 0 : RA / RB;

  if (inst.oe) {
    context.setOV(RB == 0);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(DIVDU) { DIVDU(context, inst); }
EXPORT_SEMANTIC(DIVDU);

void SEMANTIC(DIVWU)(PPUContext &context, Instruction inst) {
  const u32 RA = static_cast<u32>(context.gpr[inst.ra]);
  const u32 RB = static_cast<u32>(context.gpr[inst.rb]);
  context.gpr[inst.rd] = RB == 0 ? 0 : RA / RB;
  if (inst.oe) {
    context.setOV(RB == 0);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(DIVWU) { DIVWU(context, inst); }
EXPORT_SEMANTIC(DIVWU);

void SEMANTIC(MTSPR)(PPUContext &context, Instruction inst) {
  const u32 n = (inst.spr >> 5) | ((inst.spr & 0x1f) << 5);

  switch (n) {
  case 0x001: {
    const u64 value = context.gpr[inst.rs];
    context.xer_so = (value & 0x80000000) != 0;
    context.xer_ov = (value & 0x40000000) != 0;
    context.xer_ca = (value & 0x20000000) != 0;
    context.xer_cnt = value & 0x7f;
    break;
  }
  case 0x008:
    context.lr = context.gpr[inst.rs];
    break;
  case 0x009:
    context.ctr = context.gpr[inst.rs];
    break;
  case 0x100:
    context.vrsave = static_cast<u32>(context.gpr[inst.rs]);
    break;
  default:
    rpcsx_invalid_instruction();
  }
}
void DECODER(MTSPR) { MTSPR(context, inst); }
EXPORT_SEMANTIC(MTSPR);

void SEMANTIC(DCBI)() {}
void DECODER(DCBI) { DCBI(); }
EXPORT_SEMANTIC(DCBI);

void SEMANTIC(NAND)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = ~(context.gpr[inst.rs] & context.gpr[inst.rb]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(NAND) { NAND(context, inst); }
EXPORT_SEMANTIC(NAND);

void SEMANTIC(STVXL)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                            : context.gpr[inst.rb]) &
                   ~0xfull;
  vm::write(vm::cast(addr), context.vr[inst.vs]);
}
void DECODER(STVXL) { STVXL(context, inst); }
EXPORT_SEMANTIC(STVXL);

void SEMANTIC(DIVD)(PPUContext &context, Instruction inst) {
  const s64 RA = context.gpr[inst.ra];
  const s64 RB = context.gpr[inst.rb];
  const bool o = RB == 0 || (RA == INT64_MIN && RB == -1);
  context.gpr[inst.rd] = o ? 0 : RA / RB;
  if (inst.oe) {
    context.setOV(o);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(DIVD) { DIVD(context, inst); }
EXPORT_SEMANTIC(DIVD);

void SEMANTIC(DIVW)(PPUContext &context, Instruction inst) {
  const s32 RA = static_cast<s32>(context.gpr[inst.ra]);
  const s32 RB = static_cast<s32>(context.gpr[inst.rb]);
  const bool o = RB == 0 || (RA == INT32_MIN && RB == -1);
  context.gpr[inst.rd] = o ? 0 : static_cast<u32>(RA / RB);
  if (inst.oe) {
    context.setOV(o);
  }
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.rd], 0, context.xer_so);
  }
}
void DECODER(DIVW) { DIVW(context, inst); }
EXPORT_SEMANTIC(DIVW);

void SEMANTIC(LVLX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  const u128 data = ppu_feed_data<u128>(context, addr & -16);
  context.vr[inst.vd] = data << ((addr & 15) * 8);
}
void DECODER(LVLX) { LVLX(context, inst); }
EXPORT_SEMANTIC(LVLX);

void SEMANTIC(LDBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<le_t<u64>>(context, addr);
}
void DECODER(LDBRX) { LDBRX(context, inst); }
EXPORT_SEMANTIC(LDBRX);

void SEMANTIC(LSWX)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                     : context.gpr[inst.rb];
  u32 count = context.xer_cnt & 0x7f;
  for (; count >= 4; count -= 4, addr += 4, inst.rd = (inst.rd + 1) & 31) {
    context.gpr[inst.rd] = ppu_feed_data<u32>(context, addr);
  }
  if (count) {
    u32 value = 0;
    for (u32 byte = 0; byte < count; byte++) {
      u32 byte_value = ppu_feed_data<u8>(context, addr + byte);
      value |= byte_value << ((3 ^ byte) * 8);
    }
    context.gpr[inst.rd] = value;
  }
}
void DECODER(LSWX) { LSWX(context, inst); }
EXPORT_SEMANTIC(LSWX);

void SEMANTIC(LWBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<le_t<u32>>(context, addr);
}
void DECODER(LWBRX) { LWBRX(context, inst); }
EXPORT_SEMANTIC(LWBRX);

void SEMANTIC(LFSX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.fpr[inst.frd] = ppu_feed_data<f32>(context, addr);
}
void DECODER(LFSX) { LFSX(context, inst); }
EXPORT_SEMANTIC(LFSX);

void SEMANTIC(SRW)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] =
      (context.gpr[inst.rs] & 0xffffffff) >> (context.gpr[inst.rb] & 0x3f);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRW) { SRW(context, inst); }
EXPORT_SEMANTIC(SRW);

void SEMANTIC(SRD)(PPUContext &context, Instruction inst) {
  const u32 n = context.gpr[inst.rb] & 0x7f;
  context.gpr[inst.ra] = n & 0x40 ? 0 : context.gpr[inst.rs] >> n;
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRD) { SRD(context, inst); }
EXPORT_SEMANTIC(SRD);

void SEMANTIC(LVRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];

  if ((addr & 15) == 0) {
    context.vr[inst.vd] = u128(0);
  } else {
    const auto data = ppu_feed_data<u128>(context, addr & -16);
    context.vr[inst.vd] = data >> ((~addr & 15) * 8) >> 8;
  }
}
void DECODER(LVRX) { LVRX(context, inst); }
EXPORT_SEMANTIC(LVRX);

void SEMANTIC(LSWI)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] : 0;
  u64 N = inst.rb ? inst.rb : 32;
  u8 reg = inst.rd;

  while (N > 0) {
    if (N > 3) {
      context.gpr[reg] = ppu_feed_data<u32>(context, addr);
      addr += 4;
      N -= 4;
    } else {
      u32 buf = 0;
      u32 i = 3;
      while (N > 0) {
        N = N - 1;
        buf |= ppu_feed_data<u8>(context, addr) << (i * 8);
        addr++;
        i--;
      }
      context.gpr[reg] = buf;
    }
    reg = (reg + 1) % 32;
  }
}
void DECODER(LSWI) { LSWI(context, inst); }
EXPORT_SEMANTIC(LSWI);

void SEMANTIC(LFSUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  context.fpr[inst.frd] = ppu_feed_data<f32>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LFSUX) { LFSUX(context, inst); }
EXPORT_SEMANTIC(LFSUX);

void SEMANTIC(SYNC)() { std::atomic_thread_fence(std::memory_order::seq_cst); }
void DECODER(SYNC) { SYNC(); }
EXPORT_SEMANTIC(SYNC);

void SEMANTIC(LFDX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.fpr[inst.frd] = ppu_feed_data<f64>(context, addr);
}
void DECODER(LFDX) { LFDX(context, inst); }
EXPORT_SEMANTIC(LFDX);

void SEMANTIC(LFDUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  context.fpr[inst.frd] = ppu_feed_data<f64>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LFDUX) { LFDUX(context, inst); }
EXPORT_SEMANTIC(LFDUX);

void SEMANTIC(STVLX)(v128 s, std::uint64_t a, std::uint64_t b) {
  const u64 addr = a + b;
  const u32 tail = u32(addr & 15);
  std::uint8_t data[16];
  for (u32 j = 0; j < 16 - tail; j++)
    data[j] = s.u8r[j];

  rpcsx_vm_write(addr, data, 16 - tail);
}
void DECODER(STVLX) {
  STVLX(context.vr[inst.vs], inst.ra ? context.gpr[inst.ra] : 0,
        context.gpr[inst.rb]);
}
EXPORT_SEMANTIC(STVLX);

void SEMANTIC(STDBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<le_t<u64>>(vm::cast(addr), context.gpr[inst.rs]);
}
void DECODER(STDBRX) { STDBRX(context, inst); }
EXPORT_SEMANTIC(STDBRX);

void SEMANTIC(STSWX)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                     : context.gpr[inst.rb];
  u32 count = context.xer_cnt & 0x7F;
  for (; count >= 4; count -= 4, addr += 4, inst.rs = (inst.rs + 1) & 31) {
    vm::write(vm::cast(addr), static_cast<u32>(context.gpr[inst.rs]));
  }
  if (count) {
    u32 value = static_cast<u32>(context.gpr[inst.rs]);
    for (u32 byte = 0; byte < count; byte++) {
      u8 byte_value = static_cast<u8>(value >> ((3 ^ byte) * 8));
      vm::write(vm::cast(addr + byte), byte_value);
    }
  }
}
void DECODER(STSWX) { STSWX(context, inst); }
EXPORT_SEMANTIC(STSWX);

void SEMANTIC(STWBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<le_t<u32>>(vm::cast(addr), static_cast<u32>(context.gpr[inst.rs]));
}
void DECODER(STWBRX) { STWBRX(context, inst); }
EXPORT_SEMANTIC(STWBRX);

void SEMANTIC(STFSX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<f32>(vm::cast(addr), static_cast<float>(context.fpr[inst.frs]));
}
void DECODER(STFSX) { STFSX(context, inst); }
EXPORT_SEMANTIC(STFSX);

void SEMANTIC(STVRX)(v128 s, std::uint64_t a, std::uint64_t b) {
  const u64 addr = a + b;
  const u32 tail = u32(addr & 15);
  std::uint8_t data[16];
  for (u32 i = 15; i > 15 - tail; i--)
    data[i] = s.u8r[i];

  // FIXME: verify
  rpcsx_vm_write(addr - 16, data + 15 - tail, tail + 1);
  // u8 *ptr = vm::_ptr<u8>(addr - 16);
}
void DECODER(STVRX) {
  STVRX(context.vr[inst.vs], inst.ra ? context.gpr[inst.ra] : 0,
        context.gpr[inst.rb]);
}
EXPORT_SEMANTIC(STVRX);

void SEMANTIC(STFSUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write(vm::cast(addr), static_cast<f32>(context.fpr[inst.frs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STFSUX) { STFSUX(context, inst); }
EXPORT_SEMANTIC(STFSUX);

void SEMANTIC(STSWI)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] : 0;
  u64 N = inst.rb ? inst.rb : 32;
  u8 reg = inst.rd;

  while (N > 0) {
    if (N > 3) {
      vm::write<u32>(vm::cast(addr), static_cast<u32>(context.gpr[reg]));
      addr += 4;
      N -= 4;
    } else {
      u32 buf = static_cast<u32>(context.gpr[reg]);
      while (N > 0) {
        N = N - 1;
        vm::write<u8>(vm::cast(addr), (0xFF000000 & buf) >> 24);
        buf <<= 8;
        addr++;
      }
    }
    reg = (reg + 1) % 32;
  }
}
void DECODER(STSWI) { STSWI(context, inst); }
EXPORT_SEMANTIC(STSWI);

void SEMANTIC(STFDX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<f64>(vm::cast(addr), context.fpr[inst.frs]);
}
void DECODER(STFDX) { STFDX(context, inst); }
EXPORT_SEMANTIC(STFDX);

void SEMANTIC(STFDUX)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + context.gpr[inst.rb];
  vm::write<f64>(vm::cast(addr), context.fpr[inst.frs]);
  context.gpr[inst.ra] = addr;
}
void DECODER(STFDUX) { STFDUX(context, inst); }
EXPORT_SEMANTIC(STFDUX);

void SEMANTIC(LVLXL)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  const u128 data = ppu_feed_data<u128>(context, addr & -16);
  context.vr[inst.vd] = data << ((addr & 15) * 8);
}
void DECODER(LVLXL) { LVLXL(context, inst); }
EXPORT_SEMANTIC(LVLXL);

void SEMANTIC(LHBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  context.gpr[inst.rd] = ppu_feed_data<le_t<u16>>(context, addr);
}
void DECODER(LHBRX) { LHBRX(context, inst); }
EXPORT_SEMANTIC(LHBRX);

void SEMANTIC(SRAW)(PPUContext &context, Instruction inst) {
  s32 RS = static_cast<s32>(context.gpr[inst.rs]);
  u8 shift = context.gpr[inst.rb] & 63;
  if (shift > 31) {
    context.gpr[inst.ra] = 0 - (RS < 0);
    context.xer_ca = (RS < 0);
  } else {
    context.gpr[inst.ra] = RS >> shift;
    context.xer_ca =
        (RS < 0) && ((context.gpr[inst.ra] << shift) != static_cast<u64>(RS));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRAW) { SRAW(context, inst); }
EXPORT_SEMANTIC(SRAW);

void SEMANTIC(SRAD)(PPUContext &context, Instruction inst) {
  s64 RS = context.gpr[inst.rs];
  u8 shift = context.gpr[inst.rb] & 127;
  if (shift > 63) {
    context.gpr[inst.ra] = 0 - (RS < 0);
    context.xer_ca = (RS < 0);
  } else {
    context.gpr[inst.ra] = RS >> shift;
    context.xer_ca =
        (RS < 0) && ((context.gpr[inst.ra] << shift) != static_cast<u64>(RS));
  }

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRAD) { SRAD(context, inst); }
EXPORT_SEMANTIC(SRAD);

void SEMANTIC(LVRXL)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];

  if ((addr & 15) == 0) {
    context.vr[inst.vd] = u128(0);
  } else {
    const u128 data = ppu_feed_data<u128>(context, addr & -16);
    context.vr[inst.vd] = data >> ((~addr & 15) * 8) >> 8;
  }
}
void DECODER(LVRXL) { LVRXL(context, inst); }
EXPORT_SEMANTIC(LVRXL);

void SEMANTIC(DSS)() {}
void DECODER(DSS) { DSS(); }
EXPORT_SEMANTIC(DSS);

void SEMANTIC(SRAWI)(PPUContext &context, Instruction inst) {
  s32 RS = static_cast<u32>(context.gpr[inst.rs]);
  context.gpr[inst.ra] = RS >> inst.sh32;
  context.xer_ca =
      (RS < 0) && (static_cast<u32>(context.gpr[inst.ra] << inst.sh32) !=
                   static_cast<u32>(RS));

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRAWI) { SRAWI(context, inst); }
EXPORT_SEMANTIC(SRAWI);

void SEMANTIC(SRADI)(PPUContext &context, Instruction inst) {
  auto sh = inst.sh64;
  s64 RS = context.gpr[inst.rs];
  context.gpr[inst.ra] = RS >> sh;
  context.xer_ca =
      (RS < 0) && ((context.gpr[inst.ra] << sh) != static_cast<u64>(RS));

  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(SRADI) { SRADI(context, inst); }
EXPORT_SEMANTIC(SRADI);

void SEMANTIC(EIEIO)() { std::atomic_thread_fence(std::memory_order::seq_cst); }
void DECODER(EIEIO) { EIEIO(); }
EXPORT_SEMANTIC(EIEIO);

void SEMANTIC(STVLXL)(v128 s, u64 a, u64 b) {
  const u64 addr = a + b;
  const u32 tail = u32(addr & 15);
  // FIXME
  for (u32 j = 0; j < 16 - tail; j++)
    vm::write(addr + j, s.u8r[j]);
}
void DECODER(STVLXL) {
  STVLXL(context.vr[inst.vs], inst.ra ? context.gpr[inst.ra] : 0,
         context.gpr[inst.rb]);
}
EXPORT_SEMANTIC(STVLXL);

void SEMANTIC(STHBRX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<le_t<u16>>(vm::cast(addr), static_cast<u16>(context.gpr[inst.rs]));
}
void DECODER(STHBRX) { STHBRX(context, inst); }
EXPORT_SEMANTIC(STHBRX);

void SEMANTIC(EXTSH)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = static_cast<s16>(context.gpr[inst.rs]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(EXTSH) { EXTSH(context, inst); }
EXPORT_SEMANTIC(EXTSH);

void SEMANTIC(STVRXL)(v128 s, u64 a, u64 b) {
  const u64 addr = a + b;
  const u32 tail = u32(addr & 15);

  // FIXME
  for (u32 i = 15; i > 15 - tail; i--)
    vm::write(addr - 16 + i, s.u8r[i]);
}
void DECODER(STVRXL) {
  STVRXL(context.vr[inst.vs], inst.ra ? context.gpr[inst.ra] : 0,
         context.gpr[inst.rb]);
}
EXPORT_SEMANTIC(STVRXL);

void SEMANTIC(EXTSB)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = static_cast<s8>(context.gpr[inst.rs]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(EXTSB) { EXTSB(context, inst); }
EXPORT_SEMANTIC(EXTSB);

void SEMANTIC(STFIWX)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  vm::write<u32>(vm::cast(addr),
                 static_cast<u32>(std::bit_cast<u64>(context.fpr[inst.frs])));
}
void DECODER(STFIWX) { STFIWX(context, inst); }
EXPORT_SEMANTIC(STFIWX);

void SEMANTIC(EXTSW)(PPUContext &context, Instruction inst) {
  context.gpr[inst.ra] = static_cast<s32>(context.gpr[inst.rs]);
  if (inst.rc) {
    context.cr.fields[0].update<s64>(context.gpr[inst.ra], 0, context.xer_so);
  }
}
void DECODER(EXTSW) { EXTSW(context, inst); }
EXPORT_SEMANTIC(EXTSW);

void SEMANTIC(ICBI)() {}
void DECODER(ICBI) { ICBI(); }
EXPORT_SEMANTIC(ICBI);

void SEMANTIC(DCBZ)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + context.gpr[inst.rb]
                           : context.gpr[inst.rb];
  const u32 addr0 = vm::cast(addr) & ~127;

  alignas(64) static constexpr u8 zero_buf[128]{};
  do_cell_atomic_128_store(addr0, zero_buf);
}
void DECODER(DCBZ) { DCBZ(context, inst); }
EXPORT_SEMANTIC(DCBZ);

void SEMANTIC(LWZ)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u32>(context, addr);
}
void DECODER(LWZ) { LWZ(context, inst); }
EXPORT_SEMANTIC(LWZ);

void SEMANTIC(LWZU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u32>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LWZU) { LWZU(context, inst); }
EXPORT_SEMANTIC(LWZU);

void SEMANTIC(LBZ)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u8>(context, addr);
}
void DECODER(LBZ) { LBZ(context, inst); }
EXPORT_SEMANTIC(LBZ);

void SEMANTIC(LBZU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u8>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LBZU) { LBZU(context, inst); }
EXPORT_SEMANTIC(LBZU);

void SEMANTIC(STW)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  const u32 value = static_cast<u32>(context.gpr[inst.rs]);
  vm::write<u32>(vm::cast(addr), value);

  // Insomniac engine v3 & v4 (newer R&C, Fuse, Resitance 3)
  // if (value == 0xAAAAAAAA) [[unlikely]] {
  //   vm::reservation_update(vm::cast(addr));
  // }
}
void DECODER(STW) { STW(context, inst); }
EXPORT_SEMANTIC(STW);

void SEMANTIC(STWU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  vm::write<u32>(vm::cast(addr), static_cast<u32>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STWU) { STWU(context, inst); }
EXPORT_SEMANTIC(STWU);

void SEMANTIC(STB)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  vm::write<u8>(vm::cast(addr), static_cast<u8>(context.gpr[inst.rs]));
}
void DECODER(STB) { STB(context, inst); }
EXPORT_SEMANTIC(STB);

void SEMANTIC(STBU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  vm::write<u8>(vm::cast(addr), static_cast<u8>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STBU) { STBU(context, inst); }
EXPORT_SEMANTIC(STBU);

void SEMANTIC(LHZ)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u16>(context, addr);
}
void DECODER(LHZ) { LHZ(context, inst); }
EXPORT_SEMANTIC(LHZ);

void SEMANTIC(LHZU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<u16>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LHZU) { LHZU(context, inst); }
EXPORT_SEMANTIC(LHZU);

void SEMANTIC(LHA)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<s16>(context, addr);
}
void DECODER(LHA) { LHA(context, inst); }
EXPORT_SEMANTIC(LHA);

void SEMANTIC(LHAU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.gpr[inst.rd] = ppu_feed_data<s16>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LHAU) { LHAU(context, inst); }
EXPORT_SEMANTIC(LHAU);

void SEMANTIC(STH)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  vm::write<u16>(vm::cast(addr), static_cast<u16>(context.gpr[inst.rs]));
}
void DECODER(STH) { STH(context, inst); }
EXPORT_SEMANTIC(STH);

void SEMANTIC(STHU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  vm::write<u16>(vm::cast(addr), static_cast<u16>(context.gpr[inst.rs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STHU) { STHU(context, inst); }
EXPORT_SEMANTIC(STHU);

void SEMANTIC(LMW)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  for (u32 i = inst.rd; i < 32; ++i, addr += 4) {
    context.gpr[i] = ppu_feed_data<u32>(context, addr);
  }
}
void DECODER(LMW) { LMW(context, inst); }
EXPORT_SEMANTIC(LMW);

void SEMANTIC(STMW)(PPUContext &context, Instruction inst) {
  u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  for (u32 i = inst.rs; i < 32; ++i, addr += 4) {
    vm::write<u32>(vm::cast(addr), static_cast<u32>(context.gpr[i]));
  }
}
void DECODER(STMW) { STMW(context, inst); }
EXPORT_SEMANTIC(STMW);

void SEMANTIC(LFS)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.fpr[inst.frd] = ppu_feed_data<f32>(context, addr);
}
void DECODER(LFS) { LFS(context, inst); }
EXPORT_SEMANTIC(LFS);

void SEMANTIC(LFSU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.fpr[inst.frd] = ppu_feed_data<f32>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LFSU) { LFSU(context, inst); }
EXPORT_SEMANTIC(LFSU);

void SEMANTIC(LFD)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  context.fpr[inst.frd] = ppu_feed_data<f64>(context, addr);
}
void DECODER(LFD) { LFD(context, inst); }
EXPORT_SEMANTIC(LFD);

void SEMANTIC(LFDU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  context.fpr[inst.frd] = ppu_feed_data<f64>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LFDU) { LFDU(context, inst); }
EXPORT_SEMANTIC(LFDU);

void SEMANTIC(STFS)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  vm::write<f32>(vm::cast(addr), static_cast<float>(context.fpr[inst.frs]));
}
void DECODER(STFS) { STFS(context, inst); }
EXPORT_SEMANTIC(STFS);

void SEMANTIC(STFSU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  vm::write<f32>(vm::cast(addr), static_cast<float>(context.fpr[inst.frs]));
  context.gpr[inst.ra] = addr;
}
void DECODER(STFSU) { STFSU(context, inst); }
EXPORT_SEMANTIC(STFSU);

void SEMANTIC(STFD)(PPUContext &context, Instruction inst) {
  const u64 addr = inst.ra ? context.gpr[inst.ra] + inst.simm16 : inst.simm16;
  vm::write<f64>(vm::cast(addr), context.fpr[inst.frs]);
}
void DECODER(STFD) { STFD(context, inst); }
EXPORT_SEMANTIC(STFD);

void SEMANTIC(STFDU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + inst.simm16;
  vm::write<f64>(vm::cast(addr), context.fpr[inst.frs]);
  context.gpr[inst.ra] = addr;
}
void DECODER(STFDU) { STFDU(context, inst); }
EXPORT_SEMANTIC(STFDU);

void SEMANTIC(LD)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.simm16 & ~3) + (inst.ra ? context.gpr[inst.ra] : 0);
  context.gpr[inst.rd] = ppu_feed_data<u64>(context, addr);
}
void DECODER(LD) { LD(context, inst); }
EXPORT_SEMANTIC(LD);

void SEMANTIC(LDU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + (inst.simm16 & ~3);
  context.gpr[inst.rd] = ppu_feed_data<u64>(context, addr);
  context.gpr[inst.ra] = addr;
}
void DECODER(LDU) { LDU(context, inst); }
EXPORT_SEMANTIC(LDU);

void SEMANTIC(LWA)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.simm16 & ~3) + (inst.ra ? context.gpr[inst.ra] : 0);
  context.gpr[inst.rd] = ppu_feed_data<s32>(context, addr);
}
void DECODER(LWA) { LWA(context, inst); }
EXPORT_SEMANTIC(LWA);

void SEMANTIC(STD)(PPUContext &context, Instruction inst) {
  const u64 addr = (inst.simm16 & ~3) + (inst.ra ? context.gpr[inst.ra] : 0);
  vm::write<u64>(vm::cast(addr), context.gpr[inst.rs]);
}
void DECODER(STD) { STD(context, inst); }
EXPORT_SEMANTIC(STD);

void SEMANTIC(STDU)(PPUContext &context, Instruction inst) {
  const u64 addr = context.gpr[inst.ra] + (inst.simm16 & ~3);
  vm::write<u64>(vm::cast(addr), context.gpr[inst.rs]);
  context.gpr[inst.ra] = addr;
}
void DECODER(STDU) { STDU(context, inst); }
EXPORT_SEMANTIC(STDU);

static void ppu_set_fpcc(PPUContext &context, bool updateCr, f64 a, f64 b,
                         u64 cr_field = 1) {
  static_assert(std::endian::native == std::endian::little, "Not implemented");

  bool fpcc[4];
#if defined(ARCH_X64) && !defined(_M_X64)
  __asm__("comisd %[b], %[a]\n"
          : "=@ccb"(fpcc[0]), "=@cca"(fpcc[1]), "=@ccz"(fpcc[2]),
            "=@ccp"(fpcc[3])
          : [a] "x"(a), [b] "x"(b)
          : "cc");
  if (fpcc[3]) [[unlikely]] {
    fpcc[0] = fpcc[1] = fpcc[2] = false;
  }
#else
  const auto cmp = a <=> b;
  fpcc[0] = cmp == std::partial_ordering::less;
  fpcc[1] = cmp == std::partial_ordering::greater;
  fpcc[2] = cmp == std::partial_ordering::equivalent;
  fpcc[3] = cmp == std::partial_ordering::unordered;
#endif

  auto data = std::bit_cast<CrField>(fpcc);

  // Write FPCC
  context.fpscr.fields[4] = data;

  if (updateCr) {
    // Previous behaviour was throwing an exception; TODO
    context.cr.fields[cr_field] = data;
  }
}

void SEMANTIC(FDIVS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(context.fpr[inst.fra] / context.fpr[inst.frb]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FDIVS) { FDIVS(context, inst); }
EXPORT_SEMANTIC(FDIVS);

void SEMANTIC(FSUBS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(context.fpr[inst.fra] - context.fpr[inst.frb]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FSUBS) { FSUBS(context, inst); }
EXPORT_SEMANTIC(FSUBS);

void SEMANTIC(FADDS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(context.fpr[inst.fra] + context.fpr[inst.frb]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FADDS) { FADDS(context, inst); }
EXPORT_SEMANTIC(FADDS);

void SEMANTIC(FSQRTS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(std::sqrt(context.fpr[inst.frb]));
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FSQRTS) { FSQRTS(context, inst); }
EXPORT_SEMANTIC(FSQRTS);

void SEMANTIC(FRES)(PPUContext &context, Instruction inst) {
  const f64 a = context.fpr[inst.frb];
  const u64 b = std::bit_cast<u64>(a);
  const u64 e = (b >> 52) & 0x7ff; // double exp
  const u64 i = (b >> 45) & 0x7f;  // mantissa LUT index
  const u64 r = e >= (0x3ff + 0x80)
                    ? 0
                    : (0x7ff - 2 - e) << 52 | u64{ppu_fres_mantissas[i]}
                                                  << (32 - 3);

  context.fpr[inst.frd] = f32(std::bit_cast<f64>(
      a == a ? (b & 0x8000'0000'0000'0000) | r : (0x8'0000'0000'0000 | b)));
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FRES) { FRES(context, inst); }
EXPORT_SEMANTIC(FRES);

void SEMANTIC(FMULS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(context.fpr[inst.fra] * context.fpr[inst.frc]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMULS) { FMULS(context, inst); }
EXPORT_SEMANTIC(FMULS);

void SEMANTIC(FMADDS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], context.fpr[inst.frb]));

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMADDS) { FMADDS(context, inst); }
EXPORT_SEMANTIC(FMADDS);

void SEMANTIC(FMSUBS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], -context.fpr[inst.frb]));

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMSUBS) { FMSUBS(context, inst); }
EXPORT_SEMANTIC(FMSUBS);

void SEMANTIC(FNMSUBS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(-std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], -context.fpr[inst.frb]));

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FNMSUBS) { FNMSUBS(context, inst); }
EXPORT_SEMANTIC(FNMSUBS);

void SEMANTIC(FNMADDS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(-std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], context.fpr[inst.frb]));

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FNMADDS) { FNMADDS(context, inst); }
EXPORT_SEMANTIC(FNMADDS);

void SEMANTIC(MTFSB1)(PPUContext &context, Instruction inst) {
  const u32 bit = inst.crbd;
  context.fpscr.bits[bit] = 1;
  context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl, context.fpscr.fe,
                           context.fpscr.fu);
}
void DECODER(MTFSB1) { MTFSB1(context, inst); }
EXPORT_SEMANTIC(MTFSB1);

void SEMANTIC(MCRFS)(PPUContext &context, Instruction inst) {
  std::memcpy(context.cr.fields + inst.crfd, context.fpscr.fields + inst.crfs,
              sizeof(u32));
}
void DECODER(MCRFS) { MCRFS(context, inst); }
EXPORT_SEMANTIC(MCRFS);

void SEMANTIC(MTFSB0)(PPUContext &context, Instruction inst) {
  const u32 bit = inst.crbd;
  // if (bit < 16 || bit > 19)
  //   ppu_log.warning("MTFSB0(%d)", bit);
  context.fpscr.bits[bit] = 0;
  context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl, context.fpscr.fe,
                           context.fpscr.fu);
}
void DECODER(MTFSB0) { MTFSB0(context, inst); }
EXPORT_SEMANTIC(MTFSB0);

void SEMANTIC(MTFSFI)(PPUContext &context, Instruction inst) {
  const u32 bf = inst.crfd;

  if (bf != 4) {
    // Do nothing on non-FPCC field (TODO)
    // ppu_log.warning("MTFSFI(%d)", inst.crfd);
  } else {
    static constexpr auto all_values = [] {
      std::array<CrField, 16> values{};

      for (u32 i = 0; i < values.size(); i++) {
        u32 value = 0, im = i;
        value |= (im & 1) << (8 * 3);
        im >>= 1;
        value |= (im & 1) << (8 * 2);
        im >>= 1;
        value |= (im & 1) << (8 * 1);
        im >>= 1;
        value |= (im & 1) << (8 * 0);
        values[i] = std::bit_cast<CrField>(value);
      }

      return values;
    }();

    context.fpscr.fields[bf] = all_values[inst.i];
  }

  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(MTFSFI) { MTFSFI(context, inst); }
EXPORT_SEMANTIC(MTFSFI);

void SEMANTIC(MFFS)(PPUContext &context, Instruction inst) {
  // ppu_log.warning("MFFS");
  context.fpr[inst.frd] = std::bit_cast<f64>(
      u64{context.fpscr.fl} << 15 | u64{context.fpscr.fg} << 14 |
      u64{context.fpscr.fe} << 13 | u64{context.fpscr.fu} << 12);
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(MFFS) { MFFS(context, inst); }
EXPORT_SEMANTIC(MFFS);

void SEMANTIC(MTFSF)(PPUContext &context, Instruction inst) {
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(MTFSF) { MTFSF(context, inst); }
EXPORT_SEMANTIC(MTFSF);

void SEMANTIC(FCMPU)(PPUContext &context, Instruction inst) {
  const f64 a = context.fpr[inst.fra];
  const f64 b = context.fpr[inst.frb];
  ppu_set_fpcc(context, true, a, b, inst.crfd);
}
void DECODER(FCMPU) { FCMPU(context, inst); }
EXPORT_SEMANTIC(FCMPU);

void SEMANTIC(FCTIW)(PPUContext &context, Instruction inst, f64 &d, f64 b) {
#if defined(ARCH_X64)
  const auto val = _mm_set_sd(b);
  const auto res = _mm_xor_si128(
      _mm_cvtpd_epi32(val),
      _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(0x80000000))));
  d = std::bit_cast<f64, s64>(_mm_cvtsi128_si32(res));
#elif defined(ARCH_ARM64)
  d = std::bit_cast<f64, s64>(!(b == b)
                                  ? INT32_MIN
                                  : vqmovnd_s64(std::bit_cast<f64>(vrndi_f64(
                                        std::bit_cast<float64x1_t>(b)))));
#endif
  ppu_set_fpcc(context, inst.rc, 0., 0.); // undefined (TODO)
}
void DECODER(FCTIW) {
  FCTIW(context, inst, context.fpr[inst.frd], context.fpr[inst.frb]);
}
EXPORT_SEMANTIC(FCTIW);

void SEMANTIC(FCTIWZ)(PPUContext &context, Instruction inst, f64 &d, f64 b) {
#if defined(ARCH_X64)
  const auto val = _mm_set_sd(b);
  const auto res = _mm_xor_si128(
      _mm_cvttpd_epi32(val),
      _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(0x80000000))));
  d = std::bit_cast<f64, s64>(_mm_cvtsi128_si32(res));
#elif defined(ARCH_ARM64)
  d = std::bit_cast<f64, s64>(!(b == b)
                                  ? INT32_MIN
                                  : vqmovnd_s64(std::bit_cast<s64>(vcvt_s64_f64(
                                        std::bit_cast<float64x1_t>(b)))));
#endif
  ppu_set_fpcc(context, inst.rc, 0., 0.); // undefined (TODO)
}
void DECODER(FCTIWZ) {
  FCTIWZ(context, inst, context.fpr[inst.frd], context.fpr[inst.frb]);
}
EXPORT_SEMANTIC(FCTIWZ);

void SEMANTIC(FRSP)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = f32(context.fpr[inst.frb]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FRSP) { FRSP(context, inst); }
EXPORT_SEMANTIC(FRSP);

void SEMANTIC(FDIV)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.fra] / context.fpr[inst.frb];
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FDIV) { FDIV(context, inst); }
EXPORT_SEMANTIC(FDIV);

void SEMANTIC(FSUB)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.fra] - context.fpr[inst.frb];
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FSUB) { FSUB(context, inst); }
EXPORT_SEMANTIC(FSUB);

void SEMANTIC(FADD)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.fra] + context.fpr[inst.frb];
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FADD) { FADD(context, inst); }
EXPORT_SEMANTIC(FADD);

void SEMANTIC(FSQRT)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = std::sqrt(context.fpr[inst.frb]);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FSQRT) { FSQRT(context, inst); }
EXPORT_SEMANTIC(FSQRT);

void SEMANTIC(FSEL)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.fra] >= 0.0 ? context.fpr[inst.frc]
                                                       : context.fpr[inst.frb];
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(FSEL) { FSEL(context, inst); }
EXPORT_SEMANTIC(FSEL);

void SEMANTIC(FMUL)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.fra] * context.fpr[inst.frc];
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMUL) { FMUL(context, inst); }
EXPORT_SEMANTIC(FMUL);

void SEMANTIC(FRSQRTE)(PPUContext &context, Instruction inst) {
  const u64 b = std::bit_cast<u64>(context.fpr[inst.frb]);
  context.fpr[inst.frd] =
      std::bit_cast<f64>(u64{ppu_frqrte_lut.data[b >> 49]} << 32);
  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FRSQRTE) { FRSQRTE(context, inst); }
EXPORT_SEMANTIC(FRSQRTE);

void SEMANTIC(FMSUB)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = std::fma(context.fpr[inst.fra], context.fpr[inst.frc],
                                   -context.fpr[inst.frb]);

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMSUB) { FMSUB(context, inst); }
EXPORT_SEMANTIC(FMSUB);

void SEMANTIC(FMADD)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = std::fma(context.fpr[inst.fra], context.fpr[inst.frc],
                                   context.fpr[inst.frb]);

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FMADD) { FMADD(context, inst); }
EXPORT_SEMANTIC(FMADD);

void SEMANTIC(FNMSUB)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = -std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], -context.fpr[inst.frb]);

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FNMSUB) { FNMSUB(context, inst); }
EXPORT_SEMANTIC(FNMSUB);

void SEMANTIC(FNMADD)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = -std::fma(
      context.fpr[inst.fra], context.fpr[inst.frc], context.fpr[inst.frb]);

  ppu_set_fpcc(context, inst.rc, context.fpr[inst.frd], 0.);
}
void DECODER(FNMADD) { FNMADD(context, inst); }
EXPORT_SEMANTIC(FNMADD);

void SEMANTIC(FCMPO)(PPUContext &context, Instruction inst) {
  const f64 a = context.fpr[inst.fra];
  const f64 b = context.fpr[inst.frb];
  ppu_set_fpcc(context, true, a, b, inst.crfd);
}
void DECODER(FCMPO) { FCMPO(context, inst); }
EXPORT_SEMANTIC(FCMPO);

void SEMANTIC(FNEG)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = -context.fpr[inst.frb];
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(FNEG) { FNEG(context, inst); }
EXPORT_SEMANTIC(FNEG);

void SEMANTIC(FMR)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = context.fpr[inst.frb];
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(FMR) { FMR(context, inst); }
EXPORT_SEMANTIC(FMR);

void SEMANTIC(FNABS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = -std::fabs(context.fpr[inst.frb]);
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(FNABS) { FNABS(context, inst); }
EXPORT_SEMANTIC(FNABS);

void SEMANTIC(FABS)(PPUContext &context, Instruction inst) {
  context.fpr[inst.frd] = std::fabs(context.fpr[inst.frb]);
  if (inst.rc) {
    context.cr.fields[1].set(context.fpscr.fg, context.fpscr.fl,
                             context.fpscr.fe, context.fpscr.fu);
  }
}
void DECODER(FABS) { FABS(context, inst); }
EXPORT_SEMANTIC(FABS);

void SEMANTIC(FCTID)(PPUContext &context, Instruction inst, f64 &d, f64 b) {
#if defined(ARCH_X64)
  const auto val = _mm_set_sd(b);
  const auto res = _mm_xor_si128(
      _mm_set1_epi64x(_mm_cvtsd_si64(val)),
      _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(f64(1ull << 63)))));
  d = std::bit_cast<f64>(_mm_cvtsi128_si64(res));
#elif defined(ARCH_ARM64)
  d = std::bit_cast<f64, s64>(
      !(b == b) ? f64{INT64_MIN}
                : std::bit_cast<f64>(vrndi_f64(std::bit_cast<float64x1_t>(b))));
#endif
  ppu_set_fpcc(context, inst.rc, 0., 0.); // undefined (TODO)
}
void DECODER(FCTID) {
  FCTID(context, inst, context.fpr[inst.frd], context.fpr[inst.frb]);
}
EXPORT_SEMANTIC(FCTID);

void SEMANTIC(FCTIDZ)(PPUContext &context, Instruction inst, f64 &d, f64 b) {
#if defined(ARCH_X64)
  const auto val = _mm_set_sd(b);
  const auto res = _mm_xor_si128(
      _mm_set1_epi64x(_mm_cvttsd_si64(val)),
      _mm_castpd_si128(_mm_cmpge_pd(val, _mm_set1_pd(f64(1ull << 63)))));
  d = std::bit_cast<f64>(_mm_cvtsi128_si64(res));
#elif defined(ARCH_ARM64)
  d = std::bit_cast<f64>(!(b == b)
                             ? int64x1_t{INT64_MIN}
                             : vcvt_s64_f64(std::bit_cast<float64x1_t>(b)));
#endif
  ppu_set_fpcc(context, inst.rc, 0., 0.); // undefined (TODO)
}
void DECODER(FCTIDZ) {
  FCTIDZ(context, inst, context.fpr[inst.frd], context.fpr[inst.frb]);
}
EXPORT_SEMANTIC(FCTIDZ);

void SEMANTIC(FCFID)(PPUContext &context, Instruction inst, f64 &d, f64 b) {
  f64 r = static_cast<f64>(std::bit_cast<s64>(b));
  d = r;
  ppu_set_fpcc(context, inst.rc, r, 0.);
}
void DECODER(FCFID) {
  FCFID(context, inst, context.fpr[inst.frd], context.fpr[inst.frb]);
}
EXPORT_SEMANTIC(FCFID);

void SEMANTIC(RFID)() { rpcsx_unimplemented_instruction(); }
void DECODER(RFID) { RFID(); }
EXPORT_SEMANTIC(RFID);

void SEMANTIC(RFSCV)() { rpcsx_unimplemented_instruction(); }
void DECODER(RFSCV) { RFSCV(); }
EXPORT_SEMANTIC(RFSCV);

void SEMANTIC(HRFID)() { rpcsx_unimplemented_instruction(); }
void DECODER(HRFID) { HRFID(); }
EXPORT_SEMANTIC(HRFID);

void SEMANTIC(STOP)() { rpcsx_unimplemented_instruction(); }
void DECODER(STOP) { STOP(); }
EXPORT_SEMANTIC(STOP);

void SEMANTIC(URFID)() { rpcsx_unimplemented_instruction(); }
void DECODER(URFID) { URFID(); }
EXPORT_SEMANTIC(URFID);

void SEMANTIC(SUBFCO)() { rpcsx_unimplemented_instruction(); }
void DECODER(SUBFCO) { SUBFCO(); }
EXPORT_SEMANTIC(SUBFCO);

void SEMANTIC(ADDCO)() { rpcsx_unimplemented_instruction(); }
void DECODER(ADDCO) { ADDCO(); }
EXPORT_SEMANTIC(ADDCO);

void SEMANTIC(UNK)() { rpcsx_unimplemented_instruction(); }
void DECODER(UNK) { UNK(); }
EXPORT_SEMANTIC(UNK);

void SEMANTIC(SUBFEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(SUBFEO) { SUBFEO(); }
EXPORT_SEMANTIC(SUBFEO);

void SEMANTIC(ADDEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(ADDEO) { ADDEO(); }
EXPORT_SEMANTIC(ADDEO);

void SEMANTIC(SUBFO)() { rpcsx_unimplemented_instruction(); }
void DECODER(SUBFO) { SUBFO(); }
EXPORT_SEMANTIC(SUBFO);

void SEMANTIC(NEGO)() { rpcsx_unimplemented_instruction(); }
void DECODER(NEGO) { NEGO(); }
EXPORT_SEMANTIC(NEGO);

void SEMANTIC(SUBFMEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(SUBFMEO) { SUBFMEO(); }
EXPORT_SEMANTIC(SUBFMEO);

void SEMANTIC(MULLDO)() { rpcsx_unimplemented_instruction(); }
void DECODER(MULLDO) { MULLDO(); }
EXPORT_SEMANTIC(MULLDO);

void SEMANTIC(SUBFZEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(SUBFZEO) { SUBFZEO(); }
EXPORT_SEMANTIC(SUBFZEO);

void SEMANTIC(ADDZEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(ADDZEO) { ADDZEO(); }
EXPORT_SEMANTIC(ADDZEO);

void SEMANTIC(ADDO)() { rpcsx_unimplemented_instruction(); }
void DECODER(ADDO) { ADDO(); }
EXPORT_SEMANTIC(ADDO);

void SEMANTIC(DIVDUO)() { rpcsx_unimplemented_instruction(); }
void DECODER(DIVDUO) { DIVDUO(); }
EXPORT_SEMANTIC(DIVDUO);

void SEMANTIC(ADDMEO)() { rpcsx_unimplemented_instruction(); }
void DECODER(ADDMEO) { ADDMEO(); }
EXPORT_SEMANTIC(ADDMEO);

void SEMANTIC(MULLWO)() { rpcsx_unimplemented_instruction(); }
void DECODER(MULLWO) { MULLWO(); }
EXPORT_SEMANTIC(MULLWO);

void SEMANTIC(DIVWO)() { rpcsx_unimplemented_instruction(); }
void DECODER(DIVWO) { DIVWO(); }
EXPORT_SEMANTIC(DIVWO);

void SEMANTIC(DIVWUO)() { rpcsx_unimplemented_instruction(); }
void DECODER(DIVWUO) { DIVWUO(); }
EXPORT_SEMANTIC(DIVWUO);

void SEMANTIC(DIVDO)() { rpcsx_unimplemented_instruction(); }
void DECODER(DIVDO) { DIVDO(); }
EXPORT_SEMANTIC(DIVDO);
