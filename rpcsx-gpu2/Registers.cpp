#include "Registers.hpp"

amdgpu::Registers::Context amdgpu::Registers::Context::Default = [] {
  amdgpu::Registers::Context result{};
  result.paScScreenScissor.bottom = 0x4000;
  result.paScScreenScissor.right = 0x4000;

  result.paScWindowScissor.top = 0x8000;
  result.paScWindowScissor.bottom = 0x4000;
  result.paScWindowScissor.right = 0x4000;

  for (auto &clipRect : result.paScClipRect) {
    clipRect.bottom = 0x4000;
    clipRect.right = 0x4000;
  }

  result.unk_8c = 0xaa99aaaa;
  result.paScGenericScissor.top = 0x8000;
  result.paScGenericScissor.bottom = 0x4000;
  result.paScGenericScissor.right = 0x4000;

  for (auto &vportScissor : result.paScVportScissor) {
    vportScissor.top = 0x8000;
    vportScissor.bottom = 0x4000;
    vportScissor.right = 0x4000;
  }

  for (auto &vportZ : result.paScVportZ) {
    vportZ.min = 0.0f;
    vportZ.max = 1.0f;
  }

  result.unk_d4 = 0x2a00161a;
  result.spiPsInControl = 2;
  result.paClClipCntl = 0x0009'0000;
  result.paSuScModeCntl.polyMode = gnm::PolyMode::Dual;
  result.vgtGsPerEs = 256;
  result.vgtEsPerGs = 128;
  result.vgtGsPerVs = 2;
  result.iaMultiVgtParam = 0xff;
  result.unk_2f7 = 0x00001000;
  result.paSuVtxCntl.pixCenterHalf = true;
  result.paSuVtxCntl.roundMode = gnm::RoundMode::RoundToEven;
  result.paClGbVertClipAdj = 1.0f;
  result.paClGbVertDiscAdj = 1.0f;
  result.paClGbHorzClipAdj = 1.0f;
  result.paClGbHorzDiscAdj = 1.0f;
  result.unk_316 = 0xe;
  result.vgtOutDeallocCntl = 0x10;
  return result;
}();

