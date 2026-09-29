// AI-generated code; reviewed for this repository's vNext rewrite.
// FlowIR lowering core: entry point, emission primitives, blocks, statements,
// structured control flow (if/loop/while/next), and the CFG verifier.
// Expression, runtime-loop, and switch lowering live in flowir_expr.cpp,
// flowir_loops.cpp, and flowir_switch.cpp (see flowir_lowerer.hpp).
#include "flowir_lowerer.hpp"

#include <algorithm>
#include <bit>
#include <charconv>
#include <format>
#include <limits>

namespace NG::flowir
{
  namespace detail
  {
    auto integerLiteralPayload(std::string_view text, TypeId type) -> int64_t
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

    auto binaryOperatorCode(std::string_view op) -> int64_t
    {
      static constexpr std::string_view kOperators[] = {"+",  "-",  "*",  "/", "%", "==", "!=", "<",  "<=", ">",
                                                        ">=", "&&", "||", "&", "|", "^",  "<<", ">>", ".."};
      const auto found = std::ranges::find(kOperators, op);
      return found == std::end(kOperators) ? 0 : std::distance(std::begin(kOperators), found) + 1;
    }

    auto prefixOperatorCode(std::string_view op) -> int64_t
    {
      static constexpr std::string_view kOperators[] = {"!", "-", "+", "move", "clone"};
      const auto found = std::ranges::find(kOperators, op);
      return found == std::end(kOperators) ? 0 : std::distance(std::begin(kOperators), found) + 1;
    }

    auto fieldOrdinal(const typecheck::TypeDescriptor &descriptor, std::string_view name) -> std::optional<size_t>
    {
      const auto found = std::ranges::find(descriptor.fieldNames, name);
      if (found == descriptor.fieldNames.end())
        return std::nullopt;
      return static_cast<size_t>(std::distance(descriptor.fieldNames.begin(), found));
    }

    // ---- Entry point ---------------------------------------------------------

    auto FunctionLowerer::lower(const hir::Function &source) -> Function
    {
      function_ = Function{
        .source = source.id, .name = source.name, .nativeFunction = source.nativeFunction, .externC = source.externC};
      function_.typeDescriptors = types_.typeDescriptors;
      function_.derivedEqTypes = types_.derivedEqTypes;
      if (const auto found = types_.functionTypeIds.find(source.id.value); found != types_.functionTypeIds.end())
        function_.declaredResultType = found->second.returnType;
      reserveSyntheticLocalIds(source);
      for (const auto &parameter : source.parameters)
      {
        function_.parameterLocals.push_back(parameter.local);
        function_.localTypes.emplace(parameter.local.value, types_.localTypeIds.at(parameter.local.value));
      }
      function_.entry = appendBlock();
      current_ = function_.entry;
      functionBody_ = &source.body;
      lowerBlock(source.body);
      if (!terminated())
      {
        if (const auto drops = types_.fallthroughDrops.find(source.id.value); drops != types_.fallthroughDrops.end())
          lowerDropCalls(drops->second);
        std::vector<ValueId> arguments;
        if (fallthroughReturn_.has_value())
          arguments.push_back(*fallthroughReturn_);
        block().terminator = Terminator{.kind = TerminatorKind::Return, .arguments = std::move(arguments)};
      }
      return std::move(function_);
    }

    // ---- Emission primitives -------------------------------------------------

    auto FunctionLowerer::appendBlock() -> BlockId
    {
      const BlockId id{static_cast<uint32_t>(function_.blocks.size())};
      function_.blocks.push_back(Block{.id = id});
      return id;
    }

    auto FunctionLowerer::emit(Instruction instruction, std::optional<TypeId> type) -> ValueId
    {
      const ValueId result{nextValue_++};
      instruction.result = result;
      if (type.has_value())
        function_.valueTypes.emplace(result.value, *type);
      block().instructions.push_back(std::move(instruction));
      return result;
    }

