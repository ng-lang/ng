// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/flowir.hpp"

#include <algorithm>
#include <bit>
#include <limits>
#include <unordered_map>
#include <utility>

namespace NG::vnext::flowir
{
  namespace
  {
    class FunctionLowerer final
    {
    public:
      explicit FunctionLowerer(const typecheck::TypeCheckResult *types) : types_(types) {}

      [[nodiscard]] auto lower(const hir::Function &source) -> Function
      {
        function_ = Function{.source = source.id, .name = source.name, .nativeFunction = source.nativeFunction};
        if (types_ != nullptr) function_.typeDescriptors = types_->typeDescriptors;
        reserveSyntheticLocalIds(source);
        for (const auto &parameter : source.parameters)
        {
          function_.parameterLocals.push_back(parameter.local);
          if (types_ != nullptr) function_.localTypes.emplace(parameter.local.value, types_->localTypeIds.at(parameter.local.value));
        }
        function_.entry = appendBlock();
        current_ = function_.entry;
        lowerBlock(source.body);
        if (!block().terminator.has_value())
        {
          if (types_ != nullptr)
          {
            if (const auto drops = types_->fallthroughDrops.find(source.id.value); drops != types_->fallthroughDrops.end())
              lowerDropCalls(drops->second);
          }
          block().terminator = Terminator{.kind = TerminatorKind::Return, .targets = {}, .arguments = {}};
        }
        return std::move(function_);
      }

    private:
      struct LoweredPlace
      {
        std::optional<hir::LocalId> rootLocal;
        std::optional<ValueId> rootRef;
        std::vector<PlaceStep> steps;
      };

      [[nodiscard]] auto appendBlock() -> BlockId
      {
        const BlockId id{static_cast<uint32_t>(function_.blocks.size())};
        function_.blocks.push_back(Block{.id = id});
        return id;
      }

      [[nodiscard]] auto block() -> Block & { return function_.blocks[current_.value]; }

      [[nodiscard]] auto lowerExpression(const hir::Expression &expression) -> ValueId
      {
        if (expression.kind == hir::ExpressionKind::Binary && (expression.text == "&&" || expression.text == "||"))
        {
          return lowerLogicalExpression(expression);
        }

        if (expression.kind == hir::ExpressionKind::Prefix &&
            (expression.text == "ref" || expression.text == "ref mut" || expression.text == "*"))
        {
          return lowerReferenceExpression(expression);
        }

        if (expression.kind == hir::ExpressionKind::GenericApplication)
          throw VerificationError("const predicate application is not a runtime value");

        if (expression.kind == hir::ExpressionKind::Call && expression.methodCall)
          return lowerMethodCall(expression);

        if (expression.kind == hir::ExpressionKind::TupleLiteral &&
            std::any_of(expression.operands.begin(), expression.operands.end(), [](const auto &operand) {
              return operand->kind == hir::ExpressionKind::Prefix && operand->text == "...";
            }))
          return lowerTupleSplice(expression);

        if (expression.kind == hir::ExpressionKind::ArrayLiteral &&
            std::any_of(expression.operands.begin(), expression.operands.end(), [](const auto &operand) {
              return operand->kind == hir::ExpressionKind::Prefix && operand->text == "...";
            }))
          return lowerMapLiteral(expression);

        if (expression.kind == hir::ExpressionKind::Call && types_ != nullptr &&
            !expression.operands.empty() && expression.operands[0]->resolvedName.has_value() &&
            expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function &&
            types_->callFoldSpreadPositions.contains(&expression))
          return lowerFoldCall(expression);

        if (expression.kind == hir::ExpressionKind::Index && types_ != nullptr &&
            types_->typeDescriptors.at(types_->typeIdOf(*expression.operands[1]).value).kind == typecheck::TypeKind::Range)
        {
          const ValueId receiver = lowerExpression(*expression.operands[0]);
          const ValueId range = lowerExpression(*expression.operands[1]);
          const ValueId result{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::Slice, .result = result, .operands = {receiver, range}});
          return result;
        }

