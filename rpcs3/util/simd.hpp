#pragma once

#include "util/StrFmt.h"
#include "util/types.hpp"
#include "util/v128.hpp"
#include "util/sysinfo.hpp"
#include "util/asm.hpp"
#include "util/JIT.h"
#include <rx/simd.hpp>

#if defined(ARCH_X64)
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

#include <immintrin.h>
#include <emmintrin.h>
#endif

#if defined(ARCH_ARM64)
#include <arm_neon.h>
#endif

#include <algorithm>
#include <cmath>
#include <math.h>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

using namespace rx;

namespace asmjit
{
	struct vec_builder;
}

inline thread_local asmjit::vec_builder* g_vc = nullptr;

namespace asmjit
{
#if defined(ARCH_X64)
	using gpr_type = x86::Gp;
	using vec_type = x86::Xmm;
	using mem_type = x86::Mem;
#else
	struct gpr_type : Operand
	{
		gpr_type() = default;
		gpr_type(u32)
		{
		}
	};

	struct vec_type : Operand
	{
		vec_type() = default;
		vec_type(u32)
		{
		}
	};

	struct mem_type : Operand
	{
	};
#endif

	struct mem_lazy : Operand
	{
		const Operand& eval(bool is_lv);
	};

	enum class arg_class : u32
	{
		reg_lv, // const auto x = gv_...(y, z);
		reg_rv, // r = gv_...(y, z);
		imm_lv,
		imm_rv,
		mem_lv,
		mem_rv,
	};

	constexpr arg_class operator+(arg_class _base, u32 off)
	{
		return arg_class(u32(_base) + off);
	}

	template <typename... Args>
	constexpr bool any_operand_v = (std::is_base_of_v<Operand, std::decay_t<Args>> || ...);

	template <typename T, typename D = std::decay_t<T>>
	constexpr arg_class arg_classify =
		std::is_same_v<v128, D>        ? arg_class::imm_lv + !std::is_reference_v<T> :
		std::is_base_of_v<mem_type, D> ? arg_class::mem_lv :
		std::is_base_of_v<mem_lazy, D> ? arg_class::mem_lv + !std::is_reference_v<T> :
		std::is_reference_v<T>         ? arg_class::reg_lv :
										 arg_class::reg_rv;

	struct vec_builder : native_asm
	{
		using base = native_asm;

		bool fail_flag = false;

		vec_builder(CodeHolder* ch)
			: native_asm(ch)
		{
			if (!g_vc)
			{
				g_vc = this;
			}
		}

		~vec_builder()
		{
			if (g_vc == this)
			{
				g_vc = nullptr;
			}
		}

		u32 vec_allocated = 0xffffffff << 6;

		vec_type vec_alloc()
		{
			if (!~vec_allocated)
			{
				fail_flag = true;
				return vec_type{0};
			}

			const u32 idx = std::countr_one(vec_allocated);
			vec_allocated |= vec_allocated + 1;
			return vec_type{idx};
		}

		template <u32 Size>
		std::array<vec_type, Size> vec_alloc()
		{
			std::array<vec_type, Size> r;
			for (auto& x : r)
			{
				x = vec_alloc();
			}
			return r;
		}

		void vec_dealloc(vec_type vec)
		{
			vec_allocated &= ~(1u << vec.id());
		}

		void emit_consts()
		{
			//  (TODO: sort in use order)
			for (u32 sz = 1; sz <= 16; sz++)
			{
				for (auto& [key, _label] : consts[sz - 1])
				{
					base::align(AlignMode::kData, 1u << std::countr_zero<u32>(sz));
					base::bind(_label);
					base::embed(&key, sz);
				}
			}
		}

		std::unordered_map<v128, Label> consts[16]{};

#if defined(ARCH_X64)
		std::unordered_map<v128, vec_type> const_allocs{};

