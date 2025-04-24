#pragma once

#include "Opcode.hpp"
#include <array>
#include <cstdint>
#include <rx/refl.hpp>

namespace rx::cell::ppu {
template <typename T> using DecoderTable = std::array<T, 0x20000>;

extern DecoderTable<Opcode> g_ppuOpcodeTable;
// extern std::array<Form, rx::fieldCount<Opcode>> g_opcodeForms;

inline Opcode getOpcode(std::uint32_t instruction) {
  auto decode = [](std::uint32_t inst) {
    return ((inst >> 26) | (inst << 6)) & 0x1ffff; // Rotate + mask
  };

  return g_ppuOpcodeTable[decode(instruction)];
}

Opcode fixOpcode(Opcode opcode, std::uint32_t instruction);
} // namespace rx::cell::ppu
