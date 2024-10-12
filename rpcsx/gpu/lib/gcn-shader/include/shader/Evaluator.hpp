#pragma once
#include "eval.hpp"
#include <map>

namespace shader::eval {
class Evaluator {
  std::map<ir::Value, Value> values;

public:
  virtual ~Evaluator() = default;

  void invalidate(ir::Value node) { values.erase(node); }
  void invalidate() { values.clear(); }
  void setValue(ir::Value node, Value value) { values[node] = value; }

  Value eval(const ir::Operand &op, ir::Value type = nullptr);
  virtual Value eval(ir::Value op);
  virtual Value eval(ir::InstructionId instId,
                     std::span<const ir::Operand> operands);
};
} // namespace shader::eval
