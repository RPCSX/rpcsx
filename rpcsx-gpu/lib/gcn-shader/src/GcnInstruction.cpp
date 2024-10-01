#include "GcnInstruction.hpp"

#include <cstdint>
#include <iostream>

using namespace shader;

static constexpr bool isVop3b(unsigned op) {
  return op == ir::vop3::ADD_I32 || op == ir::vop3::ADDC_U32 ||
         op == ir::vop3::SUB_I32 || op == ir::vop3::SUBB_U32 ||
         op == ir::vop3::SUBBREV_U32 || op == ir::vop3::SUBREV_I32 ||
         op == ir::vop3::DIV_SCALE_F32 || op == ir::vop3::DIV_SCALE_F64;
}

constexpr std::uint32_t genMask(std::uint32_t offset, std::uint32_t bitCount) {
  return ((1u << bitCount) - 1u) << offset;
}

constexpr std::uint32_t getMaskEnd(std::uint32_t mask) {
  return 32 - std::countl_zero(mask);
}

constexpr std::uint32_t fetchMaskedValue(std::uint32_t hex,
                                         std::uint32_t mask) {
  return (hex & mask) >> std::countr_zero(mask);
}

static GcnOperand createVgprGcnOperand(unsigned id) {
  return GcnOperand::createVgpr(id);
}

static constexpr auto createSgprOperands() {
  std::array<GcnOperand, 512> result;

  for (auto &op : result) {
    op.kind = GcnOperand::Kind::Invalid;
  }

  for (std::size_t i = 0; i < 104; ++i) {
    result[i] = GcnOperand::createSgpr(i);
  }

  for (std::size_t i = 256; i < 512; ++i) {
    result[i] = GcnOperand::createVgpr(i - 256);
  }

  result[106] = GcnOperand::createVccLo();
  result[107] = GcnOperand::createVccHi();
  result[124] = GcnOperand::createM0();
  result[126] = GcnOperand::createExecLo();
  result[127] = GcnOperand::createExecHi();

  for (std::size_t i = 128; i < 193; ++i) {
    result[i] = GcnOperand::createConstant(static_cast<std::uint32_t>(i - 128));
  }
  for (std::size_t i = 193; i < 209; ++i) {
    result[i] = GcnOperand::createConstant(
        static_cast<std::uint32_t>(-static_cast<std::int32_t>(i - 192)));
  }

  result[240] = GcnOperand::createConstant(0.5f);
  result[241] = GcnOperand::createConstant(-0.5f);
  result[242] = GcnOperand::createConstant(1.0f);
  result[243] = GcnOperand::createConstant(-1.0f);
  result[244] = GcnOperand::createConstant(2.0f);
  result[245] = GcnOperand::createConstant(-2.0f);
  result[246] = GcnOperand::createConstant(4.0f);
  result[247] = GcnOperand::createConstant(-4.0f);
  result[251] = GcnOperand::createVccZ();
  result[252] = GcnOperand::createExecZ();
  result[253] = GcnOperand::createScc();
  result[254] = GcnOperand::createLdsDirect();

  return result;
}

static GcnOperand createImmediateGcnOperand(std::uint64_t &address) {
  auto result = GcnOperand::createImmediateConstant(address);
  address += sizeof(std::uint32_t);
  return result;
}

static GcnOperand createSgprGcnOperand(std::uint64_t &address, unsigned id) {
  static constexpr auto g_operands = createSgprOperands();

  if (id == 255) {
    return createImmediateGcnOperand(address);
  }

  return g_operands[id];
}

static void
readVop2Inst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto src0Mask = genMask(0, 9);
  constexpr auto vsrc1Mask = genMask(getMaskEnd(src0Mask), 8);
  constexpr auto vdstMask = genMask(getMaskEnd(vsrc1Mask), 8);
  constexpr auto opMask = genMask(getMaskEnd(vdstMask), 6);

  std::uint32_t words[] = {readMemory(address)};
  address += sizeof(std::uint32_t);

  std::uint32_t src0 = fetchMaskedValue(words[0], src0Mask);
  std::uint32_t vsrc1 = fetchMaskedValue(words[0], vsrc1Mask);
  std::uint32_t vdst = fetchMaskedValue(words[0], vdstMask);
  auto op = static_cast<ir::vop2::Op>(fetchMaskedValue(words[0], opMask));

  inst.op = op;
  bool writesVcc = op == ir::vop2::ADD_I32 || op == ir::vop2::ADDC_U32 ||
                   op == ir::vop2::SUB_I32 || op == ir::vop2::SUBB_U32 ||
                   op == ir::vop2::SUBBREV_U32 || op == ir::vop2::SUBREV_I32;
  bool readsVcc = op == ir::vop2::ADDC_U32 || op == ir::vop2::SUBB_U32 ||
                  op == ir::vop2::SUBBREV_U32 || op == ir::vop2::CNDMASK_B32;

  inst.addOperand(createVgprGcnOperand(vdst).withW());
  if (writesVcc) {
    inst.addOperand(GcnOperand::createVccLo().withW());
  }
  inst.addOperand(createSgprGcnOperand(address, src0).withR());
  inst.addOperand(createVgprGcnOperand(vsrc1).withR());

  if (readsVcc) {
    inst.addOperand(GcnOperand::createVccLo().withR());
  }

  if (op == ir::vop2::MADMK_F32 || op == ir::vop2::MADAK_F32) {
    inst.addOperand(createImmediateGcnOperand(address));
  }
}

