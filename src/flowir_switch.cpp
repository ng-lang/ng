// AI-generated code; reviewed for this repository's vNext rewrite.
// FlowIR switch lowering: enum-variant, scalar-literal, and union (INC-23
// DEC-D) switches over one shared case/tail/exit CFG skeleton.
#include "flowir_lowerer.hpp"

#include <algorithm>

namespace NG::flowir::detail
{
  using typecheck::builtin::Bool;
  using typecheck::builtin::I64;

  void FunctionLowerer::lowerSwitch(const hir::Statement &statement)
  {
    const ValueId scrutinee = lowerExpression(*statement.expression);
    if (kindOf(*statement.expression) == typecheck::TypeKind::Union)
      lowerUnionSwitch(statement, scrutinee);
    else if (!statement.switchCases.empty() && !statement.switchCases.front().literalTexts.empty())
      lowerLiteralSwitch(statement, scrutinee);
    else
      lowerEnumSwitch(statement, scrutinee);
  }

  void FunctionLowerer::lowerSwitchCases(const hir::Statement &statement,
                                         const std::function<void(size_t, BlockId, BlockId)> &emitTest,
                                         const std::function<void(size_t)> &bindCase)
  {
    const size_t caseCount = statement.switchCases.size();
    std::vector<BlockId> caseBlocks;
    caseBlocks.reserve(caseCount);
    for (size_t index = 0; index < caseCount; ++index)
      caseBlocks.push_back(appendBlock());
    const BlockId tailBlock = appendBlock();
    const BlockId exitBlock = appendBlock();
    if (caseCount == 0)
      jump(tailBlock);

    // Test chain: each case's check block branches to the case or onward;
    // the last case falls through to the `otherwise` tail.
    for (size_t index = 0; index < caseCount; ++index)
    {
      const BlockId next = index + 1 < caseCount ? appendBlock() : tailBlock;
      emitTest(index, caseBlocks[index], next);
      current_ = next;
    }

    current_ = tailBlock;
    if (statement.alternative != nullptr)
      lowerBlock(*statement.alternative);
    jumpIfOpen(exitBlock);

    for (size_t index = 0; index < caseCount; ++index)
    {
      current_ = caseBlocks[index];
      bindCase(index);
      lowerBlock(*statement.switchCases[index].body);
      jumpIfOpen(exitBlock);
    }
    current_ = exitBlock;
  }

  void FunctionLowerer::branchOnAny(size_t count, const std::function<ValueId(size_t)> &test, BlockId caseBlock,
                                    BlockId next)
  {
    for (size_t index = 0; index < count; ++index)
    {
      const ValueId matches = test(index);
      const BlockId onMismatch = index + 1 < count ? appendBlock() : next;
      branch(matches, caseBlock, onMismatch);
      current_ = onMismatch;
    }
  }

  /// Enum switch: compares the variant ordinal against each case's variant
  /// and binds the payload (or its tuple fields) in the case block.
  void FunctionLowerer::lowerEnumSwitch(const hir::Statement &statement, ValueId scrutinee)
  {
    const auto &enumType = descriptorOf(typeOf(*statement.expression));
    const ValueId variantIndex = emit(Instruction{.kind = InstructionKind::EnumVariantIndex, .source = scrutinee}, I64);
    const auto emitTest = [&](size_t index, BlockId caseBlock, BlockId next)
    {
      const auto variant = fieldOrdinal(enumType, statement.switchCases[index].variantName).value_or(index);
      const ValueId expected = integerConstant(static_cast<int64_t>(variant));
      branch(binary("==", variantIndex, expected, Bool), caseBlock, next);
    };
    const auto bindCase = [&](size_t index)
    {
      const auto &switchCase = statement.switchCases[index];
      if (!switchCase.binding.has_value())
        return;
      const TypeId declaredType = types_.localTypeIds.at(switchCase.binding->value);
      const auto variant = fieldOrdinal(enumType, switchCase.variantName);
      const TypeId payloadType = variant.has_value() ? enumType.elements.at(*variant) : declaredType;
      const ValueId payload =
          emit(Instruction{.kind = InstructionKind::ExtractEnumPayload, .source = scrutinee}, payloadType);
      function_.localTypes.emplace(switchCase.binding->value, declaredType);
      if (switchCase.bindings.empty())
      {
        bindLocal(*switchCase.binding, payload, payloadType);
        return;
      }
      // `case Cell(value, rest)`: bind each field of the tuple payload.
      const auto &tuple = descriptorOf(payloadType);
      for (size_t field = 0; field <= switchCase.bindings.size(); ++field)
      {
        const hir::LocalId target = field == 0 ? *switchCase.binding : switchCase.bindings[field - 1];
        const TypeId fieldType = tuple.elements[field];
        const ValueId element = extractTuple(payload, field, fieldType);
        function_.localTypes.emplace(target.value, fieldType);
        bindLocal(target, element, fieldType);
      }
    };
    lowerSwitchCases(statement, emitTest, bindCase);
  }

