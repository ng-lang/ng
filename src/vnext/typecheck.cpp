// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/typecheck.hpp"

#include <format>
#include <unordered_map>
#include <unordered_set>

namespace NG::vnext::typecheck
{
  namespace
  {
    struct Signature
    {
      std::vector<std::string> parameters;
      std::string returnType;
    };

    class Checker final
    {
    public:
      void check(const hir::Module &module)
      {
        for (const auto &function : module.functions)
        {
          Signature signature;
          for (const auto &parameter : function.parameters)
          {
            requireKnownType(parameter.typeName, parameter.span);
            signature.parameters.push_back(parameter.typeName);
          }
          signature.returnType = function.returnTypeName.value_or("unit");
          requireKnownType(signature.returnType, function.span);
          signatures_.emplace(function.id.value, std::move(signature));
        }
        for (const auto &function : module.functions)
        {
          checkFunction(function);
        }
      }

    private:
      using LocalTypes = std::unordered_map<uint32_t, std::string>;
      using LoopTypes = std::unordered_map<uint32_t, std::vector<std::string>>;

      void checkFunction(const hir::Function &function)
      {
        LocalTypes locals;
        for (const auto &parameter : function.parameters)
        {
          locals.emplace(parameter.local.value, parameter.typeName);
        }
        checkBlock(function.body, locals, {}, function.returnTypeName.value_or("unit"));
      }

      void checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, const std::string &returnType)
      {
        for (const auto &statement : block.statements)
        {
          checkStatement(statement, locals, loops, returnType);
        }
        if (block.tailExpression != nullptr)
        {
          static_cast<void>(infer(*block.tailExpression, locals));
        }
      }

      void checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops,
                          const std::string &returnType)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
          locals.emplace(statement.local->value, infer(*statement.expression, locals));
          return;
        case hir::StatementKind::Assign:
          requireType(locals.at(statement.local->value), infer(*statement.expression, locals), statement.expression->span,
                      "assignment value");
          return;
        case hir::StatementKind::Return:
          if (statement.expression != nullptr)
          {
            requireType(returnType, infer(*statement.expression, locals), statement.expression->span, "return value");
          }
          else
          {
            requireType(returnType, "unit", statement.span, "return value");
          }
          return;
        case hir::StatementKind::If:
          requireType("bool", infer(*statement.expression, locals), statement.expression->span, "if condition");
          checkBlock(*statement.consequence, locals, loops, returnType);
          if (statement.alternative != nullptr)
          {
            checkBlock(*statement.alternative, locals, loops, returnType);
          }
          return;
        case hir::StatementKind::Loop:
        {
          std::vector<std::string> types;
          types.reserve(statement.arguments.size());
          for (const auto &initializer : statement.arguments)
          {
            types.push_back(infer(*initializer, locals));
          }
          LocalTypes loopLocals = locals;
          for (size_t index = 0; index < statement.loopBindings.size(); ++index)
          {
            loopLocals.emplace(statement.loopBindings[index].value, types[index]);
          }
          LoopTypes loopTypes = loops;
          loopTypes.emplace(statement.loop->value, std::move(types));
          checkBlock(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType);
          return;
        }
        case hir::StatementKind::Next:
          checkNext(statement, locals, loops);
          return;
        case hir::StatementKind::Expression:
          static_cast<void>(infer(*statement.expression, locals));
          return;
        }
      }

      void checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops)
      {
        const std::vector<std::string> *expected = nullptr;
        if (statement.nextTarget->kind == hir::NextTargetKind::Loop)
        {
          expected = &loops.at(statement.nextTarget->id);
        }
        else
        {
          expected = &signatures_.at(statement.nextTarget->id).parameters;
        }

        if (statement.arguments.size() != expected->size())
        {
          throw TypeError(std::format("next argument count mismatch: expected {}, got {}", expected->size(),
                                      statement.arguments.size()),
                          statement.span);
        }
        for (size_t index = 0; index < expected->size(); ++index)
        {
          requireType((*expected)[index], infer(*statement.arguments[index], locals), statement.arguments[index]->span,
                      std::format("next argument {}", index + 1));
        }
      }

      [[nodiscard]] auto infer(const hir::Expression &expression, const LocalTypes &locals) const -> std::string
      {
        switch (expression.kind)
        {
        case hir::ExpressionKind::IntegerLiteral: return "i64";
        case hir::ExpressionKind::BooleanLiteral: return "bool";
        case hir::ExpressionKind::ResolvedName:
          if (expression.resolvedName->kind == hir::ResolvedNameKind::Local)
          {
            return locals.at(expression.resolvedName->id);
          }
          throw TypeError("function name cannot be used as a value", expression.span);
        case hir::ExpressionKind::Grouped: return infer(*expression.operands[0], locals);
        case hir::ExpressionKind::Prefix:
          if (expression.text == "!")
          {
            requireType("bool", infer(*expression.operands[0], locals), expression.span, "prefix operand");
            return "bool";
          }
          requireType("i64", infer(*expression.operands[0], locals), expression.span, "prefix operand");
          return "i64";
        case hir::ExpressionKind::Binary:
          requireType(infer(*expression.operands[0], locals), infer(*expression.operands[1], locals), expression.span,
                      "binary operands");
          if (expression.text == "==" || expression.text == "!=" || expression.text == "<" || expression.text == "<=" ||
              expression.text == ">" || expression.text == ">=")
          {
            return "bool";
          }
          if (expression.text == "&&" || expression.text == "||")
          {
            requireType("bool", infer(*expression.operands[0], locals), expression.span, "logical operand");
            return "bool";
          }
          if (expression.text == "&" || expression.text == "|" || expression.text == "^" || expression.text == "<<" ||
              expression.text == ">>")
          {
            requireType("i64", infer(*expression.operands[0], locals), expression.span, "bitwise operand");
            return "i64";
          }
          return infer(*expression.operands[0], locals);
        case hir::ExpressionKind::Call:
          if (expression.operands[0]->resolvedName.has_value() &&
              expression.operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function)
          {
            const auto &signature = signatures_.at(expression.operands[0]->resolvedName->id);
            const size_t suppliedCount = expression.operands.size() - 1;
            if (suppliedCount != signature.parameters.size())
            {
              throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(),
                                          suppliedCount),
                              expression.span);
            }
            for (size_t index = 0; index < suppliedCount; ++index)
            {
              requireType(signature.parameters[index], infer(*expression.operands[index + 1], locals),
                          expression.operands[index + 1]->span, std::format("call argument {}", index + 1));
            }
            return signature.returnType;
          }
          throw TypeError("call target is not a function", expression.operands[0]->span);
        case hir::ExpressionKind::Index: throw TypeError("index expressions are not yet supported", expression.span);
        case hir::ExpressionKind::Member: throw TypeError("member expressions are not yet supported", expression.span);
        }
        return "unknown";
      }

      static void requireKnownType(const std::string &type, syntax::SourceSpan span)
      {
        static const std::unordered_set<std::string> supportedTypes{"i64", "u8", "bool", "unit"};
        if (!supportedTypes.contains(type))
        {
          throw TypeError(std::format("unknown type `{}`", type), span);
        }
      }

      static void requireType(const std::string &expected, const std::string &actual, syntax::SourceSpan span,
                              std::string_view context)
      {
        if (expected != actual)
        {
          throw TypeError(std::format("{} type mismatch: expected {}, got {}", context, expected, actual), span);
        }
      }

      std::unordered_map<uint32_t, Signature> signatures_;
    };
  } // namespace

  void TypeChecker::check(const hir::Module &module)
  {
    Checker{}.check(module);
  }
} // namespace NG::vnext::typecheck
