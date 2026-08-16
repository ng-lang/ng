// AI-generated code; reviewed for this repository's vNext rewrite.
#include "flowir.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <format>
#include <limits>
#include <unordered_map>
#include <utility>

namespace NG::flowir
{
  namespace
  {
    /// Encodes an integer literal's text as the int64 bit payload carried by
    /// `Evaluate` instructions. Unsigned literals above i64::max keep their
    /// two's-complement bits; the checker has already validated the text.
    [[nodiscard]] auto integerLiteralPayload(std::string_view text, typecheck::TypeId type) -> int64_t
    {
      if (typecheck::isUnsignedIntegerBuiltin(type))
      {
        uint64_t magnitude{};
        const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), magnitude);
        if (error != std::errc{} || end != text.data() + text.size())
          throw VerificationError(std::format("invalid integer literal `{}`", text));
        return std::bit_cast<int64_t>(magnitude);
      }
      int64_t value{};
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
      if (error != std::errc{} || end != text.data() + text.size())
        throw VerificationError(std::format("invalid integer literal `{}`", text));
      return value;
    }

    class FunctionLowerer final
    {
    public:
      explicit FunctionLowerer(const typecheck::TypeCheckResult *types) : types_(types) {}

      [[nodiscard]] auto lower(const hir::Function &source) -> Function
      {
        function_ = Function{.source = source.id,
                             .name = source.name,
                             .nativeFunction = source.nativeFunction,
                             .externC = source.externC};
        if (types_ != nullptr)
        {
          function_.typeDescriptors = types_->typeDescriptors;
          if (const auto found = types_->functionTypeIds.find(source.id.value); found != types_->functionTypeIds.end())
            function_.declaredResultType = found->second.returnType;
        }
        reserveSyntheticLocalIds(source);
        for (const auto &parameter : source.parameters)
        {
          function_.parameterLocals.push_back(parameter.local);
          if (types_ != nullptr)
            function_.localTypes.emplace(parameter.local.value, types_->localTypeIds.at(parameter.local.value));
        }
        function_.entry = appendBlock();
        current_ = function_.entry;
        lowerBlock(source.body);
        if (!block().terminator.has_value())
        {
          if (types_ != nullptr)
          {
            if (const auto drops = types_->fallthroughDrops.find(source.id.value);
                drops != types_->fallthroughDrops.end())
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

        if (expression.kind == hir::ExpressionKind::Binary && expression.text == "<<" && types_ != nullptr &&
            types_->typeDescriptors.at(types_->typeIdOf(*expression.operands[0]).value).kind ==
                typecheck::TypeKind::DynamicArray)
        {
          // Value-semantics array append (`xs << value`).
          const ValueId left = lowerExpression(*expression.operands[0]);
          const ValueId right = lowerExpression(*expression.operands[1]);
          const ValueId result{nextValue_++};
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::AppendArray, .result = result, .operands = {left, right}});
          return result;
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
            std::any_of(expression.operands.begin(), expression.operands.end(), [](const auto &operand)
                        { return operand->kind == hir::ExpressionKind::Prefix && operand->text == "..."; }))
          return lowerTupleSplice(expression);

        if (expression.kind == hir::ExpressionKind::ArrayLiteral &&
            std::any_of(expression.operands.begin(), expression.operands.end(), [](const auto &operand)
                        { return operand->kind == hir::ExpressionKind::Prefix && operand->text == "..."; }))
        {
          // Map literals need typed metadata for their loop contracts.
          if (types_ == nullptr)
            throw VerificationError("array map literals require type metadata");
          return lowerMapLiteral(expression);
        }

        if (expression.kind == hir::ExpressionKind::ArrayLiteral && types_ != nullptr)
        {
          // List collection literals type as the expected recursive-list
          // enum; lower them as a Nil seed plus right-to-left Cons folds.
          const typecheck::TypeId resultType = types_->typeIdOf(expression);
          if (types_->typeDescriptors.at(resultType.value).kind == typecheck::TypeKind::Enum)
            return lowerListLiteral(expression, resultType);
        }

        if (expression.kind == hir::ExpressionKind::Call && types_ != nullptr && !expression.operands.empty() &&
            expression.operands[0]->resolvedName.has_value() &&
            expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function &&
            types_->callFoldSpreadPositions.contains(&expression))
          return lowerFoldCall(expression);

        if (expression.kind == hir::ExpressionKind::Index && types_ != nullptr &&
            types_->typeDescriptors.at(types_->typeIdOf(*expression.operands[1]).value).kind ==
                typecheck::TypeKind::Range)
        {
          const ValueId receiver = lowerExpression(*expression.operands[0]);
          const ValueId range = lowerExpression(*expression.operands[1]);
          const ValueId result{nextValue_++};
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::Slice, .result = result, .operands = {receiver, range}});
          return result;
        }

        if (types_ != nullptr && types_->traitViewCoercions.contains(&expression))
        {
          const auto &[traitName, concrete] = types_->traitViewCoercions.at(&expression);
          uint32_t traitType = 0;
          for (size_t index = 0; index < types_->typeDescriptors.size(); ++index)
            if (types_->typeDescriptors[index].kind == typecheck::TypeKind::Trait &&
                types_->typeDescriptors[index].name == traitName)
            {
              traitType = static_cast<uint32_t>(index);
              break;
            }
          auto place = lowerPlace(expression);
          std::vector<ValueId> indexValues;
          for (const auto &step : place.steps)
            if (step.kind == PlaceStep::Kind::Index)
              indexValues.push_back(step.indexValue);
          const ValueId result{nextValue_++};
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::MakeTraitView,
                                                     .result = result,
                                                     .placeRootLocal = place.rootLocal,
                                                     .placeRootRef = place.rootRef,
                                                     .placeSteps = std::move(place.steps),
                                                     .traitType = traitType,
                                                     .payload = concrete.value,
                                                     .operands = std::move(indexValues)});
          return result;
        }

        const bool directCall = expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
                                expression.operands[0]->resolvedName.has_value() &&
                                expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
        if (expression.kind == hir::ExpressionKind::Prefix && (expression.text == "-" || expression.text == "+") &&
            !expression.operands.empty() && expression.operands[0]->kind == hir::ExpressionKind::IntegerLiteral)
        {
          // Fold the sign onto the literal payload so `-9223372036854775808`
          // (i64::min) encodes as one instruction instead of overflowing the
          // positive operand encoding.
          const typecheck::TypeId type = types_ != nullptr ? types_->typeIdOf(expression) : typecheck::builtin::I64;
          uint64_t magnitude{};
          const auto [end, error] =
              std::from_chars(expression.operands[0]->text.data(),
                              expression.operands[0]->text.data() + expression.operands[0]->text.size(), magnitude);
          if (error != std::errc{} || end != expression.operands[0]->text.data() + expression.operands[0]->text.size())
            throw VerificationError(std::format("invalid integer literal `{}`", expression.operands[0]->text));
          int64_t folded{};
          if (expression.text == "-")
          {
            if (typecheck::isUnsignedIntegerBuiltin(type))
              throw VerificationError(std::format("negated unsigned literal `-{}`", expression.operands[0]->text));
            folded = magnitude == (1ULL << 63) ? std::numeric_limits<int64_t>::min() : -static_cast<int64_t>(magnitude);
          }
          else
          {
            folded = static_cast<int64_t>(magnitude);
          }
          const ValueId value{nextValue_++};
          if (types_ != nullptr)
            function_.valueTypes.emplace(value.value, type);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::Evaluate,
                          .result = value,
                          .expressionKind = hir::ExpressionKind::IntegerLiteral,
                          .payload = folded,
                          .text = std::format("{}{}", expression.text, expression.operands[0]->text)});
          return value;
        }
        std::vector<ValueId> operands;
        operands.reserve(expression.operands.size() - (directCall ? 1 : 0));
        const bool hasSpreadPositions =
            directCall && types_ != nullptr && types_->callSpreadPositions.contains(&expression);
        if (hasSpreadPositions)
        {
          const auto spreadPositions = types_->callSpreadPositions.find(&expression);
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
          if (const auto packCount = types_->callPackArgCounts.find(&expression);
              packCount != types_->callPackArgCounts.end())
          {
            // Variadic call: splice the trailing arguments into one tuple value.
            std::vector<ValueId> packOperands;
            for (size_t index = operands.size() - packCount->second; index < operands.size(); ++index)
              packOperands.push_back(operands[index]);
            const ValueId packed{nextValue_++};
            if (const auto packedType = types_->callPackTupleTypes.find(&expression);
                packedType != types_->callPackTupleTypes.end())
              function_.valueTypes.emplace(packed.value, packedType->second);
            block().instructions.push_back(Instruction{
              .kind = InstructionKind::TupleSplice, .result = packed, .operands = std::move(packOperands)});
            operands.resize(operands.size() - packCount->second);
            operands.push_back(packed);
          }
        }
        int64_t payload{};
        if (expression.kind == hir::ExpressionKind::IntegerLiteral)
        {
          const typecheck::TypeId type = types_ != nullptr ? types_->typeIdOf(expression) : typecheck::builtin::I64;
          payload = integerLiteralPayload(expression.text, type);
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
          if (found != descriptor.fieldNames.end())
            payload = static_cast<int64_t>(std::distance(descriptor.fieldNames.begin(), found));
        }
        else if (expression.kind == hir::ExpressionKind::EnumLiteral && types_ != nullptr)
        {
          payload = static_cast<int64_t>(types_->typeIdOf(expression).value) |
                    (static_cast<int64_t>(*expression.variant) << 32);
          // Multi-field variants pack their constructor arguments into one
          // tuple payload value.
          const auto &enumType = types_->typeDescriptors.at(types_->typeIdOf(expression).value);
          if (*expression.variant < enumType.elements.size() && enumType.variantHasPayload.at(*expression.variant) &&
              types_->typeDescriptors.at(enumType.elements.at(*expression.variant).value).kind ==
                  typecheck::TypeKind::Tuple)
          {
            const ValueId packed{nextValue_++};
            function_.valueTypes.emplace(packed.value, enumType.elements.at(*expression.variant));
            block().instructions.push_back(
                Instruction{.kind = InstructionKind::TupleSplice, .result = packed, .operands = std::move(operands)});
            operands = std::vector<ValueId>{packed};
          }
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
          if (expression.text == "!")
            payload = 1;
          else if (expression.text == "-")
            payload = 2;
          else if (expression.text == "+")
            payload = 3;
          else if (expression.text == "move")
            payload = 4;
          else if (expression.text == "clone")
            payload = 5;
        }
        else if (expression.kind == hir::ExpressionKind::Binary)
        {
          if (expression.text == "+")
            payload = 1;
          else if (expression.text == "-")
            payload = 2;
          else if (expression.text == "*")
            payload = 3;
          else if (expression.text == "/")
            payload = 4;
          else if (expression.text == "%")
            payload = 5;
          else if (expression.text == "==")
            payload = 6;
          else if (expression.text == "!=")
            payload = 7;
          else if (expression.text == "<")
            payload = 8;
          else if (expression.text == "<=")
            payload = 9;
          else if (expression.text == ">")
            payload = 10;
          else if (expression.text == ">=")
            payload = 11;
          else if (expression.text == "&&")
            payload = 12;
          else if (expression.text == "||")
            payload = 13;
          else if (expression.text == "&")
            payload = 14;
          else if (expression.text == "|")
            payload = 15;
          else if (expression.text == "^")
            payload = 16;
          else if (expression.text == "<<")
            payload = 17;
          else if (expression.text == ">>")
            payload = 18;
          else if (expression.text == "..")
            payload = 19;
        }

        std::optional<hir::DefId> callTarget;
        if (directCall)
        {
          callTarget = types_ != nullptr && types_->callTargets.contains(&expression)
                           ? types_->callTargets.at(&expression)
                           : hir::DefId{expression.operands[0]->resolvedName->id};
        }

        const ValueId value{nextValue_++};
        if (types_ != nullptr)
          function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
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
        if (types_ != nullptr)
          function_.localTypes.emplace(resultLocal.value, types_->typeIdOf(expression));

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
        if (types_ != nullptr)
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = result,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = resultLocal.value});
        return result;
      }

      [[nodiscard]] auto lowerReferenceExpression(const hir::Expression &expression) -> ValueId
      {
        const ValueId result{nextValue_++};
        if (types_ != nullptr)
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        if (expression.text == "ref" || expression.text == "ref mut")
        {
          auto place = lowerPlace(*expression.operands[0]);
          if (!place.rootLocal.has_value() || place.rootRef.has_value())
            throw VerificationError("reference root must be a local binding");
          std::vector<ValueId> indexValues;
          for (const auto &step : place.steps)
          {
            if (step.kind == PlaceStep::Kind::Index)
              indexValues.push_back(step.indexValue);
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
        if (types_ != nullptr)
          function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::TupleSplice, .result = result, .operands = std::move(operands)});
        return result;
      }

      /// Lowers a list collection literal (`let xs: List<i64> = [1, 2, 3];`)
      /// into a Nil seed plus right-to-left Cons folds so the first element
      /// ends up at the head.
      [[nodiscard]] auto lowerListLiteral(const hir::Expression &expression, typecheck::TypeId listType) -> ValueId
      {
        const auto &descriptor = types_->typeDescriptors.at(listType.value);
        uint32_t consVariant{};
        uint32_t nilVariant{};
        typecheck::TypeId element{};
        bool foundCons = false;
        bool foundNil = false;
        for (uint32_t variant = 0; variant < descriptor.elements.size(); ++variant)
        {
          if (!descriptor.variantHasPayload[variant])
          {
            nilVariant = variant;
            foundNil = true;
            continue;
          }
          const auto &payload = types_->typeDescriptors.at(descriptor.elements[variant].value);
          if (payload.kind == typecheck::TypeKind::Tuple && payload.elements.size() == 2)
          {
            consVariant = variant;
            element = payload.elements[0];
            foundCons = true;
          }
        }
        if (!foundCons || !foundNil)
          throw VerificationError("list literal target is not a recursive list enum");
        const hir::LocalId accLocal{nextSyntheticLocal_++};
        function_.localTypes.emplace(accLocal.value, listType);
        const ValueId nil{nextValue_++};
        function_.valueTypes.emplace(nil.value, listType);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::Evaluate,
                        .result = nil,
                        .expressionKind = hir::ExpressionKind::EnumLiteral,
                        .payload = static_cast<int64_t>(listType.value) | (static_cast<int64_t>(nilVariant) << 32)});
        const ValueId seed{nextValue_++};
        function_.valueTypes.emplace(seed.value, listType);
        block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                   .result = seed,
                                                   .local = accLocal,
                                                   .source = nil,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName});
        for (auto it = expression.operands.rbegin(); it != expression.operands.rend(); ++it)
        {
          const ValueId elementValue = lowerExpression(**it);
          const ValueId refAcc{nextValue_++};
          function_.valueTypes.emplace(refAcc.value,
                                       types_->typeDescriptors.at(descriptor.elements[consVariant].value).elements[1]);
          block().instructions.push_back(Instruction{.kind = InstructionKind::MakeRef,
                                                     .result = refAcc,
                                                     .placeRootLocal = accLocal,
                                                     .placeMutable = false,
                                                     .placeSteps = {}});
          const ValueId packed{nextValue_++};
          function_.valueTypes.emplace(packed.value, descriptor.elements[consVariant]);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::TupleSplice, .result = packed, .operands = {elementValue, refAcc}});
          const ValueId cons{nextValue_++};
          function_.valueTypes.emplace(cons.value, listType);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::Evaluate,
                          .result = cons,
                          .expressionKind = hir::ExpressionKind::EnumLiteral,
                          .payload = static_cast<int64_t>(listType.value) | (static_cast<int64_t>(consVariant) << 32),
                          .operands = {packed}});
          const ValueId bound{nextValue_++};
          function_.valueTypes.emplace(bound.value, listType);
          block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                     .result = bound,
                                                     .local = accLocal,
                                                     .source = cons,
                                                     .expressionKind = hir::ExpressionKind::ResolvedName});
        }
        const ValueId result{nextValue_++};
        function_.valueTypes.emplace(result.value, listType);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = result,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = accLocal.value});
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
        const bool isMap = callNode->kind == hir::ExpressionKind::Call && types_->callTargets.contains(callNode);
        const hir::Expression &sourceExpr = isMap ? *callNode->operands[1] : *callNode;
        const ValueId source = lowerExpression(sourceExpr);
        std::optional<hir::DefId> target;
        if (isMap)
          target = types_->callTargets.at(callNode);

        const ValueId zero{nextValue_++};
        function_.valueTypes.emplace(zero.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = zero,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 0});
        const ValueId one{nextValue_++};
        function_.valueTypes.emplace(one.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = one,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 1});
        const ValueId empty{nextValue_++};
        function_.valueTypes.emplace(empty.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = empty,
                                                   .expressionKind = hir::ExpressionKind::ArrayLiteral,
                                                   .operands = std::move(leading)});
        const bool listSource =
            types_->typeDescriptors.at(types_->typeIdOf(sourceExpr).value).kind == typecheck::TypeKind::Enum;
        const ValueId length{nextValue_++};
        function_.valueTypes.emplace(length.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = listSource ? InstructionKind::EnumListLength : InstructionKind::ArrayLength,
                        .result = length,
                        .source = source});

        const hir::LocalId indexLocal{nextSyntheticLocal_++};
        const hir::LocalId resultLocal{nextSyntheticLocal_++};
        function_.localTypes.emplace(indexLocal.value, typecheck::builtin::I64);
        function_.localTypes.emplace(resultLocal.value, types_->typeIdOf(expression));
        const BlockId header = appendBlock();
        const BlockId body = appendBlock();
        const BlockId exit = appendBlock();
        function_.blocks[header.value].parameterCount = 2;
        function_.blocks[header.value].parameterLocals = {indexLocal, resultLocal};
        block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {header}, .arguments = {zero, empty}};

        current_ = header;
        const ValueId indexRead{nextValue_++};
        function_.valueTypes.emplace(indexRead.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = indexRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = indexLocal.value});
        const ValueId condition{nextValue_++};
        function_.valueTypes.emplace(condition.value, typecheck::builtin::Bool);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = condition,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = "<",
                                                   .payload = 8,
                                                   .operands = {indexRead, length}});
        block().terminator =
            Terminator{.kind = TerminatorKind::Branch, .targets = {body, exit}, .arguments = {condition}};

        current_ = body;
        // The accumulator flows through the loop header; append to it rather
        // than to the seed so each iteration extends the growing array.
        const ValueId accumulatorRead{nextValue_++};
        function_.valueTypes.emplace(accumulatorRead.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = accumulatorRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = resultLocal.value});
        const ValueId element{nextValue_++};
        const bool rangeSource =
            types_->typeDescriptors.at(types_->typeIdOf(sourceExpr).value).kind == typecheck::TypeKind::Range;
        if (rangeSource)
        {
          const ValueId start{nextValue_++};
          function_.valueTypes.emplace(start.value, typecheck::builtin::I64);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::RangeStart, .result = start, .source = source});
          function_.valueTypes.emplace(element.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {start, indexRead}});
        }
        else if (listSource)
        {
          const auto &sourceDescriptor = types_->typeDescriptors.at(types_->typeIdOf(sourceExpr).value);
          for (const auto payload : sourceDescriptor.elements)
          {
            const auto &payloadDescriptor = types_->typeDescriptors.at(payload.value);
            if (payloadDescriptor.kind == typecheck::TypeKind::Tuple && payloadDescriptor.elements.size() == 2 &&
                types_->typeDescriptors.at(payloadDescriptor.elements[1].value).kind == typecheck::TypeKind::Reference)
              function_.valueTypes.emplace(element.value, payloadDescriptor.elements[0]);
          }
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::EnumListGet, .result = element, .operands = {source, indexRead}});
        }
        else
        {
          const auto sourceType = types_->typeDescriptors.at(types_->typeIdOf(sourceExpr).value);
          function_.valueTypes.emplace(element.value, sourceType.element);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Index,
                                                     .operands = {source, indexRead}});
        }
        ValueId appendedSource = element;
        if (isMap)
        {
          const ValueId mapped{nextValue_++};
          function_.valueTypes.emplace(mapped.value, types_->typeIdOf(*callNode));
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = mapped,
                                                     .expressionKind = hir::ExpressionKind::Call,
                                                     .callTarget = target,
                                                     .operands = {element}});
          appendedSource = mapped;
        }
        if (filterMode)
        {
          const BlockId appendPart = appendBlock();
          const BlockId skipPart = appendBlock();
          block().terminator = Terminator{
            .kind = TerminatorKind::Branch, .targets = {appendPart, skipPart}, .arguments = {appendedSource}};

          current_ = appendPart;
          const ValueId appended{nextValue_++};
          function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{
            .kind = InstructionKind::AppendArray, .result = appended, .operands = {accumulatorRead, element}});
          const ValueId nextIndex{nextValue_++};
          function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = nextIndex,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {indexRead, one}});
          block().terminator =
              Terminator{.kind = TerminatorKind::LoopBackedge, .targets = {header}, .arguments = {nextIndex, appended}};

          current_ = skipPart;
          const ValueId skipNext{nextValue_++};
          function_.valueTypes.emplace(skipNext.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = skipNext,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {indexRead, one}});
          block().terminator = Terminator{
            .kind = TerminatorKind::LoopBackedge, .targets = {header}, .arguments = {skipNext, accumulatorRead}};
          current_ = exit;
          ValueId accumulator{nextValue_++};
          function_.valueTypes.emplace(accumulator.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = accumulator,
                                                     .expressionKind = hir::ExpressionKind::ResolvedName,
                                                     .payload = resultLocal.value});
          for (const auto *trailingElement : trailing)
          {
            const ValueId elementValue = lowerExpression(*trailingElement);
            const ValueId appended{nextValue_++};
            function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
            block().instructions.push_back(Instruction{
              .kind = InstructionKind::AppendArray, .result = appended, .operands = {accumulator, elementValue}});
            accumulator = appended;
          }
          return accumulator;
        }
        const ValueId appended{nextValue_++};
        function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{
          .kind = InstructionKind::AppendArray, .result = appended, .operands = {accumulatorRead, appendedSource}});
        const ValueId nextIndex{nextValue_++};
        function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextIndex,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = "+",
                                                   .payload = 1,
                                                   .operands = {indexRead, one}});
        block().terminator =
            Terminator{.kind = TerminatorKind::LoopBackedge, .targets = {header}, .arguments = {nextIndex, appended}};

        current_ = exit;
        ValueId accumulator{nextValue_++};
        function_.valueTypes.emplace(accumulator.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = accumulator,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = resultLocal.value});
        for (const auto *trailingElement : trailing)
        {
          const ValueId elementValue = lowerExpression(*trailingElement);
          const ValueId appended{nextValue_++};
          function_.valueTypes.emplace(appended.value, types_->typeIdOf(expression));
          block().instructions.push_back(Instruction{
            .kind = InstructionKind::AppendArray, .result = appended, .operands = {accumulator, elementValue}});
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
        if (types_->callTargets.contains(&expression))
          target = types_->callTargets.at(&expression);

        const ValueId zero{nextValue_++};
        function_.valueTypes.emplace(zero.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = zero,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 0});
        const ValueId one{nextValue_++};
        function_.valueTypes.emplace(one.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = one,
                                                   .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                   .payload = 1});
        const ValueId length{nextValue_++};
        function_.valueTypes.emplace(length.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::ArrayLength, .result = length, .source = sourceValue});

        const hir::LocalId indexLocal{nextSyntheticLocal_++};
        const hir::LocalId accumulatorLocal{nextSyntheticLocal_++};
        function_.localTypes.emplace(indexLocal.value, typecheck::builtin::I64);
        function_.localTypes.emplace(accumulatorLocal.value, types_->typeIdOf(expression));
        // A spread in the first position is a right fold (`f(xs..., acc)`),
        // which iterates the source backwards; otherwise it is a left fold.
        const bool rightFold = spreadPosition == 0;
        const ValueId initialIndex{nextValue_++};
        function_.valueTypes.emplace(initialIndex.value, typecheck::builtin::I64);
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
        block().terminator = Terminator{
          .kind = TerminatorKind::Jump, .targets = {header}, .arguments = {initialIndex, initialAccumulator}};

        current_ = header;
        const ValueId indexRead{nextValue_++};
        function_.valueTypes.emplace(indexRead.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = indexRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = indexLocal.value});
        const ValueId condition{nextValue_++};
        function_.valueTypes.emplace(condition.value, typecheck::builtin::Bool);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = condition,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = rightFold ? ">=" : "<",
                                                   .payload = rightFold ? 11 : 8,
                                                   .operands = rightFold ? std::vector<ValueId>{indexRead, zero}
                                                                         : std::vector<ValueId>{indexRead, length}});
        block().terminator =
            Terminator{.kind = TerminatorKind::Branch, .targets = {body, exit}, .arguments = {condition}};

        current_ = body;
        const ValueId element{nextValue_++};
        const bool rangeSource =
            types_->typeDescriptors.at(types_->typeIdOf(source).value).kind == typecheck::TypeKind::Range;
        if (rangeSource)
        {
          const ValueId start{nextValue_++};
          function_.valueTypes.emplace(start.value, typecheck::builtin::I64);
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::RangeStart, .result = start, .source = sourceValue});
          function_.valueTypes.emplace(element.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Binary,
                                                     .text = "+",
                                                     .payload = 1,
                                                     .operands = {start, indexRead}});
        }
        else
        {
          const auto sourceType = types_->typeDescriptors.at(types_->typeIdOf(source).value);
          function_.valueTypes.emplace(element.value, sourceType.element);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = element,
                                                     .expressionKind = hir::ExpressionKind::Index,
                                                     .operands = {sourceValue, indexRead}});
        }
        const ValueId accumulatorRead{nextValue_++};
        function_.valueTypes.emplace(accumulatorRead.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = accumulatorRead,
                                                   .expressionKind = hir::ExpressionKind::ResolvedName,
                                                   .payload = accumulatorLocal.value});
        std::vector<ValueId> argumentValues(2);
        argumentValues[spreadPosition] = element;
        argumentValues[accumulatorPosition] = accumulatorRead;
        const ValueId nextAccumulator{nextValue_++};
        function_.valueTypes.emplace(nextAccumulator.value, types_->typeIdOf(expression));
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextAccumulator,
                                                   .expressionKind = hir::ExpressionKind::Call,
                                                   .callTarget = target,
                                                   .operands = std::move(argumentValues)});
        const ValueId nextIndex{nextValue_++};
        function_.valueTypes.emplace(nextIndex.value, typecheck::builtin::I64);
        block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                   .result = nextIndex,
                                                   .expressionKind = hir::ExpressionKind::Binary,
                                                   .text = rightFold ? "-" : "+",
                                                   .payload = rightFold ? 2 : 1,
                                                   .operands = {indexRead, one}});
        block().terminator = Terminator{
          .kind = TerminatorKind::LoopBackedge, .targets = {header}, .arguments = {nextIndex, nextAccumulator}};

        current_ = exit;
        const ValueId result{nextValue_++};
        function_.valueTypes.emplace(result.value, types_->typeIdOf(expression));
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
        const bool qualified =
            receiverNode.kind == hir::ExpressionKind::ResolvedName && !receiverNode.resolvedName.has_value();
        const hir::Expression &receiver = qualified ? *expression.operands[1] : receiverNode;
        if (types_ != nullptr && types_->traitViewCalls.contains(&expression))
        {
          const ValueId view = lowerExpression(receiver);
          std::vector<ValueId> operands{view};
          for (size_t index = qualified ? 2 : 1; index < expression.operands.size(); ++index)
            operands.push_back(lowerExpression(*expression.operands[index]));
          const ValueId value{nextValue_++};
          function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::CallTrait,
                          .result = value,
                          .payload = static_cast<int64_t>(types_->traitViewCalls.at(&expression).second),
                          .operands = std::move(operands)});
          return value;
        }
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
            if (found != types_->methodReceiverMutable.end())
              mutableReference = found->second;
            const auto refType = types_->methodReceiverRefTypes.find(&expression);
            if (refType != types_->methodReceiverRefTypes.end())
              referenceType = refType->second;
          }
          std::vector<ValueId> indexValues;
          for (const auto &step : place.steps)
          {
            if (step.kind == PlaceStep::Kind::Index)
              indexValues.push_back(step.indexValue);
          }
          const ValueId result{nextValue_++};
          if (types_ != nullptr && referenceType.value != 0)
            function_.valueTypes.emplace(result.value, referenceType);
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
          function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
          block().instructions.push_back(
              Instruction{.kind = InstructionKind::LoadRef, .result = value, .operands = {receiverValue}});
          return value;
        }
        std::vector<ValueId> operands{receiverValue};
        for (size_t index = qualified ? 2 : 1; index < expression.operands.size(); ++index)
          operands.push_back(lowerExpression(*expression.operands[index]));
        std::optional<hir::DefId> callTarget;
        if (types_ != nullptr && types_->callTargets.contains(&expression))
          callTarget = types_->callTargets.at(&expression);
        const ValueId value{nextValue_++};
        if (types_ != nullptr)
          function_.valueTypes.emplace(value.value, types_->typeIdOf(expression));
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
            place.steps.push_back(
                PlaceStep{.kind = PlaceStep::Kind::Member, .field = std::stoll(expression.operands[1]->text)});
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
            if (found != descriptor.fieldNames.end())
              field = std::distance(descriptor.fieldNames.begin(), found);
          }
          if (field < 0)
            throw VerificationError("member place has no field ordinal");
          place.steps.push_back(PlaceStep{.kind = PlaceStep::Kind::Member, .field = field});
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
          if (step.kind == PlaceStep::Kind::Index)
            operands.push_back(step.indexValue);
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
        const auto visitExpression = [&observe](const auto &self, const hir::Expression &expression) -> void
        {
          if (expression.resolvedName.has_value() && expression.resolvedName->kind == hir::ResolvedNameKind::Local)
            observe(hir::LocalId{expression.resolvedName->id});
          for (const auto &operand : expression.operands)
            self(self, *operand);
        };
        const auto visitBlock = [&observe, &visitExpression](const auto &self, const hir::Block &block) -> void
        {
          for (const auto &statement : block.statements)
          {
            if (statement.local.has_value())
              observe(*statement.local);
            for (const auto local : statement.destructuredLocals)
              observe(local);
            if (statement.restLocal.has_value())
              observe(*statement.restLocal);
            for (const auto local : statement.loopBindings)
              observe(local);
            if (statement.expression != nullptr)
              visitExpression(visitExpression, *statement.expression);
            for (const auto &argument : statement.arguments)
              visitExpression(visitExpression, *argument);
            for (const auto &switchCase : statement.switchCases)
            {
              if (switchCase.binding.has_value())
                observe(*switchCase.binding);
              for (const auto binding : switchCase.bindings)
                observe(binding);
              if (switchCase.body != nullptr)
                self(self, *switchCase.body);
            }
            if (statement.consequence != nullptr)
              self(self, *statement.consequence);
            if (statement.alternative != nullptr)
              self(self, *statement.alternative);
            if (statement.body != nullptr)
              self(self, *statement.body);
          }
          if (block.tailExpression != nullptr)
            visitExpression(visitExpression, *block.tailExpression);
        };
        for (const auto &parameter : source.parameters)
          observe(parameter.local);
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
        if (!block().terminator.has_value())
          emitBlockDrops(source);
        currentHirBlock_ = previousBlock;
      }

      /// Emits the block-scoped drop calls recorded for a HIR block.
      void emitBlockDrops(const hir::Block &source)
      {
        if (types_ == nullptr)
          return;
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
                function_.valueTypes.emplace(extracted.value,
                                             types_->typeDescriptors.at(tuple.value).elements.at(index));
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
              if (types_ != nullptr)
                function_.valueTypes.emplace(binding.value, function_.valueTypes.at(extracted.value));
              block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                         .result = binding,
                                                         .local = statement.destructuredLocals[index],
                                                         .source = extracted,
                                                         .expressionKind = hir::ExpressionKind::ResolvedName});
            }
            if (statement.restLocal.has_value())
            {
              // `let (first, ...rest) = tuple;`: extract the remaining
              // elements and splice them into the rest tuple.
              std::vector<ValueId> restElements;
              if (types_ != nullptr)
              {
                const auto tuple = types_->typeIdOf(*statement.expression);
                const auto &descriptor = types_->typeDescriptors.at(tuple.value);
                for (size_t index = statement.destructuredLocals.size(); index < descriptor.elements.size(); ++index)
                {
                  const ValueId extracted{nextValue_++};
                  function_.valueTypes.emplace(extracted.value, descriptor.elements[index]);
                  block().instructions.push_back(Instruction{.kind = InstructionKind::ExtractTuple,
                                                             .result = extracted,
                                                             .source = initializer,
                                                             .payload = static_cast<int64_t>(index),
                                                             .expressionKind = hir::ExpressionKind::Index,
                                                             .operands = {initializer}});
                  restElements.push_back(extracted);
                }
              }
              const ValueId restValue{nextValue_++};
              if (types_ != nullptr)
              {
                function_.valueTypes.emplace(restValue.value, types_->localTypeIds.at(statement.restLocal->value));
                function_.localTypes.emplace(statement.restLocal->value,
                                             types_->localTypeIds.at(statement.restLocal->value));
              }
              block().instructions.push_back(Instruction{
                .kind = InstructionKind::TupleSplice, .result = restValue, .operands = std::move(restElements)});
              const ValueId restBinding{nextValue_++};
              if (types_ != nullptr)
                function_.valueTypes.emplace(restBinding.value, function_.valueTypes.at(restValue.value));
              block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                         .result = restBinding,
                                                         .local = statement.restLocal,
                                                         .source = restValue,
                                                         .expressionKind = hir::ExpressionKind::ResolvedName});
            }
          }
          else
          {
            if (types_ != nullptr)
              function_.localTypes.emplace(statement.local->value, types_->localTypeIds.at(statement.local->value));
            const ValueId binding{nextValue_++};
            if (types_ != nullptr)
              function_.valueTypes.emplace(binding.value, types_->typeIdOf(*statement.expression));
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
          block().terminator =
              Terminator{.kind = TerminatorKind::Return, .targets = {}, .arguments = std::move(values)};
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
            if (found != types_->constIfSelections.end())
              consequence = found->second;
          }
          if (consequence)
            lowerBlock(*statement.consequence);
          else if (statement.alternative != nullptr)
            lowerBlock(*statement.alternative);
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
          try
          {
            static_cast<void>(lowerExpression(*statement.expression));
          }
          catch (const std::exception &error)
          {
            throw;
          }
          return;
        }
      }

      void lowerIf(const hir::Statement &statement)
      {
        const ValueId condition = lowerExpression(*statement.expression);
        const BlockId thenBlock = appendBlock();
        const BlockId joinBlock = appendBlock();
        const BlockId elseBlock = statement.alternative != nullptr ? appendBlock() : joinBlock;
        block().terminator =
            Terminator{.kind = TerminatorKind::Branch, .targets = {thenBlock, elseBlock}, .arguments = {condition}};

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
        block().terminator =
            Terminator{.kind = TerminatorKind::Jump, .targets = {header}, .arguments = std::move(initializers)};
        function_.blocks[header.value].parameterCount = statement.loopBindings.size();
        function_.blocks[header.value].parameterLocals = statement.loopBindings;
        if (types_ != nullptr)
        {
          for (const auto local : statement.loopBindings)
            function_.localTypes.emplace(local.value, types_->localTypeIds.at(local.value));
        }
        function_.blocks[header.value].terminator =
            Terminator{.kind = TerminatorKind::Jump, .targets = {body}, .arguments = {}};

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

        if (currentHirBlock_ != nullptr)
          emitBlockDrops(*currentHirBlock_);
        if (statement.nextTarget->kind == hir::NextTargetKind::Loop)
        {
          block().terminator = Terminator{.kind = TerminatorKind::LoopBackedge,
                                          .targets = {loopHeaders_.at(statement.nextTarget->id)},
                                          .arguments = std::move(arguments)};
          return;
        }
        block().terminator =
            Terminator{.kind = TerminatorKind::TailRecur, .targets = {}, .arguments = std::move(arguments)};
      }

      /// Lowers a scalar literal-or switch into an equality-dispatch chain:
      /// each case's literals compare against the scrutinee and branch to the
      /// shared case block; unmatched values fall through to `otherwise`.
      void lowerLiteralSwitch(const hir::Statement &statement, const ValueId scrutinee)
      {
        const size_t caseCount = statement.switchCases.size();
        std::vector<BlockId> caseBlocks;
        caseBlocks.reserve(caseCount);
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
          caseBlocks.push_back(appendBlock());
        const BlockId tailBlock = appendBlock();
        const BlockId exitBlock = appendBlock();
        if (caseCount == 0)
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {tailBlock}, .arguments = {}};
        }

        BlockId nextCheck = current_;
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
        {
          const auto &switchCase = statement.switchCases[indexCase];
          const BlockId afterCase = indexCase + 1 < caseCount ? appendBlock() : tailBlock;
          for (size_t indexLiteral = 0; indexLiteral < switchCase.literalTexts.size(); ++indexLiteral)
          {
            current_ = nextCheck;
            std::string text = switchCase.literalTexts[indexLiteral];
            hir::ExpressionKind literalKind = hir::ExpressionKind::IntegerLiteral;
            if (text == "true" || text == "false")
              literalKind = hir::ExpressionKind::BooleanLiteral;
            else if (!text.empty() && !std::isdigit(static_cast<unsigned char>(text[0])) && text[0] != '-')
              literalKind = hir::ExpressionKind::StringLiteral;
            int64_t literalPayload = 0;
            if (literalKind == hir::ExpressionKind::IntegerLiteral)
              literalPayload = integerLiteralPayload(text, types_ != nullptr ? types_->typeIdOf(*statement.expression)
                                                                             : typecheck::builtin::I64);
            else if (literalKind == hir::ExpressionKind::BooleanLiteral)
              literalPayload = text == "true" ? 1 : 0;
            const ValueId literal{nextValue_++};
            if (types_ != nullptr)
              function_.valueTypes.emplace(literal.value, types_->typeIdOf(*statement.expression));
            block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                       .result = literal,
                                                       .expressionKind = literalKind,
                                                       .payload = literalPayload,
                                                       .text = std::move(text)});
            const ValueId matches{nextValue_++};
            if (types_ != nullptr)
              function_.valueTypes.emplace(matches.value, typecheck::builtin::Bool);
            block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                       .result = matches,
                                                       .expressionKind = hir::ExpressionKind::Binary,
                                                       .text = "==",
                                                       .payload = 6,
                                                       .operands = {scrutinee, literal}});
            nextCheck = indexLiteral + 1 < switchCase.literalTexts.size() ? appendBlock() : afterCase;
            block().terminator = Terminator{
              .kind = TerminatorKind::Branch, .targets = {caseBlocks[indexCase], nextCheck}, .arguments = {matches}};
          }
        }

        current_ = tailBlock;
        if (statement.alternative != nullptr)
          lowerBlock(*statement.alternative);
        if (!block().terminator.has_value())
        {
          block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {exitBlock}, .arguments = {}};
        }
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
        {
          current_ = caseBlocks[indexCase];
          lowerBlock(*statement.switchCases[indexCase].body);
          if (!block().terminator.has_value())
          {
            block().terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {exitBlock}, .arguments = {}};
          }
        }
        current_ = exitBlock;
      }

      void lowerSwitch(const hir::Statement &statement)
      {
        const ValueId scrutinee = lowerExpression(*statement.expression);
        if (!statement.switchCases.empty() && !statement.switchCases.front().literalTexts.empty())
        {
          lowerLiteralSwitch(statement, scrutinee);
          return;
        }
        const ValueId index{nextValue_++};
        if (types_ != nullptr)
          function_.valueTypes.emplace(index.value, typecheck::builtin::I64);
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::EnumVariantIndex, .result = index, .source = scrutinee});

        const auto variantOf = [&](size_t fallback) -> int64_t
        {
          if (types_ == nullptr)
            return static_cast<int64_t>(fallback);
          const auto scrutineeType = types_->typeIdOf(*statement.expression);
          const auto &descriptor = types_->typeDescriptors.at(scrutineeType.value);
          const auto found = std::find(descriptor.fieldNames.begin(), descriptor.fieldNames.end(),
                                       statement.switchCases[fallback].variantName);
          if (found == descriptor.fieldNames.end())
            return static_cast<int64_t>(fallback);
          return std::distance(descriptor.fieldNames.begin(), found);
        };

        const size_t caseCount = statement.switchCases.size();
        std::vector<BlockId> caseBlocks;
        caseBlocks.reserve(caseCount);
        for (size_t indexCase = 0; indexCase < caseCount; ++indexCase)
          caseBlocks.push_back(appendBlock());
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
          if (types_ != nullptr)
            function_.valueTypes.emplace(variantConstant.value, typecheck::builtin::I64);
          block().instructions.push_back(Instruction{.kind = InstructionKind::Evaluate,
                                                     .result = variantConstant,
                                                     .expressionKind = hir::ExpressionKind::IntegerLiteral,
                                                     .payload = variantOf(indexCase)});
          const ValueId matches{nextValue_++};
          if (types_ != nullptr)
            function_.valueTypes.emplace(matches.value, typecheck::builtin::Bool);
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
          block().terminator = Terminator{
            .kind = TerminatorKind::Branch, .targets = {caseBlocks[indexCase], nextCheck}, .arguments = {matches}};
        }

        current_ = tailBlock;
        if (statement.alternative != nullptr)
          lowerBlock(*statement.alternative);
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
              const auto &enumType = types_->typeDescriptors.at(types_->typeIdOf(*statement.expression).value);
              const auto foundVariant =
                  std::find(enumType.fieldNames.begin(), enumType.fieldNames.end(), switchCase.variantName);
              const typecheck::TypeId payloadType = foundVariant != enumType.fieldNames.end()
                                                        ? enumType.elements.at(static_cast<size_t>(
                                                              std::distance(enumType.fieldNames.begin(), foundVariant)))
                                                        : types_->localTypeIds.at(switchCase.binding->value);
              function_.valueTypes.emplace(payload.value, payloadType);
              function_.localTypes.emplace(switchCase.binding->value,
                                           types_->localTypeIds.at(switchCase.binding->value));
            }
            block().instructions.push_back(
                Instruction{.kind = InstructionKind::ExtractEnumPayload, .result = payload, .source = scrutinee});
            if (switchCase.bindings.empty())
            {
              const ValueId binding{nextValue_++};
              if (types_ != nullptr)
                function_.valueTypes.emplace(binding.value, function_.valueTypes.at(payload.value));
              block().instructions.push_back(Instruction{
                .kind = InstructionKind::BindLocal, .result = binding, .local = switchCase.binding, .source = payload});
            }
            else
            {
              std::fflush(stderr);
              const auto &tuple = types_->typeDescriptors.at(function_.valueTypes.at(payload.value).value);
              for (size_t index = 0; index < switchCase.bindings.size() + 1; ++index)
              {
                const ValueId element{nextValue_++};
                if (types_ != nullptr)
                  function_.valueTypes.emplace(element.value, tuple.elements[index]);
                block().instructions.push_back(Instruction{.kind = InstructionKind::ExtractTuple,
                                                           .result = element,
                                                           .source = payload,
                                                           .payload = static_cast<int64_t>(index),
                                                           .operands = {payload}});
                const std::optional<hir::LocalId> target =
                    index == 0 ? switchCase.binding : switchCase.bindings[index - 1];
                const ValueId binding{nextValue_++};
                if (types_ != nullptr)
                {
                  function_.valueTypes.emplace(binding.value, tuple.elements[index]);
                  function_.localTypes.emplace(target->value, tuple.elements[index]);
                }
                block().instructions.push_back(Instruction{
                  .kind = InstructionKind::BindLocal, .result = binding, .local = target, .source = element});
              }
            }
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
        const auto countIndexSteps = [&instruction]()
        {
          return static_cast<size_t>(std::count_if(instruction.placeSteps.begin(), instruction.placeSteps.end(),
                                                   [](const auto &step)
                                                   { return step.kind == PlaceStep::Kind::Index; }));
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
        if ((instruction.kind == InstructionKind::EnumVariantIndex ||
             instruction.kind == InstructionKind::ExtractEnumPayload) &&
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
      const auto requireTargetCount = [&terminator](size_t expected, std::string_view name)
      {
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
} // namespace NG::flowir
