// Unit tests for shader::readGcnInst — decodes GCN (Sea Islands) machine code
// words into GcnInstruction. Encodings below are hand-assembled from the AMD
// "Sea Islands Series Instruction Set Architecture" reference; each test
// documents the field layout it exercises.

#include <shader/GcnInstruction.hpp>

#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <vector>

namespace {
int g_failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++g_failures;                                                            \
    }                                                                          \
  } while (false)

struct DecodeResult {
  shader::GcnInstruction inst;
  std::uint64_t consumedBytes;
};

DecodeResult decode(std::initializer_list<std::uint32_t> words,
                    std::uint64_t baseAddress = 0x1000) {
  std::vector<std::uint32_t> memory(words);
  DecodeResult result;
  std::uint64_t address = baseAddress;

  shader::readGcnInst(result.inst, address,
                      [&](std::uint64_t byteAddress) -> std::uint32_t {
                        auto index = (byteAddress - baseAddress) /
                                     sizeof(std::uint32_t);
                        CHECK(index < memory.size());
                        return index < memory.size() ? memory[index] : 0;
                      });

  result.consumedBytes = address - baseAddress;
  return result;
}

using shader::GcnOperand;
namespace ir = shader::ir;

// SOP1: [31:23]=0b101111101, sdst[22:16], op[15:8], ssrc0[7:0]
void testSop1MovB32() {
  // s_mov_b32 s0, s1
  auto [inst, size] = decode({0xBE800301});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Sop1);
  CHECK(inst.op == ir::sop1::MOV_B32);
  CHECK(inst.operandCount == 2);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(0).value == 0);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(1).value == 1);
  CHECK(inst.getOperand(1).access == GcnOperand::R);
}

// Scalar source field encodes inline constants: 128 -> 0, 129..192 -> 1..64
void testSop1MovInlineConstant() {
  // s_mov_b32 s5, 1  (ssrc0 = 129)
  auto [inst, size] = decode({0xBE850381});

  CHECK(size == 4);
  CHECK(inst.op == ir::sop1::MOV_B32);
  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(0).value == 5);
  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Constant);
  CHECK(inst.getOperand(1).value == 1);
}

// Scalar source 106 -> VCC_LO
void testSop1MovVccLo() {
  // s_mov_b32 s0, vcc_lo  (ssrc0 = 106)
  auto [inst, size] = decode({0xBE80036A});

  CHECK(inst.op == ir::sop1::MOV_B32);
  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::VccLo);
}

// SOPP: [31:23]=0b101111111, op[22:16], simm16[15:0]
void testSoppEndpgm() {
  // s_endpgm
  auto [inst, size] = decode({0xBF810000});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Sopp);
  CHECK(inst.op == ir::sopp::ENDPGM);
  CHECK(inst.operandCount == 1);
  // simm16 is exposed as a byte offset (<< 2)
  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Constant);
  CHECK(inst.getOperand(0).value == 0);
}

void testSoppBranchSimm() {
  // s_branch 3  (op 2, simm16 = 3 -> byte offset 12)
  auto [inst, size] = decode({0xBF820003});

  CHECK(inst.kind == ir::Kind::Sopp);
  CHECK(inst.op == ir::sopp::BRANCH);
  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Constant);
  CHECK(inst.getOperand(0).value == 12);
}

// SOP2: [31:30]=0b10, op[29:23], sdst[22:16], ssrc1[15:8], ssrc0[7:0]
void testSop2AddU32() {
  // s_add_u32 s0, s1, s2
  auto [inst, size] = decode({0x80000201});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Sop2);
  CHECK(inst.op == ir::sop2::ADD_U32);
  CHECK(inst.operandCount == 3);
  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(0).value == 0);
  CHECK(inst.getOperand(0).access == GcnOperand::W);
  CHECK(inst.getOperand(1).value == 1);
  CHECK(inst.getOperand(2).value == 2);
}