		template <typename T, u32 Size = sizeof(T)>
		x86::Mem get_const(const T& data, u32 esize = Size)
		{
			static_assert(Size <= 16);

			// Find existing const
			v128 key{};
			std::memcpy(&key, &data, Size);

			if (Size == 16 && esize == 4 && key._u64[0] == key._u64[1] && key._u32[0] == key._u32[1])
			{
				x86::Mem r = get_const<u32>(key._u32[0]);
				r.setBroadcast(x86::Mem::Broadcast::k1To4);
				return r;
			}

			if (Size == 16 && esize == 8 && key._u64[0] == key._u64[1])
			{
				x86::Mem r = get_const<u64>(key._u64[0]);
				r.setBroadcast(x86::Mem::Broadcast::k1To2);
				return r;
			}

			auto& _label = consts[Size - 1][key];

			if (!_label.isValid())
				_label = base::newLabel();

			return x86::Mem(_label, 0, Size);
		}
#endif
	};

	struct free_on_exit
	{
		Operand x{};

		free_on_exit() = default;
		free_on_exit(const free_on_exit&) = delete;
		free_on_exit& operator=(const free_on_exit&) = delete;

		~free_on_exit()
		{
			if (x.isReg())
			{
				vec_type v;
				v.copyFrom(x);
				g_vc->vec_dealloc(v);
			}
		}
	};

#if defined(ARCH_X64)
	inline Operand arg_eval(v128& _c, u32 esize)
	{
		const auto found = g_vc->const_allocs.find(_c);

		if (found != g_vc->const_allocs.end())
		{
			return found->second;
		}

		vec_type reg = g_vc->vec_alloc();

		// TODO: PSHUFD style broadcast? Needs known const layout
		if (utils::has_avx() && _c._u64[0] == _c._u64[1])
		{
			if (_c._u32[0] == _c._u32[1])
			{
				if (utils::has_avx2() && _c._u16[0] == _c._u16[1])
				{
					if (_c._u8[0] == _c._u8[1])
					{
						ensure(!g_vc->vpbroadcastb(reg, g_vc->get_const(_c._u8[0])));
					}
					else
					{
						ensure(!g_vc->vpbroadcastw(reg, g_vc->get_const(_c._u16[0])));
					}
				}
				else
				{
					ensure(!g_vc->vbroadcastss(reg, g_vc->get_const(_c._u32[0])));
				}
			}
			else
			{
				ensure(!g_vc->vbroadcastsd(reg, g_vc->get_const(_c._u32[0])));
			}
		}
		else if (!_c._u)
		{
			ensure(!g_vc->pxor(reg, reg));
		}
		else if (!~_c._u)
		{
			ensure(!g_vc->pcmpeqd(reg, reg));
		}
		else
		{
			ensure(!g_vc->movaps(reg, g_vc->get_const(_c, esize)));
		}

		g_vc->const_allocs.emplace(_c, reg);
		return reg;
	}

	inline Operand arg_eval(v128&& _c, u32 esize)
	{
		const auto found = g_vc->const_allocs.find(_c);

		if (found != g_vc->const_allocs.end())
		{
			vec_type r = found->second;
			g_vc->const_allocs.erase(found);
			g_vc->vec_dealloc(r);
			return r;
		}

		// Hack: assume can use mem op (TODO)
		return g_vc->get_const(_c, esize);
	}

	template <typename T>
		requires(std::is_base_of_v<mem_lazy, std::decay_t<T>>)
	inline decltype(auto) arg_eval(T&& mem, u32)
	{
		return mem.eval(std::is_reference_v<T>);
	}

	inline decltype(auto) arg_eval(const Operand& mem, u32)
	{
		return mem;
	}

	inline decltype(auto) arg_eval(Operand& mem, u32)
	{
		return mem;
	}

	inline decltype(auto) arg_eval(Operand&& mem, u32)
	{
		return std::move(mem);
	}

	inline void arg_free(const v128&)
	{
	}

	inline void arg_free(const Operand& op)
	{
		if (op.isReg())
		{
			g_vc->vec_dealloc(vec_type{op.id()});
		}
	}

