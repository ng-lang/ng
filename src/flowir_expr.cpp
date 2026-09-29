// AI-generated code; reviewed for this repository's vNext rewrite.
// FlowIR expression lowering: dispatch, the generic `Evaluate` form (operands,
// spreads, packs, payloads), references/spans/places, method calls, trait
// views, and derive(Eq/Show) expansion.
#include "flowir_lowerer.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <format>
#include <limits>

namespace NG::flowir::detail
{
  namespace
  {
    [[nodiscard]] auto hasSpreadOperand(const hir::Expression &expression) -> bool
    {
      return std::ranges::any_of(expression.operands, [](const auto &operand) { return isSpread(*operand); });
    }

    [[nodiscard]] auto calleeIs(const hir::Expression &expression, hir::ResolvedNameKind kind) -> bool
    {
      return expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
             expression.operands[0]->resolvedName.has_value() && expression.operands[0]->resolvedName->kind == kind;
    }
  } // namespace

  auto FunctionLowerer::lowerExpression(const hir::Expression &expression) -> ValueId
  {
    using hir::ExpressionKind;
    const auto &text = expression.text;
    switch (expression.kind)
    {
    case ExpressionKind::Binary:
      if (text == "&&" || text == "||")
        return lowerLogicalExpression(expression);
      if (text == "<<" && kindOf(*expression.operands[0]) == typecheck::TypeKind::DynamicArray)
      {
        // Value-semantics array append (`xs << value`).
        const ValueId left = lowerExpression(*expression.operands[0]);
        const ValueId right = lowerExpression(*expression.operands[1]);
        return emit(Instruction{.kind = InstructionKind::AppendArray, .operands = {left, right}}, typeOf(expression));
      }
      if (text == "==" || text == "!=")
      {
        // INC-8 derive(Eq): struct ==/!= expands to field-by-field compares.
        const TypeId left = typeOf(*expression.operands[0]);
        if (left == typeOf(*expression.operands[1]) && function_.derivedEqTypes.contains(left.value))
          return lowerStructEq(expression, left, text == "!=");
      }
      break;
    case ExpressionKind::Prefix:
      if (text == "ref" || text == "ref mut" || text == "*")
        return lowerReferenceExpression(expression);
      if (text == "span")
        return lowerSpanExpression(expression);
      break;
    case ExpressionKind::Member:
    case ExpressionKind::Index:
      // INC-16 S2 place-through-ref: reads whose receiver is a scoped
      // reference (`items[i]`, `box.value` on `T ref`) deref it first.
      if (kindOf(*expression.operands[0]) == typecheck::TypeKind::Reference)
        return lowerReadThroughReference(expression);
      break;
    case ExpressionKind::GenericApplication:
      throw VerificationError("const predicate application is not a runtime value");
    case ExpressionKind::Closure:
      // INC-14 S1: a closure literal has no runtime value at its binding site
      // (the synthesized code function receives call parameters directly), so
      // it lowers as an inert zero word (closures map to `Long` natively).
      return emit(Instruction{.kind = InstructionKind::Evaluate, .expressionKind = ExpressionKind::IntegerLiteral},
                  typeOf(expression));
    case ExpressionKind::Call:
      if (expression.methodCall)
        return lowerMethodCall(expression);
      break;
    case ExpressionKind::TupleLiteral:
      if (hasSpreadOperand(expression))
        return lowerTupleSplice(expression);
      break;
    case ExpressionKind::ArrayLiteral:
      if (hasSpreadOperand(expression))
        return lowerMapLiteral(expression);
      // List collection literals type as the expected recursive-list enum.
      if (kindOf(expression) == typecheck::TypeKind::Enum)
        return lowerListLiteral(expression, typeOf(expression));
      break;
    default:
      break;
    }

    if (calleeIs(expression, hir::ResolvedNameKind::Function) && types_.callFoldSpreadPositions.contains(&expression))
      return lowerFoldCall(expression);

    if (expression.kind == ExpressionKind::Index && kindOf(*expression.operands[1]) == typecheck::TypeKind::Range)
    {
      const ValueId receiver = lowerExpression(*expression.operands[0]);
      const ValueId range = lowerExpression(*expression.operands[1]);
      return emit(Instruction{.kind = InstructionKind::Slice, .operands = {receiver, range}}, typeOf(expression));
    }

    if (types_.traitViewCoercions.contains(&expression))
      return lowerTraitViewCoercion(expression);

    if (expression.kind == ExpressionKind::Prefix && (text == "-" || text == "+") && !expression.operands.empty() &&
        expression.operands[0]->kind == ExpressionKind::IntegerLiteral)
      return lowerSignedIntegerLiteral(expression);

    return lowerEvaluate(expression);
  }