// VOP2: [31]=0, op[30:25], vdst[24:17], vsrc1[16:9], src0[8:0]
// src0 uses the scalar-source encoding where 256+n selects VGPR n.
void testVop2AddF32() {
  // v_add_f32 v2, v0, v1  (src0 = 256 -> v0)
  auto [inst, size] = decode({0x06040300});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Vop2);
  CHECK(inst.op == ir::vop2::ADD_F32);
  CHECK(inst.operandCount == 3);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(0).value == 2);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(1).value == 0);
  CHECK(inst.getOperand(1).access == GcnOperand::R);

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(2).value == 1);
  CHECK(inst.getOperand(2).access == GcnOperand::R);
}

// src0 = 255 selects a 32-bit literal that follows the instruction word.
void testVop2LiteralSource() {
  // v_add_f32 v0, 1.0f(literal), v0
  constexpr std::uint64_t base = 0x2000;
  auto [inst, size] = decode({0x060000FF, 0x3F800000}, base);

  CHECK(size == 8);
  CHECK(inst.kind == ir::Kind::Vop2);
  CHECK(inst.op == ir::vop2::ADD_F32);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Immediate);
  CHECK(inst.getOperand(1).address == base + 4);
}

// v_add_i32 writes VCC as carry-out: decoder appends an implicit VccLo def.
void testVop2AddI32ImplicitVcc() {
  // v_add_i32 v0, vcc, v0, v1  (op 37)
  auto [inst, size] = decode({0x4A000300});

  CHECK(inst.kind == ir::Kind::Vop2);
  CHECK(inst.op == ir::vop2::ADD_I32);
  CHECK(inst.operandCount == 4);
  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::VccLo);
  CHECK(inst.getOperand(1).access == GcnOperand::W);
}

// VOP1: [31:25]=0b0111111, vdst[24:17], op[16:9], src0[8:0]
void testVop1MovB32() {
  // v_mov_b32 v3, v0  (src0 = 256 -> v0)
  auto [inst, size] = decode({0x7E060300});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Vop1);
  CHECK(inst.op == ir::vop1::MOV_B32);
  CHECK(inst.operandCount == 2);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(0).value == 3);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(1).value == 0);
  CHECK(inst.getOperand(1).access == GcnOperand::R);
}

// VOPC: [31:25]=0b0111110, op[24:17], vsrc1[16:9], src0[8:0]
// Comparisons implicitly read/write VCC.
void testVopcCmpEqF32() {
  // v_cmp_eq_f32 vcc, s0, v1
  auto [inst, size] = decode({0x7C040200});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Vopc);
  CHECK(inst.op == ir::vopc::CMP_EQ_F32);
  CHECK(inst.operandCount == 3);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::VccLo);
  CHECK(inst.getOperand(0).access == (GcnOperand::R | GcnOperand::W));

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(1).value == 0);

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(2).value == 1);
}

// SOPK: [31:28]=0b1011, op[27:23], sdst[22:16], simm16[15:0]
void testSopkMovkI32SignExtension() {
  // s_movk_i32 s4, -2  (simm16 = 0xFFFE, must sign-extend)
  auto [inst, size] = decode({0xB004FFFE});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Sopk);
  CHECK(inst.op == ir::sopk::MOVK_I32);
  CHECK(inst.operandCount == 2);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(0).value == 4);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Constant);
  CHECK(inst.getOperand(1).value == 0xFFFFFFFE);
}

// SMRD: [31:27]=0b11000, op[26:22], sdst[21:15], sbase[14:9], imm[8],
// offset[7:0]. sbase selects an aligned SGPR pair (field value << 1); with
// imm=1 the offset field is a dword offset exposed as bytes (<< 2).
void testSmrdLoadDwordImmediateOffset() {
  // s_load_dword s6, s[0:1], 0x4  (dword offset 4 -> byte offset 16)
  auto [inst, size] = decode({0xC0030104});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Smrd);
  CHECK(inst.op == ir::smrd::LOAD_DWORD);
  CHECK(inst.operandCount == 4);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(0).value == 6);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Pointer);
  CHECK(inst.getOperand(1).access == GcnOperand::R);

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(2).value == 0);

  CHECK(inst.getOperand(3).kind == GcnOperand::Kind::Constant);
  CHECK(inst.getOperand(3).value == 16);
}