static void
readSop2Inst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto ssrc0Mask = genMask(0, 8);
  constexpr auto ssrc1Mask = genMask(getMaskEnd(ssrc0Mask), 8);
  constexpr auto sdstMask = genMask(getMaskEnd(ssrc1Mask), 7);
  constexpr auto opMask = genMask(getMaskEnd(sdstMask), 7);

  std::uint32_t words[] = {readMemory(address)};
  address += sizeof(std::uint32_t);

  std::uint32_t ssrc0 = fetchMaskedValue(words[0], ssrc0Mask);
  std::uint32_t ssrc1 = fetchMaskedValue(words[0], ssrc1Mask);
  auto op = static_cast<ir::sop2::Op>(fetchMaskedValue(words[0], opMask));
  std::uint32_t sdst = fetchMaskedValue(words[0], sdstMask);

  inst.op = op;
  inst.addOperand(createSgprGcnOperand(address, sdst).withW());
  inst.addOperand(createSgprGcnOperand(address, ssrc0).withR());
  inst.addOperand(createSgprGcnOperand(address, ssrc1).withR());
}

static void
readSopkInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto simmMask = genMask(0, 16);
  constexpr auto sdstMask = genMask(getMaskEnd(simmMask), 7);
  constexpr auto opMask = genMask(getMaskEnd(sdstMask), 5);

  std::uint32_t words[] = {readMemory(address)};
  address += sizeof(std::uint32_t);

  auto simm = static_cast<std::int16_t>(fetchMaskedValue(words[0], simmMask));
  auto op = static_cast<ir::sopk::Op>(fetchMaskedValue(words[0], opMask));
  auto sdst = fetchMaskedValue(words[0], sdstMask);

  inst.op = op;
  inst.addOperand(createSgprGcnOperand(address, sdst).withW());

  inst.addOperand(GcnOperand::createConstant(static_cast<std::uint32_t>(simm)));
  if (op <= 16) {
    inst.addOperand(createImmediateGcnOperand(address));
  }
}

static void
readSmrdInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto offsetMask = genMask(0, 8);
  constexpr auto immMask = genMask(getMaskEnd(offsetMask), 1);
  constexpr auto sbaseMask = genMask(getMaskEnd(immMask), 6);
  constexpr auto sdstMask = genMask(getMaskEnd(sbaseMask), 7);
  constexpr auto opMask = genMask(getMaskEnd(sdstMask), 5);

  std::uint32_t words[] = {readMemory(address)};
  address += sizeof(std::uint32_t);

  auto offset = fetchMaskedValue(words[0], offsetMask);
  auto imm = fetchMaskedValue(words[0], immMask);
  auto sbase = fetchMaskedValue(words[0], sbaseMask) << 1;
  auto sdst = fetchMaskedValue(words[0], sdstMask);
  auto op = static_cast<ir::smrd::Op>(fetchMaskedValue(words[0], opMask));

  int loadSize = 0;
  bool isBuffer = false;

  if (op >= ir::smrd::Op::LOAD_DWORD && op <= ir::smrd::Op::LOAD_DWORDX16) {
    loadSize = sizeof(std::uint32_t) * (1 << (op - ir::smrd::Op::LOAD_DWORD));
  } else if (op >= ir::smrd::Op::BUFFER_LOAD_DWORD &&
             op <= ir::smrd::Op::BUFFER_LOAD_DWORDX16) {
    loadSize =
        sizeof(std::uint32_t) * (1 << (op - ir::smrd::Op::BUFFER_LOAD_DWORD));
    isBuffer = true;
  }

  inst.op = op;
  if (op != ir::smrd::DCACHE_INV) {
    inst.addOperand(createSgprGcnOperand(address, sdst).withW());

    if (op != ir::smrd::MEMTIME) {
      auto baseOperand = createSgprGcnOperand(address, sbase);
      auto offsetOperand = imm ? GcnOperand::createConstant(
                                     std::uint32_t(std::int8_t(offset << 2)))
                               : createSgprGcnOperand(address, offset).withR();

      if (isBuffer) {
        inst.addOperand(GcnOperand::createBuffer(baseOperand).withR());
      } else {
        inst.addOperand(
            GcnOperand::createPointer(baseOperand, loadSize, offsetOperand)
                .withR());
      }

      inst.addOperand(baseOperand);
      inst.addOperand(offsetOperand);
    }
  }
}

