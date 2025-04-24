#pragma once

#include "PPUOpcodes.h"
#include "rx/cpu/cell/ppu/Instruction.hpp"
#include "rx/cpu/cell/ppu/Opcode.hpp"
#include "rx/cpu/cell/ppu/PPUContext.hpp"
#include "rx/refl.hpp"
#include <array>

class ppu_thread;

using ppu_intrp_func_t = void (*)(ppu_thread& ppu_, ppu_opcode_t op, be_t<u32>* this_op, struct ppu_intrp_func* next_fn);

struct ppu_intrp_func
{
	ppu_intrp_func_t fn;
};

template <typename IT>
struct ppu_interpreter_t;

namespace asmjit
{
	struct ppu_builder;
}

struct ppu_interpreter_rt_base
{
protected:
	std::unique_ptr<ppu_interpreter_t<ppu_intrp_func_t>> ptrs;

	ppu_interpreter_rt_base() noexcept;

	ppu_interpreter_rt_base(const ppu_interpreter_rt_base&) = delete;

	ppu_interpreter_rt_base& operator=(const ppu_interpreter_rt_base&) = delete;

	virtual ~ppu_interpreter_rt_base();
};

struct ppu_interpreter_rt : ppu_interpreter_rt_base
{
	ppu_interpreter_rt() noexcept;

	ppu_intrp_func_t decode(u32 op) const noexcept;

private:
	ppu_decoder<ppu_interpreter_t<ppu_intrp_func_t>, ppu_intrp_func_t> table;
};

struct PPUContext;

struct PPUInterpreter
{
	std::array<void (*)(PPUContext& context, rx::cell::ppu::Instruction inst), rx::fieldCount<rx::cell::ppu::Opcode>> impl;
	PPUInterpreter();
	void interpret(PPUContext& context, std::uint32_t inst);
};
