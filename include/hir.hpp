// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "syntax/ast.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::hir
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
    std::string code;

    ResolutionError(std::string message, syntax::SourceSpan sourceSpan)
        : ResolutionError("E-resolve", std::move(message), sourceSpan)
    {
    }

    ResolutionError(std::string errorCode, std::string message, syntax::SourceSpan sourceSpan)
        : std::runtime_error(std::move(message)), span(sourceSpan), code(std::move(errorCode))
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
    FloatLiteral,
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
    Closure,
  };

  struct TypeArgument;
  struct Type;
  struct Block;

  struct Expression
  {
    ExpressionKind kind;
    syntax::SourceSpan span;
    std::string text;
    /// Numeric literal suffix (`i8`, `u8`, `f32`, ...), stripped from `text`.
    std::string numericSuffix;
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
    /// Anonymous closure literal (`|x| body`): parameter locals declared in a
    /// closure body scope, resolved parameter type trees, and the body block.
    /// INC-14 S3: explicit capture list resolved to enclosing-scope locals.
    struct CaptureInfo
    {
      LocalId local;
      bool isMove = false;
    };
    std::vector<std::string> closureParameterNames;
    std::vector<LocalId> closureParameterIds;
    std::vector<std::shared_ptr<Type>> closureParameterTypes;
    std::unique_ptr<Block> closureBody;
    std::vector<CaptureInfo> closureCaptures;
    /// INC-14 S3: closure-body-scope locals that shadow the captured names;
    /// the closure code function body references these (parallel to
    /// closureCaptures). Original enclosing locals live in closureCaptures.
    std::vector<LocalId> closureCaptureParams;
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
    While,
    Continue,
    Break,
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
    /// Scalar literal-or pattern texts (`case 1 | "a" | true`); empty for
    /// variant patterns.
    std::vector<std::string> literalTexts;
    /// INC-23 (DEC-D): union type-pattern member type (`case (x: i64)`).
    /// A unique_ptr so a forward-declared hir::Type (defined later in this
    /// header) completes lazily at destruction sites.
    std::unique_ptr<Type> memberType;
    std::optional<LocalId> binding;
    /// Additional tuple-payload bindings (`case Cell(value, rest)`).
    std::vector<LocalId> bindings;
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
    /// `let (first, ...rest) = tuple;`: the rest binding (a tuple of the
    /// remaining elements).
    std::optional<LocalId> restLocal;
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
    ScopedSpan,
    RawPointer,
    Pack,
    Union,
    Closure,
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
    /// Union members (`A | B`).
    std::vector<Type> members;
    bool isMutable{};
    /// Closure type (`|T| -> U`): parameters then result.
    std::vector<Type> parameters;
    std::unique_ptr<Type> result;
    /// D-002 A1.1 origin contracts: resolved parameter ordinals for
    /// scoped references with declared origins (e.g. `T ref(a | b)`).
    std::vector<uint32_t> originOrdinals;
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
    /// Origin module index from the module loader; drives name visibility.
    uint32_t originModule{};
    std::vector<std::string> genericParameters;
    std::vector<std::string> packParameters;
    /// `F<_>` type-constructor parameters (kind `* -> *`).
    std::vector<std::string> constructorParameters;
    /// `F<_, ...>` variadic type-constructor parameters.
    std::vector<std::string> variadicConstructorParameters;
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
    /// `extern "C"` (D-004): a declared C-ABI function with no NG body; the
    /// native tier calls the C symbol directly. Mutual with `nativeFunction`.
    bool externC{};
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
    /// `derive(Copy + Clone)` synthesized impl targets.
    std::vector<std::string> derivedTraits;
    /// `repr(C)` (D-004): layout is deliberately shared with C; field types
    /// must be ABI-safe and the record may cross `extern "C"` boundaries.
    bool reprC{};
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
    /// `auto trait X {}`: every concrete type implements it implicitly.
    bool autoTrait{};
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
    std::vector<std::string> genericParameters;
    std::optional<std::string> packParameter;
    syntax::SourceSpan span;
  };

  struct Impl
  {
    std::string traitName;
    /// Generic impl type parameter names (`impl<T> Trait for List<T>`).
    std::vector<std::string> genericParameters;
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
  /// Deep-copies an anonymous closure `|params| body` into an independent code
  /// function whose parameter locals are remapped. The typechecker uses this
  /// to synthesize closures as ordinary concrete functions (INC-14 S1).
  [[nodiscard]] auto makeClosureFunction(const Expression &closure, const std::string &name, DefId id,
                                         uint32_t &nextLocal) -> Function;

  class Resolver final
  {
  public:
    [[nodiscard]] auto resolve(const syntax::SourceUnit &unit) -> Module;

  private:
    using Scope = std::unordered_map<std::string, LocalId>;

    struct ActiveLoop
    {
      LoopId id;
      /// Whether a bare `next` inside this construct is captured (a `loop`
      /// WITH binders). `while` and binderless loops never capture bare `next`.
      bool capturesNext;
      /// Whether this construct is a `while` (only `while` allows `continue`).
      bool isWhile;
    };

    void declareModuleItems(const syntax::SourceUnit &unit);
    auto appendMethodFunction(Module &module, TraitMethod &method, std::string name) -> DefId;
    [[nodiscard]] auto resolveFunction(syntax::FunctionDeclaration &function, DefId id) -> Function;
    [[nodiscard]] auto resolveStruct(const syntax::StructDeclaration &structure, StructId id) -> Struct;
    [[nodiscard]] auto resolveEnum(const syntax::EnumDeclaration &enumeration, EnumId id) -> Enum;
    [[nodiscard]] auto resolveConstDeclaration(const syntax::ConstDeclaration &declaration, DefId id)
        -> ConstDeclaration;
    [[nodiscard]] auto resolveTrait(const syntax::TraitDeclaration &declaration) -> Trait;
    [[nodiscard]] auto resolveImpl(const syntax::ImplDeclaration &declaration) -> Impl;
    [[nodiscard]] auto resolveTraitMethod(syntax::TraitMethodDeclaration &method) -> TraitMethod;
    /// `isFunctionBody` marks the top-level body of a function/method: only
    /// there is a trailing block tail an implicit return (INC-13 try desugar
    /// moves it into the Ok arm as an explicit return).
    [[nodiscard]] auto resolveBlock(syntax::Block &block, bool introduceScope, bool isFunctionBody = false) -> Block;
    [[nodiscard]] auto resolveStatement(syntax::Statement &statement) -> Statement;
    [[nodiscard]] auto resolveExpression(const syntax::Expression &expression) -> ExpressionPtr;
    [[nodiscard]] auto resolveName(const syntax::IdentifierExpression &expression) const -> ResolvedName;
    [[nodiscard]] auto resolveEnumVariant(const syntax::IdentifierExpression &owner,
                                          const syntax::MemberExpression &member, Expression &resolved) const -> bool;
    [[nodiscard]] auto suggestionFor(const std::string &name) const -> std::string;
    auto declareLocal(const std::string &name, syntax::SourceSpan span) -> LocalId;

    /// INC-13 (D-007): resolves a statement list, restructuring the rest of
    /// the list into the Ok arm when a root `?` (try) statement appears.
    void resolveStatementList(syntax::Block &source, bool isFunctionBody, Block &out);
    /// INC-13 (D-007): desugars `expr?` into a scrutinee let + switch whose
    /// Err arm early-returns `Result.Err($err)`; the Ok arm binds the payload
    /// and resolves the remaining statements/tail (recursing for nested `?`).
    void desugarTry(const syntax::Statement &statement, syntax::TryExpression &tryExpr, syntax::Block &source,
                    size_t startIndex, bool isFunctionBody, Block &out);
    /// Extracts a try operator that roots a let initializer or a standalone
    /// expression statement (the only positions INC-13 desugars).
    [[nodiscard]] static auto rootTryOf(syntax::Statement &statement) -> syntax::TryExpression *;

    std::unordered_map<std::string, std::vector<DefId>> functions_;
    std::unordered_map<const syntax::FunctionDeclaration *, DefId> functionIds_;
    std::unordered_map<std::string, StructId> structs_;
    std::unordered_map<std::string, EnumId> enums_;
    std::unordered_map<std::string, syntax::SourceSpan> traits_;
    std::unordered_map<std::string, syntax::SourceSpan> opaqueTypes_;
    /// Per-module visible top-level names (from the source unit); empty
    /// means unrestricted.
    std::unordered_map<uint32_t, std::unordered_set<std::string>> visibleNames_;
    uint32_t currentOrigin_{};
    std::unordered_map<std::string, std::vector<std::string>> enumVariants_;
    std::vector<Scope> scopes_;
    std::vector<ActiveLoop> loops_;
    std::unordered_map<std::string, uint32_t> constParameters_;
    std::unordered_map<uint32_t, bool> localMutability_;
    std::optional<DefId> currentFunction_;
    /// Name (as written) of the enclosing function's return type; INC-13 `?`
    /// is only allowed when it starts with the `Result` prefix.
    std::string currentReturnTypeName_;
    /// Deepest nesting of plain function/method bodies (guards the try
    /// desugar's implicit-return handling for nested blocks).
    int functionBodyDepth_{};
    /// Count of enclosing closure bodies; `?` is rejected inside them.
    int closureBodyDepth_{};
    /// Monotone counter for synthesized `$tryN`/`$okN`/`$errN` locals.
    uint32_t nextTryId_{};
    /// Maps the current function's parameter names to their declaration
    /// ordinal (D-002 A1.1 origin contracts). Set during return-type lowering.
    std::unordered_map<std::string, uint32_t> paramOrdinals_;
    uint32_t nextLocal_{};
    uint32_t nextLoop_{};
    uint32_t nextConstId_{};
  };
} // namespace NG::hir