static void
readVop3Inst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto vdstMask = genMask(0, 8);

  constexpr auto absMask = genMask(getMaskEnd(vdstMask), 3);
  constexpr auto clmpMask = genMask(getMaskEnd(absMask), 1);

  constexpr auto sdstMask = genMask(getMaskEnd(vdstMask), 7);

  constexpr auto opMask = genMask(getMaskEnd(clmpMask) + 5, 9);

  constexpr auto src0Mask = genMask(0, 9);
  constexpr auto src1Mask = genMask(getMaskEnd(src0Mask), 9);
  constexpr auto src2Mask = genMask(getMaskEnd(src1Mask), 9);
  constexpr auto omodMask = genMask(getMaskEnd(src2Mask), 2);
  constexpr auto negMask = genMask(getMaskEnd(omodMask), 3);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::vop3::Op>(fetchMaskedValue(words[0], opMask));
  auto vdst = fetchMaskedValue(words[0], vdstMask);
  auto abs = fetchMaskedValue(words[0], absMask);
  auto clmp = fetchMaskedValue(words[0], clmpMask) != 0;
  auto sdst = fetchMaskedValue(words[0], sdstMask);

  auto src0 = fetchMaskedValue(words[1], src0Mask);
  auto src1 = fetchMaskedValue(words[1], src1Mask);
  auto src2 = fetchMaskedValue(words[1], src2Mask);
  auto omod = fetchMaskedValue(words[1], omodMask);
  auto neg = fetchMaskedValue(words[1], negMask);

  if (op == ir::vop3::Op::MUL_HI_U32) {
    std::printf(".");
  }

  inst.op = op;
  bool vop3b = isVop3b(op);

  if (!vop3b) {
    abs = 0;
    clmp = false;
  }

  if (op >= 0 && op < ir::vopc::OpCount + 0) {
    inst.addOperand(createSgprGcnOperand(address, vdst)
                        .withRW()
                        .withOutputModifier(omod)
                        .withClamp(clmp));
  } else {
    inst.addOperand(
        createVgprGcnOperand(vdst).withRW().withOutputModifier(omod).withClamp(
            clmp));
  }

  if (vop3b) {
    inst.addOperand(createSgprGcnOperand(address, sdst).withRW());
  }

  bool writesVcc = op == ir::vop3::MAD_I64_I32 || op == ir::vop3::MAD_U64_U32 ||
                   op == ir::vop3::MQSAD_U32_U8 ||
                   op == ir::vop3::DIV_SCALE_F32 ||
                   op == ir::vop3::DIV_SCALE_F64;
  bool readsVcc = op == ir::vop3::DIV_FMAS_F32 || op == ir::vop3::DIV_FMAS_F64;

  bool usesSrc2 =
      op >= ir::vop3::MAD_LEGACY_F32 && op <= ir::vop3::DIV_FIXUP_F64;

  if (writesVcc) {
    inst.addOperand(GcnOperand::createVccLo().withRW());
  }

  inst.addOperand(createSgprGcnOperand(address, src0)
                      .withR()
                      .withAbs((abs & 1) != 0)
                      .withNeg((neg & 1) != 0));

  if (op >= 0 && op < ir::vopc::OpCount + 0) {
    // vopc
    inst.addOperand(createSgprGcnOperand(address, src1)
                        .withR()
                        .withAbs(((abs >> 1) & 1) != 0)
                        .withNeg(((neg >> 1) & 1) != 0));

  } else if (op >= 256 && op < ir::vop2::OpCount + 256) {
    // vop2
    inst.addOperand(createSgprGcnOperand(address, src1)
                        .withR()
                        .withAbs(((abs >> 1) & 1) != 0)
                        .withNeg(((neg >> 1) & 1) != 0));

    if (op == ir::vop3::ADDC_U32 || op == ir::vop3::SUBB_U32 ||
        op == ir::vop3::SUBBREV_U32 || op == ir::vop3::CNDMASK_B32) {
      inst.addOperand(createSgprGcnOperand(address, src2)
                          .withR()
                          .withAbs(((abs >> 2) & 1) != 0)
                          .withNeg(((neg >> 2) & 1) != 0));
    } else if (op == ir::vop3::MADMK_F32 || op == ir::vop3::MADAK_F32) {
      inst.addOperand(createImmediateGcnOperand(address));
    }
  } else if (op >= 384 && op < ir::vop1::OpCount + 384) {
    // vop1
  } else {
    inst.addOperand(createSgprGcnOperand(address, src1)
                        .withR()
                        .withAbs(((abs >> 1) & 1) != 0)
                        .withNeg(((neg >> 1) & 1) != 0));

    if (usesSrc2) {
      inst.addOperand(createSgprGcnOperand(address, src2)
                          .withR()
                          .withAbs(((abs >> 2) & 1) != 0)
                          .withNeg(((neg >> 2) & 1) != 0));
    }
  }

  if (readsVcc) {
    inst.addOperand(GcnOperand::createVccLo().withR());
  }
}

