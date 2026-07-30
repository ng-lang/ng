// AI-generated code; reviewed for this repository's vNext rewrite.
#include "vnext/typecheck.hpp"

#include <charconv>
#include <format>
#include <unordered_map>

namespace NG::vnext::typecheck
{
  namespace
  {
    class Checker final
    {
    public:
      [[nodiscard]] auto check(const hir::Module &module) -> TypeCheckResult
      {
        for (const auto &function : module.functions)
        {
          FunctionTypeIds signature;
          FunctionType displaySignature;
          for (const auto &parameter : function.parameters)
          {
            const TypeId type = interner_.resolve(parameter.type);
            signature.parameters.push_back(type);
            displaySignature.parameters.push_back(interner_.display(type));
          }
          signature.returnType = function.returnType != nullptr ? interner_.resolve(*function.returnType) : builtin::Unit;
          displaySignature.returnType = interner_.display(signature.returnType);
          signatures_.emplace(function.id.value, signature);
          functionTypeIds_.emplace(function.id.value, signature);
          functionTypes_.emplace(function.id.value, std::move(displaySignature));
        }
        for (const auto &function : module.functions) checkFunction(function);
        return TypeCheckResult{.expressionTypes = std::move(expressionTypes_),
                               .expressionTypeIds = std::move(expressionTypeIds_),
                               .localTypes = std::move(localDisplayTypes_),
                               .localTypeIds = std::move(localTypeIds_),
                               .functionTypes = std::move(functionTypes_),
                               .functionTypeIds = std::move(functionTypeIds_),
                               .typeDescriptors = interner_.descriptors()};
      }

    private:
      using LocalTypes = std::unordered_map<uint32_t, TypeId>;
      using LoopTypes = std::unordered_map<uint32_t, std::vector<TypeId>>;

      void record(const hir::Expression &expression, TypeId type)
      {
        expressionTypeIds_.insert_or_assign(&expression, type);
        expressionTypes_.insert_or_assign(&expression, interner_.display(type));
      }

      void recordLocal(hir::LocalId local, TypeId type)
      {
        localTypeIds_.insert_or_assign(local.value, type);
        localDisplayTypes_.insert_or_assign(local.value, interner_.display(type));
      }

      void checkFunction(const hir::Function &function)
      {
        LocalTypes locals;
        const auto &signature = signatures_.at(function.id.value);
        for (size_t index = 0; index < function.parameters.size(); ++index)
        {
          locals.emplace(function.parameters[index].local.value, signature.parameters[index]);
          recordLocal(function.parameters[index].local, signature.parameters[index]);
        }
        checkBlock(function.body, locals, {}, signature.returnType);
      }

      void checkBlock(const hir::Block &block, LocalTypes locals, LoopTypes loops, TypeId returnType)
      {
        for (const auto &statement : block.statements) checkStatement(statement, locals, loops, returnType);
        if (block.tailExpression != nullptr) static_cast<void>(infer(*block.tailExpression, locals));
      }

      void checkStatement(const hir::Statement &statement, LocalTypes &locals, LoopTypes &loops, TypeId returnType)
      {
        switch (statement.kind)
        {
        case hir::StatementKind::Let:
        {
          const TypeId type = infer(*statement.expression, locals);
          locals.emplace(statement.local->value, type);
          recordLocal(*statement.local, type);
          return;
        }
        case hir::StatementKind::Assign:
          if (statement.assignmentTarget != nullptr)
          {
            const TypeId target = infer(*statement.assignmentTarget, locals);
            static_cast<void>(inferExpected(*statement.expression, target, locals, "assignment value"));
          }
          else
          {
            static_cast<void>(inferExpected(*statement.expression, locals.at(statement.local->value), locals, "assignment value"));
          }
          return;
        case hir::StatementKind::Return:
          if (statement.expression != nullptr) static_cast<void>(inferExpected(*statement.expression, returnType, locals, "return value"));
          else requireType(returnType, builtin::Unit, statement.span, "return value");
          return;
        case hir::StatementKind::If:
          requireType(builtin::Bool, infer(*statement.expression, locals), statement.expression->span, "if condition");
          checkBlock(*statement.consequence, locals, loops, returnType);
          if (statement.alternative != nullptr) checkBlock(*statement.alternative, locals, loops, returnType);
          return;
        case hir::StatementKind::Loop:
        {
          std::vector<TypeId> types;
          for (const auto &initializer : statement.arguments) types.push_back(infer(*initializer, locals));
          LocalTypes loopLocals = locals;
          for (size_t index = 0; index < statement.loopBindings.size(); ++index)
          {
            loopLocals.emplace(statement.loopBindings[index].value, types[index]);
            recordLocal(statement.loopBindings[index], types[index]);
          }
          LoopTypes loopTypes = loops;
          loopTypes.emplace(statement.loop->value, types);
          checkBlock(*statement.body, std::move(loopLocals), std::move(loopTypes), returnType);
          return;
        }
        case hir::StatementKind::Next: checkNext(statement, locals, loops); return;
        case hir::StatementKind::Expression: static_cast<void>(infer(*statement.expression, locals)); return;
        }
      }

