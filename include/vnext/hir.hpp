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
    Index,
    Member,
    Binary,
  };

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
    std::vector<std::unique_ptr<Expression>> operands;
  };

  using ExpressionPtr = std::unique_ptr<Expression>;

  enum class StatementKind
  {
    Let,
    Assign,
    Return,
    If,
    Loop,
    Next,
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
  };

  struct Type;

  struct TypeArgument
  {
    syntax::GenericArgumentKind kind;
    std::unique_ptr<Type> type;
    uint64_t constInteger{};
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

  struct Function
  {
    DefId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<std::string> genericParameters;
    std::vector<Parameter> parameters;
    std::optional<std::string> returnTypeName;
    std::unique_ptr<Type> returnType;
    Block body;
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

  struct Module
  {
    std::vector<Function> functions;
    std::vector<Struct> structs;
    std::vector<Enum> enums;
  };

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
    [[nodiscard]] auto resolveBlock(const syntax::Block &block, bool introduceScope) -> Block;
    [[nodiscard]] auto resolveStatement(const syntax::Statement &statement) -> Statement;
    [[nodiscard]] auto resolveExpression(const syntax::Expression &expression) -> ExpressionPtr;
    [[nodiscard]] auto resolveName(const syntax::IdentifierExpression &expression) const -> ResolvedName;
    auto declareLocal(const std::string &name, syntax::SourceSpan span) -> LocalId;

    std::unordered_map<std::string, std::vector<DefId>> functions_;
    std::unordered_map<const syntax::FunctionDeclaration *, DefId> functionIds_;
    std::unordered_map<std::string, StructId> structs_;
    std::unordered_map<std::string, EnumId> enums_;
    std::unordered_map<std::string, std::vector<std::string>> enumVariants_;
    std::vector<Scope> scopes_;
    std::vector<ActiveLoop> loops_;
    std::unordered_map<uint32_t, bool> localMutability_;
    std::optional<DefId> currentFunction_;
    uint32_t nextLocal_{};
    uint32_t nextLoop_{};
  };
} // namespace NG::vnext::hir