	template <typename T>
	inline bool arg_use_evex(const auto& op)
	{
		constexpr auto _class = arg_classify<T>;
		if constexpr (_class == arg_class::imm_rv)
			return g_vc->const_allocs.count(op) == 0;
		else if constexpr (_class == arg_class::imm_lv)
			return false;
		else if (op.isMem())
		{
			// Check if broadcast is set, or if the offset immediate can use disp8*N encoding
			mem_type mem{};
			mem.copyFrom(op);
			if (mem.hasBaseLabel())
				return false;
			if (mem.hasBroadcast())
				return true;
			if (!mem.hasOffset() || mem.offset() % mem.size() || u64(mem.offset() + 128) < 256 || u64(mem.offset() / mem.size() + 128) >= 256)
				return false;
			return true;
		}

		return false;
	}

	template <typename A, typename... Args>
	vec_type unary_op(x86::Inst::Id op, x86::Inst::Id op2, A&& a, Args&&... args)
	{
		if constexpr (arg_classify<A> == arg_class::reg_rv)
		{
			if (op)
			{
				ensure(!g_vc->emit(op, a, std::forward<Args>(args)...));
			}
			else
			{
				ensure(!g_vc->emit(op2, a, a, std::forward<Args>(args)...));
			}

			return a;
		}
		else
		{
			vec_type r = g_vc->vec_alloc();

			if (op)
			{
				if (op2 && utils::has_avx())
				{
					// Assume op2 is AVX (but could be PSHUFD as well for example)
					ensure(!g_vc->emit(op2, r, arg_eval(std::forward<A>(a), 16), std::forward<Args>(args)...));
				}
				else
				{
					// TODO
					ensure(!g_vc->emit(x86::Inst::Id::kIdMovaps, r, arg_eval(std::forward<A>(a), 16)));
					ensure(!g_vc->emit(op, r, std::forward<Args>(args)...));
				}
			}
			else
			{
				ensure(!g_vc->emit(op2, r, arg_eval(std::forward<A>(a), 16), std::forward<Args>(args)...));
			}

			return r;
		}
	}

	template <typename D, typename S>
	void store_op(x86::Inst::Id op, x86::Inst::Id evex_op, D&& d, S&& s)
	{
		static_assert(arg_classify<D> == arg_class::mem_lv);

		mem_type dst;
		dst.copyFrom(arg_eval(std::forward<D>(d), 16));

		if (utils::has_avx512() && evex_op)
		{
			if (!dst.hasBaseLabel() && dst.hasOffset() && dst.offset() % dst.size() == 0 && u64(dst.offset() + 128) >= 256 && u64(dst.offset() / dst.size() + 128) < 256)
			{
				ensure(!g_vc->evex().emit(evex_op, dst, arg_eval(std::forward<S>(s), 16)));
				return;
			}
		}

		ensure(!g_vc->emit(op, dst, arg_eval(std::forward<S>(s), 16)));
	}

