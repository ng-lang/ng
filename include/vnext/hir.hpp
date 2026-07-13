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

  struct LocalId
  {
    uint32_t value{};
    auto operator==(const LocalId &) const -> bool = default;
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
    std::vector<std::unique_ptr<Expression>> operands;
  };

  using ExpressionPtr = std::unique_ptr<Expression>;

  enum class StatementKind
  {
    Let,
    Return,
    If,
    Expression,
  };

  struct Block;

  struct Statement
  {
    StatementKind kind;
    syntax::SourceSpan span;
    std::optional<LocalId> local;
    ExpressionPtr expression;
    std::unique_ptr<Block> consequence;
    std::unique_ptr<Block> alternative;
  };

  struct Block
  {
    syntax::SourceSpan span;
    std::vector<Statement> statements;
    ExpressionPtr tailExpression;
  };

  struct Parameter
  {
    std::string name;
    LocalId local;
    syntax::SourceSpan span;
  };

  struct Function
  {
    DefId id;
    std::string name;
    syntax::SourceSpan span;
    std::vector<Parameter> parameters;
    Block body;
  };

  struct Module
  {
    std::vector<Function> functions;
  };

  class Resolver final
  {
  public:
    [[nodiscard]] auto resolve(const syntax::SourceUnit &unit) -> Module;

  private:
    using Scope = std::unordered_map<std::string, LocalId>;

    [[nodiscard]] auto resolveFunction(const syntax::FunctionDeclaration &function, DefId id) -> Function;
    [[nodiscard]] auto resolveBlock(const syntax::Block &block, bool introduceScope) -> Block;
    [[nodiscard]] auto resolveStatement(const syntax::Statement &statement) -> Statement;
    [[nodiscard]] auto resolveExpression(const syntax::Expression &expression) -> ExpressionPtr;
    [[nodiscard]] auto resolveName(const syntax::IdentifierExpression &expression) const -> ResolvedName;
    auto declareLocal(const std::string &name, syntax::SourceSpan span) -> LocalId;

    std::unordered_map<std::string, DefId> functions_;
    std::vector<Scope> scopes_;
    uint32_t nextLocal_{};
  };
} // namespace NG::vnext::hir
