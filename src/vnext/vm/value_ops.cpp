// AI-generated code; reviewed for this repository's vNext rewrite.
#include "value_ops.hpp"

#include <format>
#include <limits>

namespace NG::vnext::vm::detail
{
  namespace
  {
    [[nodiscard]] auto checkedAdd(int64_t left, int64_t right) -> int64_t
    {
      if ((right > 0 && left > std::numeric_limits<int64_t>::max() - right) ||
          (right < 0 && left < std::numeric_limits<int64_t>::min() - right))
        throw bytecode::BytecodeError("integer addition overflow");
      return left + right;
    }

    [[nodiscard]] auto checkedSubtract(int64_t left, int64_t right) -> int64_t
    {
      if ((right > 0 && left < std::numeric_limits<int64_t>::min() + right) ||
          (right < 0 && left > std::numeric_limits<int64_t>::max() + right))
        throw bytecode::BytecodeError("integer subtraction overflow");
      return left - right;
    }

    [[nodiscard]] auto checkedMultiply(int64_t left, int64_t right) -> int64_t
    {
      if (left == 0 || right == 0) return 0;
      if ((left > 0 && right > 0 && left > std::numeric_limits<int64_t>::max() / right) ||
          (left > 0 && right < 0 && right < std::numeric_limits<int64_t>::min() / left) ||
          (left < 0 && right > 0 && left < std::numeric_limits<int64_t>::min() / right) ||
          (left < 0 && right < 0 && left < std::numeric_limits<int64_t>::max() / right))
        throw bytecode::BytecodeError("integer multiplication overflow");
      return left * right;
    }
  } // namespace

