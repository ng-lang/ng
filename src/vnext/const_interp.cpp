// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/const_interp.hpp"

#include <charconv>
#include <format>
#include <limits>

namespace NG::vnext::const_eval
{
  namespace
  {
    constexpr size_t MaxCallDepth{64};
    constexpr uint32_t MaxSteps{1'000'000};

    [[nodiscard]] auto checkedAdd(int64_t left, int64_t right, syntax::SourceSpan span) -> int64_t
    {
      int64_t result{};
      if (__builtin_add_overflow(left, right, &result))
        throw ConstEvalError("const integer addition overflow", span);
      return result;
    }

    [[nodiscard]] auto checkedSub(int64_t left, int64_t right, syntax::SourceSpan span) -> int64_t
    {
      int64_t result{};
      if (__builtin_sub_overflow(left, right, &result))
        throw ConstEvalError("const integer subtraction overflow", span);
      return result;
    }

    [[nodiscard]] auto checkedMul(int64_t left, int64_t right, syntax::SourceSpan span) -> int64_t
    {
      int64_t result{};
      if (__builtin_mul_overflow(left, right, &result))
        throw ConstEvalError("const integer multiplication overflow", span);
      return result;
    }

    [[nodiscard]] auto parseInteger(std::string_view text, syntax::SourceSpan span) -> int64_t
    {
      int64_t value{};
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
      if (error != std::errc{} || end != text.data() + text.size())
        throw ConstEvalError(std::format("const integer `{}` is out of range", text), span);
      return value;
    }
  } // namespace

  ConstInterpreter::ConstInterpreter(const hir::Module &module, const std::unordered_set<uint32_t> &constFunctions,
                                     ConstInterner &interner, PredicateEvaluator predicate)
    : module_(module), constFunctions_(constFunctions), interner_(interner), predicate_(std::move(predicate))
  {
  }

