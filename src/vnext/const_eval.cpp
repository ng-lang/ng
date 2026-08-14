// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/const_eval.hpp"

#include <charconv>
#include <format>
#include <limits>
#include <optional>
#include <utility>

namespace NG::vnext::const_eval
{
  namespace
  {
    [[nodiscard]] auto checkedAdd(int64_t left, int64_t right) -> std::optional<int64_t>
    {
      int64_t result{};
      if (__builtin_add_overflow(left, right, &result)) return std::nullopt;
      return result;
    }

    [[nodiscard]] auto checkedSub(int64_t left, int64_t right) -> std::optional<int64_t>
    {
      int64_t result{};
      if (__builtin_sub_overflow(left, right, &result)) return std::nullopt;
      return result;
    }

    [[nodiscard]] auto checkedMul(int64_t left, int64_t right) -> std::optional<int64_t>
    {
      int64_t result{};
      if (__builtin_mul_overflow(left, right, &result)) return std::nullopt;
      return result;
    }

    [[nodiscard]] auto checkedDiv(int64_t left, int64_t right) -> std::optional<int64_t>
    {
      if (left == std::numeric_limits<int64_t>::min() && right == -1) return std::nullopt;
      return left / right;
    }

    [[nodiscard]] auto checkedMod(int64_t left, int64_t right) -> std::optional<int64_t>
    {
      if (left == std::numeric_limits<int64_t>::min() && right == -1) return std::nullopt;
      return left % right;
    }

    [[nodiscard]] auto parseIntegerLiteral(const syntax::ConstIntegerLiteral &literal) -> int64_t
    {
      int64_t value{};
      const auto [end, error] = std::from_chars(literal.text.data(), literal.text.data() + literal.text.size(), value);
      if (error != std::errc{} || end != literal.text.data() + literal.text.size())
        throw ConstEvalError(std::format("const integer `{}` is out of range", literal.text), literal.span);
      return value;
    }
  } // namespace

  ConstInterner::ConstInterner() : values_({ConstValue{.kind = ConstValueKind::Invalid}})
  {
  }

  auto ConstInterner::append(ConstValue value) -> ConstValueId
  {
    values_.push_back(std::move(value));
    return ConstValueId{static_cast<uint32_t>(values_.size() - 1)};
  }

  auto ConstInterner::internUnit() -> ConstValueId
  {
    for (uint32_t index = 1; index < values_.size(); ++index)
      if (values_[index].kind == ConstValueKind::Unit) return ConstValueId{index};
    return append(ConstValue{.kind = ConstValueKind::Unit});
  }

  auto ConstInterner::internBool(bool value) -> ConstValueId
  {
    for (uint32_t index = 1; index < values_.size(); ++index)
      if (values_[index].kind == ConstValueKind::Bool && values_[index].boolValue == value) return ConstValueId{index};
    return append(ConstValue{.kind = ConstValueKind::Bool, .boolValue = value});
  }

  auto ConstInterner::internInteger(int64_t value) -> ConstValueId
  {
    if (const auto found = integers_.find(value); found != integers_.end()) return found->second;
    const ConstValueId id = append(ConstValue{.kind = ConstValueKind::Integer, .integerValue = value});
    integers_.emplace(value, id);
    return id;
  }

  auto ConstInterner::internString(std::string value) -> ConstValueId
  {
    if (const auto found = strings_.find(value); found != strings_.end()) return found->second;
    const ConstValueId id = append(ConstValue{.kind = ConstValueKind::String, .stringValue = value});
    strings_.emplace(std::move(value), id);
    return id;
  }

  auto ConstInterner::internTuple(const std::vector<ConstValueId> &elements) -> ConstValueId
  {
    for (uint32_t index = 1; index < values_.size(); ++index)
      if (values_[index].kind == ConstValueKind::Tuple && values_[index].tupleElements == elements) return ConstValueId{index};
    return append(ConstValue{.kind = ConstValueKind::Tuple, .tupleElements = elements});
  }