  /// Scalar literal-or switch: each case's literals compare against the
  /// scrutinee; the scrutinee type (integer, bool, or string — enforced by
  /// the checker) fixes the literal kind.
  void FunctionLowerer::lowerLiteralSwitch(const hir::Statement &statement, ValueId scrutinee)
  {
    const TypeId type = typeOf(*statement.expression);
    const auto kind = type == typecheck::builtin::String ? hir::ExpressionKind::StringLiteral
                      : type == Bool                     ? hir::ExpressionKind::BooleanLiteral
                                                         : hir::ExpressionKind::IntegerLiteral;
    const auto emitTest = [&](size_t index, BlockId caseBlock, BlockId next)
    {
      const auto &texts = statement.switchCases[index].literalTexts;
      branchOnAny(
          texts.size(), [&](size_t alternative)
          { return binary("==", scrutinee, literal(texts[alternative], kind, type), Bool); }, caseBlock, next);
    };
    lowerSwitchCases(statement, emitTest, [](size_t) {});
  }

  /// INC-23 (DEC-D) union switch. Type patterns (`case (x: i64)`) dispatch on
  /// the member tag and bind the payload; member-literal patterns
  /// (`case 42 | "go"`) compare the union against each literal through the
  /// union-equality path (tag + payload, canonical member selection).
  void FunctionLowerer::lowerUnionSwitch(const hir::Statement &statement, ValueId scrutinee)
  {
    const auto &unionType = descriptorOf(typeOf(*statement.expression));
    const auto memberLiteral = [&](const std::string &text) -> ValueId
    {
      switch (typecheck::unionLiteralFamily(text))
      {
      case typecheck::UnionLiteralFamily::Boolean:
        return literal(text, hir::ExpressionKind::BooleanLiteral, Bool);
      case typecheck::UnionLiteralFamily::Float:
        return literal(text, hir::ExpressionKind::FloatLiteral, typecheck::builtin::F64);
      case typecheck::UnionLiteralFamily::String:
        return literal(text, hir::ExpressionKind::StringLiteral, typecheck::builtin::String);
      case typecheck::UnionLiteralFamily::Integer:
        break;
      }
      return literal(text, hir::ExpressionKind::IntegerLiteral, I64);
    };
    const auto emitTest = [&](size_t index, BlockId caseBlock, BlockId next)
    {
      const auto &switchCase = statement.switchCases[index];
      if (switchCase.memberType == nullptr)
      {
        const auto &texts = switchCase.literalTexts;
        branchOnAny(
            texts.size(), [&](size_t alternative)
            { return binary("==", scrutinee, memberLiteral(texts[alternative]), Bool); }, caseBlock, next);
        return;
      }
      const TypeId memberType = types_.localTypeIds.at(switchCase.binding->value);
      const auto found = std::ranges::find(unionType.elements, memberType);
      const auto member = found != unionType.elements.end() ? std::distance(unionType.elements.begin(), found) : 0;
      const ValueId tag = emit(Instruction{.kind = InstructionKind::EnumVariantIndex, .source = scrutinee}, I64);
      branch(binary("==", tag, integerConstant(member), Bool), caseBlock, next);
    };
    const auto bindCase = [&](size_t index)
    {
      const auto &switchCase = statement.switchCases[index];
      if (switchCase.memberType == nullptr)
        return;
      const TypeId memberType = types_.localTypeIds.at(switchCase.binding->value);
      function_.localTypes.emplace(switchCase.binding->value, memberType);
      const ValueId payload =
          emit(Instruction{.kind = InstructionKind::ExtractEnumPayload, .source = scrutinee}, memberType);
      bindLocal(*switchCase.binding, payload, memberType);
    };
    lowerSwitchCases(statement, emitTest, bindCase);
  }
} // namespace NG::flowir::detail
