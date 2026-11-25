// AI-generated code; reviewed for this repository's vNext rewrite.
// FlowIR runtime-loop lowering: array map/filter comprehensions, fold calls,
// and recursive-list literals. Comprehensions and folds share one counted
// loop skeleton (`openIndexLoop`) carrying `(index, accumulator)`.
#include "flowir_lowerer.hpp"

#include <algorithm>

namespace NG::flowir::detail
{
  using typecheck::builtin::Bool;
  using typecheck::builtin::I64;

  auto FunctionLowerer::openIndexLoop(ValueId initialIndex, ValueId initialCarried, TypeId carriedType,
                                      std::string_view test, ValueId bound) -> IndexLoop
  {
    const hir::LocalId indexLocal{nextSyntheticLocal_++};
    const hir::LocalId carriedLocal{nextSyntheticLocal_++};
    function_.localTypes.emplace(indexLocal.value, I64);
    function_.localTypes.emplace(carriedLocal.value, carriedType);
    const BlockId header = appendBlock();
    const BlockId body = appendBlock();
    const BlockId exit = appendBlock();
    function_.blocks[header.value].parameterCount = 2;
    function_.blocks[header.value].parameterLocals = {indexLocal, carriedLocal};
    jump(header, {initialIndex, initialCarried});

    current_ = header;
    const ValueId index = readLocal(indexLocal, I64);
    branch(binary(test, index, bound, Bool), body, exit);
    current_ = body;
    return IndexLoop{.header = header, .exit = exit, .carriedLocal = carriedLocal, .index = index};
  }