  auto ConstInterner::value(ConstValueId id) const -> const ConstValue & { return values_.at(id.value); }

  auto ConstEvaluator::evaluate(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> ConstValueId
  {
    fuel_ = 1'000'000;
    return evaluateNode(expression, bindings);
  }

  auto ConstEvaluator::evaluateInteger(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> int64_t
  {
    return asInteger(evaluate(expression, bindings), expression.span);
  }

  auto ConstEvaluator::evaluateArrayLength(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> uint64_t
  {
    const int64_t length = evaluateInteger(expression, bindings);
    if (length < 0) throw ConstEvalError(std::format("array length must be non-negative, got {}", length), expression.span);
    return static_cast<uint64_t>(length);
  }

  auto ConstEvaluator::evaluateNode(const syntax::ConstExpr &expression, const ConstBindings &bindings) const -> ConstValueId
  {
    consumeFuel(expression.span);
    if (const auto *literal = dynamic_cast<const syntax::ConstIntegerLiteral *>(&expression))
      return interner_.internInteger(parseIntegerLiteral(*literal));
    if (const auto *identifier = dynamic_cast<const syntax::ConstIdentifier *>(&expression))
    {
      const auto found = bindings.find(identifier->name);
      if (found == bindings.end()) throw ConstEvalError(std::format("unresolved const name `{}`", identifier->name), identifier->span);
      return found->second;
    }
    if (const auto *unary = dynamic_cast<const syntax::ConstUnaryExpr *>(&expression))
    {
      const int64_t operand = asInteger(evaluateNode(*unary->operand, bindings), unary->operand->span);
      if (unary->operatorText == "+") return interner_.internInteger(operand);
      if (unary->operatorText == "-")
      {
        if (operand == std::numeric_limits<int64_t>::min())
          throw ConstEvalError("const integer negation overflow", unary->span);
        return interner_.internInteger(-operand);
      }
      throw ConstEvalError(std::format("unsupported const unary operator `{}`", unary->operatorText), unary->span);
    }
    if (const auto *binary = dynamic_cast<const syntax::ConstBinaryExpr *>(&expression))
    {
      const int64_t left = asInteger(evaluateNode(*binary->left, bindings), binary->left->span);
      const int64_t right = asInteger(evaluateNode(*binary->right, bindings), binary->right->span);
      if ((binary->operatorText == "/" || binary->operatorText == "%") && right == 0)
        throw ConstEvalError(std::format("const integer {} by zero", binary->operatorText == "/" ? "division" : "modulo"),
                             binary->right->span);
      std::optional<int64_t> result;
      if (binary->operatorText == "+") result = checkedAdd(left, right);
      else if (binary->operatorText == "-") result = checkedSub(left, right);
      else if (binary->operatorText == "*") result = checkedMul(left, right);
      else if (binary->operatorText == "/") result = checkedDiv(left, right);
      else if (binary->operatorText == "%") result = checkedMod(left, right);
      else throw ConstEvalError(std::format("unsupported const operator `{}`", binary->operatorText), binary->span);
      if (!result.has_value())
        throw ConstEvalError(std::format("const integer `{}` overflow", binary->operatorText), binary->span);
      return interner_.internInteger(*result);
    }
    throw ConstEvalError("unsupported const expression", expression.span);
  }

  auto ConstEvaluator::asInteger(ConstValueId id, syntax::SourceSpan span) const -> int64_t
  {
    const auto &value = interner_.value(id);
    if (value.kind != ConstValueKind::Integer)
      throw ConstEvalError(std::format("const value of kind `{}` is not an integer", static_cast<int>(value.kind)), span);
    return value.integerValue;
  }

  void ConstEvaluator::consumeFuel(syntax::SourceSpan span) const
  {
    if (fuel_ == 0) throw ConstEvalError("const evaluation exceeded the fuel budget", span);
    --fuel_;
  }
} // namespace NG::vnext::const_eval