  // ---- Generic Evaluate ------------------------------------------------------

  /// INC-14 S1 closure calls: the callee is a closure-typed local rather than
  /// a function name. Both forms skip the callee operand: the synthesized
  /// closure code function receives the parameters directly.
  auto FunctionLowerer::isDirectCall(const hir::Expression &expression) const -> bool
  {
    if (calleeIs(expression, hir::ResolvedNameKind::Function))
      return true;
    return calleeIs(expression, hir::ResolvedNameKind::Local) && types_.callTargets.contains(&expression) &&
           kindOf(*expression.operands[0]) == typecheck::TypeKind::Closure;
  }

  auto FunctionLowerer::lowerEvaluate(const hir::Expression &expression) -> ValueId
  {
    const bool directCall = isDirectCall(expression);
    std::vector<ValueId> operands = lowerOperands(expression, directCall);
    int64_t payload = evaluatePayload(expression, operands);

    std::optional<hir::DefId> callTarget;
    if (directCall)
    {
      const auto target = types_.callTargets.find(&expression);
      callTarget =
          target != types_.callTargets.end() ? target->second : hir::DefId{expression.operands[0]->resolvedName->id};
    }
    // INC-21 S2: carry the checker's growth decision for `pushBack` calls
    // (payload 1 grows the array buffer in place, 0 copies on write).
    if (expression.kind == hir::ExpressionKind::Call)
      if (const auto inPlace = types_.inPlaceMutationCalls.find(&expression);
          inPlace != types_.inPlaceMutationCalls.end())
        payload = inPlace->second ? 1 : 0;

    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = expression.kind,
                            .text = expression.text,
                            .payload = payload,
                            .callTarget = callTarget,
                            .operands = std::move(operands)},
                typeOf(expression));
  }

  /// Lowers the operands of a generic `Evaluate`. Direct calls additionally
  /// prepend captured locals (INC-14 S3), flatten tuple spreads statically,
  /// and splice variadic trailing arguments into one pack tuple.
  auto FunctionLowerer::lowerOperands(const hir::Expression &expression, bool directCall) -> std::vector<ValueId>
  {
    std::vector<ValueId> operands;
    if (!directCall)
    {
      for (const auto &operand : expression.operands)
        operands.push_back(lowerExpression(*operand));
      return operands;
    }

    // Captured current-frame locals lead: the synthesized code function /
    // specialized instance takes capture params first.
    if (const auto captures = types_.callCaptures.find(&expression); captures != types_.callCaptures.end())
      for (const uint32_t captureLocal : captures->second)
        operands.push_back(readLocal(hir::LocalId{captureLocal}, types_.localTypeIds.at(captureLocal)));

    const auto spreads = types_.callSpreadPositions.find(&expression);
    const std::vector<size_t> noSpreads;
    const auto &spreadPositions = spreads != types_.callSpreadPositions.end() ? spreads->second : noSpreads;
    size_t spreadIndex = 0;
    for (size_t index = 1; index < expression.operands.size(); ++index)
    {
      if (spreadIndex < spreadPositions.size() && spreadPositions[spreadIndex] == index - 1)
      {
        // Tuple spreads flatten statically: each element becomes an argument.
        const auto &spreadOperand = *expression.operands[index]->operands[0];
        const ValueId tuple = lowerExpression(spreadOperand);
        const auto &descriptor = descriptorOf(typeOf(spreadOperand));
        for (size_t element = 0; element < descriptor.elements.size(); ++element)
          operands.push_back(extractTuple(tuple, element, descriptor.elements[element]));
        ++spreadIndex;
        continue;
      }
      operands.push_back(lowerExpression(*expression.operands[index]));
    }

    if (const auto packCount = types_.callPackArgCounts.find(&expression); packCount != types_.callPackArgCounts.end())
    {
      // Variadic call: splice the trailing arguments into one tuple value.
      const auto packBegin = operands.end() - static_cast<std::ptrdiff_t>(packCount->second);
      std::vector<ValueId> packOperands(packBegin, operands.end());
      operands.erase(packBegin, operands.end());
      std::optional<TypeId> packedType;
      if (const auto found = types_.callPackTupleTypes.find(&expression); found != types_.callPackTupleTypes.end())
        packedType = found->second;
      operands.push_back(
          emit(Instruction{.kind = InstructionKind::TupleSplice, .operands = std::move(packOperands)}, packedType));
    }
    return operands;
  }

  /// The `Evaluate` payload: literal bits, local ids, field ordinals, enum
  /// tags, struct type ids, or operator codes. Enum and struct literals also
  /// reshape `operands` (tuple-packed payload, declaration field order).
  auto FunctionLowerer::evaluatePayload(const hir::Expression &expression, std::vector<ValueId> &operands) -> int64_t
  {
    using hir::ExpressionKind;
    switch (expression.kind)
    {
    case ExpressionKind::IntegerLiteral:
      return integerLiteralPayload(expression.text, typeOf(expression));
    case ExpressionKind::FloatLiteral:
      return std::bit_cast<int64_t>(std::stod(expression.text));
    case ExpressionKind::BooleanLiteral:
      return expression.text == "true" ? 1 : 0;
    case ExpressionKind::ResolvedName:
      return expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local
                 ? expression.resolvedName->id
                 : 0;
    case ExpressionKind::Index:
      return expression.text.empty() ? 0 : std::stoll(expression.text);
    case ExpressionKind::Member:
      return static_cast<int64_t>(
          fieldOrdinal(descriptorOf(typeOf(*expression.operands[0])), expression.text).value_or(0));
    case ExpressionKind::EnumLiteral:
    {
      const TypeId enumType = typeOf(expression);
      const uint32_t variant = *expression.variant;
      // Multi-field variants pack their constructor arguments into one tuple.
      const auto &descriptor = descriptorOf(enumType);
      if (variant < descriptor.elements.size() && descriptor.variantHasPayload.at(variant) &&
          descriptorOf(descriptor.elements.at(variant)).kind == typecheck::TypeKind::Tuple)
      {
        const ValueId packed = emit(Instruction{.kind = InstructionKind::TupleSplice, .operands = std::move(operands)},
                                    descriptor.elements.at(variant));
        operands = {packed};
      }
      return enumLiteralPayload(enumType, variant);
    }
    case ExpressionKind::StructLiteral:
    {
      const TypeId structType = typeOf(expression);
      std::vector<ValueId> ordered;
      for (const auto &field : descriptorOf(structType).fieldNames)
      {
        const auto found = std::ranges::find(expression.memberNames, field);
        ordered.push_back(operands.at(static_cast<size_t>(std::distance(expression.memberNames.begin(), found))));
      }
      operands = std::move(ordered);
      return structType.value;
    }
    case ExpressionKind::Prefix:
      return prefixOperatorCode(expression.text);
    case ExpressionKind::Binary:
      return binaryOperatorCode(expression.text);
    default:
      return 0;
    }
  }

  auto FunctionLowerer::lowerReadThroughReference(const hir::Expression &expression) -> ValueId
  {
    const TypeId referent = descriptorOf(typeOf(*expression.operands[0])).element;
    const ValueId reference = lowerExpression(*expression.operands[0]);
    std::vector<ValueId> operands{
      emit(Instruction{.kind = InstructionKind::LoadRef, .operands = {reference}}, referent)};
    for (size_t index = 1; index < expression.operands.size(); ++index)
      operands.push_back(lowerExpression(*expression.operands[index]));
    int64_t payload = 0;
    if (expression.kind == hir::ExpressionKind::Member)
      payload = static_cast<int64_t>(fieldOrdinal(descriptorOf(referent), expression.text).value_or(0));
    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = expression.kind,
                            .text = expression.text,
                            .payload = payload,
                            .operands = std::move(operands)},
                typeOf(expression));
  }

  auto FunctionLowerer::lowerTraitViewCoercion(const hir::Expression &expression) -> ValueId
  {
    const auto &[traitName, concrete] = types_.traitViewCoercions.at(&expression);
    const auto trait =
        std::ranges::find_if(types_.typeDescriptors, [&](const auto &descriptor)
                             { return descriptor.kind == typecheck::TypeKind::Trait && descriptor.name == traitName; });
    const auto traitType = trait == types_.typeDescriptors.end()
                               ? 0U
                               : static_cast<uint32_t>(std::distance(types_.typeDescriptors.begin(), trait));
    auto place = lowerPlace(expression);
    auto indexValues = indexOperands(place.steps);
    return emit(Instruction{.kind = InstructionKind::MakeTraitView,
                            .payload = concrete.value,
                            .operands = std::move(indexValues),
                            .placeRootLocal = place.rootLocal,
                            .placeRootRef = place.rootRef,
                            .placeSteps = std::move(place.steps),
                            .traitType = traitType},
                typeOf(expression));
  }

  /// Folds the sign onto the literal payload so `-9223372036854775808`
  /// (i64::min) encodes as one instruction instead of overflowing the
  /// positive operand encoding.
  auto FunctionLowerer::lowerSignedIntegerLiteral(const hir::Expression &expression) -> ValueId
  {
    const TypeId type = typeOf(expression);
    const std::string_view digits = expression.operands[0]->text;
    uint64_t magnitude{};
    const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), magnitude);
    if (error != std::errc{} || end != digits.data() + digits.size())
      throw VerificationError(std::format("invalid integer literal `{}`", digits));
    int64_t folded = static_cast<int64_t>(magnitude);
    if (expression.text == "-")
    {
      if (typecheck::isUnsignedIntegerBuiltin(type))
        throw VerificationError(std::format("negated unsigned literal `-{}`", digits));
      folded = magnitude == (1ULL << 63) ? std::numeric_limits<int64_t>::min() : -static_cast<int64_t>(magnitude);
    }
    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = hir::ExpressionKind::IntegerLiteral,
                            .text = std::format("{}{}", expression.text, digits),
                            .payload = folded},
                type);
  }

  // ---- Short-circuit logic and derive(Eq) ------------------------------------

  auto FunctionLowerer::lowerLogicalExpression(const hir::Expression &expression) -> ValueId
  {
    const ValueId left = lowerExpression(*expression.operands[0]);
    const BlockId rightBlock = appendBlock();
    const BlockId shortCircuitBlock = appendBlock();
    const BlockId joinBlock = appendBlock();
    const hir::LocalId resultLocal{nextSyntheticLocal_++};
    auto &join = function_.blocks[joinBlock.value];
    join.parameterCount = 1;
    join.parameterLocals = {resultLocal};
    function_.localTypes.emplace(resultLocal.value, typeOf(expression));

    if (expression.text == "&&")
      branch(left, rightBlock, shortCircuitBlock);
    else
      branch(left, shortCircuitBlock, rightBlock);

    current_ = rightBlock;
    const ValueId right = lowerExpression(*expression.operands[1]);
    if (!terminated())
      jump(joinBlock, {right});

    current_ = shortCircuitBlock;
    jump(joinBlock, {left});

    current_ = joinBlock;
    return readLocal(resultLocal, typeOf(expression));
  }

  /// INC-8: derive(Eq) — expand `a == b` / `a != b` for Eq-derived struct
  /// types into field-by-field comparisons (AND of per-field ==).
  auto FunctionLowerer::lowerStructEq(const hir::Expression &expression, TypeId structType, bool negate) -> ValueId
  {
    const auto left = lowerExpression(*expression.operands[0]);
    const auto right = lowerExpression(*expression.operands[1]);
    return lowerStructEqDirect(left, right, structType, negate);
  }

  /// Core Eq expansion over already-lowered values; nested Eq-derived fields
  /// recurse.
  auto FunctionLowerer::lowerStructEqDirect(ValueId left, ValueId right, TypeId structType, bool negate) -> ValueId
  {
    const auto &fields = descriptorOf(structType).elements;
    // A zero-field struct is always equal.
    if (fields.empty())
      return literal(negate ? "false" : "true", hir::ExpressionKind::BooleanLiteral, typecheck::builtin::Bool);

    std::optional<ValueId> accumulator;
    for (size_t index = 0; index < fields.size(); ++index)
    {
      const TypeId fieldType = fields[index];
      const ValueId leftField = extractMember(left, index, fieldType);
      const ValueId rightField = extractMember(right, index, fieldType);
      const auto kind = descriptorOf(fieldType).kind;
      const ValueId equal = (kind == typecheck::TypeKind::Struct || kind == typecheck::TypeKind::Enum) &&
                                    function_.derivedEqTypes.contains(fieldType.value)
                                ? lowerStructEqDirect(leftField, rightField, fieldType, false)
                                : binary("==", leftField, rightField, typecheck::builtin::Bool);
      accumulator = accumulator ? binary("&&", *accumulator, equal, typecheck::builtin::Bool) : equal;
    }
    if (!negate)
      return *accumulator;
    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = hir::ExpressionKind::Prefix,
                            .text = "!",
                            .payload = prefixOperatorCode("!"),
                            .operands = {*accumulator}},
                typecheck::builtin::Bool);
  }

  // ---- References, spans, tuples ---------------------------------------------

  auto FunctionLowerer::lowerReferenceExpression(const hir::Expression &expression) -> ValueId
  {
    if (expression.text == "*")
    {
      const ValueId reference = lowerExpression(*expression.operands[0]);
      return emit(Instruction{.kind = InstructionKind::LoadRef, .operands = {reference}}, typeOf(expression));
    }
    auto place = lowerPlace(*expression.operands[0]);
    if (!place.rootLocal.has_value() || place.rootRef.has_value())
      throw VerificationError("reference root must be a local binding");
    auto indexValues = indexOperands(place.steps);
    return emit(Instruction{.kind = InstructionKind::MakeRef,
                            .operands = std::move(indexValues),
                            .placeRootLocal = place.rootLocal,
                            .placeMutable = expression.text == "ref mut",
                            .placeSteps = std::move(place.steps)},
                typeOf(expression));
  }

  /// `span xs` (whole view) or `span xs[lo..hi]` (window view). The checker
  /// already validated the operand shape and the shared borrow.
  auto FunctionLowerer::lowerSpanExpression(const hir::Expression &expression) -> ValueId
  {
    const hir::Expression *arrayPlace = expression.operands[0].get();
    std::vector<ValueId> bounds;
    if (arrayPlace->kind == hir::ExpressionKind::Index)
    {
      const auto &range = *arrayPlace->operands[1];
      bounds.push_back(lowerExpression(*range.operands[0]));
      bounds.push_back(lowerExpression(*range.operands[1]));
      arrayPlace = arrayPlace->operands[0].get();
    }
    auto place = lowerPlace(*arrayPlace);
    if (!place.rootLocal.has_value() || place.rootRef.has_value() || !place.steps.empty())
      throw VerificationError("span root must be a direct local binding");
    return emit(Instruction{.kind = InstructionKind::MakeSpan,
                            .operands = std::move(bounds),
                            .placeRootLocal = place.rootLocal},
                typeOf(expression));
  }

  /// Lowers a tuple literal containing spreads to a runtime splice.
  auto FunctionLowerer::lowerTupleSplice(const hir::Expression &expression) -> ValueId
  {
    std::vector<ValueId> operands;
    for (const auto &element : expression.operands)
      operands.push_back(lowerExpression(isSpread(*element) ? *element->operands[0] : *element));
    return emit(Instruction{.kind = InstructionKind::TupleSplice, .operands = std::move(operands)}, typeOf(expression));
  }

  // ---- Method calls and derive(Show) -----------------------------------------

  /// Lowers a method call: trait-view receivers dispatch dynamically; other
  /// receivers are borrowed (or passed through when already references) and
  /// call the selected impl/default method, with derived clone/show expanded
  /// in place.
  auto FunctionLowerer::lowerMethodCall(const hir::Expression &expression) -> ValueId
  {
    const auto &receiverNode = *expression.operands[0]->operands[0];
    const bool qualified =
        receiverNode.kind == hir::ExpressionKind::ResolvedName && !receiverNode.resolvedName.has_value();
    const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
    const size_t firstArgument = qualified ? 2 : 1;
    const auto lowerArguments = [&](ValueId self)
    {
      std::vector<ValueId> operands{self};
      for (size_t index = firstArgument; index < expression.operands.size(); ++index)
        operands.push_back(lowerExpression(*expression.operands[index]));
      return operands;
    };

    if (const auto traitCall = types_.traitViewCalls.find(&expression); traitCall != types_.traitViewCalls.end())
    {
      auto operands = lowerArguments(lowerExpression(receiver));
      return emit(Instruction{.kind = InstructionKind::CallTrait,
                              .payload = static_cast<int64_t>(traitCall->second.second),
                              .operands = std::move(operands)},
                  typeOf(expression));
    }

    const ValueId self = lowerMethodReceiver(expression, receiver);
    if (types_.derivedCloneCalls.contains(&expression))
    {
      // Derived clone / Box.clone: Box receivers deep-copy, others load.
      const auto kind =
          types_.boxCloneCalls.contains(&expression) ? InstructionKind::BoxClone : InstructionKind::LoadRef;
      return emit(Instruction{.kind = kind, .operands = {self}}, typeOf(expression));
    }
    if (const auto show = types_.derivedShowCalls.find(&expression); show != types_.derivedShowCalls.end())
    {
      // INC-11 derive(Show): load the `Self ref` receiver and synthesize the
      // string in place.
      const ValueId structValue = emit(Instruction{.kind = InstructionKind::LoadRef, .operands = {self}}, show->second);
      return buildShowString(structValue, show->second);
    }
    auto operands = lowerArguments(self);
    std::optional<hir::DefId> callTarget;
    if (const auto target = types_.callTargets.find(&expression); target != types_.callTargets.end())
      callTarget = target->second;
    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = hir::ExpressionKind::Call,
                            .text = expression.text,
                            .callTarget = callTarget,
                            .operands = std::move(operands)},
                typeOf(expression));
  }

  /// Reference-typed receivers pass through; value places are borrowed with
  /// the checker-selected mutability and reference type.
  auto FunctionLowerer::lowerMethodReceiver(const hir::Expression &call, const hir::Expression &receiver) -> ValueId
  {
    if (typeOf(receiver).value != 0 && kindOf(receiver) == typecheck::TypeKind::Reference)
      return lowerExpression(receiver);
    auto place = lowerPlace(receiver);
    if (!place.rootLocal.has_value() || place.rootRef.has_value())
      throw VerificationError("method receiver must be a local place");
    const auto mutableFound = types_.methodReceiverMutable.find(&call);
    const bool mutableReference = mutableFound != types_.methodReceiverMutable.end() && mutableFound->second;
    std::optional<TypeId> referenceType;
    if (const auto found = types_.methodReceiverRefTypes.find(&call);
        found != types_.methodReceiverRefTypes.end() && found->second.value != 0)
      referenceType = found->second;
    auto indexValues = indexOperands(place.steps);
    return emit(Instruction{.kind = InstructionKind::MakeRef,
                            .operands = std::move(indexValues),
                            .placeRootLocal = place.rootLocal,
                            .placeMutable = mutableReference,
                            .placeSteps = std::move(place.steps)},
                referenceType);
  }

  /// INC-11 derive(Show): `name{field: value, ...}` (no space inside the
  /// braces, research A4) from string literals, per-field native `toString`
  /// calls, and nested derived-Show structs.
  auto FunctionLowerer::buildShowString(ValueId structValue, TypeId structType) -> ValueId
  {
    using typecheck::builtin::String;
    const auto &descriptor = descriptorOf(structType);
    const auto text = [&](std::string value)
    { return literal(std::move(value), hir::ExpressionKind::StringLiteral, String); };
    const auto concat = [&](ValueId left, ValueId right) { return binary("+", left, right, String); };
    // Strings pass through, Show-derived structs recurse, scalars call the
    // recorded native `toString`.
    const auto fieldToString = [&](ValueId field, TypeId fieldType) -> ValueId
    {
      if (fieldType == String)
        return field;
      if (descriptorOf(fieldType).kind == typecheck::TypeKind::Struct &&
          types_.derivedShowTypes.contains(fieldType.value))
        return buildShowString(field, fieldType);
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::Call,
                              .text = "toString",
                              .callTarget = types_.showToStringTargets.at(fieldType.value),
                              .operands = {field}},
                  String);
    };

    ValueId accumulator = concat(text(descriptor.name), text("{"));
    for (size_t index = 0; index < descriptor.elements.size(); ++index)
    {
      if (index != 0)
        accumulator = concat(accumulator, text(", "));
      accumulator = concat(accumulator, text(descriptor.fieldNames[index]));
      accumulator = concat(accumulator, text(": "));
      const ValueId field = extractMember(structValue, index, descriptor.elements[index]);
      accumulator = concat(accumulator, fieldToString(field, descriptor.elements[index]));
    }
    return concat(accumulator, text("}"));
  }

  // ---- Places ----------------------------------------------------------------

  /// Lowers a place expression to its root (a current-frame local or a
  /// reference value) plus the steps to the leaf. Index expressions along the
  /// path are lowered into the current block.
  auto FunctionLowerer::lowerPlace(const hir::Expression &expression) -> LoweredPlace
  {
    switch (expression.kind)
    {
    case hir::ExpressionKind::ResolvedName:
      if (!expression.resolvedName.has_value() || expression.resolvedName->kind != hir::ResolvedNameKind::Local)
        throw VerificationError("place root is not a local binding");
      // D-002 A1.2 place-through-ref: a reference-typed local is itself the
      // ref object, so `arr[i]` on `array<T> ref` continues like `*arr`.
      if (kindOf(expression) == typecheck::TypeKind::Reference)
        return LoweredPlace{.rootRef = lowerExpression(expression)};
      return LoweredPlace{.rootLocal = hir::LocalId{expression.resolvedName->id}};
    case hir::ExpressionKind::Index:
    {
      auto place = lowerPlace(*expression.operands[0]);
      const auto &index = *expression.operands[1];
      // Constant projections (tuples, literal array indexes) become member
      // steps so the backend can verify the leaf type statically.
      if (index.kind == hir::ExpressionKind::IntegerLiteral)
        place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = std::stoll(index.text)});
      else
        place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Index, .indexValue = lowerExpression(index)});
      return place;
    }
    case hir::ExpressionKind::Member:
    {
      auto place = lowerPlace(*expression.operands[0]);
      // A1.2 place-through-ref: `s.field` on `S ref` resolves against S.
      const auto &receiver = descriptorOf(typeOf(*expression.operands[0]));
      const auto &owner = receiver.kind == typecheck::TypeKind::Reference ? descriptorOf(receiver.element) : receiver;
      const auto field = fieldOrdinal(owner, expression.text);
      if (!field.has_value())
        throw VerificationError("member place has no field ordinal");
      place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = static_cast<int64_t>(*field)});
      return place;
    }
    case hir::ExpressionKind::Prefix:
      if (expression.text != "*")
        throw VerificationError("expression is not an assignable place");
      return LoweredPlace{.rootRef = lowerExpression(*expression.operands[0])};
    case hir::ExpressionKind::Grouped:
      return lowerPlace(*expression.operands[0]);
    default:
      throw VerificationError("expression is not an assignable place");
    }
  }
} // namespace NG::flowir::detail
