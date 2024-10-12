#pragma once

#include "dialect/builtin.hpp" // IWYU pragma: export
#include "dialect/ds.hpp"     // IWYU pragma: export
#include "dialect/exp.hpp"    // IWYU pragma: export
#include "dialect/memssa.hpp" // IWYU pragma: export
#include "dialect/mimg.hpp"   // IWYU pragma: export
#include "dialect/mtbuf.hpp"  // IWYU pragma: export
#include "dialect/mubuf.hpp"  // IWYU pragma: export
#include "dialect/smrd.hpp"   // IWYU pragma: export
#include "dialect/sop1.hpp"   // IWYU pragma: export
#include "dialect/sop2.hpp"   // IWYU pragma: export
#include "dialect/sopc.hpp"   // IWYU pragma: export
#include "dialect/sopk.hpp"   // IWYU pragma: export
#include "dialect/sopp.hpp"   // IWYU pragma: export
#include "dialect/vintrp.hpp" // IWYU pragma: export
#include "dialect/vop1.hpp"   // IWYU pragma: export
#include "dialect/vop2.hpp"   // IWYU pragma: export
#include "dialect/vop3.hpp"   // IWYU pragma: export
#include "dialect/vopc.hpp"   // IWYU pragma: export

#include "dialect/spv.hpp" // IWYU pragma: export

#include "dialect/amdgpu.hpp"  // IWYU pragma: export
#include <concepts>

namespace shader::ir {
template <> inline constexpr Kind kOpToKind<spv::Op> = Kind::Spv;
template <> inline constexpr Kind kOpToKind<builtin::Op> = Kind::Builtin;
template <> inline constexpr Kind kOpToKind<amdgpu::Op> = Kind::AmdGpu;
template <> inline constexpr Kind kOpToKind<vop2::Op> = Kind::Vop2;
template <> inline constexpr Kind kOpToKind<sop2::Op> = Kind::Sop2;
template <> inline constexpr Kind kOpToKind<sopk::Op> = Kind::Sopk;
template <> inline constexpr Kind kOpToKind<smrd::Op> = Kind::Smrd;
template <> inline constexpr Kind kOpToKind<vop3::Op> = Kind::Vop3;
template <> inline constexpr Kind kOpToKind<mubuf::Op> = Kind::Mubuf;
template <> inline constexpr Kind kOpToKind<mtbuf::Op> = Kind::Mtbuf;
template <> inline constexpr Kind kOpToKind<mimg::Op> = Kind::Mimg;
template <> inline constexpr Kind kOpToKind<ds::Op> = Kind::Ds;
template <> inline constexpr Kind kOpToKind<vintrp::Op> = Kind::Vintrp;
template <> inline constexpr Kind kOpToKind<exp::Op> = Kind::Exp;
template <> inline constexpr Kind kOpToKind<vop1::Op> = Kind::Vop1;
template <> inline constexpr Kind kOpToKind<vopc::Op> = Kind::Vopc;
template <> inline constexpr Kind kOpToKind<sop1::Op> = Kind::Sop1;
template <> inline constexpr Kind kOpToKind<sopc::Op> = Kind::Sopc;
template <> inline constexpr Kind kOpToKind<sopp::Op> = Kind::Sopp;
template <> inline constexpr Kind kOpToKind<memssa::Op> = Kind::MemSSA;

template <typename T>
  requires(kOpToKind<std::remove_cvref_t<T>> != Kind::Count)
constexpr InstructionId getInstructionId(T op) {
  return getInstructionId(kOpToKind<std::remove_cvref_t<T>>, op);
}

constexpr bool operator==(ir::Instruction lhs, InstructionId rhs) {
  return lhs && lhs.getInstId() == rhs;
}

template <typename L, typename R>
constexpr bool operator==(L lhs, R rhs)
  requires requires {
    requires(!std::is_same_v<L, R>);
    { getInstructionId(lhs) == rhs } -> std::convertible_to<bool>;
  }
{
  return getInstructionId(lhs) == rhs;
}

template <typename L, typename R>
constexpr bool operator==(L lhs, R rhs)
  requires requires {
    requires(!std::is_same_v<L, R>);
    { getTypeId(lhs) == rhs } -> std::convertible_to<bool>;
  }
{
  return getTypeId(lhs) == rhs;
}
} // namespace ir
