// AI-generated code; reviewed for this repository's vNext rewrite.
#include "value_ops.hpp"

namespace NG::vnext::vm::detail
{
  void evaluateInstruction(const bytecode::DecodedInstruction &instruction, std::vector<int64_t> &values,
                           const std::unordered_map<uint32_t, int64_t> &locals)
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
      const int64_t operand = values.at(instruction.operands[5]);
      switch (payload)
      {
      case 1: values[result] = operand == 0; return;
      case 2: values[result] = -operand; return;
      case 3: values[result] = operand; return;
      default: throw bytecode::BytecodeError("unsupported prefix operation");
      }
    }
    if (kind != hir::ExpressionKind::Binary)
    {
      throw bytecode::BytecodeError("unsupported expression evaluation");
    }

    const int64_t left = values.at(instruction.operands[5]);
    const int64_t right = values.at(instruction.operands[6]);
    switch (payload)
    {
    case 1: values[result] = left + right; return;
    case 2: values[result] = left - right; return;
    case 3: values[result] = left * right; return;
    case 4:
      if (right == 0) throw bytecode::BytecodeError("integer division by zero");
      values[result] = left / right;
      return;
    case 5:
      if (right == 0) throw bytecode::BytecodeError("integer remainder by zero");
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
