#include "Decoder.hpp"
#include "Instruction.hpp"
#include "Opcode.hpp"
#include <bit>
#include <cstdint>

struct InstructionEncodingInfo {
  std::uint32_t value;
  rx::cell::ppu::Opcode opcode;
  rx::cell::ppu::Opcode rcOpcode;
  std::uint32_t magn = 0;

  constexpr InstructionEncodingInfo(std::uint32_t value,
                                    rx::cell::ppu::Opcode opcode,
                                    rx::cell::ppu::Opcode rcOpcode)
      : value(value), opcode(opcode), rcOpcode(rcOpcode) {}

  constexpr InstructionEncodingInfo(std::uint32_t value,
                                    rx::cell::ppu::Opcode opcode,
                                    rx::cell::ppu::Opcode rcOpcode,
                                    std::uint32_t magn)
      : value(value), opcode(opcode), rcOpcode(rcOpcode), magn(magn) {}
};

static constexpr rx::cell::ppu::DecoderTable<rx::cell::ppu::Opcode>
buildOpcodeTable() {
  // Main opcodes (field 0..5)
  rx::cell::ppu::DecoderTable<rx::cell::ppu::Opcode> result;
  result.fill(rx::cell::ppu::Opcode::Invalid);

  auto fill_table =
      [&](std::uint32_t main_op, std::uint32_t count, std::uint32_t sh,
          std::initializer_list<InstructionEncodingInfo> entries) noexcept {
        if (sh < 11) {
          for (const auto &v : entries) {
            for (std::uint32_t i = 0; i < 1u << (v.magn + (11 - sh - count));
                 i++) {
              for (std::uint32_t j = 0; j < 1u << sh; j++) {
                const std::uint32_t k =
                    (((i << (count - v.magn)) | v.value) << sh) | j;
                result[(k << 6) | main_op] = i & 1 ? v.rcOpcode : v.opcode;
              }
            }
          }
        } else {
          // Main table (special case)
          for (const auto &v : entries) {
            for (std::uint32_t i = 0; i < 1u << 11; i++) {
              result[i << 6 | v.value] = i & 1 ? v.rcOpcode : v.opcode;
            }
          }
        }
      };

#define GET(name) rx::cell::ppu::Opcode::name, rx::cell::ppu::Opcode::name
#define GETRC(name) rx::cell::ppu::Opcode::name, rx::cell::ppu::Opcode::name##_

  fill_table(
      0x00, 6, -1,
      {
          {0x02, GET(TDI)},     {0x03, GET(TWI)},      {0x07, GET(MULLI)},
          {0x08, GET(SUBFIC)},  {0x0a, GET(CMPLI)},    {0x0b, GET(CMPI)},
          {0x0c, GET(ADDIC)},   {0x0d, GET(ADDIC)},    {0x0e, GET(ADDI)},
          {0x0f, GET(ADDIS)},   {0x10, GET(BC)},       {0x11, GET(SC)},
          {0x12, GET(B)},       {0x14, GETRC(RLWIMI)}, {0x15, GETRC(RLWINM)},
          {0x17, GETRC(RLWNM)}, {0x18, GET(ORI)},      {0x19, GET(ORIS)},
          {0x1a, GET(XORI)},    {0x1b, GET(XORIS)},    {0x1c, GET(ANDI)},
          {0x1d, GET(ANDIS)},   {0x20, GET(LWZ)},      {0x21, GET(LWZU)},
          {0x22, GET(LBZ)},     {0x23, GET(LBZU)},     {0x24, GET(STW)},
          {0x25, GET(STWU)},    {0x26, GET(STB)},      {0x27, GET(STBU)},
          {0x28, GET(LHZ)},     {0x29, GET(LHZU)},     {0x2a, GET(LHA)},
          {0x2b, GET(LHAU)},    {0x2c, GET(STH)},      {0x2d, GET(STHU)},
          {0x2e, GET(LMW)},     {0x2f, GET(STMW)},     {0x30, GET(LFS)},
          {0x31, GET(LFSU)},    {0x32, GET(LFD)},      {0x33, GET(LFDU)},
          {0x34, GET(STFS)},    {0x35, GET(STFSU)},    {0x36, GET(STFD)},
          {0x37, GET(STFDU)},
      });

  // Group 0x04 opcodes (field 21..31)
  fill_table(0x04, 11, 0,
             {
                 {0x0, GET(VADDUBM)},       {0x2, GET(VMAXUB)},
                 {0x4, GET(VRLB)},          {0x006, GET(VCMPEQUB)},
                 {0x406, GET(VCMPEQUB_)},   {0x8, GET(VMULOUB)},
                 {0xa, GET(VADDFP)},        {0xc, GET(VMRGHB)},
                 {0xe, GET(VPKUHUM)},

                 {0x20, GET(VMHADDSHS), 5}, {0x21, GET(VMHRADDSHS), 5},
                 {0x22, GET(VMLADDUHM), 5}, {0x24, GET(VMSUMUBM), 5},
                 {0x25, GET(VMSUMMBM), 5},  {0x26, GET(VMSUMUHM), 5},
                 {0x27, GET(VMSUMUHS), 5},  {0x28, GET(VMSUMSHM), 5},
                 {0x29, GET(VMSUMSHS), 5},  {0x2a, GET(VSEL), 5},
                 {0x2b, GET(VPERM), 5},     {0x2c, GET(VSLDOI), 5},
                 {0x2e, GET(VMADDFP), 5},   {0x2f, GET(VNMSUBFP), 5},

                 {0x40, GET(VADDUHM)},      {0x42, GET(VMAXUH)},
                 {0x44, GET(VRLH)},         {0x046, GET(VCMPEQUH)},
                 {0x446, GET(VCMPEQUH_)},   {0x48, GET(VMULOUH)},
                 {0x4a, GET(VSUBFP)},       {0x4c, GET(VMRGHH)},
                 {0x4e, GET(VPKUWUM)},      {0x80, GET(VADDUWM)},
                 {0x82, GET(VMAXUW)},       {0x84, GET(VRLW)},
                 {0x086, GET(VCMPEQUW)},    {0x486, GET(VCMPEQUW_)},
                 {0x8c, GET(VMRGHW)},       {0x8e, GET(VPKUHUS)},
                 {0x0c6, GET(VCMPEQFP)},    {0x4c6, GET(VCMPEQFP_)},
                 {0xce, GET(VPKUWUS)},

                 {0x102, GET(VMAXSB)},      {0x104, GET(VSLB)},
                 {0x108, GET(VMULOSB)},     {0x10a, GET(VREFP)},
                 {0x10c, GET(VMRGLB)},      {0x10e, GET(VPKSHUS)},
                 {0x142, GET(VMAXSH)},      {0x144, GET(VSLH)},
                 {0x148, GET(VMULOSH)},     {0x14a, GET(VRSQRTEFP)},
                 {0x14c, GET(VMRGLH)},      {0x14e, GET(VPKSWUS)},
                 {0x180, GET(VADDCUW)},     {0x182, GET(VMAXSW)},
                 {0x184, GET(VSLW)},        {0x18a, GET(VEXPTEFP)},
                 {0x18c, GET(VMRGLW)},      {0x18e, GET(VPKSHSS)},
                 {0x1c4, GET(VSL)},         {0x1c6, GET(VCMPGEFP)},
                 {0x5c6, GET(VCMPGEFP_)},   {0x1ca, GET(VLOGEFP)},
                 {0x1ce, GET(VPKSWSS)},     {0x200, GET(VADDUBS)},
                 {0x202, GET(VMINUB)},      {0x204, GET(VSRB)},
                 {0x206, GET(VCMPGTUB)},    {0x606, GET(VCMPGTUB_)},
                 {0x208, GET(VMULEUB)},     {0x20a, GET(VRFIN)},
                 {0x20c, GET(VSPLTB)},      {0x20e, GET(VUPKHSB)},
                 {0x240, GET(VADDUHS)},     {0x242, GET(VMINUH)},
                 {0x244, GET(VSRH)},        {0x246, GET(VCMPGTUH)},
                 {0x646, GET(VCMPGTUH_)},   {0x248, GET(VMULEUH)},
                 {0x24a, GET(VRFIZ)},       {0x24c, GET(VSPLTH)},
                 {0x24e, GET(VUPKHSH)},     {0x280, GET(VADDUWS)},
                 {0x282, GET(VMINUW)},      {0x284, GET(VSRW)},
                 {0x286, GET(VCMPGTUW)},    {0x686, GET(VCMPGTUW_)},
                 {0x28a, GET(VRFIP)},       {0x28c, GET(VSPLTW)},
                 {0x28e, GET(VUPKLSB)},     {0x2c4, GET(VSR)},
                 {0x2c6, GET(VCMPGTFP)},    {0x6c6, GET(VCMPGTFP_)},
                 {0x2ca, GET(VRFIM)},       {0x2ce, GET(VUPKLSH)},
                 {0x300, GET(VADDSBS)},     {0x302, GET(VMINSB)},
                 {0x304, GET(VSRAB)},       {0x306, GET(VCMPGTSB)},
                 {0x706, GET(VCMPGTSB_)},   {0x308, GET(VMULESB)},
                 {0x30a, GET(VCFUX)},       {0x30c, GET(VSPLTISB)},
                 {0x30e, GET(VPKPX)},       {0x340, GET(VADDSHS)},
                 {0x342, GET(VMINSH)},      {0x344, GET(VSRAH)},
                 {0x346, GET(VCMPGTSH)},    {0x746, GET(VCMPGTSH_)},
                 {0x348, GET(VMULESH)},     {0x34a, GET(VCFSX)},
                 {0x34c, GET(VSPLTISH)},    {0x34e, GET(VUPKHPX)},
                 {0x380, GET(VADDSWS)},     {0x382, GET(VMINSW)},
                 {0x384, GET(VSRAW)},       {0x386, GET(VCMPGTSW)},
                 {0x786, GET(VCMPGTSW_)},   {0x38a, GET(VCTUXS)},
                 {0x38c, GET(VSPLTISW)},    {0x3c6, GET(VCMPBFP)},
                 {0x7c6, GET(VCMPBFP_)},    {0x3ca, GET(VCTSXS)},
                 {0x3ce, GET(VUPKLPX)},     {0x400, GET(VSUBUBM)},
                 {0x402, GET(VAVGUB)},      {0x404, GET(VAND)},
                 {0x40a, GET(VMAXFP)},      {0x40c, GET(VSLO)},
                 {0x440, GET(VSUBUHM)},     {0x442, GET(VAVGUH)},
                 {0x444, GET(VANDC)},       {0x44a, GET(VMINFP)},
                 {0x44c, GET(VSRO)},        {0x480, GET(VSUBUWM)},
                 {0x482, GET(VAVGUW)},      {0x484, GET(VOR)},
                 {0x4c4, GET(VXOR)},        {0x502, GET(VAVGSB)},
                 {0x504, GET(VNOR)},        {0x542, GET(VAVGSH)},
                 {0x580, GET(VSUBCUW)},     {0x582, GET(VAVGSW)},
                 {0x600, GET(VSUBUBS)},     {0x604, GET(MFVSCR)},
                 {0x608, GET(VSUM4UBS)},    {0x640, GET(VSUBUHS)},
                 {0x644, GET(MTVSCR)},      {0x648, GET(VSUM4SHS)},
                 {0x680, GET(VSUBUWS)},     {0x688, GET(VSUM2SWS)},
                 {0x700, GET(VSUBSBS)},     {0x708, GET(VSUM4SBS)},
                 {0x740, GET(VSUBSHS)},     {0x780, GET(VSUBSWS)},
                 {0x788, GET(VSUMSWS)},
             });

  // Group 0x13 opcodes (field 21..30)
  fill_table(0x13, 10, 1,
             {
                 {0x000, GET(MCRF)},
                 {0x010, GET(BCLR)},
                 {0x012, GET(RFID)},
                 {0x021, GET(CRNOR)},
                 {0x052, GET(RFSCV)},
                 {0x081, GET(CRANDC)},
                 {0x096, GET(ISYNC)},
                 {0x0c1, GET(CRXOR)},
                 {0x0e1, GET(CRNAND)},
                 {0x101, GET(CRAND)},
                 {0x112, GET(HRFID)},
                 {0x121, GET(CREQV)},
                 {0x132, GET(URFID)},
                 {0x172, GET(STOP)},
                 {0x1a1, GET(CRORC)},
                 {0x1c1, GET(CROR)},
                 {0x210, GET(BCCTR)},
             });

  // Group 0x1e opcodes (field 27..30)
  fill_table(0x1e, 4, 1,
             {
                 {0x0, GETRC(RLDICL)},
                 {0x1, GETRC(RLDICL)},
                 {0x2, GETRC(RLDICR)},
                 {0x3, GETRC(RLDICR)},
                 {0x4, GETRC(RLDIC)},
                 {0x5, GETRC(RLDIC)},
                 {0x6, GETRC(RLDIMI)},
                 {0x7, GETRC(RLDIMI)},
                 {0x8, GETRC(RLDCL)},
                 {0x9, GETRC(RLDCR)},
             });

  // Group 0x1f opcodes (field 21..30)
  fill_table(0x1f, 10, 1,
             {
                 {0x000, GET(CMP)},       {0x004, GET(TW)},
                 {0x006, GET(LVSL)},      {0x007, GET(LVEBX)},
                 {0x008, GETRC(SUBFC)},   {0x208, GETRC(SUBFCO)},
                 {0x009, GETRC(MULHDU)},  {0x00a, GETRC(ADDC)},
                 {0x20a, GETRC(ADDCO)},   {0x00b, GETRC(MULHWU)},
                 {0x013, GET(MFOCRF)},    {0x014, GET(LWARX)},
                 {0x015, GET(LDX)},       {0x017, GET(LWZX)},
                 {0x018, GETRC(SLW)},     {0x01a, GETRC(CNTLZW)},
                 {0x01b, GETRC(SLD)},     {0x01c, GETRC(AND)},
                 {0x020, GET(CMPL)},      {0x026, GET(LVSR)},
                 {0x027, GET(LVEHX)},     {0x028, GETRC(SUBF)},
                 {0x228, GETRC(SUBFO)},   {0x035, GET(LDUX)},
                 {0x036, GET(DCBST)},     {0x037, GET(LWZUX)},
                 {0x03a, GETRC(CNTLZD)},  {0x03c, GETRC(ANDC)},
                 {0x044, GET(TD)},        {0x047, GET(LVEWX)},
                 {0x049, GETRC(MULHD)},   {0x04b, GETRC(MULHW)},
                 {0x054, GET(LDARX)},     {0x056, GET(DCBF)},
                 {0x057, GET(LBZX)},      {0x067, GET(LVX)},
                 {0x068, GETRC(NEG)},     {0x268, GETRC(NEGO)},
                 {0x077, GET(LBZUX)},     {0x07c, GETRC(NOR)},
                 {0x087, GET(STVEBX)},    {0x088, GETRC(SUBFE)},
                 {0x288, GETRC(SUBFEO)},  {0x08a, GETRC(ADDE)},
                 {0x28a, GETRC(ADDEO)},   {0x090, GET(MTOCRF)},
                 {0x095, GET(STDX)},      {0x096, GET(STWCX)},
                 {0x097, GET(STWX)},      {0x0a7, GET(STVEHX)},
                 {0x0b5, GET(STDUX)},     {0x0b7, GET(STWUX)},
                 {0x0c7, GET(STVEWX)},    {0x0c8, GETRC(SUBFZE)},
                 {0x2c8, GETRC(SUBFZEO)}, {0x0ca, GETRC(ADDZE)},
                 {0x2ca, GETRC(ADDZEO)},  {0x0d6, GET(STDCX)},
                 {0x0d7, GET(STBX)},      {0x0e7, GET(STVX)},
                 {0x0e8, GETRC(SUBFME)},  {0x2e8, GETRC(SUBFMEO)},
                 {0x0e9, GETRC(MULLD)},   {0x2e9, GETRC(MULLDO)},
                 {0x0ea, GETRC(ADDME)},   {0x2ea, GETRC(ADDMEO)},
                 {0x0eb, GETRC(MULLW)},   {0x2eb, GETRC(MULLWO)},
                 {0x0f6, GET(DCBTST)},    {0x0f7, GET(STBUX)},
                 {0x10a, GETRC(ADD)},     {0x30a, GETRC(ADDO)},
                 {0x116, GET(DCBT)},      {0x117, GET(LHZX)},
                 {0x11c, GETRC(EQV)},     {0x136, GET(ECIWX)},
                 {0x137, GET(LHZUX)},     {0x13c, GETRC(XOR)},
                 {0x153, GET(MFSPR)},     {0x155, GET(LWAX)},
                 {0x156, GET(DST)},       {0x157, GET(LHAX)},
                 {0x167, GET(LVXL)},      {0x173, GET(MFTB)},
                 {0x175, GET(LWAUX)},     {0x176, GET(DSTST)},
                 {0x177, GET(LHAUX)},     {0x197, GET(STHX)},
                 {0x19c, GETRC(ORC)},     {0x1b6, GET(ECOWX)},
                 {0x1b7, GET(STHUX)},     {0x1bc, GETRC(OR)},
                 {0x1c9, GETRC(DIVDU)},   {0x3c9, GETRC(DIVDUO)},
                 {0x1cb, GETRC(DIVWU)},   {0x3cb, GETRC(DIVWUO)},
                 {0x1d3, GET(MTSPR)},     {0x1d6, GET(DCBI)},
                 {0x1dc, GETRC(NAND)},    {0x1e7, GET(STVXL)},
                 {0x1e9, GETRC(DIVD)},    {0x3e9, GETRC(DIVDO)},
                 {0x1eb, GETRC(DIVW)},    {0x3eb, GETRC(DIVWO)},
                 {0x207, GET(LVLX)},      {0x214, GET(LDBRX)},
                 {0x215, GET(LSWX)},      {0x216, GET(LWBRX)},
                 {0x217, GET(LFSX)},      {0x218, GETRC(SRW)},
                 {0x21b, GETRC(SRD)},     {0x227, GET(LVRX)},
                 {0x237, GET(LFSUX)},     {0x255, GET(LSWI)},
                 {0x256, GET(SYNC)},      {0x257, GET(LFDX)},
                 {0x277, GET(LFDUX)},     {0x287, GET(STVLX)},
                 {0x294, GET(STDBRX)},    {0x295, GET(STSWX)},
                 {0x296, GET(STWBRX)},    {0x297, GET(STFSX)},
                 {0x2a7, GET(STVRX)},     {0x2b7, GET(STFSUX)},
                 {0x2d5, GET(STSWI)},     {0x2d7, GET(STFDX)},
                 {0x2f7, GET(STFDUX)},    {0x307, GET(LVLXL)},
                 {0x316, GET(LHBRX)},     {0x318, GETRC(SRAW)},
                 {0x31a, GETRC(SRAD)},    {0x327, GET(LVRXL)},
                 {0x336, GET(DSS)},       {0x338, GETRC(SRAWI)},
                 {0x33a, GETRC(SRADI)},   {0x33b, GETRC(SRADI)},
                 {0x356, GET(EIEIO)},     {0x387, GET(STVLXL)},
                 {0x396, GET(STHBRX)},    {0x39a, GETRC(EXTSH)},
                 {0x3a7, GET(STVRXL)},    {0x3ba, GETRC(EXTSB)},
                 {0x3d7, GET(STFIWX)},    {0x3da, GETRC(EXTSW)},
                 {0x3d6, GET(ICBI)},      {0x3f6, GET(DCBZ)},
             });

  // Group 0x3a opcodes (field 30..31)
  fill_table(0x3a, 2, 0,
             {
                 {0x0, GET(LD)},
                 {0x1, GET(LDU)},
                 {0x2, GET(LWA)},
             });

  // Group 0x3b opcodes (field 21..30)
  fill_table(0x3b, 10, 1,
             {
                 {0x12, GETRC(FDIVS), 5},
                 {0x14, GETRC(FSUBS), 5},
                 {0x15, GETRC(FADDS), 5},
                 {0x16, GETRC(FSQRTS), 5},
                 {0x18, GETRC(FRES), 5},
                 {0x19, GETRC(FMULS), 5},
                 {0x1c, GETRC(FMSUBS), 5},
                 {0x1d, GETRC(FMADDS), 5},
                 {0x1e, GETRC(FNMSUBS), 5},
                 {0x1f, GETRC(FNMADDS), 5},
             });

  // Group 0x3e opcodes (field 30..31)
  fill_table(0x3e, 2, 0,
             {
                 {0x0, GET(STD)},
                 {0x1, GET(STDU)},
             });

  // Group 0x3f opcodes (field 21..30)
  fill_table(0x3f, 10, 1,
             {
                 {0x026, GETRC(MTFSB1)},     {0x040, GET(MCRFS)},
                 {0x046, GETRC(MTFSB0)},     {0x086, GETRC(MTFSFI)},
                 {0x247, GETRC(MFFS)},       {0x2c7, GETRC(MTFSF)},

                 {0x000, GET(FCMPU)},        {0x00c, GETRC(FRSP)},
                 {0x00e, GETRC(FCTIW)},      {0x00f, GETRC(FCTIWZ)},

                 {0x012, GETRC(FDIV), 5},    {0x014, GETRC(FSUB), 5},
                 {0x015, GETRC(FADD), 5},    {0x016, GETRC(FSQRT), 5},
                 {0x017, GETRC(FSEL), 5},    {0x019, GETRC(FMUL), 5},
                 {0x01a, GETRC(FRSQRTE), 5}, {0x01c, GETRC(FMSUB), 5},
                 {0x01d, GETRC(FMADD), 5},   {0x01e, GETRC(FNMSUB), 5},
                 {0x01f, GETRC(FNMADD), 5},

                 {0x020, GET(FCMPO)},        {0x028, GETRC(FNEG)},
                 {0x048, GETRC(FMR)},        {0x088, GETRC(FNABS)},
                 {0x108, GETRC(FABS)},       {0x32e, GETRC(FCTID)},
                 {0x32f, GETRC(FCTIDZ)},     {0x34e, GETRC(FCFID)},
             });

  return result;
}