// VOP3a (64-bit): word0 [31:26]=0b110100, op[25:17], clamp[11], abs[10:8],
// vdst[7:0]; word1 neg[31:29], omod[28:27], src2[26:18], src1[17:9], src0[8:0]
void testVop3MadF32Modifiers() {
  // v_mad_f32 v1, |v2|, -v3, v4 clamp omod:1
  auto [inst, size] = decode({0xD2820901, 0x4C120702});

  CHECK(size == 8);
  CHECK(inst.kind == ir::Kind::Vop3);
  CHECK(inst.op == ir::vop3::MAD_F32);
  CHECK(inst.operandCount == 4);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(0).value == 1);
  CHECK(inst.getOperand(0).access == (GcnOperand::R | GcnOperand::W));
  CHECK(inst.getOperand(0).clamp == true);
  CHECK(inst.getOperand(0).omod == 1);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(1).value == 2);
  CHECK(inst.getOperand(1).abs == true);
  CHECK(inst.getOperand(1).neg == false);

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(2).value == 3);
  CHECK(inst.getOperand(2).abs == false);
  CHECK(inst.getOperand(2).neg == true);

  CHECK(inst.getOperand(3).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(3).value == 4);
}

// VOP3b variant: carry-writing ops replace abs/clamp bits with an explicit
// SGPR carry destination in word0 [14:8].
void testVop3bAddI32SgprCarry() {
  // v_add_i32 v0, s10, v1, v2
  auto [inst, size] = decode({0xD24A0A00, 0x00020501});

  CHECK(size == 8);
  CHECK(inst.kind == ir::Kind::Vop3);
  CHECK(inst.op == ir::vop3::ADD_I32);
  CHECK(inst.operandCount == 4);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(0).value == 0);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Sgpr);
  CHECK(inst.getOperand(1).value == 10);
  CHECK(inst.getOperand(1).access == (GcnOperand::R | GcnOperand::W));

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(2).value == 1);

  CHECK(inst.getOperand(3).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(3).value == 2);
}

// VINTRP: [31:26]=0b110010, vdst[25:18], op[17:16], attr[15:10],
// attrChan[9:8], vsrc[7:0]
void testVintrpP1() {
  // v_interp_p1_f32 v5, v0, attr2.y
  auto [inst, size] = decode({0xC8140900});

  CHECK(size == 4);
  CHECK(inst.kind == ir::Kind::Vintrp);
  CHECK(inst.op == ir::vintrp::P1_F32);
  CHECK(inst.operandCount == 3);

  CHECK(inst.getOperand(0).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(0).value == 5);
  CHECK(inst.getOperand(0).access == GcnOperand::W);

  CHECK(inst.getOperand(1).kind == GcnOperand::Kind::Vgpr);
  CHECK(inst.getOperand(1).value == 0);

  CHECK(inst.getOperand(2).kind == GcnOperand::Kind::Attr);
  CHECK(inst.getOperand(2).attrId == 2);
  CHECK(inst.getOperand(2).attrChannel == 1);
}

// P2 additionally reads the accumulator VGPR: vdst access becomes RW.
void testVintrpP2ReadsVdst() {
  // v_interp_p2_f32 v5, v1, attr2.y
  auto [inst, size] = decode({0xC8150901});

  CHECK(inst.kind == ir::Kind::Vintrp);
  CHECK(inst.op == ir::vintrp::P2_F32);
  CHECK(inst.getOperand(0).access == (GcnOperand::R | GcnOperand::W));
}

} // namespace

int main() {
  testSop1MovB32();
  testSop1MovInlineConstant();
  testSop1MovVccLo();
  testSoppEndpgm();
  testSoppBranchSimm();
  testSop2AddU32();
  testVop2AddF32();
  testVop2LiteralSource();
  testVop2AddI32ImplicitVcc();
  testVop1MovB32();
  testVopcCmpEqF32();
  testSopkMovkI32SignExtension();
  testSmrdLoadDwordImmediateOffset();
  testVop3MadF32Modifiers();
  testVop3bAddI32SgprCarry();
  testVintrpP1();
  testVintrpP2ReadsVdst();

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }

  std::puts("all checks passed");
  return 0;
}
