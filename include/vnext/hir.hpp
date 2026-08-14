// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/syntax/ast.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::vnext::hir
{
  struct DefId
  {
    uint32_t value{};
    auto operator==(const DefId &) const -> bool = default;
  };

  struct StructId
  {
    uint32_t value{};
    auto operator==(const StructId &) const -> bool = default;
  };

  struct EnumId
  {
    uint32_t value{};
    auto operator==(const EnumId &) const -> bool = default;
  };

  /// Locals are numbered module-globally: IDs never repeat across functions,
  /// so local side tables keyed by `LocalId` cannot collide between bodies.
  struct LocalId
  {
    uint32_t value{};
    auto operator==(const LocalId &) const -> bool = default;
  };

  struct LoopId
  {
    uint32_t value{};
    auto operator==(const LoopId &) const -> bool = default;
  };

  struct ResolutionError : std::runtime_error
  {
    syntax::SourceSpan span;

    ResolutionError(std::string message, syntax::SourceSpan sourceSpan)
      : std::runtime_error(std::move(message)), span(sourceSpan)
    {
    }
  };

  enum class ResolvedNameKind
  {
    Function,
    Local,
    ConstParameter,
  };

  struct ResolvedName
  {
    ResolvedNameKind kind;
    uint32_t id;
  };

  enum class ExpressionKind
  {
    IntegerLiteral,
    StringLiteral,
    ArrayLiteral,
    TupleLiteral,
    StructLiteral,
    EnumLiteral,
    BooleanLiteral,
    ResolvedName,
    Prefix,
    Grouped,
    Call,
    GenericApplication,
    TypeTest,
    TraitBound,
    Index,
    Member,
    Binary,
  };

  struct TypeArgument;
  struct Type;

  struct Expression
  {
    ExpressionKind kind;
    syntax::SourceSpan span;
    std::string text;
    std::optional<ResolvedName> resolvedName;
    std::vector<DefId> functionCandidates;
    std::optional<StructId> structId;
    std::optional<EnumId> enumId;
    std::optional<uint32_t> variant;
    std::vector<std::string> memberNames;
    std::vector<TypeArgument> genericArguments;
    /// Tested type for a `T is Type` where-clause constraint.
    std::unique_ptr<Type> testedType;
    /// Trait names for a `T: Trait` where-clause constraint.
    std::vector<std::string> traitNames;
    /// Method call (`receiver.method(...)` or qualified `Trait.method(...)`):
    /// operands[0] is the Member callee whose operands[0] is the receiver.
    bool methodCall{};
    std::vector<std::unique_ptr<Expression>> operands;
  };

  using ExpressionPtr = std::unique_ptr<Expression>;

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

  enum class NextTargetKind
  {
    Loop,
    Function,
  };

  struct NextTarget
  {
    NextTargetKind kind;
    uint32_t id;
  };

  struct Block;

  struct Type;

  /// One switch case: the variant name is resolved to its ordinal by the
  /// checker; the optional payload binding is a lexical local scoped to the
  /// case body.
  struct SwitchCase
  {
    std::string variantName;
    std::optional<LocalId> binding;
    std::unique_ptr<Block> body;
    syntax::SourceSpan span;
  };

  struct Statement
  {
    StatementKind kind;
    syntax::SourceSpan span;
    std::optional<LocalId> local;
    std::shared_ptr<Type> bindingType;
    std::vector<LocalId> destructuredLocals;
    std::vector<size_t> destructuredIndices;
    bool mutableBinding{};
    std::optional<LoopId> loop;
    std::optional<NextTarget> nextTarget;
    ExpressionPtr expression;
    ExpressionPtr assignmentTarget;
    std::vector<ExpressionPtr> arguments;
    std::vector<LocalId> loopBindings;
    std::vector<SwitchCase> switchCases;
    std::unique_ptr<Block> consequence;
    std::unique_ptr<Block> alternative;
    std::unique_ptr<Block> body;
  };

  struct Block
  {
    syntax::SourceSpan span;
    std::vector<Statement> statements;
    ExpressionPtr tailExpression;
  };

  enum class TypeKind
  {
    Named,
    Applied,
    ScopedReference,
    RawPointer,
    Pack,
  };

  struct Type;

  struct TypeArgument
  {
    syntax::GenericArgumentKind kind;
    std::unique_ptr<Type> type;
    syntax::ConstExprPtr constExpr;
    syntax::SourceSpan span;
  };

  struct Type
  {
    TypeKind kind;
    syntax::SourceSpan span;
    std::string name;
    std::vector<TypeArgument> arguments;
    std::unique_ptr<Type> target;
    bool isMutable{};
  };

  struct Parameter
  {
    std::string name;
    std::string typeName;
    Type type;
    LocalId local;
    syntax::SourceSpan span;
  };

  struct ConstParameter
  {
    std::string name;
    std::string typeName;
    Type type;
    syntax::SourceSpan span;
  };

  struct Function
  {
    DefId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<std::string> genericParameters;
    std::vector<std::string> packParameters;
    /// `F<_>` type-constructor parameters (kind `* -> *`).
    std::vector<std::string> constructorParameters;
    std::vector<ConstParameter> constParameters;
    /// Kind of every generic parameter in declaration order; explicit call
    /// site generic arguments map positionally against this order.
    std::vector<syntax::GenericParameterKind> genericParameterOrder;
    std::vector<Parameter> parameters;
    std::optional<std::string> returnTypeName;
    std::unique_ptr<Type> returnType;
    Block body;
    /// `const fun` (D-013): compile-time capable and runtime callable.
    bool constFunction{};
    /// `export` visibility (D-009); enforcement arrives with module privacy.
    bool exported{};
    /// `native fun` (D-004): implemented by the embedding at runtime; the
    /// declaration has no NG body.
    bool nativeFunction{};
    /// Where-clause constraint (D-014): predicate applications, `T is Type`,
    /// trait bounds, and boolean combinations; evaluated per concrete instance.
    ExpressionPtr whereClause;
    /// Trait bounds declared on generic parameters (`T: Show`).
    std::vector<std::pair<std::string, std::vector<std::string>>> traitBounds;
  };

  struct StructField
  {
    std::string name;
    Type type;
    syntax::SourceSpan span;
  };

  struct Struct
  {
    StructId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<std::string> genericParameters;
    std::vector<StructField> fields;
  };

  struct EnumVariant
  {
    std::string name;
    std::unique_ptr<Type> payloadType;
    syntax::SourceSpan span;
  };

  struct Enum
  {
    EnumId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<std::string> genericParameters;
    std::vector<EnumVariant> variants;
  };

  enum class ConstDeclarationBodyKind
  {
    Expression,
    Native,
    Delete,
  };

  /// Module-level const predicate (D-012). `pattern` holds the lowered header
  /// type arguments in declaration order; `typeParameters` are the declared
  /// generic parameters (prefix list plus implicitly introduced bare
  /// identifiers in the pattern).
  struct ConstDeclaration
  {
    DefId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<std::string> typeParameters;
    std::vector<std::unique_ptr<Type>> pattern;
    std::unique_ptr<Type> targetType;
    ConstDeclarationBodyKind bodyKind;
    syntax::ConstExprPtr body;
  };

  struct TraitMethod
  {
    std::string name;
    std::vector<Parameter> parameters;
    std::unique_ptr<Type> returnType;
    std::optional<Block> body;
    syntax::SourceSpan span;
  };

  struct Trait
  {
    std::string name;
    std::vector<std::string> supertraits;
    std::vector<TraitMethod> methods;
    /// DefId of each default method lowered as a module function; empty for
    /// declaration-only methods.
    std::vector<DefId> methodIds;
    syntax::SourceSpan span;
  };

  /// A declaration-site abstract (`type Name;`) or native opaque
  /// (`type Name = native;`) type.
  struct OpaqueType
  {
    std::string name;
    bool abstract;
    syntax::SourceSpan span;
  };

  struct Impl
  {
    std::string traitName;
    std::unique_ptr<Type> targetType;
    std::vector<TraitMethod> methods;
    /// DefId of each provided impl method lowered as a module function.
    std::vector<DefId> methodIds;
    syntax::SourceSpan span;
  };

  struct Module
  {
    std::vector<Function> functions;
    std::vector<Struct> structs;
    std::vector<Enum> enums;
    std::vector<ConstDeclaration> consts;
    std::vector<Trait> traits;
    std::vector<Impl> impls;
    /// `type Name;` abstract types and `type Name = native;` opaque handles.
    std::vector<OpaqueType> opaqueTypes;
  };

  /// Deep-clones a resolved function for generic instantiation, renumbering
  /// every `LocalId` and `LoopId` so the instance occupies a fresh identity
  /// space. Types, where clauses, and names are copied structurally.
  [[nodiscard]] auto cloneFunction(const Function &source, uint32_t &nextLocal) -> Function;

  class Resolver final
  {
  public:
    [[nodiscard]] auto resolve(const syntax::SourceUnit &unit) -> Module;

  private:
    using Scope = std::unordered_map<std::string, LocalId>;

    struct ActiveLoop
    {
      LoopId id;
    };

    [[nodiscard]] auto resolveFunction(const syntax::FunctionDeclaration &function, DefId id) -> Function;
    [[nodiscard]] auto resolveStruct(const syntax::StructDeclaration &structure, StructId id) -> Struct;
    [[nodiscard]] auto resolveEnum(const syntax::EnumDeclaration &enumeration, EnumId id) -> Enum;
    [[nodiscard]] auto resolveConstDeclaration(const syntax::ConstDeclaration &declaration, DefId id) -> ConstDeclaration;
    [[nodiscard]] auto resolveTrait(const syntax::TraitDeclaration &declaration) -> Trait;
    [[nodiscard]] auto resolveImpl(const syntax::ImplDeclaration &declaration) -> Impl;
    [[nodiscard]] auto resolveTraitMethod(const syntax::TraitMethodDeclaration &method) -> TraitMethod;
    [[nodiscard]] auto resolveBlock(const syntax::Block &block, bool introduceScope) -> Block;
    [[nodiscard]] auto resolveStatement(const syntax::Statement &statement) -> Statement;
    [[nodiscard]] auto resolveExpression(const syntax::Expression &expression) -> ExpressionPtr;
    [[nodiscard]] auto resolveName(const syntax::IdentifierExpression &expression) const -> ResolvedName;
    auto declareLocal(const std::string &name, syntax::SourceSpan span) -> LocalId;

    std::unordered_map<std::string, std::vector<DefId>> functions_;
    std::unordered_map<const syntax::FunctionDeclaration *, DefId> functionIds_;
    std::unordered_map<std::string, StructId> structs_;
    std::unordered_map<std::string, EnumId> enums_;
    std::unordered_map<std::string, syntax::SourceSpan> traits_;
    std::unordered_map<std::string, syntax::SourceSpan> opaqueTypes_;
    std::unordered_map<std::string, std::vector<std::string>> enumVariants_;
    std::vector<Scope> scopes_;
    std::vector<ActiveLoop> loops_;
    std::unordered_map<std::string, uint32_t> constParameters_;
    std::unordered_map<uint32_t, bool> localMutability_;
    std::optional<DefId> currentFunction_;
    uint32_t nextLocal_{};
    uint32_t nextLoop_{};
    uint32_t nextConstId_{};
    uint32_t nextImplMethodId_{};
  };
} // namespace NG::vnext::hir