      void checkNext(const hir::Statement &statement, const LocalTypes &locals, const LoopTypes &loops)
      {
        const std::vector<TypeId> *expected = statement.nextTarget->kind == hir::NextTargetKind::Loop
                                                ? &loops.at(statement.nextTarget->id)
                                                : &signatures_.at(statement.nextTarget->id).parameters;
        if (statement.arguments.size() != expected->size())
          throw TypeError(std::format("next argument count mismatch: expected {}, got {}", expected->size(), statement.arguments.size()),
                          statement.span);
        for (size_t index = 0; index < expected->size(); ++index)
          static_cast<void>(inferExpected(*statement.arguments[index], (*expected)[index], locals,
                                          std::format("next argument {}", index + 1)));
      }

      [[nodiscard]] auto inferExpected(const hir::Expression &expression, TypeId expected, const LocalTypes &locals,
                                       std::string_view context) -> TypeId
      {
        const auto &descriptor = interner_.descriptor(expected);
        if (expression.kind == hir::ExpressionKind::ArrayLiteral &&
            (descriptor.kind == TypeKind::DynamicArray || descriptor.kind == TypeKind::FixedArray))
        {
          if (descriptor.kind == TypeKind::FixedArray && expression.operands.size() != *descriptor.length)
            throw TypeError(std::format("fixed array length mismatch: expected {}, got {}", *descriptor.length,
                                        expression.operands.size()), expression.span);
          for (const auto &element : expression.operands)
            static_cast<void>(inferExpected(*element, descriptor.element, locals, "array element"));
          record(expression, expected);
          return expected;
        }
        if (expression.kind == hir::ExpressionKind::TupleLiteral && descriptor.kind == TypeKind::Tuple)
        {
          if (expression.operands.size() != descriptor.elements.size())
            throw TypeError(std::format("tuple length mismatch: expected {}, got {}", descriptor.elements.size(),
                                        expression.operands.size()), expression.span);
          for (size_t index = 0; index < expression.operands.size(); ++index)
            static_cast<void>(inferExpected(*expression.operands[index], descriptor.elements[index], locals,
                                            std::format("tuple element {}", index)));
          record(expression, expected);
          return expected;
        }
        const TypeId actual = infer(expression, locals);
        requireType(expected, actual, expression.span, context);
        return actual;
      }