static void
readMubufInst(GcnInstruction &inst, std::uint64_t &address,
              const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto offsetMask = genMask(0, 12);
  constexpr auto offenMask = genMask(getMaskEnd(offsetMask), 1);
  constexpr auto idxenMask = genMask(getMaskEnd(offenMask), 1);
  constexpr auto glcMask = genMask(getMaskEnd(idxenMask), 1);
  constexpr auto ldsMask = genMask(getMaskEnd(glcMask) + 1, 1);
  constexpr auto opMask = genMask(getMaskEnd(ldsMask) + 1, 7);

  constexpr auto vaddrMask = genMask(0, 8);
  constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  constexpr auto slcMask = genMask(getMaskEnd(srsrcMask) + 1, 1);
  constexpr auto tfeMask = genMask(getMaskEnd(slcMask), 1);
  constexpr auto soffsetMask = genMask(getMaskEnd(tfeMask), 8);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto offset = fetchMaskedValue(words[0], offsetMask);
  auto offen = fetchMaskedValue(words[0], offenMask) != 0;
  auto idxen = fetchMaskedValue(words[0], idxenMask) != 0;
  auto glc = fetchMaskedValue(words[0], glcMask) != 0;
  auto lds = fetchMaskedValue(words[0], ldsMask) != 0;
  auto op = static_cast<ir::mubuf::Op>(fetchMaskedValue(words[0], opMask));

  auto vaddr = fetchMaskedValue(words[1], vaddrMask);
  auto vdata = fetchMaskedValue(words[1], vdataMask);
  auto srsrc = fetchMaskedValue(words[1], srsrcMask) << 2;
  bool slc = fetchMaskedValue(words[1], slcMask) != 0;
  bool tfe = fetchMaskedValue(words[1], tfeMask) != 0;
  auto soffset = fetchMaskedValue(words[1], soffsetMask);

  bool isLoadOp =
      op == ir::mubuf::LOAD_FORMAT_X || op == ir::mubuf::LOAD_FORMAT_XY ||
      op == ir::mubuf::LOAD_FORMAT_XYZ || op == ir::mubuf::LOAD_FORMAT_XYZW ||
      op == ir::mubuf::LOAD_UBYTE || op == ir::mubuf::LOAD_SBYTE ||
      op == ir::mubuf::LOAD_USHORT || op == ir::mubuf::LOAD_SSHORT ||
      op == ir::mubuf::LOAD_DWORD || op == ir::mubuf::LOAD_DWORDX2 ||
      op == ir::mubuf::LOAD_DWORDX4 || op == ir::mubuf::LOAD_DWORDX3;

  bool supportsLds =
      op == ir::mubuf::LOAD_FORMAT_X || op == ir::mubuf::LOAD_SBYTE ||
      op == ir::mubuf::LOAD_UBYTE || op == ir::mubuf::LOAD_USHORT ||
      op == ir::mubuf::LOAD_SSHORT || op == ir::mubuf::LOAD_DWORD;

  std::uint8_t dataAccess = 0;
  std::uint8_t bufferAccess = 0;
  if (!supportsLds || !lds) {
    if (isLoadOp) {
      dataAccess |= GcnOperand::W;
    } else {
      dataAccess |= GcnOperand::R;
    }
  }

  if (isLoadOp) {
    bufferAccess = GcnOperand::R;
  } else if ((op >= ir::mubuf::STORE_FORMAT_X &&
              op <= ir::mubuf::STORE_FORMAT_XYZW) ||
             (op >= ir::mubuf::STORE_BYTE && op <= ir::mubuf::STORE_DWORDX3)) {
    bufferAccess = GcnOperand::W;
  } else {
    bufferAccess = GcnOperand::R | GcnOperand::W;
  }

  inst.op = op;
  inst.addOperand(createVgprGcnOperand(vdata).withAccess(dataAccess));

  if (offen) {
    inst.addOperand(createVgprGcnOperand(vaddr + (idxen ? 1 : 0)).withR());
  } else {
    inst.addOperand(GcnOperand::createConstant(0u));
  }

  if (idxen) {
    inst.addOperand(createVgprGcnOperand(vaddr).withR());
  } else {
    inst.addOperand(GcnOperand::createConstant(0u));
  }

  auto srsrcOperand = createSgprGcnOperand(address, srsrc).withR();
  inst.addOperand(
      GcnOperand::createBuffer(srsrcOperand).withAccess(bufferAccess));
  inst.addOperand(srsrcOperand);
  inst.addOperand(createSgprGcnOperand(address, soffset).withR());

  inst.addOperand(GcnOperand::createConstant(offset));
  inst.addOperand(GcnOperand::createConstant(idxen));
  inst.addOperand(GcnOperand::createConstant(glc));
  inst.addOperand(GcnOperand::createConstant(lds));
  inst.addOperand(GcnOperand::createConstant(slc));
  inst.addOperand(GcnOperand::createConstant(tfe));
}
static void
readMtbufInst(GcnInstruction &inst, std::uint64_t &address,
              const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto offsetMask = genMask(0, 12);
  constexpr auto offenMask = genMask(getMaskEnd(offsetMask), 1);
  constexpr auto idxenMask = genMask(getMaskEnd(offenMask), 1);
  constexpr auto glcMask = genMask(getMaskEnd(idxenMask), 1);
  constexpr auto opMask = genMask(getMaskEnd(glcMask) + 1, 3);
  constexpr auto dfmtMask = genMask(getMaskEnd(opMask), 4);
  constexpr auto nfmtMask = genMask(getMaskEnd(dfmtMask), 4);

  constexpr auto vaddrMask = genMask(0, 8);
  constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  constexpr auto slcMask = genMask(getMaskEnd(srsrcMask) + 1, 1);
  constexpr auto tfeMask = genMask(getMaskEnd(slcMask), 1);
  constexpr auto soffsetMask = genMask(getMaskEnd(tfeMask), 8);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::mtbuf::Op>(fetchMaskedValue(words[0], opMask));

  auto offset = fetchMaskedValue(words[0], offsetMask);
  auto offen = fetchMaskedValue(words[0], offenMask) != 0;
  auto idxen = fetchMaskedValue(words[0], idxenMask) != 0;
  auto glc = fetchMaskedValue(words[0], glcMask) != 0;
  auto dfmt = fetchMaskedValue(words[0], dfmtMask);
  auto nfmt = fetchMaskedValue(words[0], nfmtMask);

  auto vaddr = fetchMaskedValue(words[1], vaddrMask);
  auto vdata = fetchMaskedValue(words[1], vdataMask);
  auto srsrc = fetchMaskedValue(words[1], srsrcMask) << 2;
  auto slc = fetchMaskedValue(words[1], slcMask) != 0;
  auto tfe = fetchMaskedValue(words[1], tfeMask) != 0;
  auto soffset = fetchMaskedValue(words[1], soffsetMask);

  inst.op = op;

  bool isLoadOp =
      op == ir::mtbuf::LOAD_FORMAT_X || op == ir::mtbuf::LOAD_FORMAT_XY ||
      op == ir::mtbuf::LOAD_FORMAT_XYZ || op == ir::mtbuf::LOAD_FORMAT_XYZW;

  std::uint8_t dataAccess = 0;
  std::uint8_t bufferAccess = 0;
  if (isLoadOp) {
    dataAccess = GcnOperand::W;
    bufferAccess = GcnOperand::R;
  } else {
    dataAccess = GcnOperand::R;
    bufferAccess = GcnOperand::W;
  }

  inst.op = op;
  inst.addOperand(createVgprGcnOperand(vdata).withAccess(dataAccess));

  if (offen) {
    inst.addOperand(createVgprGcnOperand(vaddr + (idxen ? 1 : 0)).withR());
  } else {
    inst.addOperand(GcnOperand::createConstant(0u));
  }

  if (idxen) {
    inst.addOperand(createVgprGcnOperand(vaddr).withR());
  } else {
    inst.addOperand(GcnOperand::createConstant(0u));
  }

  inst.addOperand(GcnOperand::createConstant(dfmt));
  inst.addOperand(GcnOperand::createConstant(nfmt));

  auto srsrcOperand = createSgprGcnOperand(address, srsrc).withR();
  inst.addOperand(
      GcnOperand::createBuffer(srsrcOperand).withAccess(bufferAccess));
  inst.addOperand(srsrcOperand);
  inst.addOperand(createSgprGcnOperand(address, soffset).withR());

  inst.addOperand(GcnOperand::createConstant(offset));
  inst.addOperand(GcnOperand::createConstant(idxen));
  inst.addOperand(GcnOperand::createConstant(glc));
  inst.addOperand(GcnOperand::createConstant(slc));
  inst.addOperand(GcnOperand::createConstant(tfe));
}