    auto FunctionLowerer::integerConstant(int64_t value) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::IntegerLiteral,
                              .payload = value},
                  typecheck::builtin::I64);
    }

    auto FunctionLowerer::literal(std::string text, hir::ExpressionKind kind, TypeId type) -> ValueId
    {
      int64_t payload = 0;
      if (kind == hir::ExpressionKind::IntegerLiteral)
        payload = integerLiteralPayload(text, type);
      else if (kind == hir::ExpressionKind::FloatLiteral)
        payload = std::bit_cast<int64_t>(std::stod(text));
      else if (kind == hir::ExpressionKind::BooleanLiteral)
        payload = text == "true" ? 1 : 0;
      return emit(
          Instruction{
            .kind = InstructionKind::Evaluate, .expressionKind = kind, .text = std::move(text), .payload = payload},
          type);
    }

    auto FunctionLowerer::readLocal(hir::LocalId local, TypeId type) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::ResolvedName,
                              .payload = local.value},
                  type);
    }

    auto FunctionLowerer::binary(std::string_view op, ValueId left, ValueId right, TypeId type) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::Binary,
                              .text = std::string{op},
                              .payload = binaryOperatorCode(op),
                              .operands = {left, right}},
                  type);
    }

    auto FunctionLowerer::bindLocal(hir::LocalId local, ValueId source, TypeId type) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::BindLocal,
                              .local = local,
                              .source = source,
                              .expressionKind = hir::ExpressionKind::ResolvedName},
                  type);
    }

    auto FunctionLowerer::extractMember(ValueId source, size_t field, TypeId type) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::Evaluate,
                              .expressionKind = hir::ExpressionKind::Member,
                              .text = std::to_string(field),
                              .payload = static_cast<int64_t>(field),
                              .operands = {source}},
                  type);
    }

    auto FunctionLowerer::extractTuple(ValueId source, size_t element, TypeId type) -> ValueId
    {
      return emit(Instruction{.kind = InstructionKind::ExtractTuple,
                              .source = source,
                              .expressionKind = hir::ExpressionKind::Index,
                              .payload = static_cast<int64_t>(element),
                              .operands = {source}},
                  type);
    }

    void FunctionLowerer::jump(BlockId target, std::vector<ValueId> arguments)
    {
      block().terminator =
          Terminator{.kind = TerminatorKind::Jump, .targets = {target}, .arguments = std::move(arguments)};
    }

    void FunctionLowerer::jumpIfOpen(BlockId target)
    {
      if (!terminated())
        jump(target);
    }

    void FunctionLowerer::branch(ValueId condition, BlockId whenTrue, BlockId whenFalse)
    {
      block().terminator =
          Terminator{.kind = TerminatorKind::Branch, .targets = {whenTrue, whenFalse}, .arguments = {condition}};
    }

    void FunctionLowerer::backedge(BlockId header, std::vector<ValueId> arguments)
    {
      block().terminator =
          Terminator{.kind = TerminatorKind::LoopBackedge, .targets = {header}, .arguments = std::move(arguments)};
    }

    auto FunctionLowerer::typeOf(const hir::Expression &expression) const -> TypeId
    {
      return types_.typeIdOf(expression);
    }

    auto FunctionLowerer::descriptorOf(TypeId type) const -> const typecheck::TypeDescriptor &
    {
      return types_.typeDescriptors.at(type.value);
    }

    auto FunctionLowerer::kindOf(const hir::Expression &expression) const -> typecheck::TypeKind
    {
      return descriptorOf(typeOf(expression)).kind;
    }

    auto FunctionLowerer::indexOperands(const std::vector<PlaceStep> &steps) -> std::vector<ValueId>
    {
      std::vector<ValueId> values;
      for (const auto &step : steps)
        if (step.kind == PlaceStep::Kind::Index)
          values.push_back(step.indexValue);
      return values;
    }

    // ---- Blocks and scoped drops ---------------------------------------------

    void FunctionLowerer::reserveSyntheticLocalIds(const hir::Function &source)
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
          for (const auto *nested : {statement.consequence.get(), statement.alternative.get(), statement.body.get()})
            if (nested != nullptr)
              self(self, *nested);
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

    void FunctionLowerer::lowerBlock(const hir::Block &source)
    {
      const hir::Block *previousBlock = currentHirBlock_;
      currentHirBlock_ = &source;
      for (const auto &statement : source.statements)
      {
        if (terminated())
          break;
        lowerStatement(statement);
      }
      if (!terminated() && source.tailExpression != nullptr)
      {
        const ValueId tailValue = lowerExpression(*source.tailExpression);
        if (functionBody_ == &source && function_.declaredResultType != typecheck::builtin::Unit)
          fallthroughReturn_ = tailValue;
      }
      if (!terminated())
        emitBlockDrops(source);
      currentHirBlock_ = previousBlock;
    }

    /// Emits the block-scoped drop calls recorded for a HIR block.
    void FunctionLowerer::emitBlockDrops(const hir::Block &source)
    {
      if (const auto drops = types_.blockDrops.find(&source); drops != types_.blockDrops.end())
        lowerDropCalls(drops->second);
    }

    /// Lowers the checker-recorded drop calls: borrow the local (drop
    /// receivers are `Self ref`) and call the drop method.
    void FunctionLowerer::lowerDropCalls(const std::vector<std::pair<uint32_t, uint32_t>> &drops)
    {
      for (const auto &[localValue, targetValue] : drops)
      {
        const auto signature = types_.functionTypeIds.find(targetValue);
        const bool known = signature != types_.functionTypeIds.end();
        std::optional<TypeId> receiverType;
        std::optional<TypeId> resultType;
        if (known)
        {
          resultType = signature->second.returnType;
          if (!signature->second.parameters.empty())
            receiverType = signature->second.parameters.front();
        }
        const ValueId reference = emit(
            Instruction{.kind = InstructionKind::MakeRef, .placeRootLocal = hir::LocalId{localValue}}, receiverType);
        emit(Instruction{.kind = InstructionKind::Evaluate,
                         .expressionKind = hir::ExpressionKind::Call,
                         .callTarget = hir::DefId{targetValue},
                         .operands = {reference}},
             resultType);
      }
    }

    // ---- Statements ----------------------------------------------------------

    void FunctionLowerer::lowerStatement(const hir::Statement &statement)
    {
      switch (statement.kind)
      {
      case hir::StatementKind::Let:
        lowerLet(statement);
        return;
      case hir::StatementKind::Assign:
        lowerAssignment(statement);
        return;
      case hir::StatementKind::Return:
        lowerReturn(statement);
        return;
      case hir::StatementKind::If:
        lowerIf(statement);
        return;
      case hir::StatementKind::ConstIf:
        lowerConstIf(statement);
        return;
      case hir::StatementKind::Loop:
        lowerLoop(statement);
        return;
      case hir::StatementKind::While:
        lowerWhile(statement);
        return;
      case hir::StatementKind::Continue:
        lowerLoopExit(statement, loopHeaders_);
        return;
      case hir::StatementKind::Break:
        lowerLoopExit(statement, loopExits_);
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

    void FunctionLowerer::lowerLet(const hir::Statement &statement)
    {
      const ValueId initializer = lowerExpression(*statement.expression);
      if (!statement.destructuredLocals.empty())
      {
        lowerTupleDestructuring(statement, initializer);
        return;
      }
      function_.localTypes.emplace(statement.local->value, types_.localTypeIds.at(statement.local->value));
      bindLocal(*statement.local, initializer, typeOf(*statement.expression));
    }

    /// `let (a, b) = tuple;` extracts each element into its binding;
    /// `let (first, ...rest) = tuple;` splices the remaining elements into
    /// the rest tuple.
    void FunctionLowerer::lowerTupleDestructuring(const hir::Statement &statement, ValueId initializer)
    {
      const auto &tuple = descriptorOf(typeOf(*statement.expression));
      for (size_t index = 0; index < statement.destructuredLocals.size(); ++index)
      {
        const hir::LocalId local = statement.destructuredLocals[index];
        const TypeId elementType = tuple.elements.at(index);
        const ValueId extracted = extractTuple(initializer, index, elementType);
        function_.localTypes.emplace(local.value, types_.localTypeIds.at(local.value));
        bindLocal(local, extracted, elementType);
      }
      if (!statement.restLocal.has_value())
        return;
      std::vector<ValueId> restElements;
      for (size_t index = statement.destructuredLocals.size(); index < tuple.elements.size(); ++index)
        restElements.push_back(extractTuple(initializer, index, tuple.elements[index]));
      const TypeId restType = types_.localTypeIds.at(statement.restLocal->value);
      function_.localTypes.emplace(statement.restLocal->value, restType);
      const ValueId restValue =
          emit(Instruction{.kind = InstructionKind::TupleSplice, .operands = std::move(restElements)}, restType);
      bindLocal(*statement.restLocal, restValue, restType);
    }

    void FunctionLowerer::lowerAssignment(const hir::Statement &statement)
    {
      const ValueId value = lowerExpression(*statement.expression);
      LoweredPlace place = statement.assignmentTarget != nullptr ? lowerPlace(*statement.assignmentTarget)
                                                                 : LoweredPlace{.rootLocal = statement.local};
      std::vector<ValueId> operands = indexOperands(place.steps);
      operands.push_back(value);
      emit(Instruction{.kind = InstructionKind::AssignPlace,
                       .operands = std::move(operands),
                       .placeRootLocal = place.rootLocal,
                       .placeRootRef = place.rootRef,
                       .placeSteps = std::move(place.steps)},
           std::nullopt);
    }

    void FunctionLowerer::lowerReturn(const hir::Statement &statement)
    {
      std::vector<ValueId> values;
      if (statement.expression != nullptr)
        values.push_back(lowerExpression(*statement.expression));
      if (const auto drops = types_.returnDrops.find(&statement); drops != types_.returnDrops.end())
        lowerDropCalls(drops->second);
      block().terminator = Terminator{.kind = TerminatorKind::Return, .arguments = std::move(values)};
    }

    // ---- Structured control flow ---------------------------------------------

    void FunctionLowerer::lowerIf(const hir::Statement &statement)
    {
      const ValueId condition = lowerExpression(*statement.expression);
      const BlockId thenBlock = appendBlock();
      const BlockId joinBlock = appendBlock();
      const BlockId elseBlock = statement.alternative != nullptr ? appendBlock() : joinBlock;
      branch(condition, thenBlock, elseBlock);

      current_ = thenBlock;
      lowerBlock(*statement.consequence);
      jumpIfOpen(joinBlock);
      if (statement.alternative != nullptr)
      {
        current_ = elseBlock;
        lowerBlock(*statement.alternative);
        jumpIfOpen(joinBlock);
      }
      current_ = joinBlock;
    }

    void FunctionLowerer::lowerConstIf(const hir::Statement &statement)
    {
      const auto found = types_.constIfSelections.find(&statement);
      const bool consequence = found == types_.constIfSelections.end() || found->second;
      if (consequence)
        lowerBlock(*statement.consequence);
      else if (statement.alternative != nullptr)
        lowerBlock(*statement.alternative);
    }

    void FunctionLowerer::lowerLoop(const hir::Statement &statement)
    {
      std::vector<ValueId> initializers;
      initializers.reserve(statement.arguments.size());
      for (const auto &initializer : statement.arguments)
        initializers.push_back(lowerExpression(*initializer));

      const BlockId header = appendBlock();
      const BlockId body = appendBlock();
      const BlockId exit = appendBlock();
      jump(header, std::move(initializers));
      auto &headerBlock = function_.blocks[header.value];
      headerBlock.parameterCount = statement.loopBindings.size();
      headerBlock.parameterLocals = statement.loopBindings;
      headerBlock.terminator = Terminator{.kind = TerminatorKind::Jump, .targets = {body}};
      for (const auto local : statement.loopBindings)
        function_.localTypes.emplace(local.value, types_.localTypeIds.at(local.value));
      // `loop` bodies only repeat through `next`; falling off the end exits.
      lowerLoopBody(statement, header, body, exit, exit);
    }

    void FunctionLowerer::lowerWhile(const hir::Statement &statement)
    {
      const BlockId header = appendBlock();
      const BlockId body = appendBlock();
      const BlockId exit = appendBlock();
      jump(header);

      current_ = header;
      branch(lowerExpression(*statement.expression), body, exit);
      // `continue` and the body's end re-evaluate the condition (the header).
      lowerLoopBody(statement, header, body, exit, header);
    }

    void FunctionLowerer::lowerLoopBody(const hir::Statement &statement, BlockId header, BlockId body, BlockId exit,
                                        BlockId fallthrough)
    {
      loopHeaders_.emplace(statement.loop->value, header);
      loopExits_.emplace(statement.loop->value, exit);
      current_ = body;
      lowerBlock(*statement.body);
      jumpIfOpen(fallthrough);
      loopHeaders_.erase(statement.loop->value);
      loopExits_.erase(statement.loop->value);
      current_ = exit;
    }

    void FunctionLowerer::lowerLoopExit(const hir::Statement &statement,
                                        const std::unordered_map<uint32_t, BlockId> &targets)
    {
      if (currentHirBlock_ != nullptr)
        emitBlockDrops(*currentHirBlock_);
      jump(targets.at(statement.loop->value));
    }

    void FunctionLowerer::lowerNext(const hir::Statement &statement)
    {
      std::vector<ValueId> arguments;
      arguments.reserve(statement.arguments.size());
      for (const auto &argument : statement.arguments)
        arguments.push_back(lowerExpression(*argument));

      if (currentHirBlock_ != nullptr)
        emitBlockDrops(*currentHirBlock_);
      if (statement.nextTarget->kind == hir::NextTargetKind::Loop)
      {
        backedge(loopHeaders_.at(statement.nextTarget->id), std::move(arguments));
        return;
      }
      block().terminator = Terminator{.kind = TerminatorKind::TailRecur, .arguments = std::move(arguments)};
    }
  } // namespace detail

  auto Lowerer::lower(const hir::Function &function, const typecheck::TypeCheckResult &types) -> Function
  {
    return detail::FunctionLowerer{types}.lower(function);
  }

  namespace
  {
    void verifyInstruction(const Instruction &instruction)
    {
      const auto indexSteps = static_cast<size_t>(std::ranges::count_if(
          instruction.placeSteps, [](const auto &step) { return step.kind == PlaceStep::Kind::Index; }));
      const auto require = [](bool condition, const char *message)
      {
        if (!condition)
          throw VerificationError(message);
      };
      switch (instruction.kind)
      {
      case InstructionKind::ExtractTuple:
        require(instruction.operands.size() == 1, "FlowIR tuple extraction requires one source operand");
        return;
      case InstructionKind::LoadRef:
        require(instruction.operands.size() == 1, "FlowIR reference load requires one source operand");
        return;
      case InstructionKind::Slice:
        require(instruction.operands.size() == 2, "FlowIR slice requires receiver and range operands");
        return;
      case InstructionKind::ArrayLength:
        require(instruction.source.has_value(), "FlowIR array length requires a source operand");
        return;
      case InstructionKind::RangeStart:
        require(instruction.source.has_value(), "FlowIR range start requires a source operand");
        return;
      case InstructionKind::AppendArray:
        require(instruction.operands.size() == 2, "FlowIR array append requires array and element operands");
        return;
      case InstructionKind::EnumVariantIndex:
      case InstructionKind::ExtractEnumPayload:
        require(instruction.source.has_value(), "FlowIR enum operation requires a source operand");
        return;
      case InstructionKind::MakeRef:
        require(instruction.placeRootLocal.has_value() && !instruction.placeRootRef.has_value(),
                "FlowIR reference creation requires a local place root");
        require(instruction.operands.size() == indexSteps, "FlowIR reference creation index operand count mismatch");
        return;
      case InstructionKind::MakeSpan:
        require(instruction.placeRootLocal.has_value() && !instruction.placeRootRef.has_value(),
                "FlowIR span creation requires a local place root");
        require(instruction.operands.empty() || instruction.operands.size() == 2,
                "FlowIR span creation has invalid window operand count");
        return;
      case InstructionKind::AssignPlace:
        require(instruction.placeRootLocal.has_value() != instruction.placeRootRef.has_value(),
                "FlowIR place assignment requires exactly one place root");
        require(instruction.operands.size() == indexSteps + 1, "FlowIR place assignment operand count mismatch");
        return;
      default:
        return;
      }
    }

    void verifyTerminator(const Function &function, const Terminator &terminator)
    {
      const auto requireTargetCount = [&terminator](size_t expected, std::string_view name)
      {
        if (terminator.targets.size() != expected)
          throw VerificationError(std::string{name} + " has invalid target count");
      };
      switch (terminator.kind)
      {
      case TerminatorKind::Return:
        requireTargetCount(0, "return");
        if (terminator.arguments.size() > 1)
          throw VerificationError("return has too many values");
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
          throw VerificationError("branch requires exactly one condition value");
        break;
      }
      for (const auto target : terminator.targets)
        if (target.value >= function.blocks.size())
          throw VerificationError("FlowIR terminator target is out of range");
      if ((terminator.kind == TerminatorKind::Jump || terminator.kind == TerminatorKind::LoopBackedge) &&
          terminator.arguments.size() != function.blocks[terminator.targets[0].value].parameterCount)
        throw VerificationError("FlowIR terminator argument count does not match target block parameters");
    }
  } // namespace

  void Verifier::verify(const Function &function) const
  {
    if (function.blocks.empty())
      throw VerificationError("FlowIR function has no blocks");
    if (function.entry.value >= function.blocks.size())
      throw VerificationError("FlowIR entry block is out of range");

    for (size_t index = 0; index < function.blocks.size(); ++index)
    {
      const auto &block = function.blocks[index];
      if (block.id.value != index)
        throw VerificationError("FlowIR block id does not match block index");
      if (!block.terminator.has_value())
        throw VerificationError("FlowIR block has no terminator");
      for (const auto &instruction : block.instructions)
        verifyInstruction(instruction);
      verifyTerminator(function, *block.terminator);
    }
  }
} // namespace NG::flowir