        const bool directCall = expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
                                expression.operands[0]->resolvedName.has_value() &&
                                expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
        std::vector<ValueId> operands;
        operands.reserve(expression.operands.size() - (directCall ? 1 : 0));
        const auto spreadPositions = directCall && types_ != nullptr
                                         ? types_->callSpreadPositions.find(&expression)
                                         : types_->callSpreadPositions.end();
        if (directCall && types_ != nullptr && spreadPositions != types_->callSpreadPositions.end())
        {
          // Tuple spreads flatten statically: each element extracts from the
          // spread tuple into its own call argument.
          size_t spreadIndex = 0;
          for (size_t index = 1; index < expression.operands.size(); ++index)
          {
            if (spreadIndex < spreadPositions->second.size() && spreadPositions->second[spreadIndex] == index - 1)
            {
              const auto &spreadOperand = *expression.operands[index]->operands[0];
              const ValueId tuple = lowerExpression(spreadOperand);
              const auto tupleType = types_->typeIdOf(spreadOperand);

              const auto &descriptor = types_->typeDescriptors.at(tupleType.value);
              for (size_t element = 0; element < descriptor.elements.size(); ++element)
              {
                const ValueId extracted{nextValue_++};
                function_.valueTypes.emplace(extracted.value, descriptor.elements[element]);
                block().instructions.push_back(Instruction{.kind = InstructionKind::ExtractTuple,
                                                           .result = extracted,
                                                           .source = tuple,
                                                           .payload = static_cast<int64_t>(element),
                                                           .operands = {tuple}});
                operands.push_back(extracted);
              }
              ++spreadIndex;
              continue;
            }
            operands.push_back(lowerExpression(*expression.operands[index]));
          }
        }
        else
        {
          for (size_t index = directCall ? 1 : 0; index < expression.operands.size(); ++index)
          {
            operands.push_back(lowerExpression(*expression.operands[index]));
          }
        }
        if (directCall && types_ != nullptr)
        {
          if (const auto packCount = types_->callPackArgCounts.find(&expression); packCount != types_->callPackArgCounts.end())
          {
            // Variadic call: splice the trailing arguments into one tuple value.
            std::vector<ValueId> packOperands;
            for (size_t index = operands.size() - packCount->second; index < operands.size(); ++index)
              packOperands.push_back(operands[index]);
            const ValueId packed{nextValue_++};
            if (const auto packedType = types_->callPackTupleTypes.find(&expression); packedType != types_->callPackTupleTypes.end())
              function_.valueTypes.emplace(packed.value, packedType->second);
            block().instructions.push_back(Instruction{.kind = InstructionKind::TupleSplice,
                                                       .result = packed,
                                                       .operands = std::move(packOperands)});
            operands.resize(operands.size() - packCount->second);
            operands.push_back(packed);
          }
        }
        int64_t payload{};
        if (expression.kind == hir::ExpressionKind::IntegerLiteral)
        {
          payload = std::stoll(expression.text);
        }
        else if (expression.kind == hir::ExpressionKind::FloatLiteral)
        {
          payload = std::bit_cast<int64_t>(std::stod(expression.text));
        }
        else if (expression.kind == hir::ExpressionKind::BooleanLiteral)
        {
          payload = expression.text == "true" ? 1 : 0;
        }
        else if (expression.kind == hir::ExpressionKind::ResolvedName &&
                 expression.resolvedName->kind == hir::ResolvedNameKind::Local)
        {
          payload = expression.resolvedName->id;
        }
        else if (expression.kind == hir::ExpressionKind::Index && !expression.text.empty())
        {
          payload = std::stoll(expression.text);
        }
        else if (expression.kind == hir::ExpressionKind::Member && types_ != nullptr)
        {
          const auto receiver = types_->typeIdOf(*expression.operands[0]);
          const auto &descriptor = types_->typeDescriptors.at(receiver.value);
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
          if (found != descriptor.fieldNames.end()) payload = static_cast<int64_t>(std::distance(descriptor.fieldNames.begin(), found));
        }
        else if (expression.kind == hir::ExpressionKind::EnumLiteral && types_ != nullptr)
        {
          payload = static_cast<int64_t>(types_->typeIdOf(expression).value) |
                    (static_cast<int64_t>(*expression.variant) << 32);
        }
        else if (expression.kind == hir::ExpressionKind::StructLiteral && types_ != nullptr)
        {
          payload = types_->typeIdOf(expression).value;
          std::vector<ValueId> ordered;
          const auto &descriptor = types_->typeDescriptors.at(static_cast<size_t>(payload));
          for (const auto &field : descriptor.fieldNames)
          {
            const auto found = std::find(expression.memberNames.begin(), expression.memberNames.end(), field);
            ordered.push_back(operands.at(static_cast<size_t>(std::distance(expression.memberNames.begin(), found))));
          }
          operands = std::move(ordered);
        }
        else if (expression.kind == hir::ExpressionKind::Prefix)
        {
          if (expression.text == "!") payload = 1;
          else if (expression.text == "-") payload = 2;
          else if (expression.text == "+") payload = 3;
          else if (expression.text == "move") payload = 4;
          else if (expression.text == "clone") payload = 5;
        }
        else if (expression.kind == hir::ExpressionKind::Binary)
        {
          if (expression.text == "+") payload = 1;
          else if (expression.text == "-") payload = 2;
          else if (expression.text == "*") payload = 3;
          else if (expression.text == "/") payload = 4;
          else if (expression.text == "%") payload = 5;
          else if (expression.text == "==") payload = 6;
          else if (expression.text == "!=") payload = 7;
          else if (expression.text == "<") payload = 8;
          else if (expression.text == "<=") payload = 9;
          else if (expression.text == ">") payload = 10;
          else if (expression.text == ">=") payload = 11;
          else if (expression.text == "&&") payload = 12;
          else if (expression.text == "||") payload = 13;
          else if (expression.text == "&") payload = 14;
          else if (expression.text == "|") payload = 15;
          else if (expression.text == "^") payload = 16;
          else if (expression.text == "<<") payload = 17;
          else if (expression.text == ">>") payload = 18;
          else if (expression.text == "..") payload = 19;
        }

        std::optional<hir::DefId> callTarget;
        if (directCall)
        {
          callTarget = types_ != nullptr && types_->callTargets.contains(&expression)
                         ? types_->callTargets.at(&expression)
                         : hir::DefId{expression.operands[0]->resolvedName->id};
        }

        const ValueId value{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = value,
                                                   .expressionKind = expression.kind,
                                                   .text = expression.text,
                                                   .payload = payload,
                                                   .callTarget = callTarget,
                                                   .operands = std::move(operands)});
        return value;
      }

      [[nodiscard]] auto lowerLogicalExpression(const hir::Expression &expression) -> ValueId
      {
        const ValueId left = lowerExpression(*expression.operands[0]);
        const BlockId rightBlock = appendBlock();
        const BlockId shortCircuitBlock = appendBlock();
        const BlockId joinBlock = appendBlock();
        const hir::LocalId resultLocal{nextSyntheticLocal_++};
        auto &join = function_.blocks[joinBlock.value];
        join.parameterCount = 1;
        join.parameterLocals = {resultLocal};
        if (types_ != nullptr) function_.localTypes.emplace(resultLocal.value, types_->typeIdOf(expression));

        const bool isAnd = expression.text == "&&";
        block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                        .targets = isAnd ? std::vector<BlockId>{rightBlock, shortCircuitBlock}
                                                         : std::vector<BlockId>{shortCircuitBlock, rightBlock},
                                        .arguments = {left}};

