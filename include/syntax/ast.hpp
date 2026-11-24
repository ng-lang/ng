// AI-generated code; reviewed for this repository's vNext rewrite.
// Syntax AST, part 2: statements, blocks, switch patterns, closures, and
// module items. Expression, const-expression, and type nodes live in
// syntax/ast_expr.hpp.
#pragma once

#include "syntax/ast_expr.hpp"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <unordered_set>

namespace NG::syntax
{
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
    While,
    Continue,
    Break,
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
    /// `let (first, ...rest) = tuple;`: the rest binding (a tuple of the
    /// remaining elements).
    std::optional<std::string> restName;

    LetStatement(std::string bindingName, bool mutableBinding, std::shared_ptr<TypeSyntax> typeAnnotation,
                 ExpressionPtr value, SourceSpan sourceSpan, std::vector<std::string> names = {},
                 std::optional<std::string> restBinding = std::nullopt)
        : Statement(StatementKind::Let, sourceSpan), name(std::move(bindingName)), isMutable(mutableBinding),
          annotation(std::move(typeAnnotation)), initializer(std::move(value)), destructuredNames(std::move(names)),
          restName(std::move(restBinding))
    {
    }
  };

  struct AssignStatement final : Statement
  {
    ExpressionPtr target;
    ExpressionPtr value;

    AssignStatement(ExpressionPtr assignedTarget, ExpressionPtr assignedValue, SourceSpan sourceSpan)
        : Statement(StatementKind::Assign, sourceSpan), target(std::move(assignedTarget)),
          value(std::move(assignedValue))
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

  struct WhileStatement final : Statement
  {
    ExpressionPtr condition;
    Block body;

    WhileStatement(ExpressionPtr cond, Block loopBody, SourceSpan sourceSpan)
        : Statement(StatementKind::While, sourceSpan), condition(std::move(cond)), body(std::move(loopBody))
    {
    }
  };

  struct ContinueStatement final : Statement
  {
    explicit ContinueStatement(SourceSpan sourceSpan) : Statement(StatementKind::Continue, sourceSpan) {}
  };

  struct BreakStatement final : Statement
  {
    explicit BreakStatement(SourceSpan sourceSpan) : Statement(StatementKind::Break, sourceSpan) {}
  };

  struct SwitchCasePattern final
  {
    const std::string variantName;
    std::optional<std::string> bindingName;
    /// Additional tuple-payload bindings (`case Cell(value, rest)`).
    std::vector<std::string> bindingNames;
    /// Scalar literal-or patterns (`case 1 | 2 | "x" { ... }`): raw literal
    /// texts; empty for variant patterns.
    const std::vector<std::string> literalTexts;
    /// INC-23 (DEC-D): union type pattern `case (x: i64)` — the member type
    /// to match; null for literal, variant, and tuple patterns.
    TypeSyntaxPtr memberType;
    const SourceSpan span;

    SwitchCasePattern(std::string variant, std::optional<std::string> binding, std::vector<std::string> extraBindings,
                      SourceSpan sourceSpan)
        : variantName(std::move(variant)), bindingName(std::move(binding)), bindingNames(std::move(extraBindings)),
          memberType(nullptr), span(sourceSpan)
    {
    }

    SwitchCasePattern(std::optional<std::string> binding, TypeSyntaxPtr typePattern, SourceSpan sourceSpan)
        : variantName{}, bindingName(std::move(binding)), memberType(std::move(typePattern)), span(sourceSpan)
    {
    }

    static auto literals(std::vector<std::string> texts, SourceSpan sourceSpan) -> SwitchCasePattern
    {
      return SwitchCasePattern{{}, {}, {}, std::move(texts), sourceSpan};
    }

    static auto typePattern(std::string binding, TypeSyntaxPtr type, SourceSpan sourceSpan) -> SwitchCasePattern
    {
      return SwitchCasePattern{std::move(binding), std::move(type), sourceSpan};
    }

    /// INC-33: tuple pattern `case (a, b)` — empty variant name, positional
    /// bindings, no member type.
    static auto tuplePattern(std::string first, std::vector<std::string> rest, SourceSpan sourceSpan)
        -> SwitchCasePattern
    {
      return SwitchCasePattern{{}, std::move(first), std::move(rest), sourceSpan};
    }

  private:
    SwitchCasePattern(std::string variant, std::optional<std::string> binding, std::vector<std::string> extraBindings,
                      std::vector<std::string> texts, SourceSpan sourceSpan)
        : variantName(std::move(variant)), bindingName(std::move(binding)), bindingNames(std::move(extraBindings)),
          literalTexts(std::move(texts)), memberType(nullptr), span(sourceSpan)
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

  /// D-004 ownership annotation on a native / `extern "C"` parameter.
  /// Unspecified keeps the existing copy-first default; the three explicit
  /// modes are only meaningful on ABI boundary declarations (checker-enforced).
  enum class ParameterOwnership
  {
    Unspecified,
    Owning,
    Borrowed,
    Shared,
  };

  struct FunctionParameter final
  {
    const std::string name;
    TypeSyntaxPtr type;
    const SourceSpan span;
    ParameterOwnership ownership{ParameterOwnership::Unspecified};

    FunctionParameter(std::string parameterName, TypeSyntaxPtr parameterType, SourceSpan sourceSpan,
                      ParameterOwnership parameterOwnership = ParameterOwnership::Unspecified)
        : name(std::move(parameterName)), type(std::move(parameterType)), span(sourceSpan),
          ownership(parameterOwnership)
    {
    }
  };

  /// Anonymous function literal: `|params| expression` or `|params| { statements }`
  /// (D-006). `body` is a block whose tail carries the single-expression form's
  /// value, mirroring `=> body` function sugar. Parameters may declare explicit
  /// types (`|x: i64| ...`); an omitted annotation is inferred at the call site
  /// by INC-14 slice S2. An optional capture list `[x, move y]` before the head
  /// names enclosing locals captured by the closure (INC-14 S3).
  struct Capture
  {
    std::string name;
    bool isMove = false;
    SourceSpan span;
  };

  struct ClosureExpression final : Expression
  {
    std::vector<Capture> captures;
    std::vector<FunctionParameter> parameters;
    std::unique_ptr<Block> body;

    ClosureExpression(std::vector<Capture> closureCaptures, std::vector<FunctionParameter> closureParameters,
                      std::unique_ptr<Block> closureBody, SourceSpan sourceSpan)
        : Expression(ExpressionKind::Closure, sourceSpan), captures(std::move(closureCaptures)),
          parameters(std::move(closureParameters)), body(std::move(closureBody))
    {
    }
  };

  enum class GenericParameterKind
  {
    Type,
    Const,
    Pack,
    /// `F<_>`: a type constructor of kind `* -> *`.
    TypeConstructor,
    /// `F<_, ...>`: a variadic type constructor; applications take any
    /// number of type arguments.
    VariadicTypeConstructor,
  };

  struct GenericParameter
  {
    GenericParameterKind kind;
    std::string name;
    TypeSyntaxPtr type;
    std::vector<std::string> traitBounds;
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
    Trait,
    Impl,
    Import,
    Opaque,
  };

  struct ModuleItem
  {
    const ModuleItemKind kind;
    const SourceSpan span;
    /// Origin module index assigned by the module loader; 0 for single-unit
    /// parses. Name-visibility enforcement keys on it.
    uint32_t originModule{0};

    explicit ModuleItem(ModuleItemKind itemKind, SourceSpan sourceSpan) : kind(itemKind), span(sourceSpan) {}
    virtual ~ModuleItem() = default;
  };

  using ModuleItemPtr = std::unique_ptr<ModuleItem>;

  struct StructDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<std::string> genericParameters;
    /// `derive(Copy + Clone)` synthesized impl targets.
    std::vector<std::string> derivedTraits;
    /// `repr(C)` (D-004): the record layout is deliberately shared with C and
    /// part of the public ABI contract; field types must be ABI-safe.
    const bool reprC;
    std::vector<StructFieldDeclaration> fields;

    StructDeclaration(std::string structName, std::vector<std::string> parameters, std::vector<std::string> derived,
                      std::vector<StructFieldDeclaration> structFields, SourceSpan sourceSpan, bool isReprC = false)
        : ModuleItem(ModuleItemKind::Struct, sourceSpan), name(std::move(structName)),
          genericParameters(std::move(parameters)), derivedTraits(std::move(derived)), reprC(isReprC),
          fields(std::move(structFields))
    {
    }
  };

  /// `type Name;` (abstract) or `type Name = native;` (native opaque handle).
  struct OpaqueTypeDeclaration final : ModuleItem
  {
    const std::string name;
    /// True for `type Name;` (no representation); false for `= native`.
    const bool abstract;
    std::vector<std::string> genericParameters;
    /// Variadic trailing parameter (`Tail...`); each application must supply
    /// at least `genericParameters.size()` arguments.
    std::optional<std::string> packParameter;

    OpaqueTypeDeclaration(std::string typeName, bool abstractType, std::vector<std::string> parameters,
                          std::optional<std::string> pack, SourceSpan sourceSpan)
        : ModuleItem(ModuleItemKind::Opaque, sourceSpan), name(std::move(typeName)), abstract(abstractType),
          genericParameters(std::move(parameters)), packParameter(std::move(pack))
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
        : ModuleItem(ModuleItemKind::Enum, sourceSpan), name(std::move(enumName)),
          genericParameters(std::move(parameters)), variants(std::move(enumVariants))
    {
    }
  };

  enum class ConstBodyKind
  {
    Expression,
    Native,
    Delete,
  };

  /// `import name;` or `import name (a, b);` — a module directive handled by
  /// the module loader; it contributes no HIR definitions.
  struct ImportDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<std::string> names;

    ImportDeclaration(std::string moduleName, std::vector<std::string> importedNames, SourceSpan sourceSpan)
        : ModuleItem(ModuleItemKind::Import, sourceSpan), name(std::move(moduleName)), names(std::move(importedNames))
    {
    }
  };

  struct TraitMethodDeclaration final
  {
    const std::string name;
    std::vector<FunctionParameter> parameters;
    TypeSyntaxPtr returnType;
    std::optional<Block> body;
    const SourceSpan span;

    TraitMethodDeclaration(std::string methodName, std::vector<FunctionParameter> methodParameters,
                           TypeSyntaxPtr methodReturnType, std::optional<Block> methodBody, SourceSpan sourceSpan)
        : name(std::move(methodName)), parameters(std::move(methodParameters)), returnType(std::move(methodReturnType)),
          body(std::move(methodBody)), span(sourceSpan)
    {
    }
  };

  struct TraitDeclaration final : ModuleItem
  {
    const std::string name;
    std::vector<std::string> supertraits;
    std::vector<TraitMethodDeclaration> methods;
    /// `auto trait X {}`: every concrete type implements it implicitly.
    const bool autoTrait;

    TraitDeclaration(std::string traitName, std::vector<std::string> supertraitList,
                     std::vector<TraitMethodDeclaration> traitMethods, bool automatic, SourceSpan sourceSpan)
        : ModuleItem(ModuleItemKind::Trait, sourceSpan), name(std::move(traitName)),
          supertraits(std::move(supertraitList)), methods(std::move(traitMethods)), autoTrait(automatic)
    {
    }
  };

  struct ImplDeclaration final : ModuleItem
  {
    const std::string traitName;
    /// Generic impl type parameters (`impl<T> Trait for List<T>`).
    std::vector<GenericParameter> genericParameters;
    TypeSyntaxPtr targetType;
    std::vector<TraitMethodDeclaration> methods;

    ImplDeclaration(std::string implementedTrait, std::vector<GenericParameter> implGenericParameters,
                    TypeSyntaxPtr implTarget, std::vector<TraitMethodDeclaration> implMethods, SourceSpan sourceSpan)
        : ModuleItem(ModuleItemKind::Impl, sourceSpan), traitName(std::move(implementedTrait)),
          genericParameters(std::move(implGenericParameters)), targetType(std::move(implTarget)),
          methods(std::move(implMethods))
    {
    }
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
                     std::vector<TypeSyntaxPtr> patterns, TypeSyntaxPtr target, ConstBodyKind kind,
                     ConstExprPtr expression, SourceSpan sourceSpan)
        : ModuleItem(ModuleItemKind::Const, sourceSpan), name(std::move(constName)),
          parameters(std::move(genericParameters)), patternArguments(std::move(patterns)),
          targetType(std::move(target)), bodyKind(kind), body(std::move(expression))
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
    const bool constFunction;
    const bool nativeFunction;
    /// `extern "C"` (D-004): a declared C-ABI function with no NG body; the
    /// native tier calls the C symbol directly. Mutual with `nativeFunction`.
    const bool externC;
    ExpressionPtr whereClause;
    const bool exported;

    FunctionDeclaration(std::string functionName, std::vector<GenericParameter> genericParameterList,
                        std::vector<FunctionParameter> functionParameters, TypeSyntaxPtr functionReturnType,
                        Block functionBody, SourceSpan sourceSpan, bool isConstFunction = false,
                        ExpressionPtr whereCondition = nullptr, bool isExported = false, bool isNativeFunction = false,
                        bool isExternC = false)
        : ModuleItem(ModuleItemKind::Function, sourceSpan), name(std::move(functionName)),
          genericParameters(std::move(genericParameterList)), parameters(std::move(functionParameters)),
          returnType(std::move(functionReturnType)), body(std::move(functionBody)), constFunction(isConstFunction),
          nativeFunction(isNativeFunction), externC(isExternC), whereClause(std::move(whereCondition)),
          exported(isExported)
    {
    }
  };

  struct SourceUnit final
  {
    const SourceSpan span;
    std::vector<ModuleItemPtr> items;
    /// Per-module visible top-level names computed by the module loader;
    /// empty means unrestricted (single-unit parses).
    std::unordered_map<uint32_t, std::unordered_set<std::string>> visibleNames;

    SourceUnit(SourceSpan sourceSpan, std::vector<ModuleItemPtr> moduleItems,
               std::unordered_map<uint32_t, std::unordered_set<std::string>> visible = {})
        : span(sourceSpan), items(std::move(moduleItems)), visibleNames(std::move(visible))
    {
    }
  };
} // namespace NG::syntax