      [[nodiscard]] auto infer(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
      {
        TypeId type;
        switch (expression.kind)
        {
        case hir::ExpressionKind::IntegerLiteral: type = builtin::I64; break;
        case hir::ExpressionKind::StringLiteral: type = builtin::String; break;
        case hir::ExpressionKind::BooleanLiteral: type = builtin::Bool; break;
        case hir::ExpressionKind::ArrayLiteral:
        {
          if (expression.operands.empty()) throw TypeError("cannot infer the type of an empty array literal", expression.span);
          const TypeId element = infer(*expression.operands.front(), locals);
          for (size_t index = 1; index < expression.operands.size(); ++index)
            requireType(element, infer(*expression.operands[index], locals), expression.operands[index]->span, "array element");
          type = interner_.internDynamicArray(element);
          break;
        }
        case hir::ExpressionKind::TupleLiteral:
        {
          std::vector<TypeId> elements;
          elements.reserve(expression.operands.size());
          for (const auto &element : expression.operands) elements.push_back(infer(*element, locals));
          type = interner_.internTuple(elements);
          break;
        }
        case hir::ExpressionKind::ResolvedName:
          if (expression.resolvedName->kind == hir::ResolvedNameKind::Function)
            throw TypeError("function name cannot be used as a value", expression.span);
          type = locals.at(expression.resolvedName->id);
          break;
        case hir::ExpressionKind::Grouped: type = infer(*expression.operands[0], locals); break;
        case hir::ExpressionKind::Prefix:
          type = expression.text == "!" ? builtin::Bool : builtin::I64;
          requireType(type, infer(*expression.operands[0], locals), expression.span, "prefix operand");
          break;
        case hir::ExpressionKind::Binary:
        {
          const TypeId left = infer(*expression.operands[0], locals);
          const TypeId right = infer(*expression.operands[1], locals);
          requireType(left, right, expression.span, "binary operands");
          if (expression.text == "==" || expression.text == "!=") type = builtin::Bool;
          else if (expression.text == "<" || expression.text == "<=" || expression.text == ">" || expression.text == ">=")
          {
            requireType(builtin::I64, left, expression.span, "comparison operand");
            type = builtin::Bool;
          }
          else if (expression.text == "&&" || expression.text == "||")
          {
            requireType(builtin::Bool, left, expression.span, "logical operand");
            type = builtin::Bool;
          }
          else
          {
            if (expression.text != "+" || left != builtin::String) requireType(builtin::I64, left, expression.span, "binary operand");
            type = left;
          }
          break;
        }
        case hir::ExpressionKind::Call:
        {
          if (!expression.operands[0]->resolvedName.has_value() ||
              expression.operands[0]->resolvedName->kind != hir::ResolvedNameKind::Function)
            throw TypeError("call target is not a function", expression.operands[0]->span);
          const auto &signature = signatures_.at(expression.operands[0]->resolvedName->id);
          const size_t supplied = expression.operands.size() - 1;
          if (supplied != signature.parameters.size())
            throw TypeError(std::format("call argument count mismatch: expected {}, got {}", signature.parameters.size(), supplied),
                            expression.span);
          for (size_t index = 0; index < supplied; ++index)
            static_cast<void>(inferExpected(*expression.operands[index + 1], signature.parameters[index], locals,
                                            std::format("call argument {}", index + 1)));
          type = signature.returnType;
          break;
        }
        case hir::ExpressionKind::Index:
        {
          const TypeId receiver = infer(*expression.operands[0], locals);
          requireType(builtin::I64, infer(*expression.operands[1], locals), expression.operands[1]->span, "array index");
          const auto &descriptor = interner_.descriptor(receiver);
          if (descriptor.kind == TypeKind::Tuple)
          {
            if (expression.text.empty()) throw TypeError("tuple index must be an integer literal", expression.operands[1]->span);
            uint64_t index{};
            const auto [end, error] = std::from_chars(expression.text.data(), expression.text.data() + expression.text.size(), index);
            if (error != std::errc{} || end != expression.text.data() + expression.text.size() || index >= descriptor.elements.size())
              throw TypeError(std::format("tuple index {} is out of range for length {}", expression.text,
                                          descriptor.elements.size()), expression.operands[1]->span);
            type = descriptor.elements[index];
          }
          else
          {
            if (descriptor.kind != TypeKind::DynamicArray && descriptor.kind != TypeKind::FixedArray)
              throw TypeError(std::format("cannot index value of type {}", interner_.display(receiver)), expression.span);
            type = descriptor.element;
          }
          break;
        }
        case hir::ExpressionKind::Member: throw TypeError("member expressions are not yet supported", expression.span);
        }
        record(expression, type);
        return type;
      }

      void requireType(TypeId expected, TypeId actual, syntax::SourceSpan span, std::string_view context) const
      {
        if (expected != actual)
          throw TypeError(std::format("{} type mismatch: expected {}, got {}", context, interner_.display(expected),
                                      interner_.display(actual)), span);
      }

      TypeInterner interner_;
      std::unordered_map<uint32_t, FunctionTypeIds> signatures_;
      std::unordered_map<const hir::Expression *, std::string> expressionTypes_;
      std::unordered_map<const hir::Expression *, TypeId> expressionTypeIds_;
      std::unordered_map<uint32_t, std::string> localDisplayTypes_;
      std::unordered_map<uint32_t, TypeId> localTypeIds_;
      std::unordered_map<uint32_t, FunctionType> functionTypes_;
      std::unordered_map<uint32_t, FunctionTypeIds> functionTypeIds_;
    };
  } // namespace

  auto TypeChecker::check(const hir::Module &module) -> TypeCheckResult { return Checker{}.check(module); }
} // namespace NG::vnext::typecheck