        current_ = rightBlock;
        const ValueId right = lowerExpression(*expression.operands[1]);
        if (!block().terminator.has_value())
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {joinBlock}, .arguments = {right}};
        }

        current_ = shortCircuitBlock;
        block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {joinBlock}, .arguments = {left}};

        current_ = joinBlock;
        const ValueId result{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = result,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = resultLocal.value});
        return result;
      }

      [[nodiscard]] auto lowerReferenceExpression(const hir::Expression &expression) -> ValueId
      {
        const ValueId result{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        if (expression.text == "ref" || expression.text == "ref mut")
        {
          auto place = lowerPlace(*expression.operands[0]);
          if (!place.rootLocal.has_value() || place.rootRef.has_value())
            throw VerificationError("reference root must be a local binding");
          std::vector<ValueId> indexValues;
          for (const auto &step : place.steps)
          {
            if (step.kind == PlaceStep::Kind::Index) indexValues.push_back(step.indexValue);
          }
          block().instructions.push_back(Instruction{.kind = InstructionKind::MakeRef,
                                                     .result = result,
                                                     .placeRootLocal = place.rootLocal,
                                                     .placeMutable = expression.text == "ref mut",
                                                     .placeSteps = std::move(place.steps),
                                                     .operands = std::move(indexValues)});
          return result;
        }
        const ValueId reference = lowerExpression(*expression.operands[0]);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::LoadRef, .result = result, .operands = {reference}});
        return result;
      }

      /// Lowers a tuple literal containing spreads to a runtime splice.
      [[nodiscard]] auto lowerTupleSplice(const hir::Expression &expression) -> ValueId
      {
        std::vector<ValueId> operands;
        for (const auto &element : expression.operands)
        {
          if (element->kind == hir::ExpressionKind::Prefix && element->text == "...")
            operands.push_back(lowerExpression(*element->operands[0]));
          else
            operands.push_back(lowerExpression(*element));
        }
        const ValueId result{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::TupleSplice,
                                                   .result = result,
                                                   .operands = std::move(operands)});
        return result;
      }

      /// Lowers a single-spread array map literal `[f(xs)...]` into a runtime
      /// loop: element extraction, per-element calls, and array appends.
      [[nodiscard]] auto lowerMapLiteral(const hir::Expression &expression) -> ValueId
      {
        const hir::Expression *mapSpread = nullptr;
        size_t spreadIndex{};
        for (size_t index = 0; index < expression.operands.size(); ++index)
        {
          if (expression.operands[index]->kind == hir::ExpressionKind::Prefix &&
              expression.operands[index]->text == "...")
          {
            mapSpread = expression.operands[index].get();
            spreadIndex = index;
            break;
          }
        }
        if (mapSpread == nullptr)
          throw VerificationError("array map literals require a map spread");
        // Mixed comprehensions (`[0, f(xs)..., 9]`): leading literal elements
        // seed the accumulator and trailing elements append after the loop.
        std::vector<ValueId> leading;
        leading.reserve(spreadIndex);
        for (size_t index = 0; index < spreadIndex; ++index)
          leading.push_back(lowerExpression(*expression.operands[index]));
        std::vector<const hir::Expression *> trailing;
        trailing.reserve(expression.operands.size() - spreadIndex - 1);
        for (size_t index = spreadIndex + 1; index < expression.operands.size(); ++index)
          trailing.push_back(expression.operands[index].get());
        bool filterMode = false;
        const hir::Expression *callNode = mapSpread->operands[0].get();
        if (callNode->kind == hir::ExpressionKind::Prefix && callNode->text == "?")
        {
          filterMode = true;
          callNode = callNode->operands[0].get();
        }
        const auto &call = *callNode;
        const ValueId source = lowerExpression(*call.operands[1]);
        std::optional<hir::DefId> target;
        if (types_ != nullptr && types_->callTargets.contains(&call)) target = types_->callTargets.at(&call);

        const ValueId zero{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(zero.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = zero,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 0});
        const ValueId one{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(one.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = one,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 1});
        const ValueId empty{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(empty.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = empty,
                                                   .expressionKind = hir::ExpressionKind::ArrayLiteral,
                                                   .operands = std::move(leading)});
        const ValueId length{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(length.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::ArrayLength, .result = length, .source = source});

        const hir::LocalId indexLocal{nextSyntheticLocal_++};
        const hir::LocalId resultLocal{nextSyntheticLocal_++};
        if (types_ != nullptr)
        {
          function_.localTypes.emplace(indexLocal.value, typecheck::builtin::I64);
          function_.localTypes.emplace(resultLocal.value, types_->typeIdOf(expression));
        }
        const BlockId header = appendBlock();
        const BlockId body = appendBlock();
        const BlockId exit = appendBlock();
        function_.blocks[header.value].parameterCount = 2;
        function_.blocks[header.value].parameterLocals = {indexLocal, resultLocal};
        block().terminator = Terminator{.kind = TerminatorKind::Jump,
                                        .targets = {header},
                                        .arguments = {zero, empty}};

        current_ = header;
        const ValueId indexRead{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(indexRead.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = indexRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = indexLocal.value});
        const ValueId condition{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(condition.value, typecheck::builtin::Bool);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = condition,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = "<",
                                                   .payload = 8,
                                                   .operands = {indexRead, length}});
        block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                        .targets = {body, exit},
                                        .arguments = {condition}};

        current_ = body;
        const ValueId element{nextValue_++};
        const bool rangeSource = types_ != nullptr &&
                                 types_->typeDescriptors.at(types_->typeIdOf(*call.operands[1]).value).kind ==
                                     typecheck::TypeKind::Range;
        if (rangeSource)
        {
          const ValueId start{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(start.value, typecheck::builtin::I64);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::RangeStart, .result = start, .source = source});
          if (types_ != nullptr) function_.valueTypes.emplace(element.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {start, indexRead}});
        }
        else
        {
          if (types_ != nullptr)
          {
            const auto sourceType = types_->typeDescriptors.at(types_->typeIdOf(*call.operands[1]).value);
            function_.valueTypes.emplace(element.value, sourceType.element);
          }
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Index,
                                                     .operands = {source, indexRead}});
        }
        const ValueId mapped{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(mapped.value, types_->typeIdOf(call));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = mapped,
                                                   .expressionKind = hir::ExpressionKind::Call,
                                                   .callTarget = target,
                                                   .operands = {element}});
        if (filterMode)
        {
          const BlockId appendPart = appendBlock();
          const BlockId skipPart = appendBlock();
          block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                          .targets = {appendPart, skipPart},
                                          .arguments = {mapped}};

          current_ = appendPart;
          const ValueId appended{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::AppendArray,
                                                     .result = appended,
                                                     .operands = {empty, element}});
          const ValueId nextIndex{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = nextIndex,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {indexRead, one}});
          block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                          .targets = {header},
                                          .arguments = {nextIndex, appended}};

          current_ = skipPart;
          const ValueId skipNext{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(skipNext.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = skipNext,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {indexRead, one}});
          block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                          .targets = {header},
                                          .arguments = {skipNext, empty}};
          current_ = exit;
          ValueId accumulator{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(accumulator.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = accumulator,
                                                     .expressionKind = hir::ExpressionKind::ResolvedName,
                                                     .payload = resultLocal.value});
          for (const auto *trailingElement : trailing)
          {
            const ValueId elementValue = lowerExpression(*trailingElement);
            const ValueId appended{nextValue_++};
            if (types_ != nullptr) function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
            block().instructions.push_back(Instruction{.kind = InstructionKind::AppendArray,
                                                       .result = appended,
                                                       .operands = {accumulator, elementValue}});
            accumulator = appended;
          }
          return accumulator;
        }
        const ValueId appended{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::AppendArray,
                                                   .result = appended,
                                                   .operands = {empty, mapped}});
        const ValueId nextIndex{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextIndex,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = "+",
                                                   .payload = 1,
                                                   .operands = {indexRead, one}});
        block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                        .targets = {header},
                                        .arguments = {nextIndex, appended}};

        current_ = exit;
        ValueId accumulator{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(accumulator.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = accumulator,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = resultLocal.value});
        for (const auto *trailingElement : trailing)
        {
          const ValueId elementValue = lowerExpression(*trailingElement);
          const ValueId appended{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::AppendArray,
                                                     .result = appended,
                                                     .operands = {accumulator, elementValue}});
          accumulator = appended;
        }
        return accumulator;
      }

      /// Lowers a fold call (`f(acc, xs...)` / `f(xs..., acc)`) into a runtime
      /// loop: the accumulator is seeded, each source element feeds one call
      /// whose result becomes the next accumulator, and the final accumulator
      /// is the call result.
      [[nodiscard]] auto lowerFoldCall(const hir::Expression &expression) -> ValueId
      {
        const size_t spreadPosition = types_->callFoldSpreadPositions.at(&expression).front();
        const size_t accumulatorPosition = types_->callFoldAccumulatorPositions.at(&expression).front();
        const hir::Expression &spread = *expression.operands[spreadPosition + 1];
        const hir::Expression &source = *spread.operands[0];
        const hir::Expression &accumulatorOperand = *expression.operands[accumulatorPosition + 1];

        const ValueId sourceValue = lowerExpression(source);
        const ValueId initialAccumulator = lowerExpression(accumulatorOperand);
        std::optional<hir::DefId> target;
        if (types_->callTargets.contains(&expression)) target = types_->callTargets.at(&expression);

        const ValueId zero{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(zero.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = zero,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 0});
        const ValueId one{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(one.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = one,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 1});
        const ValueId length{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(length.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::ArrayLength, .result = length, .source = sourceValue});

        const hir::LocalId indexLocal{nextSyntheticLocal_++};
        const hir::LocalId accumulatorLocal{nextSyntheticLocal_++};
        if (types_ != nullptr)
        {
          function_.localTypes.emplace(indexLocal.value, typecheck::builtin::I64);
          function_.localTypes.emplace(accumulatorLocal.value, types_->typeIdOf(expression));
        }
        // A spread in the first position is a right fold (`f(xs..., acc)`),
        // which iterates the source backwards; otherwise it is a left fold.
        const bool rightFold = spreadPosition == 0;
        const ValueId initialIndex{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(initialIndex.value, typecheck::builtin::I64);
        if (rightFold)
        {
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = initialIndex,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "-",
                                                     .payload = 2,
                                                     .operands = {length, one}});
        }
        else
        {
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = initialIndex,
                                                     .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                     .payload = 0});
        }
        const BlockId header = appendBlock();
        const BlockId body = appendBlock();
        const BlockId exit = appendBlock();
        function_.blocks[header.value].parameterCount = 2;
        function_.blocks[header.value].parameterLocals = {indexLocal, accumulatorLocal};
        block().terminator = Terminator{.kind = TerminatorKind::Jump,
                                        .targets = {header},
                                        .arguments = {initialIndex, initialAccumulator}};

        current_ = header;
        const ValueId indexRead{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(indexRead.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = indexRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = indexLocal.value});
        const ValueId condition{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(condition.value, typecheck::builtin::Bool);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = condition,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = rightFold ? ">=" : "<",
                                                   .payload = rightFold ? 11 : 8,
                                                   .operands = rightFold ? std::vector<ValueId>{indexRead, zero}
                                                                         : std::vector<ValueId>{indexRead, length}});
        block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                        .targets = {body, exit},
                                        .arguments = {condition}};

        current_ = body;
        const ValueId element{nextValue_++};
        const bool rangeSource = types_->typeDescriptors.at(types_->typeIdOf(source).value).kind ==
                                 typecheck::TypeKind::Range;
        if (rangeSource)
        {
          const ValueId start{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(start.value, typecheck::builtin::I64);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::RangeStart, .result = start, .source = sourceValue});
          if (types_ != nullptr) function_.valueTypes.emplace(element.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {start, indexRead}});
        }
        else
        {
          if (types_ != nullptr)
          {
            const auto sourceType = types_->typeDescriptors.at(types_->typeIdOf(source).value);
            function_.valueTypes.emplace(element.value, sourceType.element);
          }
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Index,
                                                     .operands = {sourceValue, indexRead}});
        }
        const ValueId accumulatorRead{nextValue_++};
        if (types_ != nullptr)
          function_.valueTypes.emplace(accumulatorRead.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = accumulatorRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = accumulatorLocal.value});
        std::vector<ValueId> argumentValues(2);
        argumentValues[spreadPosition] = element;
        argumentValues[accumulatorPosition] = accumulatorRead;
        const ValueId nextAccumulator{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(nextAccumulator.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextAccumulator,
                                                   .expressionKind = hir::ExpressionKind::Call,
                                                   .callTarget = target,
                                                   .operands = std::move(argumentValues)});
        const ValueId nextIndex{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextIndex,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = rightFold ? "-" : "+",
                                                   .payload = rightFold ? 2 : 1,
                                                   .operands = {indexRead, one}});
        block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                        .targets = {header},
                                        .arguments = {nextIndex, nextAccumulator}};

        current_ = exit;
        const ValueId result{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = result,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = accumulatorLocal.value});
        return result;
      }

      /// Lowers a method call: the receiver is borrowed (`MakeRef`) when it is
      /// a value place, passed through when it is already a reference, and the
      /// call dispatches to the selected impl/default method.
      [[nodiscard]] auto lowerMethodCall(const hir::Expression &expression) -> ValueId
      {
        const auto &callee = *expression.operands[0];
        const auto &receiverNode = *callee.operands[0];
        const bool qualified = !receiverNode.resolvedName.has_value();
        const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
        ValueId receiverValue;
        if (types_ != nullptr && types_->typeIdOf(receiver).value != 0 &&
            types_->typeDescriptors.at(types_->typeIdOf(receiver).value).kind == typecheck::TypeKind::Reference)
        {
          receiverValue = lowerExpression(receiver);
        }
        else
        {
          auto place = lowerPlace(receiver);
          if (!place.rootLocal.has_value() || place.rootRef.has_value())
            throw VerificationError("method receiver must be a local place");
          bool mutableReference{};
          typecheck::TypeId referenceType{};
          if (types_ != nullptr)
          {
            const auto found = types_->methodReceiverMutable.find(&expression);
            if (found != types_->methodReceiverMutable.end()) mutableReference = found->second;
            const auto refType = types_->methodReceiverRefTypes.find(&expression);
            if (refType != types_->methodReceiverRefTypes.end()) referenceType = refType->second;
          }
          std::vector<ValueId> indexValues;
          for (const auto &step : place.steps)
          {
            if (step.kind == PlaceStep::Kind::Index) indexValues.push_back(step.indexValue);
          }
          const ValueId result{nextValue_++};
          if (types_ != nullptr && referenceType.value != 0) function_.valueTypes.emplace(result.value, referenceType);
          block().instructions.push_back(Instruction{.kind = InstructionKind::MakeRef,
                                                     .result = result,
                                                     .placeRootLocal = place.rootLocal,
                                                     .placeMutable = mutableReference,
                                                     .placeSteps = std::move(place.steps),
                                                     .operands = std::move(indexValues)});
          receiverValue = result;
        }
        if (types_ != nullptr && types_->derivedCloneCalls.contains(&expression))
        {
          // Derived clone: shared-borrow the receiver place and deep-copy the
          // referenced value (LoadRef copies); no function call is emitted.
          const ValueId value{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::LoadRef,
                                                     .result = value,
                                                     .operands = {receiverValue}});
          return value;
        }
        std::vector<ValueId> operands{receiverValue};
        for (size_t index = qualified ? 2 : 1; index < expression.operands.size(); ++index)
          operands.push_back(lowerExpression(*expression.operands[index]));
        std::optional<hir::DefId> callTarget;
        if (types_ != nullptr && types_->callTargets.contains(&expression)) callTarget = types_->callTargets.at(&expression);
        const ValueId value{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = value,
                                                   .expressionKind = hir::ExpressionKind::Call,
                                                   .text = expression.text,
                                                   .callTarget = callTarget,
                                                   .operands = std::move(operands)});
        return value;
      }

      /// Lowers a place expression to its root (a current-frame local or a
      /// reference value) plus the steps to the leaf. Index expressions along
      /// the path are lowered into the current block.
      [[nodiscard]] auto lowerPlace(const hir::Expression &expression) -> LoweredPlace
      {
        switch (expression.kind)
        {
        case hir::ExpressionKind::ResolvedName:
          if (!expression.resolvedName.has_value() || expression.resolvedName->kind != hir::ResolvedNameKind::Local)
            throw VerificationError("place root is not a local binding");
          return LoweredPlace{.rootLocal = hir::LocalId{expression.resolvedName->id}};
        case hir::ExpressionKind::Index:
        {
          auto place = lowerPlace(*expression.operands[0]);
          if (expression.operands[1]->kind == hir::ExpressionKind::IntegerLiteral)
          {
            // Constant projections (tuples and literal array indexes) become member
            // steps so bytecode metadata can verify the leaf type statically.
            place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = std::stoll(expression.operands[1]->text)});
          }
          else
          {
            place.steps.push_back(
                PlaceStep{.kind = PlaceStep::Kind::Index, .indexValue = lowerExpression(*expression.operands[1])});
          }
          return place;
        }
        case hir::ExpressionKind::Member:
        {
          auto place = lowerPlace(*expression.operands[0]);
          int64_t field = -1;
          if (types_ != nullptr)
          {
            const auto receiver = types_->typeIdOf(*expression.operands[0]);
            const auto &descriptor = types_->typeDescriptors.at(receiver.value);
            const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), expression.text);
            if (found != descriptor.fieldNames.end()) field = std::distance(descriptor.fieldNames.begin(), found);
          }
          if (field < 0) throw VerificationError("member place has no field ordinal");
          place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = field});
          return place;
        }
        case hir::ExpressionKind::Prefix:
          if (expression.text != "*") throw VerificationError("expression is not an assignable place");
          return LoweredPlace{.rootRef = lowerExpression(*expression.operands[0])};
        case hir::ExpressionKind::Grouped:
          return lowerPlace(*expression.operands[0]);
        default:
          throw VerificationError("expression is not an assignable place");
        }
      }

      void lowerAssignment(const hir::Statement &statement)
      {
        const ValueId value = lowerExpression(*statement.expression);
        LoweredPlace place;
        if (statement.assignmentTarget != nullptr)
        {
          place = lowerPlace(*statement.assignmentTarget);
        }
        else
        {
          place.rootLocal = statement.local;
        }
        std::vector<ValueId> operands;
        for (const auto &step : place.steps)
        {
          if (step.kind == PlaceStep::Kind::Index) operands.push_back(step.indexValue);
        }
        operands.push_back(value);
        block().instructions.push_back(Instruction{.kind = InstructionKind::AssignPlace,
                                                   .result = ValueId{nextValue_++},
                                                   .placeRootLocal = place.rootLocal,
                                                   .placeRootRef = place.rootRef,
                                                   .placeSteps = std::move(place.steps),
                                                   .operands = std::move(operands)});
      }

      /// Lowers the checker-recorded drop calls: borrow the local (drop
      /// receivers are `Self ref`) and call the drop method.
      void lowerDropCalls(const std::vector<std::pair<uint32_t, uint32_t>> &drops)
      {
        for (const auto &[localValue, targetValue] : drops)
        {
          const ValueId reference{nextValue_++};
          if (types_ != nullptr)
          {
            const auto signature = types_->functionTypeIds.find(targetValue);
            if (signature != types_->functionTypeIds.end() && !signature->second.parameters.empty())
              function_.valueTypes.emplace(reference.value, signature->second.parameters.front());
          }
          block().instructions.push_back(Instruction{.kind = InstructionKind::MakeRef,
                                                     .result = reference,
                                                     .placeRootLocal = hir::LocalId{localValue},
                                                     .placeMutable = false});
          const ValueId result{nextValue_++};
          if (types_ != nullptr)
          {
            const auto signature = types_->functionTypeIds.find(targetValue);
            if (signature != types_->functionTypeIds.end())
              function_.valueTypes.emplace(result.value, signature->second.returnType);
          }
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = result,
                                                     .expressionKind = hir::ExpressionKind::Call,
                                                     .callTarget = hir::DefId{targetValue},
                                                     .operands = {reference}});
        }
      }

      void reserveSyntheticLocalIds(const hir::Function &source)
      {
        uint32_t highest{};
        const auto observe = [&highest](hir::LocalId local) { highest = std::max(highest, local.value); };
        const auto visitExpression = [&observe](const auto &self, const hir::Expression &expression) -> void {
          if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
            observe(hir::LocalId{expression.resolvedName->id});
          for (const auto &operand : expression.operands) self(self, *operand);
        };
        const auto visitBlock = [&observe, &visitExpression](const auto &self, const hir::Block &block) -> void {
          for (const auto &statement : block.statements)
          {
            if (statement.local.has_value()) observe(*statement.local);
            for (const auto local : statement.destructuredLocals) observe(local);
            for (const auto local : statement.loopBindings) observe(local);
            if (statement.expression != nullptr) visitExpression(visitExpression, *statement.expression);
            for (const auto &argument : statement.arguments) visitExpression(visitExpression, *argument);
            if (statement.consequence != nullptr) self(self, *statement.consequence);
            if (statement.alternative != nullptr) self(self, *statement.alternative);
            if (statement.body != nullptr) self(self, *statement.body);
          }
          if (block.tailExpression != nullptr) visitExpression(visitExpression, *block.tailExpression);
        };
        for (const auto &parameter : source.parameters) observe(parameter.local);
        visitBlock(visitBlock, source.body);
        if (highest == std::numeric_limits<uint32_t>::max())
          throw VerificationError("FlowIR local id space is exhausted");
        nextSyntheticLocal_ = highest + 1;
      }

      void lowerBlock(const hir::Block &source)
      {
        const hir::Block *previousBlock = currentHirBlock_;
        currentHirBlock_ = &source;
        for (const auto &statement : source.statements)
        {
          if (block().terminator.has_value())
          {
            break;
          }
          lowerStatement(statement);
        }
        if (!block().terminator.has_value() && source.tailExpression != nullptr)
        {
          static_cast<void>(lowerExpression(*source.tailExpression));
        }
        if (!block().terminator.has_value()) emitBlockDrops(source);
        currentHirBlock_ = previousBlock;
      }

      /// Emits the block-scoped drop calls recorded for a HIR block.
      void emitBlockDrops(const hir::Block &source)
      {
        if (types_ == nullptr) return;
        if (const auto drops = types_->blockDrops.find(&source); drops != types_->blockDrops.end())
          lowerDropCalls(drops->second);
      }

      void lowerStatement(const hir::Statement &statement)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
        {
          const ValueId initializer = lowerExpression(*statement.expression);
          if (!statement.destructuredLocals.empty())
          {
            for (size_t index = 0; index < statement.destructuredLocals.size(); ++index)
            {
              const ValueId extracted{nextValue_++};
              if (types_ != nullptr)
              {
                const auto tuple = types_->typeIdOf(*statement.expression);
                function_.valueTypes.emplace(extracted.value, types_->typeDescriptors.at(tuple.value).elements.at(index));
                function_.localTypes.emplace(statement.destructuredLocals[index].value,
                                             types_->localTypeIds.at(statement.destructuredLocals[index].value));
              }
              block().instructions.push_back(Instruction{.kind = InstructionKind::ExtractTuple,
                                                         .result = extracted,
                                                         .source = initializer,
                                                         .payload = static_cast<int64_t>(index),
                                                         .expressionKind = hir::ExpressionKind::Index,
                                                         .operands = {initializer}});
              const ValueId binding{nextValue_++};
              if (types_ != nullptr) function_.valueTypes.emplace(binding.value, function_.valueTypes.at(extracted.value));
              block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                         .result = binding,
                                                         .local = statement.destructuredLocals[index],
                                                         .source = extracted,
                                                         .expressionKind = hir::ExpressionKind::ResolvedName});
            }
          }
          else
          {
            if (types_ != nullptr) function_.localTypes.emplace(statement.local->value, types_->localTypeIds.at(statement.local->value));
            const ValueId binding{nextValue_++};
            if (types_ != nullptr) function_.valueTypes.emplace(binding.value, types_->typeIdOf(*statement.expression));
            block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                       .result = binding,
                                                       .local = statement.local,
                                                       .source = initializer,
                                                       .expressionKind = statement.expression->kind});
          }
          return;
        }
        case hir::StatementKind::Assign:
          lowerAssignment(statement);
          return;
        case hir::StatementKind::Return:
        {
          std::vector<ValueId> values;
          if (statement.expression != nullptr)
          {
            values.push_back(lowerExpression(*statement.expression));
          }
          if (types_ != nullptr)
          {
            if (const auto drops = types_->returnDrops.find(&statement); drops != types_->returnDrops.end())
              lowerDropCalls(drops->second);
          }
          block().terminator = Terminator{.kind = TerminatorKind::Return, .targets = {}, .arguments = std::move(values)};
          return;
        }
        case hir::StatementKind::If:
          lowerIf(statement);
          return;
        case hir::StatementKind::ConstIf:
        {
          bool consequence = true;
          if (types_ != nullptr)
          {
            const auto found = types_->constIfSelections.find(&statement);
            if (found != types_->constIfSelections.end()) consequence = found->second;
          }
          if (consequence) lowerBlock(*statement.consequence);
          else if (statement.alternative != nullptr) lowerBlock(*statement.alternative);
          return;
        }
        case hir::StatementKind::Loop:
          lowerLoop(statement);
          return;
        case hir::StatementKind::Next:
          lowerNext(statement);
          return;
        case hir::StatementKind::Switch:
          lowerSwitch(statement);
          return;
        case hir::StatementKind::Expression:
          static_cast<void>(lowerExpression(*statement.expression));
          return;
        }
      }

      void lowerIf(const hir::Statement &statement)
      {
        const ValueId condition = lowerExpression(*statement.expression);
        const BlockId thenBlock = appendBlock();
        const BlockId joinBlock = appendBlock();
        const BlockId elseBlock = statement.alternative != nullptr ? appendBlock() : joinBlock;
        block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                        .targets = {thenBlock, elseBlock},
                                        .arguments = {condition}};

        current_ = thenBlock;
        lowerBlock(*statement.consequence);
        if (!block().terminator.has_value())
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {joinBlock}, .arguments = {}};
        }
        if (statement.alternative != nullptr)
        {
          current_ = elseBlock;
          lowerBlock(*statement.alternative);
          if (!block().terminator.has_value())
          {
            block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {joinBlock}, .arguments = {}};
          }
        }
        current_ = joinBlock;
      }

      void lowerLoop(const hir::Statement &statement)
      {
        std::vector<ValueId> initializers;
        initializers.reserve(statement.arguments.size());
        for (const auto &initializer : statement.arguments)
        {
          initializers.push_back(lowerExpression(*initializer));
        }

        const BlockId header = appendBlock();
        const BlockId body = appendBlock();
        const BlockId exit = appendBlock();
        block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {header}, .arguments = std::move(initializers)};
        function_.blocks[header.value].parameterCount = statement.loopBindings.size();
        function_.blocks[header.value].parameterLocals = statement.loopBindings;
        if (types_ != nullptr)
        {
          for (const auto local : statement.loopBindings)
            function_.localTypes.emplace(local.value, types_->localTypeIds.at(local.value));
        }
        function_.blocks[header.value].terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {body}, .arguments = {}};

        loopHeaders_.emplace(statement.loop->value, header);
        current_ = body;
        lowerBlock(*statement.body);
        if (!block().terminator.has_value())
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {exit}, .arguments = {}};
        }
        loopHeaders_.erase(statement.loop->value);
        current_ = exit;
      }

      void lowerNext(const hir::Statement &statement)
      {
        std::vector<ValueId> arguments;
        arguments.reserve(statement.arguments.size());
        for (const auto &argument : statement.arguments)
        {
          arguments.push_back(lowerExpression(*argument));
        }

        if (currentHirBlock_ != nullptr) emitBlockDrops(*currentHirBlock_);
        if (statement.nextTarget->kind == hir::NextTargetKind::Loop)
        {
          block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                          .targets = {loopHeaders_.at(statement.nextTarget->id)},
                                          .arguments = std::move(arguments)};
          return;
        }
        block().terminator = Terminator{.kind = TerminatorKind::TailRecur,
                                        .targets = {},
                                        .arguments = std::move(arguments)};
      }

      void lowerSwitch(const hir::Statement &statement)
      {
        const ValueId scrutinee = lowerExpression(*statement.expression);
        const ValueId index{nextValue_++};
        if (types_ != nullptr) function_.valueTypes.emplace(index.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::EnumVariantIndex, .result = index, .source = scrutinee});

        const auto variantOf = [&](size_t fallback) -> int64_t {
          if (types_ == nullptr) return static_cast<int64_t>(fallback);
          const auto scrutineeType = types_->typeIdOf(*statement.expression);
          const auto &descriptor = types_->typeDescriptors.at(scrutineeType.value);
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(), statement.switchCases[fallback].variantName);
          if (found == descriptor.fieldNames.end()) return static_cast<int64_t>(fallback);
          return std::distance(descriptor.fieldNames.begin(), found);
        };

        const size_t caseCount = statement.switchCases.size();
        std::vector<BlockId> caseBlocks;
        caseBlocks.reserve(caseCount);
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase) caseBlocks.push_back(appendBlock());
        const BlockId tailBlock = appendBlock();
        const BlockId exitBlock = appendBlock();
        if (caseCount == 0)
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {tailBlock}, .arguments = {}};
        }

        BlockId nextCheck = current_;
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
        {
          current_ = nextCheck;
          const ValueId variantConstant{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(variantConstant.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = variantConstant,
                                                     .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                     .payload = variantOf(indexCase)});
          const ValueId matches{nextValue_++};
          if (types_ != nullptr) function_.valueTypes.emplace(matches.value, typecheck::builtin::Bool);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = matches,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "==",
                                                     .payload = 6,
                                                     .operands = {index, variantConstant}});
          if (indexCase + 1 < caseCount)
          {
            nextCheck = appendBlock();
          }
          else
          {
            nextCheck = tailBlock;
          }
          block().terminator = Terminator{.kind = TerminatorKind::Branch,
                                          .targets = {caseBlocks[indexCase], nextCheck},
                                          .arguments = {matches}};
        }

        current_ = tailBlock;
        if (statement.alternative != nullptr) lowerBlock(*statement.alternative);
        if (!block().terminator.has_value())
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {exitBlock}, .arguments = {}};
        }

        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
        {
          current_ = caseBlocks[indexCase];
          const auto &switchCase = statement.switchCases[indexCase];
          if (switchCase.binding.has_value())
          {
            const ValueId payload{nextValue_++};
            if (types_ != nullptr)
            {
              function_.valueTypes.emplace(payload.value, types_->localTypeIds.at(switchCase.binding->value));
              function_.localTypes.emplace(switchCase.binding->value, types_->localTypeIds.at(switchCase.binding->value));
            }
            block().instructions.push_back(
                Instruction{.kind = InstructionKind::ExtractEnumPayload, .result = payload, .source = scrutinee});
            const ValueId binding{nextValue_++};
            if (types_ != nullptr) function_.valueTypes.emplace(binding.value, function_.valueTypes.at(payload.value));
            block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                       .result = binding,
                                                       .local = switchCase.binding,
                                                       .source = payload});
          }
          lowerBlock(*switchCase.body);
          if (!block().terminator.has_value())
          {
            block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {exitBlock}, .arguments = {}};
          }
        }
        current_ = exitBlock;
      }

      Function function_;
      BlockId current_{};
      uint32_t nextValue_{};
      uint32_t nextSyntheticLocal_{};
      const typecheck::TypeCheckResult *types_{};
      const hir::Block *currentHirBlock_{};
      std::unordered_map<uint32_t, BlockId> loopHeaders_;
    };
  } // namespace

  auto Lowerer::lower(const hir::Function &function) -> Function
  {
    return FunctionLowerer{nullptr}.lower(function);
  }

  auto Lowerer::lower(const hir::Function &function, const typecheck::TypeCheckResult &types) -> Function
  {
    return FunctionLowerer{&types}.lower(function);
  }

  void Verifier::verify(const Function &function) const
  {
    if (function.blocks.empty())
    {
      throw VerificationError("FlowIR function has no blocks");
    }
    if (function.entry.value >= function.blocks.size())
    {
      throw VerificationError("FlowIR entry block is out of range");
    }

    for (size_t index = 0; index < function.blocks.size(); ++index)
    {
      const auto &block = function.blocks[index];
      if (block.id.value != index)
      {
        throw VerificationError("FlowIR block id does not match block index");
      }
      if (!block.terminator.has_value())
      {
        throw VerificationError("FlowIR block has no terminator");
      }
      for (const auto &instruction : block.instructions)
      {
        const auto countIndexSteps = [&instruction]() {
          return static_cast<size_t>(std::count_if(instruction.placeSteps.begin(), instruction.placeSteps.end(),
                                                   [](const auto &step) { return step.kind == PlaceStep::Kind::Index; }));
        };
        if (instruction.kind == InstructionKind::ExtractTuple && instruction.operands.size() != 1)
          throw VerificationError("FlowIR tuple extraction requires one source operand");
        if (instruction.kind == InstructionKind::LoadRef && instruction.operands.size() != 1)
          throw VerificationError("FlowIR reference load requires one source operand");
        if (instruction.kind == InstructionKind::Slice && instruction.operands.size() != 2)
          throw VerificationError("FlowIR slice requires receiver and range operands");
        if (instruction.kind == InstructionKind::ArrayLength && !instruction.source.has_value())
          throw VerificationError("FlowIR array length requires a source operand");
        if (instruction.kind == InstructionKind::RangeStart && !instruction.source.has_value())
          throw VerificationError("FlowIR range start requires a source operand");
        if (instruction.kind == InstructionKind::AppendArray && instruction.operands.size() != 2)
          throw VerificationError("FlowIR array append requires array and element operands");
        if ((instruction.kind == InstructionKind::EnumVariantIndex || instruction.kind == InstructionKind::ExtractEnumPayload) &&
            !instruction.source.has_value())
          throw VerificationError("FlowIR enum operation requires a source operand");
        if (instruction.kind == InstructionKind::MakeRef)
        {
          if (!instruction.placeRootLocal.has_value() || instruction.placeRootRef.has_value())
            throw VerificationError("FlowIR reference creation requires a local place root");
          if (instruction.operands.size() != countIndexSteps())
            throw VerificationError("FlowIR reference creation index operand count mismatch");
        }
        if (instruction.kind == InstructionKind::AssignPlace)
        {
          if (instruction.placeRootLocal.has_value() == instruction.placeRootRef.has_value())
            throw VerificationError("FlowIR place assignment requires exactly one place root");
          if (instruction.operands.size() != countIndexSteps() + 1)
            throw VerificationError("FlowIR place assignment operand count mismatch");
        }
      }

      const auto &terminator = *block.terminator;
      const auto requireTargetCount = [&terminator](size_t expected, std::string_view name) {
        if (terminator.targets.size() != expected)
        {
          throw VerificationError(std::string{name} + " has invalid target count");
        }
      };
      switch (terminator.kind)
      {
      case TerminatorKind::Return:
        requireTargetCount(0, "return");
        if (terminator.arguments.size() > 1)
        {
          throw VerificationError("return has too many values");
        }
        break;
      case TerminatorKind::TailRecur:
        requireTargetCount(0, "tail recursion");
        break;
      case TerminatorKind::Jump:
      case TerminatorKind::LoopBackedge:
        requireTargetCount(1, terminator.kind == TerminatorKind::Jump ? "jump" : "loop backedge");
        break;
      case TerminatorKind::Branch:
        requireTargetCount(2, "branch");
        if (terminator.arguments.size() != 1)
        {
          throw VerificationError("branch requires exactly one condition value");
        }
        break;
      }

      for (const auto target : terminator.targets)
      {
        if (target.value >= function.blocks.size())
        {
          throw VerificationError("FlowIR terminator target is out of range");
        }
      }
      if ((terminator.kind == TerminatorKind::Jump || terminator.kind == TerminatorKind::LoopBackedge) &&
          terminator.arguments.size() != function.blocks[terminator.targets[0].value].parameterCount)
      {
        throw VerificationError("FlowIR terminator argument count does not match target block parameters");
      }
    }
  }
} // namespace NG::vnext::flowir