static void
readMimgInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto dmaskMask = genMask(8, 4);
  constexpr auto unrmMask = genMask(getMaskEnd(dmaskMask), 1);
  constexpr auto glcMask = genMask(getMaskEnd(unrmMask), 1);
  constexpr auto daMask = genMask(getMaskEnd(glcMask), 1);
  constexpr auto r128Mask = genMask(getMaskEnd(daMask), 1);
  constexpr auto tfeMask = genMask(getMaskEnd(r128Mask), 1);
  constexpr auto lweMask = genMask(getMaskEnd(tfeMask), 1);
  constexpr auto opMask = genMask(getMaskEnd(lweMask), 7);
  constexpr auto slcMask = genMask(getMaskEnd(opMask), 1);

  constexpr auto vaddrMask = genMask(0, 8);
  constexpr auto vdataMask = genMask(getMaskEnd(vaddrMask), 8);
  constexpr auto srsrcMask = genMask(getMaskEnd(vdataMask), 5);
  constexpr auto ssampMask = genMask(getMaskEnd(srsrcMask), 5);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::mimg::Op>(fetchMaskedValue(words[0], opMask));

  auto dmask = fetchMaskedValue(words[0], dmaskMask);
  auto unrm = fetchMaskedValue(words[0], unrmMask) != 0;
  auto glc = fetchMaskedValue(words[0], glcMask) != 0;
  auto da = fetchMaskedValue(words[0], daMask) != 0;
  auto r128 = fetchMaskedValue(words[0], r128Mask) != 0;
  auto tfe = fetchMaskedValue(words[0], tfeMask) != 0;
  auto lwe = fetchMaskedValue(words[0], lweMask) != 0;
  auto slc = fetchMaskedValue(words[0], slcMask) != 0;

  auto vaddr = fetchMaskedValue(words[1], vaddrMask);
  auto vdata = fetchMaskedValue(words[1], vdataMask);
  auto srsrc = fetchMaskedValue(words[1], srsrcMask) << 2;
  auto ssamp = fetchMaskedValue(words[1], ssampMask) << 2;

  std::uint8_t textureAccess = 0;
  bool hasSampler = true;

  if (op >= ir::mimg::Op::LOAD && op <= ir::mimg::Op::LOAD_MIP_PCK_SGN) {
    textureAccess = GcnOperand::R;
  } else if (op >= ir::mimg::Op::STORE && op <= ir::mimg::Op::STORE_MIP_PCK) {
    textureAccess = GcnOperand::W;
  } else if (op >= ir::mimg::Op::ATOMIC_SWAP &&
             op <= ir::mimg::Op::ATOMIC_FMAX) {
    textureAccess = GcnOperand::R | GcnOperand::W;
    hasSampler = false;
  } else if (op >= ir::mimg::Op::SAMPLE && op <= ir::mimg::Op::GATHER4_C_LZ_O) {
    textureAccess = GcnOperand::R;
  } else if (op >= ir::mimg::Op::SAMPLE_CD &&
             op <= ir::mimg::Op::SAMPLE_C_CD_CL_O) {
    textureAccess = GcnOperand::R;
  } else if (op == ir::mimg::Op::GET_RESINFO) {
    hasSampler = false;
  }

  inst.op = op;
  inst.addOperand(createVgprGcnOperand(vdata).withRW());
  inst.addOperand(createVgprGcnOperand(vaddr).withR());
  auto tbufferStart = createSgprGcnOperand(address, srsrc);
  inst.addOperand(
      GcnOperand::createTexture(tbufferStart, r128).withAccess(textureAccess));
  inst.addOperand(tbufferStart);

  if (hasSampler) {
    auto samplerStart = createSgprGcnOperand(address, ssamp);
    inst.addOperand(GcnOperand::createSampler(samplerStart, unrm).withR());
    inst.addOperand(samplerStart);
  }

  inst.addOperand(GcnOperand::createConstant(dmask));
  // inst.addOperand(GcnOperand::createConstant(glc));
  // inst.addOperand(GcnOperand::createConstant(da));
  // inst.addOperand(GcnOperand::createConstant(r128));
  // inst.addOperand(GcnOperand::createConstant(tfe));
  // inst.addOperand(GcnOperand::createConstant(lwe));
  // inst.addOperand(GcnOperand::createConstant(slc));
}
static void
readDsInst(GcnInstruction &inst, std::uint64_t &address,
           const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto offset0Mask = genMask(0, 8);
  constexpr auto offset1Mask = genMask(getMaskEnd(offset0Mask), 8);
  constexpr auto gdsMask = genMask(getMaskEnd(offset1Mask) + 1, 1);
  constexpr auto opMask = genMask(getMaskEnd(gdsMask), 8);

  constexpr auto addrMask = genMask(0, 8);
  constexpr auto data0Mask = genMask(getMaskEnd(addrMask), 8);
  constexpr auto data1Mask = genMask(getMaskEnd(data0Mask), 8);
  constexpr auto vdstMask = genMask(getMaskEnd(data1Mask), 8);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::ds::Op>(fetchMaskedValue(words[0], opMask));
  auto offset0 = fetchMaskedValue(words[0], offset0Mask);
  auto offset1 = fetchMaskedValue(words[0], offset1Mask);
  auto gds = fetchMaskedValue(words[0], gdsMask) != 0;

  auto addr = fetchMaskedValue(words[1], addrMask);
  auto data0 = fetchMaskedValue(words[1], data0Mask);
  auto data1 = fetchMaskedValue(words[1], data1Mask);
  auto vdst = fetchMaskedValue(words[1], vdstMask);

  bool hasOffset1 =
      op == ir::ds::READ2_B32 || op == ir::ds::READ2_B64 ||
      op == ir::ds::READ2ST64_B32 || op == ir::ds::READ2ST64_B64 ||
      op == ir::ds::WRITE2_B32 || op == ir::ds::WRITE2_B64 ||
      op == ir::ds::WRITE2ST64_B32 || op == ir::ds::WRITE2ST64_B64 ||
      op == ir::ds::WRXCHG2ST64_RTN_B32 || op == ir::ds::WRXCHG2ST64_RTN_B64 ||
      op == ir::ds::WRXCHG2_RTN_B32 || op == ir::ds::WRXCHG2_RTN_B64 ||
      op == ir::ds::ORDERED_COUNT;

  bool hasDst = op == ir::ds::READ_B32 || op == ir::ds::READ2_B32 ||
                op == ir::ds::READ2ST64_B32 || op == ir::ds::READ_I8 ||
                op == ir::ds::READ_U8 || op == ir::ds::READ_I16 ||
                op == ir::ds::READ_U16 || op == ir::ds::READ_B64 ||
                op == ir::ds::READ2_B64 || op == ir::ds::READ2ST64_B64 ||
                op == ir::ds::READ_B96 || op == ir::ds::READ_B128 ||
                op == ir::ds::AND_RTN_B64 || op == ir::ds::OR_RTN_B64 ||
                op == ir::ds::XOR_RTN_B64 || op == ir::ds::MSKOR_RTN_B64 ||
                op == ir::ds::APPEND || op == ir::ds::CONSUME ||
                op == ir::ds::SWIZZLE_B32 || op == ir::ds::ORDERED_COUNT;

  bool hasLoOffset = op == ir::ds::GWS_BARRIER || op == ir::ds::GWS_INIT ||
                     op == ir::ds::GWS_SEMA_BR || op == ir::ds::GWS_SEMA_P ||
                     op == ir::ds::GWS_SEMA_RELEASE_ALL;

  inst.op = op;

  if (op != ir::ds::NOP) {
    if (hasDst) {
      inst.addOperand(createVgprGcnOperand(vdst).withW());
    }
    inst.addOperand(createVgprGcnOperand(addr).withR());
    inst.addOperand(createVgprGcnOperand(data0).withRW());
    inst.addOperand(createVgprGcnOperand(data1).withRW());

    if (hasOffset1) {
      inst.addOperand(GcnOperand::createConstant(offset0));
      inst.addOperand(GcnOperand::createConstant(offset1));
    } else if (hasLoOffset) {
      inst.addOperand(GcnOperand::createConstant(offset0));
    } else {
      inst.addOperand(GcnOperand::createConstant(offset0 | (offset1 << 8)));
    }
  }
  inst.addOperand(GcnOperand::createConstant(gds));
}
static void
readVintrpInst(GcnInstruction &inst, std::uint64_t &address,
               const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto vsrcMask = genMask(0, 8);
  constexpr auto attrChanMask = genMask(getMaskEnd(vsrcMask), 2);
  constexpr auto attrMask = genMask(getMaskEnd(attrChanMask), 6);
  constexpr auto opMask = genMask(getMaskEnd(attrMask), 2);
  constexpr auto vdstMask = genMask(getMaskEnd(opMask), 8);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::vintrp::Op>(fetchMaskedValue(words[0], opMask));
  auto vsrc = fetchMaskedValue(words[0], vsrcMask);
  auto attrChan = fetchMaskedValue(words[0], attrChanMask);
  auto attr = fetchMaskedValue(words[0], attrMask);
  auto vdst = fetchMaskedValue(words[0], vdstMask);

  inst.op = op;
  std::uint8_t vdstAccess = GcnOperand::W;
  if (op == ir::vintrp::Op::P2_F32) {
    vdstAccess |= GcnOperand::R;
  }

  inst.addOperand(createVgprGcnOperand(vdst).withAccess(vdstAccess));
  inst.addOperand(createVgprGcnOperand(vsrc).withR());
  inst.addOperand(GcnOperand::createAttr(attr, attrChan));
}
static void
readExpInst(GcnInstruction &inst, std::uint64_t &address,
            const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto enMask = genMask(0, 4);
  constexpr auto targetMask = genMask(getMaskEnd(enMask), 6);
  constexpr auto comprMask = genMask(getMaskEnd(targetMask), 1);
  constexpr auto doneMask = genMask(getMaskEnd(comprMask), 1);
  constexpr auto vmMask = genMask(getMaskEnd(doneMask), 1);

  constexpr auto vsrc0Mask = genMask(0, 8);
  constexpr auto vsrc1Mask = genMask(getMaskEnd(vsrc0Mask), 8);
  constexpr auto vsrc2Mask = genMask(getMaskEnd(vsrc1Mask), 8);
  constexpr auto vsrc3Mask = genMask(getMaskEnd(vsrc2Mask), 8);

  std::uint32_t words[2];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);
  words[1] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto en = fetchMaskedValue(words[0], enMask);
  auto target = fetchMaskedValue(words[0], targetMask);
  auto compr = fetchMaskedValue(words[0], comprMask) != 0;
  auto done = fetchMaskedValue(words[0], doneMask) != 0;
  auto vm = fetchMaskedValue(words[0], vmMask) != 0;
  auto vsrc0 = fetchMaskedValue(words[1], vsrc0Mask);
  auto vsrc1 = fetchMaskedValue(words[1], vsrc1Mask);
  auto vsrc2 = fetchMaskedValue(words[1], vsrc2Mask);
  auto vsrc3 = fetchMaskedValue(words[1], vsrc3Mask);

  inst.op = 0;
  inst.addOperand(GcnOperand::createConstant(target));
  inst.addOperand(GcnOperand::createConstant(en));
  inst.addOperand(GcnOperand::createConstant(compr));
  inst.addOperand(GcnOperand::createConstant(done));
  inst.addOperand(GcnOperand::createConstant(vm));

  if (en & (1 << 0)) {
    inst.addOperand(createVgprGcnOperand(vsrc0).withR());
  }
  if (en & (1 << 1)) {
    inst.addOperand(createVgprGcnOperand(vsrc1).withR());
  }
  if (!compr) {
    if (en & (1 << 2)) {
      inst.addOperand(createVgprGcnOperand(vsrc2).withR());
    }
    if (en & (1 << 3)) {
      inst.addOperand(createVgprGcnOperand(vsrc3).withR());
    }
  }
}
static void
readVop1Inst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto src0Mask = genMask(0, 9);
  constexpr auto opMask = genMask(getMaskEnd(src0Mask), 8);
  constexpr auto vdstMask = genMask(getMaskEnd(opMask), 8);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::vop1::Op>(fetchMaskedValue(words[0], opMask));
  auto src0 = fetchMaskedValue(words[0], src0Mask);
  auto vdst = fetchMaskedValue(words[0], vdstMask);

  inst.op = op;
  inst.addOperand(createVgprGcnOperand(vdst).withW());
  inst.addOperand(createSgprGcnOperand(address, src0).withR());
}
static void
readVopcInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto src0Mask = genMask(0, 9);
  constexpr auto vsrc1Mask = genMask(getMaskEnd(src0Mask), 8);
  constexpr auto opMask = genMask(getMaskEnd(vsrc1Mask), 8);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::vopc::Op>(fetchMaskedValue(words[0], opMask));
  auto src0 = fetchMaskedValue(words[0], src0Mask);
  auto vsrc1 = fetchMaskedValue(words[0], vsrc1Mask);

  inst.op = op;
  inst.addOperand(GcnOperand::createVccLo().withRW());
  inst.addOperand(createSgprGcnOperand(address, src0).withR());
  inst.addOperand(createVgprGcnOperand(vsrc1).withR());
}

