// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/flowir.hpp"

#include <algorithm>
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
        function_ = Function{.source = source.id};
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

        const bool directCall = expression.kind == hir::ExpressionKind::Call && !expression.operands.empty() &&
                                expression.operands[0]->resolvedName.has_value() &&
                                expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
        std::vector<ValueId> operands;
        operands.reserve(expression.operands.size() - (directCall ? 1 : 0));
        for (size_t index = directCall ? 1 : 0; index < expression.operands.size(); ++index)
        {
          operands.push_back(lowerExpression(*expression.operands[index]));
        }
        int64_t payload{};
        if (expression.kind == hir::ExpressionKind::IntegerLiteral)
        {
          payload = std::stoll(expression.text);
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
        for (const auto &statement : source.statements)
        {
          if (block().terminator.has_value())
          {
            return;
          }
          lowerStatement(statement);
        }
        if (!block().terminator.has_value() && source.tailExpression != nullptr)
        {
          static_cast<void>(lowerExpression(*source.tailExpression));
        }
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
