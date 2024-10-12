#include "Evaluator.hpp"
#include "dialect.hpp"
#include "ir.hpp"

using namespace shader;

eval::Value eval::Evaluator::eval(const ir::Operand &op, ir::Value type) {
  if (auto val = op.getAsValue()) {
    auto [it, inserted] = values.try_emplace(val, Value{});
    if (inserted) {
      it->second = eval(val);
    }
    return it->second;
  }

  if (auto result = op.getAsInt32()) {
    if (type != nullptr) {
      bool isSigned = *type.getOperand(1).getAsInt32() != 0;
      switch (*type.getOperand(0).getAsInt32()) {
      case 8:
        if (isSigned) {
          return static_cast<std::int8_t>(*result);
        }

        return static_cast<std::uint8_t>(*result);

      case 16:
        if (isSigned) {
          return static_cast<std::int16_t>(*result);
        }

        return static_cast<std::uint16_t>(*result);

      case 32:
        if (isSigned) {
          return static_cast<std::int32_t>(*result);
        }

        return static_cast<std::uint32_t>(*result);
      }

      return {};
    }

    return *result;
  }

  if (auto result = op.getAsInt64()) {
    if (type != nullptr) {
      bool isSigned = *type.getOperand(1).getAsInt32() != 0;

      if (isSigned) {
        return static_cast<std::int64_t>(*result);
      }

      return static_cast<std::uint64_t>(*result);
    }

    return *result;
  }

  if (auto result = op.getAsBool()) {
    return *result;
  }

  if (auto result = op.getAsFloat()) {
    if (type != nullptr) {
      if (*type.getOperand(0).getAsInt32() == 16) {
        return static_cast<float16_t>(*result);
      }

      return static_cast<std::uint64_t>(*result);
    }

    return *result;
  }

  if (auto result = op.getAsDouble()) {
    return *result;
  }

  return {};
}
eval::Value eval::Evaluator::eval(ir::InstructionId instId,
                                  std::span<const ir::Operand> operands) {
  if (instId == ir::spv::OpConstant) {
    return eval(operands[1], operands[0].getAsValue());
  }

  if (instId == ir::spv::OpBitcast) {
    return eval(operands[1]).bitcast(operands[0].getAsValue());
  }

  if (instId == ir::spv::OpSConvert || instId == ir::spv::OpUConvert) {
    if (auto rhs = eval(operands[1])) {
      return rhs.iConvert(operands[0].getAsValue(),
                          instId == ir::spv::OpSConvert);
    }

    return {};
  }

  if (instId == ir::spv::OpSelect) {
    return eval(operands[1]).select(eval(operands[2]), eval(operands[3]));
  }

  if (instId == ir::spv::OpIAdd || instId == ir::spv::OpFAdd) {
    return eval(operands[1]) + eval(operands[2]);
  }
  if (instId == ir::spv::OpISub || instId == ir::spv::OpFSub) {
    return eval(operands[1]) - eval(operands[2]);
  }
  if (instId == ir::spv::OpSDiv || instId == ir::spv::OpUDiv ||
      instId == ir::spv::OpFDiv) {
    return eval(operands[1]) / eval(operands[2]);
  }
  if (instId == ir::spv::OpSMod || instId == ir::spv::OpUMod ||
      instId == ir::spv::OpFMod) {
    return eval(operands[1]) % eval(operands[2]);
  }
  if (instId == ir::spv::OpSRem) {
    return eval(operands[1]) % eval(operands[2]);
  }
  if (instId == ir::spv::OpFRem) {
    return eval(operands[1]) % eval(operands[2]);
  }
  if (instId == ir::spv::OpSNegate || instId == ir::spv::OpFNegate) {
    return -eval(operands[0]);
  }

  if (instId == ir::spv::OpNot) {
    return ~eval(operands[1]);
  }
  if (instId == ir::spv::OpLogicalNot) {
    return !eval(operands[1]);
  }

  if (instId == ir::spv::OpLogicalEqual || instId == ir::spv::OpIEqual) {
    return eval(operands[1]) == eval(operands[2]);
  }
  if (instId == ir::spv::OpLogicalNotEqual || instId == ir::spv::OpINotEqual) {
    return eval(operands[1]) != eval(operands[2]);
  }
  if (instId == ir::spv::OpLogicalOr) {
    return eval(operands[1]) || eval(operands[2]);
  }
  if (instId == ir::spv::OpLogicalAnd) {
    return eval(operands[1]) && eval(operands[2]);
  }
  if (instId == ir::spv::OpUGreaterThan || instId == ir::spv::OpSGreaterThan) {
    return eval(operands[1]) > eval(operands[2]);
  }
  if (instId == ir::spv::OpUGreaterThanEqual ||
      instId == ir::spv::OpSGreaterThanEqual) {
    return eval(operands[1]) >= eval(operands[2]);
  }
  if (instId == ir::spv::OpULessThan || instId == ir::spv::OpSLessThan) {
    return eval(operands[1]) < eval(operands[2]);
  }
  if (instId == ir::spv::OpULessThanEqual ||
      instId == ir::spv::OpSLessThanEqual) {
    return eval(operands[1]) <= eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdEqual) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) == eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordEqual) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) == eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdNotEqual) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) != eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordNotEqual) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) != eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdLessThan) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) < eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordLessThan) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) < eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdGreaterThan) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) > eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordGreaterThan) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) > eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdLessThanEqual) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) <= eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordLessThanEqual) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) <= eval(operands[2]);
  }
  if (instId == ir::spv::OpFOrdGreaterThanEqual) {
    return !eval(operands[1]).isNan() && !eval(operands[2]).isNan() &&
           eval(operands[1]) >= eval(operands[2]);
  }
  if (instId == ir::spv::OpFUnordGreaterThanEqual) {
    return eval(operands[1]).isNan() || eval(operands[2]).isNan() ||
           eval(operands[1]) >= eval(operands[2]);
  }
  if (instId == ir::spv::OpShiftRightLogical) {
    return eval(operands[1]) >> eval(operands[2]);
  }
  if (instId == ir::spv::OpShiftRightArithmetic) {
    return eval(operands[1]) >> eval(operands[2]);
  }
  if (instId == ir::spv::OpShiftLeftLogical) {
    return eval(operands[1]) << eval(operands[2]);
  }
  if (instId == ir::spv::OpBitwiseOr) {
    return eval(operands[1]) | eval(operands[2]);
  }
  if (instId == ir::spv::OpBitwiseXor) {
    return eval(operands[1]) ^ eval(operands[2]);
  }
  if (instId == ir::spv::OpBitwiseAnd) {
    return eval(operands[1]) & eval(operands[2]);
  }

  if (instId == ir::spv::OpIsNan) {
    return eval(operands[1]).isNan();
  }
  if (instId == ir::spv::OpIsInf) {
    return eval(operands[1]).isInf();
  }
  if (instId == ir::spv::OpIsFinite) {
    return eval(operands[1]).isFinite();
  }

  if (instId == ir::spv::OpCompositeConstruct) {
    std::vector<Value> constituents;
    constituents.reserve(operands.size() - 1);
    for (auto &op : operands.subspan(1)) {
      constituents.push_back(eval(op));
    }
    return Value::compositeConstruct(operands[0].getAsValue(), constituents);
  }

  if (instId == ir::spv::OpCompositeExtract) {
    auto composite = eval(operands[1].getAsValue());
    if (composite.empty()) {
      return{};
    }

    std::vector<Value> indexes;
    indexes.reserve(operands.size() - 2);
    for (auto &op : operands.subspan(2)) {
      indexes.push_back(eval(op));
    }

    if (indexes.size() != 1) {
      return{};
    }

    return composite.compositeExtract(indexes[0]);
  }

  return {};
}

eval::Value eval::Evaluator::eval(ir::Value op) {
  return eval(op.getInstId(), op.getOperands());
}