static void
readSop1Inst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto ssrc0Mask = genMask(0, 8);
  constexpr auto opMask = genMask(getMaskEnd(ssrc0Mask), 8);
  constexpr auto sdstMask = genMask(getMaskEnd(opMask), 7);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::sop1::Op>(fetchMaskedValue(words[0], opMask));
  auto ssrc0 = fetchMaskedValue(words[0], ssrc0Mask);
  auto sdst = fetchMaskedValue(words[0], sdstMask);

  inst.op = op;

  bool readsM0 = op == ir::sop1::MOVRELS_B32 || op == ir::sop1::MOVRELS_B64 ||
                 op == ir::sop1::MOVRELD_B32 || op == ir::sop1::MOVRELD_B64;

  inst.addOperand(createSgprGcnOperand(address, sdst).withW());
  inst.addOperand(createSgprGcnOperand(address, ssrc0).withR());

  if (readsM0) {
    inst.addOperand(GcnOperand::createM0().withR());
  }
}
static void
readSopcInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  constexpr auto ssrc0Mask = genMask(0, 8);
  constexpr auto ssrc1Mask = genMask(getMaskEnd(ssrc0Mask), 8);
  constexpr auto opMask = genMask(getMaskEnd(ssrc1Mask), 7);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::sopc::Op>(fetchMaskedValue(words[0], opMask));
  auto ssrc0 = fetchMaskedValue(words[0], ssrc0Mask);
  auto ssrc1 = fetchMaskedValue(words[0], ssrc1Mask);

  inst.op = op;
  inst.addOperand(createSgprGcnOperand(address, ssrc0).withR());
  inst.addOperand(createSgprGcnOperand(address, ssrc1).withR());
}