rx::cell::ppu::DecoderTable<rx::cell::ppu::Opcode>
    rx::cell::ppu::g_ppuOpcodeTable = buildOpcodeTable();

rx::cell::ppu::Opcode rx::cell::ppu::fixOpcode(Opcode opcode,
                                               std::uint32_t instruction) {
  auto inst = std::bit_cast<Instruction>(instruction);

  if (opcode == Opcode::ADDI) {
    if (inst.ra == 0) {
      return Opcode::LI;
    }

    return opcode;
  }

  if (opcode == Opcode::ADDIS) {
    if (inst.ra == 0) {
      return Opcode::LIS;
    }

    return opcode;
  }

  if (opcode == Opcode::CRNOR) {
    if (inst.crba == inst.crbb) {
      return Opcode::CRNOT;
    }

    return opcode;
  }

  if (opcode == Opcode::B) {
    if (inst.aa && inst.lk) {
      return Opcode::BLA;
    } else if (inst.lk) {
      return Opcode::BL;
    } else if (inst.aa) {
      return Opcode::BA;
    }

    return opcode;
  }

  if (opcode == Opcode::ORI) {
    if (inst.rs == 0 && inst.ra == 0 && inst.uimm16 == 0) {
      return Opcode::NOP;
    }

    if (inst.uimm16 == 0) {
      return Opcode::MR;
    }

    return opcode;
  }

  if (opcode == Opcode::ORIS) {
    if (inst.rs == 0 && inst.ra == 0 && inst.uimm16 == 0) {
      return Opcode::NOP;
    }

    return opcode;
  }

  if (opcode == Opcode::RLDICL) {
    if (inst.sh64 == 0) {
      return Opcode::CLRLDI;
    }

    if (inst.mbe64 == 0) {
      return Opcode::ROTLDI;
    }

    if (inst.mbe64 == 64 - inst.sh64) {
      return Opcode::SRDI;
    }

    return opcode;
  }

  if (opcode == Opcode::CMP) {
    if (inst.l10) {
      return Opcode::CMPD;
    }
    return Opcode::CMPW;
  }

  if (opcode == Opcode::CMPL) {
    if (inst.l10) {
      return Opcode::CMPLD;
    }
    return Opcode::CMPLW;
  }

  if (opcode == Opcode::NOR) {
    if (inst.rs == inst.rb) {
      return Opcode::NOT;
    }

    return opcode;
  }

  if (opcode == Opcode::MTOCRF) {
    if (!inst.l10) {
      return Opcode::MTCRF;
    }

    return opcode;
  }

  if (opcode == Opcode::MFSPR) {
    auto n = (inst.spr >> 5) | ((inst.spr & 0x1f) << 5);

    switch (n) {
    case 1:
      return Opcode::MFXER;
    case 8:
      return Opcode::MFLR;
    case 9:
      return Opcode::MFCTR;
    }

    return opcode;
  }

  if (opcode == Opcode::MFTB) {
    auto n = (inst.spr >> 5) | ((inst.spr & 0x1f) << 5);

    switch (n) {
    case 268:
      return Opcode::MFTB;
    case 269:
      return Opcode::MFTBU;
    }

    return opcode;
  }

  if (opcode == Opcode::OR) {
    if (inst.rs == inst.rb) {
      switch (inst.raw) {
      case 0x7c210b78:
        return Opcode::CCTPL;
      case 0x7c421378:
        return Opcode::CCTPM;
      case 0x7c631b78:
        return Opcode::CCTPH;
      case 0x7f9ce378:
        return Opcode::DB8CYC;
      case 0x7fbdeb78:
        return Opcode::DB10CYC;
      case 0x7fdef378:
        return Opcode::DB12CYC;
      case 0x7ffffb78:
        return Opcode::DB16CYC;
      }

      return Opcode::MR;
    }

    return opcode;
  }

  return opcode;
}