  auto ConstInterpreter::evaluateCall(const hir::Expression &call, const LocalValues &locals,
                                      const ConstBindings &constBindings, syntax::SourceSpan span) -> ConstValueId
  {
    if (depth_ == 0)
    {
      steps_ = MaxSteps;
      depth_ = 0;
    }
    const ConstBindings saved = std::move(constBindings_);
    constBindings_ = constBindings;
    if (call.kind != hir::ExpressionKind::Call || call.operands.empty() || !call.operands[0]->resolvedName.has_value() ||
        call.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
      throw ConstEvalError("const call target is not a function", span);
    const hir::DefId target{call.operands[0]->resolvedName->id};
    if (!constFunctions_.contains(target.value))
      throw ConstEvalError(std::format("function `{}` is not const-capable", call.operands[0]->text), span);
    const auto &function = module_.functions.at(target.value);
    if (!function.genericParameters.empty() || !function.constParameters.empty())
      throw ConstEvalError(std::format("compile-time calls to generic const fun `{}` are not yet supported", function.name),
                           span);
    std::vector<ConstValueId> arguments;
    arguments.reserve(call.operands.size() - 1);
    for (size_t index = 1; index < call.operands.size(); ++index)
      arguments.push_back(evaluateExpression(*call.operands[index], locals));
    if (arguments.size() != function.parameters.size())
    {
      constBindings_ = std::move(saved);
      throw ConstEvalError(std::format("const call argument count mismatch: expected {}, got {}", function.parameters.size(),
                                       arguments.size()), span);
    }
    const ConstValueId result = runFunction(function, arguments);
    constBindings_ = std::move(saved);
    return result;
  }

  auto ConstInterpreter::evaluateExpression(const hir::Expression &expression, const LocalValues &locals) -> ConstValueId
  {
    consumeStep(expression.span);
    switch (expression.kind)
    {
    case hir::ExpressionKind::IntegerLiteral: return interner_.internInteger(parseInteger(expression.text, expression.span));
    case hir::ExpressionKind::BooleanLiteral: return interner_.internBool(expression.text == "true");
    case hir::ExpressionKind::StringLiteral: return interner_.internString(expression.text);
    case hir::ExpressionKind::Grouped: return evaluateExpression(*expression.operands[0], locals);
    case hir::ExpressionKind::ResolvedName:
    {
      if (!expression.resolvedName.has_value())
        throw ConstEvalError(std::format("`{}` is not a compile-time constant", expression.text), expression.span);
      if (expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
      {
        const auto found = constBindings_.find(expression.text);
        if (found == constBindings_.end())
          throw ConstEvalError(std::format("unresolved const name `{}`", expression.text), expression.span);
        return found->second;
      }
      if (expression.resolvedName->kind != hir::ResolvedNameKind::Local)
        throw ConstEvalError(std::format("`{}` is not a compile-time constant", expression.text), expression.span);
      const auto found = locals.find(expression.resolvedName->id);
      if (found == locals.end())
        throw ConstEvalError(std::format("runtime local `{}` is not a compile-time constant", expression.text), expression.span);
      return found->second;
    }
    case hir::ExpressionKind::GenericApplication: return predicate_(expression);
    case hir::ExpressionKind::Call: return evaluateCall(expression, locals, constBindings_, expression.span);
    case hir::ExpressionKind::Prefix:
    {
      const ConstValueId operand = evaluateExpression(*expression.operands[0], locals);
      if (expression.text == "!")
        return interner_.internBool(!asBool(operand, expression.operands[0]->span));
      const int64_t value = asInteger(operand, expression.operands[0]->span);
      if (expression.text == "+") return interner_.internInteger(value);
      if (expression.text == "-")
      {
        if (value == std::numeric_limits<int64_t>::min()) throw ConstEvalError("const integer negation overflow", expression.span);
        return interner_.internInteger(-value);
      }
      throw ConstEvalError(std::format("unsupported const operator `{}`", expression.text), expression.span);
    }
    case hir::ExpressionKind::Binary:
    {
      const std::string &op = expression.text;
      if (op == "&&")
      {
        if (!asBool(evaluateExpression(*expression.operands[0], locals), expression.operands[0]->span))
          return interner_.internBool(false);
        return interner_.internBool(asBool(evaluateExpression(*expression.operands[1], locals), expression.operands[1]->span));
      }
      if (op == "||")
      {
        if (asBool(evaluateExpression(*expression.operands[0], locals), expression.operands[0]->span))
          return interner_.internBool(true);
        return interner_.internBool(asBool(evaluateExpression(*expression.operands[1], locals), expression.operands[1]->span));
      }
      const ConstValueId left = evaluateExpression(*expression.operands[0], locals);
      const ConstValueId right = evaluateExpression(*expression.operands[1], locals);
      const auto &leftValue = interner_.value(left);
      const auto &rightValue = interner_.value(right);
      if (op == "==") return interner_.internBool(leftValue == rightValue);
      if (op == "!=") return interner_.internBool(!(leftValue == rightValue));
      const int64_t l = asInteger(left, expression.operands[0]->span);
      const int64_t r = asInteger(right, expression.operands[1]->span);
      if (op == "<") return interner_.internBool(l < r);
      if (op == "<=") return interner_.internBool(l <= r);
      if (op == ">") return interner_.internBool(l > r);
      if (op == ">=") return interner_.internBool(l >= r);
      if ((op == "/" || op == "%") && r == 0)
        throw ConstEvalError(std::format("const integer {} by zero", op == "/" ? "division" : "modulo"),
                             expression.operands[1]->span);
      if (op == "+") return interner_.internInteger(checkedAdd(l, r, expression.span));
      if (op == "-") return interner_.internInteger(checkedSub(l, r, expression.span));
      if (op == "*") return interner_.internInteger(checkedMul(l, r, expression.span));
      if (op == "/")
      {
        if (l == std::numeric_limits<int64_t>::min() && r == -1) throw ConstEvalError("const integer division overflow", expression.span);
        return interner_.internInteger(l / r);
      }
      if (op == "%")
      {
        if (l == std::numeric_limits<int64_t>::min() && r == -1) throw ConstEvalError("const integer remainder overflow", expression.span);
        return interner_.internInteger(l % r);
      }
      throw ConstEvalError(std::format("unsupported const operator `{}`", op), expression.span);
    }
    default:
      throw ConstEvalError("expression is not supported during const evaluation", expression.span);
    }
  }

  auto ConstInterpreter::evaluateCondition(const hir::Expression &expression, const LocalValues &locals) -> bool
  {
    return asBool(evaluateExpression(expression, locals), expression.span);
  }

  auto ConstInterpreter::runFunction(const hir::Function &function, const std::vector<ConstValueId> &arguments)
      -> ConstValueId
  {
    if (depth_ >= MaxCallDepth)
      throw ConstEvalError(std::format("const call depth exceeded in `{}`", function.name), function.span);
    ++depth_;
    LocalValues locals;
    for (size_t index = 0; index < function.parameters.size(); ++index)
      locals.emplace(function.parameters[index].local.value, arguments[index]);

    while (true)
    {
      Control control = runBlock(function.body, locals);
      if (control.kind == Control::Kind::Return)
      {
        --depth_;
        return control.value;
      }
      if (control.kind == Control::Kind::TailRecur)
      {
        for (size_t index = 0; index < function.parameters.size(); ++index)
          locals[function.parameters[index].local.value] = control.arguments[index];
        continue;
      }
      --depth_;
      return interner_.internUnit();
    }
  }

  auto ConstInterpreter::runBlock(const hir::Block &block, LocalValues &locals) -> Control
  {
    for (const auto &statement : block.statements)
    {
      consumeStep(statement.span);
      switch (statement.kind)
      {
      case hir::StatementKind::Let:
      {
        if (!statement.destructuredLocals.empty())
          throw ConstEvalError("tuple destructuring is not supported during const evaluation", statement.span);
        const ConstValueId value = evaluateExpression(*statement.expression, locals);
        locals.insert_or_assign(statement.local->value, value);
        break;
      }
      case hir::StatementKind::Assign:
      {
        if (statement.assignmentTarget != nullptr)
          throw ConstEvalError("place assignment is not supported during const evaluation", statement.span);
        const ConstValueId value = evaluateExpression(*statement.expression, locals);
        locals.insert_or_assign(statement.local->value, value);
        break;
      }
      case hir::StatementKind::Return:
      {
        Control control{.kind = Control::Kind::Return};
        control.value = statement.expression != nullptr ? evaluateExpression(*statement.expression, locals)
                                                        : interner_.internUnit();
        return control;
      }
      case hir::StatementKind::If:
      {
        const bool consequence = asBool(evaluateExpression(*statement.expression, locals), statement.expression->span);
        const hir::Block *selected = consequence ? statement.consequence.get() : statement.alternative.get();
        if (selected == nullptr) break;
        Control control = runBlock(*selected, locals);
        if (control.kind != Control::Kind::Fallthrough) return control;
        break;
      }
      case hir::StatementKind::ConstIf:
      {
        const bool consequence = evaluateCondition(*statement.expression, locals);
        const hir::Block *selected = consequence ? statement.consequence.get() : statement.alternative.get();
        if (selected == nullptr) break;
        Control control = runBlock(*selected, locals);
        if (control.kind != Control::Kind::Fallthrough) return control;
        break;
      }
      case hir::StatementKind::Loop:
      {
        Control control = runLoop(statement, locals);
        if (control.kind != Control::Kind::Fallthrough) return control;
        break;
      }
      case hir::StatementKind::Next:
      {
        Control control{.kind = Control::Kind::Fallthrough};
        if (statement.nextTarget->kind == hir::NextTargetKind::Loop)
        {
          control.kind = Control::Kind::NextLoop;
          control.loopTarget = statement.nextTarget->id;
        }
        else
        {
          control.kind = Control::Kind::TailRecur;
        }
        control.arguments.reserve(statement.arguments.size());
        for (const auto &argument : statement.arguments)
          control.arguments.push_back(evaluateExpression(*argument, locals));
        return control;
      }
      case hir::StatementKind::Switch:
        throw ConstEvalError("switch is not supported during const evaluation", statement.span);
      case hir::StatementKind::Expression:
        static_cast<void>(evaluateExpression(*statement.expression, locals));
        break;
      }
    }
    if (block.tailExpression != nullptr)
    {
      Control control{.kind = Control::Kind::Return};
      control.value = evaluateExpression(*block.tailExpression, locals);
      return control;
    }
    return Control{};
  }

  auto ConstInterpreter::runLoop(const hir::Statement &statement, LocalValues &locals) -> Control
  {
    for (size_t index = 0; index < statement.loopBindings.size(); ++index)
      locals[statement.loopBindings[index].value] = evaluateExpression(*statement.arguments[index], locals);
    const uint32_t loopId = statement.loop->value;
    while (true)
    {
      consumeStep(statement.span);
      Control control = runBlock(*statement.body, locals);
      if (control.kind == Control::Kind::NextLoop && control.loopTarget == loopId)
      {
        for (size_t index = 0; index < statement.loopBindings.size(); ++index)
          locals[statement.loopBindings[index].value] = control.arguments[index];
        continue;
      }
      return control;
    }
  }

  auto ConstInterpreter::asBool(ConstValueId id, syntax::SourceSpan span) const -> bool
  {
    const auto &value = interner_.value(id);
    if (value.kind != ConstValueKind::Bool)
      throw ConstEvalError(std::format("const value of kind `{}` is not a bool", static_cast<int>(value.kind)), span);
    return value.boolValue;
  }

  auto ConstInterpreter::asInteger(ConstValueId id, syntax::SourceSpan span) const -> int64_t
  {
    const auto &value = interner_.value(id);
    if (value.kind != ConstValueKind::Integer)
      throw ConstEvalError(std::format("const value of kind `{}` is not an integer", static_cast<int>(value.kind)), span);
    return value.integerValue;
  }

  void ConstInterpreter::consumeStep(syntax::SourceSpan span) const
  {
    if (steps_ == 0) throw ConstEvalError("const evaluation exceeded the fuel budget", span);
    --steps_;
  }
} // namespace NG::vnext::const_eval