static void
readSoppInst(GcnInstruction &inst, std::uint64_t &address,
             const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  static constexpr auto simmMask = genMask(0, 16);
  static constexpr auto opMask = genMask(getMaskEnd(simmMask), 7);

  std::uint32_t words[1];
  words[0] = readMemory(address);
  address += sizeof(std::uint32_t);

  auto op = static_cast<ir::sopp::Op>(fetchMaskedValue(words[0], opMask));
  auto simm = static_cast<std::int16_t>(fetchMaskedValue(words[0], simmMask));

  inst.op = op;
  inst.addOperand(
      GcnOperand::createConstant(static_cast<std::uint32_t>(simm) << 2));
}

void GcnOperand::print(std::ostream &os) const {
  switch (kind) {
  case Kind::Invalid:
    os << "<invalid>";
    break;
  case Kind::Constant:
    os << '#';
    os << std::to_string(value);

    if (value != 0 &&
        std::abs(0.f - std::bit_cast<float>(value)) > 0.0000001f) {
      os << " (" << std::to_string(std::bit_cast<float>(value)) << ')';
    }
    break;
  case Kind::Immediate:
    os << "*" << std::hex << std::to_string(address) << std::dec;
    break;
  case Kind::VccLo:
    os << "vcc_lo";
    break;
  case Kind::VccHi:
    os << "vcc_hi";
    break;
  case Kind::M0:
    os << "m0";
    break;
  case Kind::ExecLo:
    os << "exec_lo";
    break;
  case Kind::ExecHi:
    os << "exec_hi";
    break;
  case Kind::Scc:
    os << "scc";
    break;
  case Kind::VccZ:
    os << "vccz";
    break;
  case Kind::ExecZ:
    os << "execz";
    break;
  case Kind::LdsDirect:
    os << "lds_direct";
    break;
  case Kind::Vgpr:
    os << 'v';
    os << std::to_string(value);
    break;
  case Kind::Sgpr:
    os << 's';
    os << std::to_string(value);
    break;
  case Kind::Attr:
    os << "attr";
    os << std::to_string(attrId);
    os << '.';
    switch (attrChannel) {
    case 0:
      os << 'x';
      break;
    case 1:
      os << 'y';
      break;
    case 2:
      os << 'z';
      break;
    case 3:
      os << 'w';
      break;
    }
    break;
  case Kind::Buffer:
    os << "V#{";
    getUnderlyingOperand(0).print(os);
    os << "..";
    getUnderlyingOperand(3).print(os);
    os << "}";
    break;
  case Kind::Sampler:
    os << "S#{";
    getUnderlyingOperand(0).print(os);
    os << "..";
    getUnderlyingOperand(3).print(os);
    os << "}";
    break;
  case Kind::Texture128:
    os << "T#{";
    getUnderlyingOperand(0).print(os);
    os << "..";
    getUnderlyingOperand(3).print(os);
    os << "}";
    break;
  case Kind::Texture256:
    os << "T#{";
    getUnderlyingOperand(0).print(os);
    os << "..";
    getUnderlyingOperand(7).print(os);
    os << "}";
    break;
  case Kind::Pointer:
    os << "ptr{";
    getUnderlyingOperand(0).print(os);
    os << "..";
    getUnderlyingOperand(1).print(os);
    os << "} + ";
    getPointerOffsetOperand().print(os);
    break;
  }
}

