// AI-generated code; reviewed for this repository's vNext rewrite.
#include "value_ops.hpp"

#include <bit>
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

    [[nodiscard]] auto walkStep(Value &current, const PlaceStep &step) -> Value &
    {
      if (step.kind == PlaceStep::Kind::Member)
      {
        if (current.isArray())
        {
          auto &array = current.asArrayMut();
          if (step.field >= array.size())
            throw bytecode::BytecodeError(std::format("array index out of bounds: index {}, length {}", step.field, array.size()));
          return array[step.field];
        }
        if (current.isTuple())
        {
          auto &tuple = current.asTupleMut();
          if (step.field >= tuple.size())
            throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", step.field, tuple.size()));
          return tuple[step.field];
        }
        if (current.isStruct())
        {
          auto &fields = current.asStructMut();
          if (step.field >= fields.size())
            throw bytecode::BytecodeError(std::format("struct field is out of range: index {}", step.field));
          return fields[step.field];
        }
        throw bytecode::BytecodeError("reference member step on a non-product value");
      }
      if (current.isArray())
      {
        auto &array = current.asArrayMut();
        if (step.index < 0 || static_cast<uint64_t>(step.index) >= array.size())
          throw bytecode::BytecodeError(std::format("array index out of bounds: index {}, length {}", step.index, array.size()));
        return array[static_cast<size_t>(step.index)];
      }
      if (current.isTuple())
      {
        auto &tuple = current.asTupleMut();
        if (step.index < 0 || static_cast<uint64_t>(step.index) >= tuple.size())
          throw bytecode::BytecodeError(std::format("tuple index out of bounds: index {}, length {}", step.index, tuple.size()));
        return tuple[static_cast<size_t>(step.index)];
      }
      throw bytecode::BytecodeError("reference index step on a non-aggregate value");
    }

    [[nodiscard]] auto decodePlaceSteps(const bytecode::DecodedInstruction &instruction, const std::vector<Value> &values)
        -> std::vector<PlaceStep>
    {
      const uint32_t count = instruction.operands[3];
      std::vector<PlaceStep> steps;
      steps.reserve(count / 2);
      for (uint32_t index = 0; index < count; index += 2)
      {
        const uint32_t kind = instruction.operands.at(4 + index);
        const uint32_t payload = instruction.operands.at(4 + index + 1);
        if (kind == 0)
        {
          steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = payload});
        }
        else
        {
          steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Index, .index = values.at(payload).asInteger()});
        }
      }
      return steps;
    }
  } // namespace

  void makeRefInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values,
                          const LocalCells &locals)
  {
    const uint32_t result = instruction.operands[0];
    if (values.size() <= result) values.resize(result + 1);
    const auto root = locals.find(instruction.operands[1]);
    if (root == locals.end()) throw bytecode::BytecodeError("bytecode reference root is not a bound local");
    values[result] = Value::reference(root->second, decodePlaceSteps(instruction, values), instruction.operands[2] != 0);
  }

  void loadRefInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values)
  {
    const uint32_t result = instruction.operands[0];
    if (values.size() <= result) values.resize(result + 1);
    const auto &reference = values.at(instruction.operands[1]).asReference();
    Value current = reference.root->deepCopy();
    for (const auto &step : reference.steps) current = walkStep(current, step).deepCopy();
    values[result] = std::move(current);
  }

  void assignPlaceInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values,
                              const LocalCells &locals)
  {
    Value *target = nullptr;
    std::vector<PlaceStep> prefix;
    if (instruction.operands[0] != 0)
    {
      const auto &reference = values.at(instruction.operands[1]).asReference();
      if (!reference.mutableRef) throw bytecode::BytecodeError("assignment through an immutable reference");
      target = reference.root.get();
      prefix = reference.steps;
    }
    else
    {
      const auto root = locals.find(instruction.operands[1]);
      if (root == locals.end()) throw bytecode::BytecodeError("bytecode place root is not a bound local");
      target = root->second.get();
    }
    for (const auto &step : prefix) target = &walkStep(*target, step);
    for (const auto &step : decodePlaceSteps(instruction, values)) target = &walkStep(*target, step);
    *target = values.at(instruction.operands[2]).deepCopy();
  }

  void loadVariantInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values)
  {
    const uint32_t result = instruction.operands[0];
    if (values.size() <= result) values.resize(result + 1);
    values[result] = Value::integer(values.at(instruction.operands[1]).asEnumVariant());
  }

  void extractPayloadInstruction(const bytecode::DecodedInstruction &instruction, std::vector<Value> &values)
  {
    const uint32_t result = instruction.operands[0];
    if (values.size() <= result) values.resize(result + 1);
    const auto &payload = values.at(instruction.operands[1]).asEnumPayload();
    values[result] = payload.empty() ? Value{} : payload.front().deepCopy();
  }

  void evaluateInstruction(const bytecode::DecodedInstruction &instruction, const std::vector<std::string> &stringConstants,
                           std::vector<Value> &values, const LocalCells &locals)
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
    if (kind == hir::ExpressionKind::FloatLiteral)
    {
      values[result] = Value::float_(std::bit_cast<double>(payload));
      return;
    }
    if (kind == hir::ExpressionKind::StringLiteral)
    {
      if (payload >= stringConstants.size()) throw bytecode::BytecodeError("string constant index is out of range");
      values[result] = Value::string(stringConstants.at(payload));
      return;
    }
    if (kind == hir::ExpressionKind::EnumLiteral)
    {
      std::vector<Value> enumPayload;
      for (size_t index = 0; index < instruction.operands[4]; ++index)
        enumPayload.push_back(values.at(instruction.operands[5 + index]).deepCopy());
      values[result] = Value::enumeration(instruction.operands[2], instruction.operands[3], std::move(enumPayload));
      return;
    }
    if (kind == hir::ExpressionKind::ArrayLiteral || kind == hir::ExpressionKind::TupleLiteral ||
        kind == hir::ExpressionKind::StructLiteral)
    {
      std::vector<Value> elements;
      elements.reserve(instruction.operands[4]);
      for (size_t index = 0; index < instruction.operands[4]; ++index)
        elements.push_back(values.at(instruction.operands[5 + index]).deepCopy());
      if (kind == hir::ExpressionKind::ArrayLiteral) values[result] = Value::array(std::move(elements));
      else if (kind == hir::ExpressionKind::TupleLiteral) values[result] = Value::tuple(std::move(elements));
      else values[result] = Value::structure(static_cast<uint32_t>(payload), std::move(elements));
      return;
    }
    if (kind == hir::ExpressionKind::ResolvedName)
    {
      const auto local = locals.find(static_cast<uint32_t>(payload));
      if (local == locals.end()) throw bytecode::BytecodeError("bytecode local is not bound");
      values[result] = local->second->deepCopy();
      return;
    }
    if (kind == hir::ExpressionKind::Grouped)
    {
      values[result] = values.at(instruction.operands[5]).deepCopy();
      return;
    }
    if (kind == hir::ExpressionKind::Prefix)
    {
      if (payload == 4)
      {
        values[result] = values.at(instruction.operands[5]);
        return;
      }
      if (payload == 5)
      {
        values[result] = values.at(instruction.operands[5]).deepCopy();
        return;
      }
      if (values.at(instruction.operands[5]).isDouble())
      {
        const double operand = values.at(instruction.operands[5]).asDouble();
        if (payload == 1) throw bytecode::BytecodeError("logical negation is not defined for floats");
        values[result] = payload == 2 ? Value::float_(-operand) : Value::float_(operand);
        return;
      }
      const int64_t operand = values.at(instruction.operands[5]).asInteger();
      switch (payload)
      {
      case 1: values[result] = Value::integer(operand == 0 ? 1 : 0); return;
      case 2:
        if (operand == std::numeric_limits<int64_t>::min()) throw bytecode::BytecodeError("integer negation overflow");
        values[result] = -operand;
        return;
      case 3: values[result] = operand; return;
      default: throw bytecode::BytecodeError("unsupported prefix operation");
      }
    }
    if (kind == hir::ExpressionKind::Member)
    {
      const auto &structure = values.at(instruction.operands[5]);
      const uint32_t field = static_cast<uint32_t>(payload);
      const auto &fields = structure.asStruct();
      if (field >= fields.size()) throw bytecode::BytecodeError("struct field is out of range");
      values[result] = fields[field].deepCopy();
      return;
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
        values[result] = tuple[static_cast<size_t>(index)].deepCopy();
        return;
      }
      const auto &array = receiver.asArray();
      if (index < 0 || static_cast<uint64_t>(index) >= array.size())
        throw bytecode::BytecodeError(std::format("array index out of bounds: index {}, length {}", index, array.size()));
      values[result] = array[static_cast<size_t>(index)].deepCopy();
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
      else if (payload == 6) values[result] = Value::integer(left == right ? 1 : 0);
      else values[result] = Value::integer(left != right ? 1 : 0);
      return;
    }
    if (payload == 19)
    {
      values[result] = Value::range(values.at(instruction.operands[5]).asInteger(),
                                    values.at(instruction.operands[6]).asInteger());
      return;
    }
    if (values.at(instruction.operands[5]).isDouble() || values.at(instruction.operands[6]).isDouble())
    {
      const double left = values.at(instruction.operands[5]).asNumber();
      const double right = values.at(instruction.operands[6]).asNumber();
      switch (payload)
      {
      case 1: values[result] = Value::float_(left + right); return;
      case 2: values[result] = Value::float_(left - right); return;
      case 3: values[result] = Value::float_(left * right); return;
      case 4: values[result] = Value::float_(left / right); return;
      case 6: values[result] = Value::integer(left == right ? 1 : 0); return;
      case 7: values[result] = Value::integer(left != right ? 1 : 0); return;
      case 8: values[result] = Value::integer(left < right ? 1 : 0); return;
      case 9: values[result] = Value::integer(left <= right ? 1 : 0); return;
      case 10: values[result] = Value::integer(left > right ? 1 : 0); return;
      case 11: values[result] = Value::integer(left >= right ? 1 : 0); return;
      default: throw bytecode::BytecodeError("unsupported float operation");
      }
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
    case 6: values[result] = Value::integer(left == right ? 1 : 0); return;
    case 7: values[result] = Value::integer(left != right ? 1 : 0); return;
    case 8: values[result] = Value::integer(left < right ? 1 : 0); return;
    case 9: values[result] = Value::integer(left <= right ? 1 : 0); return;
    case 10: values[result] = Value::integer(left > right ? 1 : 0); return;
    case 11: values[result] = Value::integer(left >= right ? 1 : 0); return;
    case 12: values[result] = Value::integer(left != 0 && right != 0 ? 1 : 0); return;
    case 13: values[result] = Value::integer(left != 0 || right != 0 ? 1 : 0); return;
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
