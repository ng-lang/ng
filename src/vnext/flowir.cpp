// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/flowir.hpp"

#include <unordered_map>
#include <utility>

namespace NG::vnext::flowir
{
  namespace
  {
    class FunctionLowerer final
    {
    public:
      [[nodiscard]] auto lower(const hir::Function &source) -> Function
      {
        function_ = Function{.source = source.id};
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
      [[nodiscard]] auto appendBlock() -> BlockId
      {
        const BlockId id{static_cast<uint32_t>(function_.blocks.size())};
        function_.blocks.push_back(Block{.id = id});
        return id;
      }

      [[nodiscard]] auto block() -> Block & { return function_.blocks[current_.value]; }

      [[nodiscard]] auto lowerExpression(const hir::Expression &expression) -> ValueId
      {
        for (const auto &operand : expression.operands)
        {
          static_cast<void>(lowerExpression(*operand));
        }
        const ValueId value{nextValue_++};
        block().instructions.push_back(
            Instruction{.kind = InstructionKind::Evaluate, .result = value, .expressionKind = expression.kind});
        return value;
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
          const ValueId binding{nextValue_++};
          block().instructions.push_back(Instruction{.kind = InstructionKind::BindLocal,
                                                     .result = binding,
                                                     .local = statement.local,
                                                     .expressionKind = statement.expression->kind});
          static_cast<void>(initializer);
          return;
        }
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
        case hir::StatementKind::Loop:
          lowerLoop(statement);
          return;
        case hir::StatementKind::Next:
          lowerNext(statement);
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

      Function function_;
      BlockId current_{};
      uint32_t nextValue_{};
      std::unordered_map<uint32_t, BlockId> loopHeaders_;
    };
  } // namespace

  auto Lowerer::lower(const hir::Function &function) -> Function
  {
    return FunctionLowerer{}.lower(function);
  }
} // namespace NG::vnext::flowir
