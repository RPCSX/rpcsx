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

  if (g_failures != 0) {
    std::printf("%d check(s) failed\n", g_failures);
    return 1;
  }

  std::puts("all checks passed");
  return 0;
}