	template <typename A, typename B, typename... Args>
	vec_type binary_op(u32 esize, x86::Inst::Id mov_op, x86::Inst::Id sse_op, x86::Inst::Id avx_op, x86::Inst::Id evex_op, A&& a, B&& b, Args&&... args)
	{
		free_on_exit e;
		Operand src1{};

		if constexpr (arg_classify<A> == arg_class::reg_rv)
		{
			// Use src1 as a destination
			src1 = arg_eval(std::forward<A>(a), 16);

			if (utils::has_avx512() && evex_op && arg_use_evex<B>(b))
			{
				ensure(!g_vc->evex().emit(evex_op, src1, src1, arg_eval(std::forward<B>(b), esize), std::forward<Args>(args)...));
				return vec_type{src1.id()};
			}

			if constexpr (arg_classify<B> == arg_class::reg_rv)
			{
				e.x = b;
			}
		}
		else if (utils::has_avx() && avx_op && (arg_classify<A> == arg_class::reg_lv || arg_classify<A> == arg_class::mem_lv))
		{
			Operand srca = arg_eval(std::forward<A>(a), 16);

			if constexpr (arg_classify<A> == arg_class::reg_lv)
			{
				if constexpr (arg_classify<B> == arg_class::reg_rv)
				{
					// Use src2 as a destination
					src1 = arg_eval(std::forward<B>(b), 16);
				}
				else
				{
					// Use new reg as a destination
					src1 = g_vc->vec_alloc();
				}
			}
			else
			{
				src1 = g_vc->vec_alloc();

				if constexpr (arg_classify<B> == arg_class::reg_rv)
				{
					e.x = b;
				}
			}

			if (utils::has_avx512() && evex_op && arg_use_evex<B>(b))
			{
				ensure(!g_vc->evex().emit(evex_op, src1, srca, arg_eval(std::forward<B>(b), esize), std::forward<Args>(args)...));
				return vec_type{src1.id()};
			}

			ensure(!g_vc->emit(avx_op, src1, srca, arg_eval(std::forward<B>(b), 16), std::forward<Args>(args)...));
			return vec_type{src1.id()};
		}
		else
			do
			{
				if constexpr (arg_classify<A> == arg_class::mem_rv)
				{
					if (a.isReg())
					{
						src1 = vec_type(a.id());

						if constexpr (arg_classify<B> == arg_class::reg_rv)
						{
							e.x = b;
						}
						break;
					}
				}

				if constexpr (arg_classify<A> == arg_class::imm_rv)
				{
					if (auto found = g_vc->const_allocs.find(a); found != g_vc->const_allocs.end())
					{
						src1 = found->second;
						g_vc->const_allocs.erase(found);

						if constexpr (arg_classify<B> == arg_class::reg_rv)
						{
							e.x = b;
						}
						break;
					}
				}

				src1 = g_vc->vec_alloc();

				if constexpr (arg_classify<B> == arg_class::reg_rv)
				{
					e.x = b;
				}

				if constexpr (arg_classify<A> == arg_class::imm_rv)
				{
					if (!a._u)
					{
						// All zeros
						ensure(!g_vc->emit(x86::Inst::kIdPxor, src1, src1));
						break;
					}
					else if (!~a._u)
					{
						// All ones
						ensure(!g_vc->emit(x86::Inst::kIdPcmpeqd, src1, src1));
						break;
					}
				}

				// Fallback to arg copy
				ensure(!g_vc->emit(mov_op, src1, arg_eval(std::forward<A>(a), 16)));
			} while (0);

		if (utils::has_avx512() && evex_op && arg_use_evex<B>(b))
		{
			ensure(!g_vc->evex().emit(evex_op, src1, src1, arg_eval(std::forward<B>(b), esize), std::forward<Args>(args)...));
		}
		else if (sse_op)
		{
			ensure(!g_vc->emit(sse_op, src1, arg_eval(std::forward<B>(b), 16), std::forward<Args>(args)...));
		}
		else
		{
			ensure(!g_vc->emit(avx_op, src1, src1, arg_eval(std::forward<B>(b), 16), std::forward<Args>(args)...));
		}

		return vec_type{src1.id()};
	}
#define FOR_X64(f, ...)                   \
	do                                    \
	{                                     \
		using enum asmjit::x86::Inst::Id; \
		return asmjit::f(__VA_ARGS__);    \
	} while (0)
#elif defined(ARCH_ARM64)
#define FOR_X64(...)                                                  \
	do                                                                \
	{                                                                 \
		fmt::throw_exception("Unimplemented for this architecture!"); \
	} while (0)
#endif
} // namespace asmjit

namespace rx
{
	inline bool g_use_avx = utils::has_avx();