  auto FunctionLowerer::loadSpreadElement(const hir::Expression &sourceExpression, ValueId source, ValueId index)
      -> ValueId
  {
    const TypeId sourceType = typeOf(sourceExpression);
    const auto &descriptor = descriptorOf(sourceType);
    if (descriptor.kind == typecheck::TypeKind::Range)
    {
      const ValueId start = emit(Instruction{.kind = InstructionKind::RangeStart, .source = source}, I64);
      return binary("+", start, index, I64);
    }
    if (descriptor.kind == typecheck::TypeKind::Enum)
    {
      // Recursive-list sources read through the std list walker instances
      // (INC-20c `spreadGet`); the element is the Cons head whose tail
      // recurses to the list itself (by reference or Box).
      std::optional<TypeId> elementType;
      for (const auto payload : descriptor.elements)
      {
        const auto &cell = descriptorOf(payload);
        if (cell.kind != typecheck::TypeKind::Tuple || cell.elements.size() != 2)
          continue;
        const auto &tail = descriptorOf(cell.elements[1]);
        const bool recursiveTail = (tail.kind == typecheck::TypeKind::Reference && tail.element == sourceType) ||
                                   (tail.kind == typecheck::TypeKind::Struct && tail.name == "Box" &&
                                    tail.typeArguments.size() == 1 && tail.typeArguments[0] == sourceType);
        if (recursiveTail && !elementType.has_value())
          elementType = cell.elements[0];
      }
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::Call,
                              .callTarget = types_.arrayListSpreadCalls.at(&sourceExpression).second,
                              .operands = {source, index}},
                  elementType);
    }
    return emit(Instruction{.kind = InstructionKind::Evaluate,
                            .expressionKind = hir::ExpressionKind::Index,
                            .operands = {source, index}},
                descriptor.element);
  }

  /// Lowers a list collection literal (`let xs: List<i64> = [1, 2, 3];`) into
  /// a Nil seed plus right-to-left Cons folds so the first element ends up at
  /// the head.
  auto FunctionLowerer::lowerListLiteral(const hir::Expression &expression, TypeId listType) -> ValueId
  {
    const auto &descriptor = descriptorOf(listType);
    std::optional<uint32_t> consVariant;
    std::optional<uint32_t> nilVariant;
    for (uint32_t variant = 0; variant < descriptor.elements.size(); ++variant)
    {
      if (!descriptor.variantHasPayload[variant])
      {
        nilVariant = variant;
        continue;
      }
      const auto &payload = descriptorOf(descriptor.elements[variant]);
      if (payload.kind == typecheck::TypeKind::Tuple && payload.elements.size() == 2)
        consVariant = variant;
    }
    if (!consVariant.has_value() || !nilVariant.has_value())
      throw VerificationError("list literal target is not a recursive list enum");
    const TypeId cellType = descriptor.elements[*consVariant];
    const TypeId tailType = descriptorOf(cellType).elements[1];

    const hir::LocalId accumulator{nextSyntheticLocal_++};
    function_.localTypes.emplace(accumulator.value, listType);
    const auto enumValue = [&](uint32_t variant, std::vector<ValueId> operands)
    {
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::EnumLiteral,
                              .payload = enumLiteralPayload(listType, variant),
                              .operands = std::move(operands)},
                  listType);
    };
    bindLocal(accumulator, enumValue(*nilVariant, {}), listType);
    for (auto it = expression.operands.rbegin(); it != expression.operands.rend(); ++it)
    {
      const ValueId element = lowerExpression(**it);
      const ValueId tail = emit(Instruction{.kind = InstructionKind::MakeRef, .placeRootLocal = accumulator}, tailType);
      const ValueId cell =
          emit(Instruction{.kind = InstructionKind::TupleSplice, .operands = {element, tail}}, cellType);
      bindLocal(accumulator, enumValue(*consVariant, {cell}), listType);
    }
    return readLocal(accumulator, listType);
  }

  /// Lowers an array map literal `[f(xs)...]` (optionally filtered with
  /// `[?p(xs)...]`, optionally mixed with leading/trailing elements) into a
  /// runtime loop: element extraction, per-element calls, and array appends.
  auto FunctionLowerer::lowerMapLiteral(const hir::Expression &expression) -> ValueId
  {
    const auto spread =
        std::ranges::find_if(expression.operands, [](const auto &operand) { return isSpread(*operand); });
    const auto spreadIndex = static_cast<size_t>(std::distance(expression.operands.begin(), spread));
    const TypeId arrayType = typeOf(expression);

    // Mixed comprehensions (`[0, f(xs)..., 9]`): leading elements seed the
    // accumulator and trailing elements append after the loop.
    std::vector<ValueId> leading;
    for (size_t index = 0; index < spreadIndex; ++index)
      leading.push_back(lowerExpression(*expression.operands[index]));

    const hir::Expression *callNode = (*spread)->operands[0].get();
    const bool filter = callNode->kind == hir::ExpressionKind::Prefix && callNode->text == "?";
    if (filter)
      callNode = callNode->operands[0].get();
    const bool isMap = callNode->kind == hir::ExpressionKind::Call && types_.callTargets.contains(callNode);
    const hir::Expression &sourceExpression = isMap ? *callNode->operands[1] : *callNode;
    const ValueId source = lowerExpression(sourceExpression);

    const ValueId zero = integerConstant(0);
    const ValueId one = integerConstant(1);
    const ValueId seed = emit(Instruction{.kind = InstructionKind::Evaluate,
                                          .expressionKind = hir::ExpressionKind::ArrayLiteral,
                                          .operands = std::move(leading)},
                              arrayType);
    const bool listSource = kindOf(sourceExpression) == typecheck::TypeKind::Enum;
    const ValueId length = listSource
                               ? emit(Instruction{.kind = InstructionKind::Evaluate,
                                                  .source = source,
                                                  .expressionKind = hir::ExpressionKind::Call,
                                                  .callTarget = types_.arrayListSpreadCalls.at(&sourceExpression).first,
                                                  .operands = {source}},
                                      I64)
                               : emit(Instruction{.kind = InstructionKind::ArrayLength, .source = source}, I64);

    const IndexLoop loop = openIndexLoop(zero, seed, arrayType, "<", length);
    // The accumulator flows through the loop header; append to it rather
    // than to the seed so each iteration extends the growing array.
    const ValueId accumulated = readLocal(loop.carriedLocal, arrayType);
    const ValueId element = loadSpreadElement(sourceExpression, source, loop.index);
    ValueId mapped = element;
    if (isMap)
      mapped = emit(Instruction{.kind = InstructionKind::Evaluate,
                                .expressionKind = hir::ExpressionKind::Call,
                                .callTarget = types_.callTargets.at(callNode),
                                .operands = {element}},
                    typeOf(*callNode));
    const auto append = [&](ValueId array, ValueId value)
    { return emit(Instruction{.kind = InstructionKind::AppendArray, .operands = {array, value}}, arrayType); };
    const auto advance = [&](ValueId carried) { backedge(loop.header, {binary("+", loop.index, one, I64), carried}); };
    if (filter)
    {
      // The predicate result selects whether the source element is kept.
      const BlockId keep = appendBlock();
      const BlockId skip = appendBlock();
      branch(mapped, keep, skip);
      current_ = keep;
      advance(append(accumulated, element));
      current_ = skip;
      advance(accumulated);
    }
    else
    {
      advance(append(accumulated, mapped));
    }

    current_ = loop.exit;
    ValueId result = readLocal(loop.carriedLocal, arrayType);
    for (size_t index = spreadIndex + 1; index < expression.operands.size(); ++index)
      result = append(result, lowerExpression(*expression.operands[index]));
    return result;
  }

  /// Lowers a fold call (`f(acc, xs...)` / `f(xs..., acc)`) into a runtime
  /// loop: each source element feeds one call whose result becomes the next
  /// accumulator; the final accumulator is the call result. A leading spread
  /// is a right fold and iterates the source backwards.
  auto FunctionLowerer::lowerFoldCall(const hir::Expression &expression) -> ValueId
  {
    const size_t spreadPosition = types_.callFoldSpreadPositions.at(&expression).front();
    const size_t accumulatorPosition = types_.callFoldAccumulatorPositions.at(&expression).front();
    const hir::Expression &sourceExpression = *expression.operands[spreadPosition + 1]->operands[0];
    const TypeId resultType = typeOf(expression);

    const ValueId source = lowerExpression(sourceExpression);
    const ValueId initialAccumulator = lowerExpression(*expression.operands[accumulatorPosition + 1]);
    std::optional<hir::DefId> target;
    if (const auto found = types_.callTargets.find(&expression); found != types_.callTargets.end())
      target = found->second;

    const ValueId zero = integerConstant(0);
    const ValueId one = integerConstant(1);
    const ValueId length = emit(Instruction{.kind = InstructionKind::ArrayLength, .source = source}, I64);
    const bool rightFold = spreadPosition == 0;
    const ValueId initialIndex = rightFold ? binary("-", length, one, I64) : integerConstant(0);
    const IndexLoop loop = rightFold ? openIndexLoop(initialIndex, initialAccumulator, resultType, ">=", zero)
                                     : openIndexLoop(initialIndex, initialAccumulator, resultType, "<", length);

    const ValueId element = loadSpreadElement(sourceExpression, source, loop.index);
    std::vector<ValueId> arguments(2);
    arguments[spreadPosition] = element;
    arguments[accumulatorPosition] = readLocal(loop.carriedLocal, resultType);
    const ValueId next = emit(Instruction{.kind = InstructionKind::Evaluate,
                                          .expressionKind = hir::ExpressionKind::Call,
                                          .callTarget = target,
                                          .operands = std::move(arguments)},
                              resultType);
    backedge(loop.header, {binary(rightFold ? "-" : "+", loop.index, one, I64), next});

    current_ = loop.exit;
    return readLocal(loop.carriedLocal, resultType);
  }
} // namespace NG::flowir::detail