  void assignIndexInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values)
  {
    auto &receiver = values.at(instruction.operands[0]);
    const int64_t index = values.at(instruction.operands[1]).asInteger();
    if (receiver.isTuple())
    {
      auto &tuple = receiver.asTupleMut();
      if (index < 0 || static_cast<uint64_t>(index) >= tuple.size())
        throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
      if (static_cast<uint64_t>(index) != instruction.operands[3])
        throw bytecode::BytecodeError("tuple projection index does not match verified metadata");
      tuple[static_cast<size_t>(index)] = values.at(instruction.operands[2]);
      return;
    }
    auto &array = receiver.asArrayMut();
    if (index < 0 || static_cast<uint64_t>(index) >= array.size())
      throw bytecode::BytecodeError(std::format("array index out of bounds: index {}, length {}", index, array.size()));
    array[static_cast<size_t>(index)] = values.at(instruction.operands[2]);
  }

  void evaluateInstruction(const bytecode::DecodedInstruction &instruction, const std::vector<std::string> &stringConstants,
                           std::vector<Value> &values, const std::unordered_map<uint32_t, Value> &locals)
  {
    const uint32_t result = instruction.operands[0];
    if (values.size() <= result) values.resize(result + 1);

    const auto kind = static_cast<hir::ExpressionKind>(instruction.operands[1]);
    const uint64_t payload = static_cast<uint64_t>(instruction.operands[2]) | (static_cast<uint64_t>(instruction.operands[3]) << 32);
    if (kind == hir::ExpressionKind::IntegerLiteral || kind == hir::ExpressionKind::BooleanLiteral)
    {
      values[result] = static_cast<int64_t>(payload);
      return;
    }
    if (kind == hir::ExpressionKind::StringLiteral)
    {
      if (payload >= stringConstants.size()) throw bytecode::BytecodeError("string constant index is out of range");
      values[result] = Value::string(stringConstants.at(payload));
      return;
    }
    if (kind == hir::ExpressionKind::ArrayLiteral || kind == hir::ExpressionKind::TupleLiteral)
    {
      std::vector<Value> elements;
      elements.reserve(instruction.operands[4]);
      for (size_t index = 0; index < instruction.operands[4]; ++index) elements.push_back(values.at(instruction.operands[5 + index]));
      values[result] = kind == hir::ExpressionKind::ArrayLiteral ? Value::array(std::move(elements)) : Value::tuple(std::move(elements));
      return;
    }
    if (kind == hir::ExpressionKind::ResolvedName)
    {
      values[result] = locals.at(static_cast<uint32_t>(payload));
      return;
    }
    if (kind == hir::ExpressionKind::Grouped)
    {
      values[result] = values.at(instruction.operands[5]);
      return;
    }
    if (kind == hir::ExpressionKind::Prefix)
    {
      const int64_t operand = values.at(instruction.operands[5]).asInteger();
      switch (payload)
      {
      case 1: values[result] = operand == 0; return;
      case 2:
        if (operand == std::numeric_limits<int64_t>::min()) throw bytecode::BytecodeError("integer negation overflow");
        values[result] = -operand;
        return;
      case 3: values[result] = operand; return;
      default: throw bytecode::BytecodeError("unsupported prefix operation");
      }
    }
    if (kind == hir::ExpressionKind::Index)
    {
      const auto &receiver = values.at(instruction.operands[5]);
      const int64_t index = values.at(instruction.operands[6]).asInteger();
      if (receiver.isTuple())
      {
        const auto &tuple = receiver.asTuple();
        if (index < 0 || static_cast<uint64_t>(index) >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", index, tuple.size()));
        if (static_cast<uint64_t>(index) != payload)
          throw bytecode::BytecodeError("tuple projection index does not match verified metadata");
        values[result] = tuple[static_cast<size_t>(index)];
        return;
      }
      const auto &array = receiver.asArray();
      if (index < 0 || static_cast<uint64_t>(index) >= array.size())
        throw bytecode::BytecodeError(std::format("array index out of bounds: index {}, length {}", index, array.size()));
      values[result] = array[static_cast<size_t>(index)];
      return;
    }
    if (kind != hir::ExpressionKind::Binary)
    {
      throw bytecode::BytecodeError("unsupported expression evaluation");
    }

    if ((payload == 1 || payload == 6 || payload == 7) && values.at(instruction.operands[5]).isString())
    {
      const auto &left = values.at(instruction.operands[5]).asString();
      const auto &right = values.at(instruction.operands[6]).asString();
      if (payload == 1) values[result] = Value::string(left + right);
      else if (payload == 6) values[result] = left == right;
      else values[result] = left != right;
      return;
    }
    const int64_t left = values.at(instruction.operands[5]).asInteger();
    const int64_t right = values.at(instruction.operands[6]).asInteger();
    switch (payload)
    {
    case 1: values[result] = checkedAdd(left, right); return;
    case 2: values[result] = checkedSubtract(left, right); return;
    case 3: values[result] = checkedMultiply(left, right); return;
    case 4:
      if (right == 0) throw bytecode::BytecodeError("integer division by zero");
      if (left == std::numeric_limits<int64_t>::min() && right == -1)
        throw bytecode::BytecodeError("integer division overflow");
      values[result] = left / right;
      return;
    case 5:
      if (right == 0) throw bytecode::BytecodeError("integer remainder by zero");
      if (left == std::numeric_limits<int64_t>::min() && right == -1)
        throw bytecode::BytecodeError("integer remainder overflow");
      values[result] = left % right;
      return;
    case 6: values[result] = left == right; return;
    case 7: values[result] = left != right; return;
    case 8: values[result] = left < right; return;
    case 9: values[result] = left <= right; return;
    case 10: values[result] = left > right; return;
    case 11: values[result] = left >= right; return;
    case 12: values[result] = left != 0 && right != 0; return;
    case 13: values[result] = left != 0 || right != 0; return;
    case 14: values[result] = left & right; return;
    case 15: values[result] = left | right; return;
    case 16: values[result] = left ^ right; return;
    case 17:
      if (right < 0 || right >= 64) throw bytecode::BytecodeError("integer shift count is out of range");
      values[result] = static_cast<int64_t>(static_cast<uint64_t>(left) << right);
      return;
    case 18:
      if (right < 0 || right >= 64) throw bytecode::BytecodeError("integer shift count is out of range");
      values[result] = static_cast<int64_t>(static_cast<uint64_t>(left) >> right);
      return;
    default: throw bytecode::BytecodeError("unsupported binary operation");
    }
  }
} // namespace NG::vnext::vm::detail
