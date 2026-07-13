// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace NG::vnext::syntax
{
  struct SourceSpan
  {
    size_t begin{};
    size_t end{};
  };

  enum class ExpressionKind
  {
    Identifier,
    IntegerLiteral,
    Prefix,
    Grouped,
    Call,
    Index,
    Member,
    Binary,
  };

  struct Expression
  {
    const ExpressionKind kind;
    const SourceSpan span;

    explicit Expression(ExpressionKind expressionKind, SourceSpan sourceSpan) : kind(expressionKind), span(sourceSpan) {}
    virtual ~Expression() = default;
  };

  using ExpressionPtr = std::unique_ptr<Expression>;

  struct IdentifierExpression final : Expression
  {
    const std::string name;

    IdentifierExpression(std::string identifier, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Identifier, sourceSpan), name(std::move(identifier))
    {
    }
  };

  struct IntegerLiteralExpression final : Expression
  {
    const std::string text;

    IntegerLiteralExpression(std::string literalText, SourceSpan sourceSpan)
      : Expression(ExpressionKind::IntegerLiteral, sourceSpan), text(std::move(literalText))
    {
    }
  };

  struct PrefixExpression final : Expression
  {
    const std::string operatorText;
    ExpressionPtr operand;

    PrefixExpression(std::string op, ExpressionPtr value, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Prefix, sourceSpan), operatorText(std::move(op)), operand(std::move(value))
    {
    }
  };

  struct GroupedExpression final : Expression
  {
    ExpressionPtr expression;

    GroupedExpression(ExpressionPtr value, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Grouped, sourceSpan), expression(std::move(value))
    {
    }
  };

  struct CallExpression final : Expression
  {
    ExpressionPtr callee;
    std::vector<ExpressionPtr> arguments;

    CallExpression(ExpressionPtr target, std::vector<ExpressionPtr> callArguments, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Call, sourceSpan), callee(std::move(target)), arguments(std::move(callArguments))
    {
    }
  };

  struct IndexExpression final : Expression
  {
    ExpressionPtr receiver;
    ExpressionPtr index;

    IndexExpression(ExpressionPtr target, ExpressionPtr indexExpression, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Index, sourceSpan), receiver(std::move(target)), index(std::move(indexExpression))
    {
    }
  };

  struct MemberExpression final : Expression
  {
    ExpressionPtr receiver;
    const std::string member;

    MemberExpression(ExpressionPtr target, std::string memberName, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Member, sourceSpan), receiver(std::move(target)), member(std::move(memberName))
    {
    }
  };

  struct BinaryExpression final : Expression
  {
    const std::string operatorText;
    ExpressionPtr left;
    ExpressionPtr right;

    BinaryExpression(std::string op, ExpressionPtr lhs, ExpressionPtr rhs, SourceSpan sourceSpan)
      : Expression(ExpressionKind::Binary, sourceSpan), operatorText(std::move(op)), left(std::move(lhs)),
        right(std::move(rhs))
    {
    }
  };

  enum class StatementKind
  {
    Let,
    Return,
    If,
    Loop,
    Next,
    Expression,
  };

  struct Statement
  {
    const StatementKind kind;
    const SourceSpan span;

    explicit Statement(StatementKind statementKind, SourceSpan sourceSpan) : kind(statementKind), span(sourceSpan) {}
    virtual ~Statement() = default;
  };

  using StatementPtr = std::unique_ptr<Statement>;

  struct LetStatement final : Statement
  {
    const std::string name;
    const bool isMutable;
    ExpressionPtr initializer;

    LetStatement(std::string bindingName, bool mutableBinding, ExpressionPtr value, SourceSpan sourceSpan)
      : Statement(StatementKind::Let, sourceSpan), name(std::move(bindingName)), isMutable(mutableBinding),
        initializer(std::move(value))
    {
    }
  };

  struct ReturnStatement final : Statement
  {
    ExpressionPtr value;

    ReturnStatement(ExpressionPtr returnValue, SourceSpan sourceSpan)
      : Statement(StatementKind::Return, sourceSpan), value(std::move(returnValue))
    {
    }
  };

  struct ExpressionStatement final : Statement
  {
    ExpressionPtr expression;

    ExpressionStatement(ExpressionPtr value, SourceSpan sourceSpan)
      : Statement(StatementKind::Expression, sourceSpan), expression(std::move(value))
    {
    }
  };

  struct Block final
  {
    const SourceSpan span;
    std::vector<StatementPtr> statements;
    ExpressionPtr tailExpression;

    Block(SourceSpan sourceSpan, std::vector<StatementPtr> blockStatements, ExpressionPtr blockTailExpression)
      : span(sourceSpan), statements(std::move(blockStatements)), tailExpression(std::move(blockTailExpression))
    {
    }
  };

  struct IfStatement final : Statement
  {
    ExpressionPtr condition;
    Block consequence;
    std::unique_ptr<Block> alternative;

    IfStatement(ExpressionPtr test, Block thenBlock, std::unique_ptr<Block> elseBlock, SourceSpan sourceSpan)
      : Statement(StatementKind::If, sourceSpan), condition(std::move(test)), consequence(std::move(thenBlock)),
        alternative(std::move(elseBlock))
    {
    }
  };

  struct LoopBinding final
  {
    const std::string name;
    ExpressionPtr initializer;
    const SourceSpan span;

    LoopBinding(std::string bindingName, ExpressionPtr initialValue, SourceSpan sourceSpan)
      : name(std::move(bindingName)), initializer(std::move(initialValue)), span(sourceSpan)
    {
    }
  };

  struct LoopStatement final : Statement
  {
    std::vector<LoopBinding> bindings;
    Block body;

    LoopStatement(std::vector<LoopBinding> loopBindings, Block loopBody, SourceSpan sourceSpan)
      : Statement(StatementKind::Loop, sourceSpan), bindings(std::move(loopBindings)), body(std::move(loopBody))
    {
    }
  };

  struct NextStatement final : Statement
  {
    std::vector<ExpressionPtr> arguments;

    NextStatement(std::vector<ExpressionPtr> nextArguments, SourceSpan sourceSpan)
      : Statement(StatementKind::Next, sourceSpan), arguments(std::move(nextArguments))
    {
    }
  };

  enum class TypeSyntaxKind
  {
    Named,
    ScopedReference,
    RawPointer,
  };

  struct TypeSyntax
  {
    const TypeSyntaxKind kind;
    const SourceSpan span;

    explicit TypeSyntax(TypeSyntaxKind typeKind, SourceSpan sourceSpan) : kind(typeKind), span(sourceSpan) {}
    virtual ~TypeSyntax() = default;
  };

  using TypeSyntaxPtr = std::unique_ptr<TypeSyntax>;

  struct NamedTypeSyntax final : TypeSyntax
  {
    const std::string name;

    NamedTypeSyntax(std::string typeName, SourceSpan sourceSpan)
      : TypeSyntax(TypeSyntaxKind::Named, sourceSpan), name(std::move(typeName))
    {
    }
  };

  struct ScopedReferenceTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr target;
    const bool isMutable;

    ScopedReferenceTypeSyntax(TypeSyntaxPtr referencedType, bool mutableReference, SourceSpan sourceSpan)
      : TypeSyntax(TypeSyntaxKind::ScopedReference, sourceSpan), target(std::move(referencedType)),
        isMutable(mutableReference)
    {
    }
  };

  struct RawPointerTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr pointee;
    const bool isMutable;

    RawPointerTypeSyntax(TypeSyntaxPtr pointedToType, bool mutablePointee, SourceSpan sourceSpan)
      : TypeSyntax(TypeSyntaxKind::RawPointer, sourceSpan), pointee(std::move(pointedToType)),
        isMutable(mutablePointee)
    {
    }
  };

  struct FunctionParameter final
  {
    const std::string name;
    TypeSyntaxPtr type;
    const SourceSpan span;

    FunctionParameter(std::string parameterName, TypeSyntaxPtr parameterType, SourceSpan sourceSpan)
      : name(std::move(parameterName)), type(std::move(parameterType)), span(sourceSpan)
    {
    }
  };

  enum class ModuleItemKind
  {
    Function,
  };

  struct ModuleItem
  {
    const ModuleItemKind kind;
    const SourceSpan span;

    explicit ModuleItem(ModuleItemKind itemKind, SourceSpan sourceSpan) : kind(itemKind), span(sourceSpan) {}
    virtual ~ModuleItem() = default;
  };

  using ModuleItemPtr = std::unique_ptr<ModuleItem>;

  struct FunctionDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<FunctionParameter> parameters;
    TypeSyntaxPtr returnType;
    Block body;

    FunctionDeclaration(std::string functionName, std::vector<FunctionParameter> functionParameters,
                        TypeSyntaxPtr functionReturnType, Block functionBody, SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Function, sourceSpan), name(std::move(functionName)),
        parameters(std::move(functionParameters)), returnType(std::move(functionReturnType)), body(std::move(functionBody))
    {
    }
  };

  struct SourceUnit final
  {
    const SourceSpan span;
    std::vector<ModuleItemPtr> items;

    SourceUnit(SourceSpan sourceSpan, std::vector<ModuleItemPtr> moduleItems)
      : span(sourceSpan), items(std::move(moduleItems))
    {
    }
  };
} // namespace NG::vnext::syntax
