// AI-generated code; reviewed for this repository's vNext rewrite.
// Syntax AST, part 1: source spans, expression nodes, const expressions, and
// type syntax. Statements, blocks, closures, and module items build on these
// in syntax/ast.hpp (include that header).
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace NG::syntax
{
  struct SourceSpan
  {
    size_t begin{};
    size_t end{};
  };

  struct TypeSyntax;
  using TypeSyntaxPtr = std::unique_ptr<TypeSyntax>;

  enum class ExpressionKind
  {
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    ArrayLiteral,
    TupleLiteral,
    StructLiteral,
    BooleanLiteral,
    /// D-004: `null` — the zero bit pattern for a nullable handle.
    NullLiteral,
    Prefix,
    Try,
    Grouped,
    Call,
    GenericApplication,
    TypeTest,
    TraitBound,
    Index,
    Member,
    Binary,
    Closure,
  };

  struct Expression
  {
    const ExpressionKind kind;
    const SourceSpan span;

    explicit Expression(ExpressionKind expressionKind, SourceSpan sourceSpan) : kind(expressionKind), span(sourceSpan)
    {
    }
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

  struct FloatLiteralExpression final : Expression
  {
    const std::string text;

    FloatLiteralExpression(std::string literalText, SourceSpan sourceSpan)
        : Expression(ExpressionKind::FloatLiteral, sourceSpan), text(std::move(literalText))
    {
    }
  };

  struct StringLiteralExpression final : Expression
  {
    const std::string value;

    StringLiteralExpression(std::string literalValue, SourceSpan sourceSpan)
        : Expression(ExpressionKind::StringLiteral, sourceSpan), value(std::move(literalValue))
    {
    }
  };

  struct ArrayLiteralExpression final : Expression
  {
    std::vector<ExpressionPtr> elements;

    ArrayLiteralExpression(std::vector<ExpressionPtr> literalElements, SourceSpan sourceSpan)
        : Expression(ExpressionKind::ArrayLiteral, sourceSpan), elements(std::move(literalElements))
    {
    }
  };

  struct TupleLiteralExpression final : Expression
  {
    std::vector<ExpressionPtr> elements;

    TupleLiteralExpression(std::vector<ExpressionPtr> literalElements, SourceSpan sourceSpan)
        : Expression(ExpressionKind::TupleLiteral, sourceSpan), elements(std::move(literalElements))
    {
    }
  };

  struct StructFieldInitializer final
  {
    std::string name;
    ExpressionPtr value;
    SourceSpan span;
  };

  struct StructLiteralExpression final : Expression
  {
    const std::string typeName;
    std::vector<StructFieldInitializer> fields;

    StructLiteralExpression(std::string name, std::vector<StructFieldInitializer> initializers, SourceSpan sourceSpan)
        : Expression(ExpressionKind::StructLiteral, sourceSpan), typeName(std::move(name)),
          fields(std::move(initializers))
    {
    }
  };

  struct BooleanLiteralExpression final : Expression
  {
    const bool value;

    BooleanLiteralExpression(bool literalValue, SourceSpan sourceSpan)
        : Expression(ExpressionKind::BooleanLiteral, sourceSpan), value(literalValue)
    {
    }
  };

  /// D-004: the null handle literal; adopts an expected `T?` type.
  struct NullLiteralExpression final : Expression
  {
    explicit NullLiteralExpression(SourceSpan sourceSpan) : Expression(ExpressionKind::NullLiteral, sourceSpan) {}
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

  struct TryExpression final : Expression
  {
    ExpressionPtr operand;

    TryExpression(ExpressionPtr value, SourceSpan sourceSpan)
        : Expression(ExpressionKind::Try, sourceSpan), operand(std::move(value))
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

  enum class ConstExprKind
  {
    IntegerLiteral,
    BooleanLiteral,
    Identifier,
    Unary,
    Binary,
  };

  struct ConstExpr
  {
    const ConstExprKind kind;
    const SourceSpan span;

    explicit ConstExpr(ConstExprKind exprKind, SourceSpan sourceSpan) : kind(exprKind), span(sourceSpan) {}
    virtual ~ConstExpr() = default;
  };

  using ConstExprPtr = std::unique_ptr<ConstExpr>;

  struct ConstIntegerLiteral final : ConstExpr
  {
    const std::string text;

    ConstIntegerLiteral(std::string literalText, SourceSpan sourceSpan)
        : ConstExpr(ConstExprKind::IntegerLiteral, sourceSpan), text(std::move(literalText))
    {
    }
  };

  struct ConstBoolLiteral final : ConstExpr
  {
    const bool value;

    ConstBoolLiteral(bool literalValue, SourceSpan sourceSpan)
        : ConstExpr(ConstExprKind::BooleanLiteral, sourceSpan), value(literalValue)
    {
    }
  };

  struct ConstIdentifier final : ConstExpr
  {
    const std::string name;

    ConstIdentifier(std::string identifier, SourceSpan sourceSpan)
        : ConstExpr(ConstExprKind::Identifier, sourceSpan), name(std::move(identifier))
    {
    }
  };

  struct ConstUnaryExpr final : ConstExpr
  {
    const std::string operatorText;
    ConstExprPtr operand;

    ConstUnaryExpr(std::string op, ConstExprPtr value, SourceSpan sourceSpan)
        : ConstExpr(ConstExprKind::Unary, sourceSpan), operatorText(std::move(op)), operand(std::move(value))
    {
    }
  };

  struct ConstBinaryExpr final : ConstExpr
  {
    const std::string operatorText;
    ConstExprPtr left;
    ConstExprPtr right;

    ConstBinaryExpr(std::string op, ConstExprPtr lhs, ConstExprPtr rhs, SourceSpan sourceSpan)
        : ConstExpr(ConstExprKind::Binary, sourceSpan), operatorText(std::move(op)), left(std::move(lhs)),
          right(std::move(rhs))
    {
    }
  };

  enum class TypeSyntaxKind
  {
    Named,
    Applied,
    ScopedReference,
    Span,
    RawPointer,
    /// D-004: `T?` nullable foreign handle.
    Nullable,
    Pack,
    Union,
    Closure,
  };

  struct TypeSyntax
  {
    const TypeSyntaxKind kind;
    const SourceSpan span;

    explicit TypeSyntax(TypeSyntaxKind typeKind, SourceSpan sourceSpan) : kind(typeKind), span(sourceSpan) {}
    virtual ~TypeSyntax() = default;
  };

  /// Postfix pack type: `T...`.
  struct PackTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr target;

    PackTypeSyntax(TypeSyntaxPtr packedType, SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Pack, sourceSpan), target(std::move(packedType))
    {
    }
  };

  struct NamedTypeSyntax final : TypeSyntax
  {
    const std::string name;

    NamedTypeSyntax(std::string typeName, SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Named, sourceSpan), name(std::move(typeName))
    {
    }
  };

  enum class GenericArgumentKind
  {
    Type,
    ConstExpr,
  };

  struct GenericArgumentSyntax
  {
    GenericArgumentKind kind;
    TypeSyntaxPtr type;
    ConstExprPtr constExpr;
    SourceSpan span;
  };

  struct AppliedTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr constructor;
    std::vector<GenericArgumentSyntax> arguments;

    AppliedTypeSyntax(TypeSyntaxPtr typeConstructor, std::vector<GenericArgumentSyntax> typeArguments,
                      SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Applied, sourceSpan), constructor(std::move(typeConstructor)),
          arguments(std::move(typeArguments))
    {
    }
  };

  /// A name applied to explicit generic arguments in expression position,
  /// e.g. the const predicate reference `is_ref<i64>`. The `<` must be
  /// adjacent to the callee (no whitespace), so `a < b` stays a comparison.
  struct GenericApplicationExpression final : Expression
  {
    const std::string name;
    std::vector<GenericArgumentSyntax> arguments;

    GenericApplicationExpression(std::string appliedName, std::vector<GenericArgumentSyntax> genericArguments,
                                 SourceSpan sourceSpan)
        : Expression(ExpressionKind::GenericApplication, sourceSpan), name(std::move(appliedName)),
          arguments(std::move(genericArguments))
    {
    }
  };

  /// Trait bound in a where clause: `T: Trait` (possibly `T: A + B`).
  struct TraitBoundExpression final : Expression
  {
    const std::string name;
    std::vector<std::string> traitNames;

    TraitBoundExpression(std::string parameterName, std::vector<std::string> traits, SourceSpan sourceSpan)
        : Expression(ExpressionKind::TraitBound, sourceSpan), name(std::move(parameterName)),
          traitNames(std::move(traits))
    {
    }
  };

  /// Direct type constraint in a where clause: `T is Type`.
  struct TypeTestExpression final : Expression
  {
    const std::string name;
    TypeSyntaxPtr testedType;

    TypeTestExpression(std::string parameterName, TypeSyntaxPtr type, SourceSpan sourceSpan)
        : Expression(ExpressionKind::TypeTest, sourceSpan), name(std::move(parameterName)), testedType(std::move(type))
    {
    }
  };
  /// `A | B | C` union of member types (legacy 19).
  struct UnionTypeSyntax final : TypeSyntax
  {
    std::vector<TypeSyntaxPtr> members;

    UnionTypeSyntax(std::vector<TypeSyntaxPtr> unionMembers, const SourceSpan &sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Union, sourceSpan), members(std::move(unionMembers))
    {
    }
  };

  /// Function type literal: `|T, U| -> R` (params then required result).
  /// The parameters may be omitted (`|| -> unit`) but the result type is
  /// mandatory, keeping `|T|` unambiguous in union position.
  struct ClosureTypeSyntax final : TypeSyntax
  {
    std::vector<TypeSyntaxPtr> parameters;
    TypeSyntaxPtr result;

    ClosureTypeSyntax(std::vector<TypeSyntaxPtr> closureParameters, TypeSyntaxPtr returnType,
                      const SourceSpan &sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Closure, sourceSpan), parameters(std::move(closureParameters)),
          result(std::move(returnType))
    {
    }
  };

  struct ScopedReferenceTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr target;
    const bool isMutable;
    /// D-002 A1.1 origin contracts: the declared source names behind
    /// `T ref(a | b)` / `T ref mut(a)`. Empty for plain `T ref` / `ref<T>`.
    std::vector<std::string> origins;

    ScopedReferenceTypeSyntax(TypeSyntaxPtr referencedType, bool mutableReference, SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::ScopedReference, sourceSpan), target(std::move(referencedType)),
          isMutable(mutableReference)
    {
    }
  };

  struct SpanTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr target;

    explicit SpanTypeSyntax(TypeSyntaxPtr viewedTarget, SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Span, sourceSpan), target(std::move(viewedTarget))
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

  /// D-004: `T?` — a nullable handle. The value occupies the same 64-bit slot
  /// as `T`; the zero bit pattern denotes null.
  struct NullableTypeSyntax final : TypeSyntax
  {
    TypeSyntaxPtr target;

    NullableTypeSyntax(TypeSyntaxPtr pointedToType, SourceSpan sourceSpan)
        : TypeSyntax(TypeSyntaxKind::Nullable, sourceSpan), target(std::move(pointedToType))
    {
    }
  };
} // namespace NG::syntax
