// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NG::vnext::syntax
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
    StringLiteral,
    ArrayLiteral,
    TupleLiteral,
    StructLiteral,
    BooleanLiteral,
    Prefix,
    Grouped,
    Call,
    GenericApplication,
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
      : Expression(ExpressionKind::StructLiteral, sourceSpan), typeName(std::move(name)), fields(std::move(initializers))
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
      : ConstExpr(ConstExprKind::Binary, sourceSpan), operatorText(std::move(op)), left(std::move(lhs)), right(std::move(rhs))
    {
    }
  };

  enum class StatementKind
  {
    Let,
    Assign,
    Return,
    If,
    ConstIf,
    Loop,
    Next,
    Switch,
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
    std::shared_ptr<TypeSyntax> annotation;
    ExpressionPtr initializer;
    std::vector<std::string> destructuredNames;

    LetStatement(std::string bindingName, bool mutableBinding, std::shared_ptr<TypeSyntax> typeAnnotation, ExpressionPtr value, SourceSpan sourceSpan,
                 std::vector<std::string> names = {})
      : Statement(StatementKind::Let, sourceSpan), name(std::move(bindingName)), isMutable(mutableBinding),
        annotation(std::move(typeAnnotation)), initializer(std::move(value)), destructuredNames(std::move(names))
    {
    }
  };

  struct AssignStatement final : Statement
  {
    ExpressionPtr target;
    ExpressionPtr value;

    AssignStatement(ExpressionPtr assignedTarget, ExpressionPtr assignedValue, SourceSpan sourceSpan)
      : Statement(StatementKind::Assign, sourceSpan), target(std::move(assignedTarget)), value(std::move(assignedValue))
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

  struct ConstIfStatement final : Statement
  {
    ExpressionPtr condition;
    Block consequence;
    std::unique_ptr<Block> alternative;

    ConstIfStatement(ExpressionPtr test, Block thenBlock, std::unique_ptr<Block> elseBlock, SourceSpan sourceSpan)
      : Statement(StatementKind::ConstIf, sourceSpan), condition(std::move(test)), consequence(std::move(thenBlock)),
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

  struct SwitchCasePattern final
  {
    const std::string variantName;
    std::optional<std::string> bindingName;
    const SourceSpan span;

    SwitchCasePattern(std::string variant, std::optional<std::string> binding, SourceSpan sourceSpan)
      : variantName(std::move(variant)), bindingName(std::move(binding)), span(sourceSpan)
    {
    }
  };

  struct SwitchCase final
  {
    SwitchCasePattern pattern;
    Block body;
  };

  struct SwitchStatement final : Statement
  {
    ExpressionPtr value;
    std::vector<SwitchCase> cases;
    std::unique_ptr<Block> otherwise;

    SwitchStatement(ExpressionPtr scrutinee, std::vector<SwitchCase> switchCases, std::unique_ptr<Block> fallback,
                    SourceSpan sourceSpan)
      : Statement(StatementKind::Switch, sourceSpan), value(std::move(scrutinee)), cases(std::move(switchCases)),
        otherwise(std::move(fallback))
    {
    }
  };

  enum class TypeSyntaxKind
  {
    Named,
    Applied,
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

    AppliedTypeSyntax(TypeSyntaxPtr typeConstructor, std::vector<GenericArgumentSyntax> typeArguments, SourceSpan sourceSpan)
      : TypeSyntax(TypeSyntaxKind::Applied, sourceSpan), constructor(std::move(typeConstructor)), arguments(std::move(typeArguments))
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

  enum class GenericParameterKind
  {
    Type,
    Const,
  };

  struct GenericParameter
  {
    GenericParameterKind kind;
    std::string name;
    TypeSyntaxPtr type;
    SourceSpan span;
  };

  struct StructFieldDeclaration final
  {
    const std::string name;
    TypeSyntaxPtr type;
    const SourceSpan span;

    StructFieldDeclaration(std::string fieldName, TypeSyntaxPtr fieldType, SourceSpan sourceSpan)
      : name(std::move(fieldName)), type(std::move(fieldType)), span(sourceSpan)
    {
    }
  };

  enum class ModuleItemKind
  {
    Function,
    Struct,
    Enum,
    Const,
  };

  struct ModuleItem
  {
    const ModuleItemKind kind;
    const SourceSpan span;

    explicit ModuleItem(ModuleItemKind itemKind, SourceSpan sourceSpan) : kind(itemKind), span(sourceSpan) {}
    virtual ~ModuleItem() = default;
  };

  using ModuleItemPtr = std::unique_ptr<ModuleItem>;

  struct StructDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<StructFieldDeclaration> fields;

    StructDeclaration(std::string structName, std::vector<StructFieldDeclaration> structFields, SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Struct, sourceSpan), name(std::move(structName)), fields(std::move(structFields))
    {
    }
  };

  struct EnumVariantDeclaration final
  {
    const std::string name;
    TypeSyntaxPtr payloadType;
    const SourceSpan span;

    EnumVariantDeclaration(std::string variantName, TypeSyntaxPtr type, SourceSpan sourceSpan)
      : name(std::move(variantName)), payloadType(std::move(type)), span(sourceSpan)
    {
    }
  };

  struct EnumDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<std::string> genericParameters;
    std::vector<EnumVariantDeclaration> variants;

    EnumDeclaration(std::string enumName, std::vector<std::string> parameters,
                    std::vector<EnumVariantDeclaration> enumVariants, SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Enum, sourceSpan), name(std::move(enumName)), genericParameters(std::move(parameters)),
        variants(std::move(enumVariants))
    {
    }
  };

  enum class ConstBodyKind
  {
    Expression,
    Native,
    Delete,
  };

  /// Module-level const predicate declaration (D-012): `const name<patterns>:
  /// type = body;` where the body is a const expression, `native`, or
  /// `delete`. Type parameters are declared by the optional prefix list
  /// (`const<T> name<...>`) or implicitly by bare identifiers in the pattern.
  struct ConstDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<GenericParameter> parameters;
    std::vector<TypeSyntaxPtr> patternArguments;
    TypeSyntaxPtr targetType;
    ConstBodyKind bodyKind;
    ConstExprPtr body;

    ConstDeclaration(std::string constName, std::vector<GenericParameter> genericParameters,
                     std::vector<TypeSyntaxPtr> patterns, TypeSyntaxPtr target, ConstBodyKind kind, ConstExprPtr expression,
                     SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Const, sourceSpan), name(std::move(constName)),
        parameters(std::move(genericParameters)), patternArguments(std::move(patterns)), targetType(std::move(target)),
        bodyKind(kind), body(std::move(expression))
    {
    }
  };

  struct FunctionDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<GenericParameter> genericParameters;
    std::vector<FunctionParameter> parameters;
    TypeSyntaxPtr returnType;
    Block body;

    FunctionDeclaration(std::string functionName, std::vector<GenericParameter> genericParameterList,
                        std::vector<FunctionParameter> functionParameters,
                        TypeSyntaxPtr functionReturnType, Block functionBody, SourceSpan sourceSpan)
      : ModuleItem(ModuleItemKind::Function, sourceSpan), name(std::move(functionName)),
        genericParameters(std::move(genericParameterList)), parameters(std::move(functionParameters)), returnType(std::move(functionReturnType)), body(std::move(functionBody))
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