	inline void gv_zeroupper()
	{
#if defined(ARCH_X64)
		if (!g_use_avx)
			return;
#if defined(_M_X64) && defined(_MSC_VER)
		_mm256_zeroupper();
#else
		__asm__ volatile("vzeroupper;");
#endif
#endif
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline asmjit::vec_type gv_gts32(A&&, B&&);

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_and32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPand, kIdVpand, kIdVpandd, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_andfs(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovaps, kIdAndps, kIdVandps, kIdVandps, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_andn32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPandn, kIdVpandn, kIdVpandnd, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_andnfs(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovaps, kIdAndnps, kIdVandnps, kIdVandnps, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_or32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPor, kIdVpor, kIdVpord, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_orfs(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovaps, kIdOrps, kIdVorps, kIdVorps, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_xor32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPxor, kIdVpxor, kIdVpxord, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_xorfs(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovaps, kIdXorps, kIdVxorps, kIdVxorps, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_not32(A&& a)
	{
#if defined(ARCH_X64)
		asmjit::vec_type ones = g_vc->vec_alloc();
		g_vc->pcmpeqd(ones, ones);
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPxor, kIdVpxor, kIdVpxord, std::move(ones), std::forward<A>(a));
#endif
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_notfs(A&& a)
	{
#if defined(ARCH_X64)
		asmjit::vec_type ones = g_vc->vec_alloc();
		g_vc->pcmpeqd(ones, ones);
		FOR_X64(binary_op, 4, kIdMovaps, kIdXorps, kIdVxorps, kIdVxorps, std::move(ones), std::forward<A>(a));
#endif
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shl16(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsllw, kIdVpsllw, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shl32(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPslld, kIdVpslld, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shl64(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsllq, kIdVpsllq, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shr16(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsrlw, kIdVpsrlw, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shr32(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsrld, kIdVpsrld, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shr64(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsrlq, kIdVpsrlq, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_sar16(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsraw, kIdVpsraw, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_sar32(A&& a, u32 count)
	{
		FOR_X64(unary_op, kIdPsrad, kIdVpsrad, std::forward<A>(a), count);
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_sar64(A&& a, u32 count)
	{
		if (count >= 64)
			count = 63;
#if defined(ARCH_X64)
		using enum asmjit::x86::Inst::Id;
		if (utils::has_avx512())
			return asmjit::unary_op(kIdNone, kIdVpsraq, std::forward<A>(a), count);
		g_vc->fail_flag = true;
		return std::forward<A>(a);
#endif
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_add8(A&& a, B&& b)
	{
		FOR_X64(binary_op, 1, kIdMovdqa, kIdPaddb, kIdVpaddb, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_add16(A&& a, B&& b)
	{
		FOR_X64(binary_op, 2, kIdMovdqa, kIdPaddw, kIdVpaddw, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_add32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPaddd, kIdVpaddd, kIdVpaddd, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_add64(A&& a, B&& b)
	{
		FOR_X64(binary_op, 8, kIdMovdqa, kIdPaddq, kIdVpaddq, kIdVpaddq, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_adds_s8(A&& a, B&& b)
	{
		FOR_X64(binary_op, 1, kIdMovdqa, kIdPaddsb, kIdVpaddsb, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_adds_s16(A&& a, B&& b)
	{
		FOR_X64(binary_op, 2, kIdMovdqa, kIdPaddsw, kIdVpaddsw, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_adds_s32(A&& a, B&& b)
	{
#if defined(ARCH_X64)
		auto s = gv_add32(a, b);
		auto m = gv_and32(gv_xor32(std::forward<A>(a), s), gv_xor32(std::forward<B>(b), s));
		auto x = gv_sar32(m, 31);
		auto y = gv_sar32(gv_and32(s, std::move(m)), 31);
		auto z = gv_xor32(gv_shr32(x, 1), std::move(y));
		return gv_xor32(std::move(z), gv_or32(std::move(s), std::move(x)));
#endif
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_addus_u8(A&& a, B&& b)
	{
		FOR_X64(binary_op, 1, kIdMovdqa, kIdPaddusb, kIdVpaddusb, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_addus_u16(A&& a, B&& b)
	{
		FOR_X64(binary_op, 2, kIdMovdqa, kIdPaddusw, kIdVpaddusw, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline asmjit::vec_type gv_minu32(A&& a, B&& b)
	{
#if defined(ARCH_X64)
		if (utils::has_sse41())
			FOR_X64(binary_op, 4, kIdMovdqa, kIdPminud, kIdVpminud, kIdVpminud, std::forward<A>(a), std::forward<B>(b));
		auto s = gv_bcst32(0x80000000);
		auto x = gv_xor32(a, s);
		auto m = gv_gts32(std::move(x), gv_xor32(std::move(s), b));
		auto z = gv_and32(m, std::move(b));
		return gv_or32(std::move(z), gv_andn32(std::move(m), std::move(a)));
#endif
		return {};
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline asmjit::vec_type gv_addus_u32(A&& a, B&& b)
	{
#if defined(ARCH_X64)
		if (utils::has_sse41())
			return gv_add32(gv_minu32(std::forward<B>(b), gv_not32(a)), std::forward<A>(a));
		auto s = gv_add32(a, b);
		auto x = gv_xor32(std::forward<B>(b), gv_bcst32(0x80000000));
		auto y = gv_xor32(std::forward<A>(a), gv_bcst32(0x7fffffff));
		return gv_or32(std::move(s), gv_gts32(std::move(x), std::move(y)));
#endif
		return {};
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline auto gv_sub8(A&& a, B&& b)
	{
		FOR_X64(binary_op, 1, kIdMovdqa, kIdPsubb, kIdVpsubb, kIdNone, std::forward<A>(a), std::forward<B>(b));
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline asmjit::vec_type gv_gts8(A&& a, B&& b)
	{
		FOR_X64(binary_op, 1, kIdMovdqa, kIdPcmpgtb, kIdVpcmpgtb, kIdNone, std::forward<A>(a), std::forward<B>(b));
		return {};
	}

	template <typename A, typename B>
		requires(asmjit::any_operand_v<A, B>)
	inline asmjit::vec_type gv_gts32(A&& a, B&& b)
	{
		FOR_X64(binary_op, 4, kIdMovdqa, kIdPcmpgtd, kIdVpcmpgtd, kIdNone, std::forward<A>(a), std::forward<B>(b));
		return {};
	}

	template <typename A, typename B, typename C>
		requires(asmjit::any_operand_v<A, B, C>)
	inline asmjit::vec_type gv_signselect8(A&& bits, B&& _true, C&& _false)
	{
		using namespace asmjit;
#if defined(ARCH_X64)
		if (utils::has_avx())
		{
			Operand arg0{};
			Operand arg1 = arg_eval(std::forward<A>(bits), 16);
			Operand arg2 = arg_eval(std::forward<B>(_true), 16);
			Operand arg3 = arg_eval(std::forward<C>(_false), 16);
			if constexpr (!std::is_reference_v<A>)
				arg0.isReg() ? arg_free(bits) : arg0.copyFrom(arg1);
			if constexpr (!std::is_reference_v<B>)
				arg0.isReg() ? arg_free(_true) : arg0.copyFrom(arg2);
			if constexpr (!std::is_reference_v<C>)
				arg0.isReg() ? arg_free(_false) : arg0.copyFrom(arg3);
			if (arg0.isNone())
				arg0 = g_vc->vec_alloc();
			g_vc->emit(x86::Inst::kIdVpblendvb, arg0, arg3, arg2, arg1);
			vec_type r;
			r.copyFrom(arg0);
			return r;
		}
#endif
		g_vc->fail_flag = true;
		return vec_type{0};
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_extend_lo_s8(A&& a)
	{
#if defined(ARCH_X64)
		using enum asmjit::x86::Inst::Id;
		if (utils::has_sse41())
			return asmjit::unary_op(kIdNone, kIdPmovsxbw, std::forward<A>(a));
		return asmjit::unary_op(kIdPsraw, kIdVpsraw, asmjit::unary_op(kIdNone, kIdPunpcklbw, std::forward<A>(a)), 8);
#endif
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_extend_hi_s8(A&& a)
	{
#if defined(ARCH_X64)
		using enum asmjit::x86::Inst::Id;
		return asmjit::unary_op(kIdPsraw, kIdVpsraw, asmjit::unary_op(kIdNone, kIdPunpckhbw, std::forward<A>(a)), 8);
#endif
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_extend_lo_s16(A&& a)
	{
#if defined(ARCH_X64)
		using enum asmjit::x86::Inst::Id;
		if (utils::has_sse41())
			return asmjit::unary_op(kIdNone, kIdPmovsxwd, std::forward<A>(a));
		return asmjit::unary_op(kIdPsrad, kIdVpsrad, asmjit::unary_op(kIdNone, kIdPunpcklwd, std::forward<A>(a)), 16);
#endif
	}

	template <typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_extend_hi_s16(A&& a)
	{
#if defined(ARCH_X64)
		using enum asmjit::x86::Inst::Id;
		return asmjit::unary_op(kIdPsrad, kIdVpsrad, asmjit::unary_op(kIdNone, kIdPunpckhwd, std::forward<A>(a)), 16);
#endif
	}

	template <u32 Count, typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shuffle_left(A&& a)
	{
		FOR_X64(unary_op, kIdPslldq, kIdVpslldq, std::forward<A>(a), Count);
	}

	template <u32 Count, typename A>
		requires(asmjit::any_operand_v<A>)
	inline auto gv_shuffle_right(A&& a)
	{
		FOR_X64(unary_op, kIdPsrldq, kIdVpsrldq, std::forward<A>(a), Count);
	}

	// For each 8-bit element, r = (a << (c & 7)) | (b >> (~c & 7) >> 1)
	template <typename A, typename B, typename C>
	inline auto gv_fshl8(A&& a, B&& b, C&& c)
	{
#if defined(ARCH_ARM64)
		const auto amt1 = vandq_s8(c, gv_bcst8(7));
		const auto amt2 = vsubq_s8(amt1, gv_bcst8(8));
		return v128(vorrq_u8(vshlq_u8(a, amt1), vshlq_u8(b, amt2)));
#else
		auto x1 = gv_sub8(gv_add8(a, a), gv_gts8(gv_bcst8(0), b));
		auto s1 = gv_shl64(c, 7);
		auto r1 = gv_signselect8(s1, std::move(x1), std::forward<A>(a));
		auto b1 = gv_signselect8(std::move(s1), gv_shl64(b, 1), std::forward<B>(b));
		auto c2 = gv_bcst8(0x3);
		auto x2 = gv_and32(gv_shr64(b1, 6), c2);
		x2 = gv_or32(std::move(x2), gv_andn32(std::move(c2), gv_shl64(r1, 2)));
		auto s2 = gv_shl64(c, 6);
		auto r2 = gv_signselect8(s2, std::move(x2), std::move(r1));
		auto b2 = gv_signselect8(std::move(s2), gv_shl64(b1, 2), std::move(b1));
		auto c3 = gv_bcst8(0xf);
		auto x3 = gv_and32(gv_shr64(std::move(b2), 4), c3);
		x3 = gv_or32(std::move(x3), gv_andn32(std::move(c3), gv_shl64(r2, 4)));
		return gv_signselect8(gv_shl64(std::move(c), 5), std::move(x3),
			std::move(r2));
#endif
	}

	// For each 8-bit element, r = (b >> (c & 7)) | (a << (~c & 7) << 1)
	template <typename A, typename B, typename C>
	inline auto gv_fshr8(A&& a, B&& b, C&& c)
	{
#if defined(ARCH_ARM64)
		const auto amt1 = vandq_s8(c, gv_bcst8(7));
		const auto amt2 = vsubq_s8(gv_bcst8(8), amt1);
		return vorrq_u8(vshlq_u8(b, vnegq_s8(amt1)), vshlq_u8(a, amt2));
#else
		auto c1 = gv_bcst8(0x7f);
		auto x1 = gv_and32(gv_shr64(b, 1), c1);
		x1 = gv_or32(std::move(x1), gv_andn32(std::move(c1), gv_shl64(a, 7)));
		auto s1 = gv_shl64(c, 7);
		auto r1 = gv_signselect8(s1, std::move(x1), std::move(b));
		auto a1 = gv_signselect8(std::move(s1), gv_shr64(a, 1), std::move(a));
		auto c2 = gv_bcst8(0x3f);
		auto x2 = gv_and32(gv_shr64(r1, 2), c2);
		x2 = gv_or32(std::move(x2), gv_andn32(std::move(c2), gv_shl64(a1, 6)));
		auto s2 = gv_shl64(c, 6);
		auto r2 = gv_signselect8(s2, std::move(x2), std::move(r1));
		auto a2 = gv_signselect8(std::move(s2), gv_shr64(a1, 2), std::move(a1));
		auto c3 = gv_bcst8(0x0f);
		auto x3 = gv_and32(gv_shr64(r2, 4), c3);
		x3 = gv_or32(std::move(x3),
			gv_andn32(std::move(c3), gv_shl64(std::move(a2), 4)));
		return gv_signselect8(gv_shl64(std::move(c), 5), std::move(x3),
			std::move(r2));
#endif
	}
} // namespace rx

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
