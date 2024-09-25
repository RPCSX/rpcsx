#pragma once
#include "../ir/Block.hpp"
#include "../ir/Builder.hpp"
#include "../ir/Value.hpp"

namespace shader::ir {
template <typename T> inline constexpr Kind kOpToKind = Kind::Count;
}

namespace shader::ir::builtin {
enum Op {
  INVALID_INSTRUCTION,
  BLOCK,
  IF_ELSE,
  LOOP,
};

inline const char *getInstructionName(unsigned id) {
  switch (id) {
  case INVALID_INSTRUCTION:
    return "<invalid instruction>";

  case BLOCK:
    return "block";

  case IF_ELSE:
    return "ifElse";

  case LOOP:
    return "loop";
  }
  return nullptr;
}

template <typename ImplT>
struct Builder : BuilderFacade<Builder<ImplT>, ImplT> {
  /**
   * Creates an invalid instruction with the given location.
   *
   * @param location the location of the instruction
   *
   * @return the created invalid instruction
   */
  Instruction createInvalidInstruction(Location location) {
    return this->template create<Instruction>(location, Kind::Builtin,
                                              INVALID_INSTRUCTION);
  }

  Instruction createIfElse(Location location, Value cond, Block ifTrue,
                           Block ifFalse = {}) {
    std::vector<Operand> operands = {{cond, ifTrue}};
    if (ifFalse) {
      operands.push_back(ifFalse);
    }
    return this->template create<Instruction>(location, Kind::Builtin, IF_ELSE,
                                              operands);
  }

  Instruction createLoop(Location location, Block body) {
    return this->template create<Instruction>(location, Kind::Builtin, IF_ELSE,
                                              {{body}});
  }

  auto createBlock(Location location) {
    return this->template create<Block>(location);
  }

  auto createRegion(Location location) {
    return this->getContext().template create<Region>(location);
  }

  /**
   * Creates an instruction with the given location, kind, op, and operands.
   *
   * @param location the location of the instruction
   * @param kind the kind of the instruction
   * @param op the opcode of the instruction
   * @param operands the operands of the instruction
   *
   * @return the created instruction
   */
  Instruction createInstruction(Location location, Kind kind, unsigned op,
                                std::span<const Operand> operands = {}) {
    return this->template create<Instruction>(location, kind, op, operands);
  }

  template <typename OpT>
  Instruction createInstruction(Location location, OpT &&op,
                                std::span<const Operand> operands = {})
    requires requires {
      this->template create<Instruction>(
          location, kOpToKind<std::remove_cvref_t<OpT>>, op, operands);
    }
  {
    return this->template create<Instruction>(
        location, kOpToKind<std::remove_cvref_t<OpT>>, op, operands);
  }

  /**
   * Creates an Instruction object with the given location, kind, opcode, and
   * operands.
   *
   * @param location the location of the instruction
   * @param kind the kind of the instruction
   * @param op the opcode of the instruction
   * @param operands variadic parameter pack of operands for the instruction
   *
   * @return the created Instruction object
   */
  template <typename... T>
  Instruction createInstruction(Location location, Kind kind, unsigned op,
                                T &&...operands)
    requires requires {
      createInstruction(location, kind, op,
                        {{Operand(std::forward<T>(operands))...}});
    }
  {
    return createInstruction(location, kind, op,
                             {{Operand(std::forward<T>(operands))...}});
  }

  template <typename OpT, typename... T>
  Instruction createInstruction(Location location, OpT &&op, T &&...operands)
    requires requires {
      createInstruction(location, std::forward<OpT>(op),
                        {{Operand(std::forward<T>(operands))...}});
    }
  {
    return createInstruction(location, std::forward<OpT>(op),
                             {{Operand(std::forward<T>(operands))...}});
  }

  /**
   * Creates a Value object with the given location, kind, opcode, and operands.
   *
   * @param location the location of the Value object
   * @param kind the kind of the Value object
   * @param op the opcode of the Value object
   * @param operands a span of operands for the Value object
   *
   * @return the created Value object
   */
  auto createValue(Location location, Kind kind, unsigned op,
                   std::span<const Operand> operands = {}) {
    return this->template create<Value>(location, kind, op, operands);
  }

  template <typename OpT>
  auto createValue(Location location, OpT &&op,
                   std::span<const Operand> operands = {})
    requires requires {
      this->template create<Value>(
          location, kOpToKind<std::remove_cvref_t<OpT>>, op, operands);
    }
  {
    return this->template create<Value>(
        location, kOpToKind<std::remove_cvref_t<OpT>>, op, operands);
  }

  /**
   * Creates a Value object with the given location, kind, opcode, and operands.
   *
   * @param location the location of the Value object
   * @param kind the kind of the Value object
   * @param op the opcode of the Value object
   * @param operands variadic parameter pack of operands for the Value object
   *
   * @return the created Value object
   */
  template <typename... T>
  auto createValue(Location location, Kind kind, unsigned op, T &&...operands)
    requires requires {
      createValue(location, kind, op,
                  {{Operand(std::forward<T>(operands))...}});
    }
  {
    return createValue(location, kind, op,
                       {{Operand(std::forward<T>(operands))...}});
  }

  template <typename OpT, typename... T>
    requires requires { kOpToKind<std::remove_cvref_t<OpT>>; }
  auto createValue(Location location, OpT &&op, T &&...operands)
    requires requires {
      createValue(location, std::forward<OpT>(op),
                  {{Operand(std::forward<T>(operands))...}});
    }
  {
    return createValue(location, std::forward<OpT>(op),
                       {{Operand(std::forward<T>(operands))...}});
  }
};
} // namespace shader::ir::builtin