void GcnOperand::dump() const {
  print(std::cerr);
  std::cerr << '\n';
}

void GcnInstruction::print(std::ostream &os) const {
  os << ir::getInstructionName(kind, op);

  if (operandCount > 0) {
    os << ' ';

    for (std::size_t i = 0; i < operandCount; ++i) {
      if (i != 0) {
        os << ", ";
      }

      operands[i].print(os);
    }
  }
}

void GcnInstruction::dump() const {
  print(std::cerr);
  std::cerr << '\n';
}

void shader::readGcnInst(
    GcnInstruction &isaInst, std::uint64_t &address,
    const std::function<std::uint32_t(std::uint64_t)> &readMemory) {
  static constexpr std::uint32_t kInstMask1 =
      static_cast<std::uint32_t>(~0u << (32 - 1));
  static constexpr std::uint32_t kInstMask2 =
      static_cast<std::uint32_t>(~0u << (32 - 2));
  static constexpr std::uint32_t kInstMask4 =
      static_cast<std::uint32_t>(~0u << (32 - 4));
  static constexpr std::uint32_t kInstMask5 =
      static_cast<std::uint32_t>(~0u << (32 - 5));
  static constexpr std::uint32_t kInstMask6 =
      static_cast<std::uint32_t>(~0u << (32 - 6));
  static constexpr std::uint32_t kInstMask7 =
      static_cast<std::uint32_t>(~0u << (32 - 7));
  static constexpr std::uint32_t kInstMask9 =
      static_cast<std::uint32_t>(~0u << (32 - 9));

  static constexpr std::uint32_t kInstMaskValVop2 = 0b0u << (32 - 1);
  static constexpr std::uint32_t kInstMaskValSop2 = 0b10u << (32 - 2);
  static constexpr std::uint32_t kInstMaskValSopk = 0b1011u << (32 - 4);
  static constexpr std::uint32_t kInstMaskValSmrd = 0b11000u << (32 - 5);
  static constexpr std::uint32_t kInstMaskValVop3 = 0b110100u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValMubuf = 0b111000u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValMtbuf = 0b111010u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValMimg = 0b111100u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValDs = 0b110110u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValVintrp = 0b110010u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValExp = 0b111110u << (32 - 6);
  static constexpr std::uint32_t kInstMaskValVop1 = 0b0111111u << (32 - 7);
  static constexpr std::uint32_t kInstMaskValVopC = 0b0111110u << (32 - 7);
  static constexpr std::uint32_t kInstMaskValSop1 = 0b101111101u << (32 - 9);
  static constexpr std::uint32_t kInstMaskValSopc = 0b101111110u << (32 - 9);
  static constexpr std::uint32_t kInstMaskValSopp = 0b101111111u << (32 - 9);

  auto instr = readMemory(address);

  switch (instr & kInstMask9) {
  case kInstMaskValSop1:
    isaInst.kind = ir::Kind::Sop1;
    return readSop1Inst(isaInst, address, readMemory);
  case kInstMaskValSopc:
    isaInst.kind = ir::Kind::Sopc;
    return readSopcInst(isaInst, address, readMemory);
  case kInstMaskValSopp:
    isaInst.kind = ir::Kind::Sopp;
    return readSoppInst(isaInst, address, readMemory);
  }

  switch (instr & kInstMask7) {
  case kInstMaskValVop1:
    isaInst.kind = ir::Kind::Vop1;
    return readVop1Inst(isaInst, address, readMemory);
  case kInstMaskValVopC:
    isaInst.kind = ir::Kind::Vopc;
    return readVopcInst(isaInst, address, readMemory);
  }

  switch (instr & kInstMask6) {
  case kInstMaskValVop3:
    isaInst.kind = ir::Kind::Vop3;
    return readVop3Inst(isaInst, address, readMemory);
  case kInstMaskValMubuf:
    isaInst.kind = ir::Kind::Mubuf;
    return readMubufInst(isaInst, address, readMemory);
  case kInstMaskValMtbuf:
    isaInst.kind = ir::Kind::Mtbuf;
    return readMtbufInst(isaInst, address, readMemory);
  case kInstMaskValMimg:
    isaInst.kind = ir::Kind::Mimg;
    return readMimgInst(isaInst, address, readMemory);
  case kInstMaskValDs:
    isaInst.kind = ir::Kind::Ds;
    return readDsInst(isaInst, address, readMemory);
  case kInstMaskValVintrp:
    isaInst.kind = ir::Kind::Vintrp;
    return readVintrpInst(isaInst, address, readMemory);
  case kInstMaskValExp:
    isaInst.kind = ir::Kind::Exp;
    return readExpInst(isaInst, address, readMemory);
  }

  if ((instr & kInstMask5) == kInstMaskValSmrd) {
    isaInst.kind = ir::Kind::Smrd;
    return readSmrdInst(isaInst, address, readMemory);
  }

  if ((instr & kInstMask4) == kInstMaskValSopk) {
    isaInst.kind = ir::Kind::Sopk;
    return readSopkInst(isaInst, address, readMemory);
  }

  if ((instr & kInstMask2) == kInstMaskValSop2) {
    isaInst.kind = ir::Kind::Sop2;
    return readSop2Inst(isaInst, address, readMemory);
  }

  if ((instr & kInstMask1) == kInstMaskValVop2) {
    isaInst.kind = ir::Kind::Vop2;
    return readVop2Inst(isaInst, address, readMemory);
  }
}
