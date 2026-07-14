// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"

namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;

TEST_CASE("vNext resolver lowers lexical names into independent HIR identities", "[vNext][HIR][Resolver]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun entry(input: i32) { let computed = helper(input); if computed { let computed = 2; computed } else { input } } "
      "fun helper(value: i32) { value }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  REQUIRE(module.functions.size() == 2);

  const auto &entry = module.functions[0];
  REQUIRE(entry.id.value == 0);
  REQUIRE(entry.parameters.size() == 1);
  REQUIRE(entry.parameters[0].local.value == 0);
  REQUIRE(entry.body.statements.size() == 2);

  const auto &binding = entry.body.statements[0];
  REQUIRE(binding.kind == hir::StatementKind::Let);
  REQUIRE(binding.local->value == 1);
  REQUIRE(binding.expression->kind == hir::ExpressionKind::Call);
  REQUIRE(binding.expression->operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function);
  REQUIRE(binding.expression->operands[0]->resolvedName->id == 1);
  REQUIRE(binding.expression->operands[1]->resolvedName->kind == hir::ResolvedNameKind::Local);
  REQUIRE(binding.expression->operands[1]->resolvedName->id == 0);

  const auto &ifStatement = entry.body.statements[1];
  REQUIRE(ifStatement.kind == hir::StatementKind::If);
  REQUIRE(ifStatement.expression->resolvedName->id == 1);
  REQUIRE(ifStatement.consequence->statements[0].local->value == 2);
  REQUIRE(ifStatement.consequence->tailExpression->resolvedName->id == 2);
  REQUIRE(ifStatement.alternative->tailExpression->resolvedName->id == 0);
}

TEST_CASE("vNext resolver targets the nearest loop and function tail next distinctly", "[vNext][HIR][Resolver]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun step(seed: i32) { loop (outer = seed) { loop (inner = outer) { next (inner); } next (outer); } next (seed); }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto &function = module.functions[0];
  REQUIRE(function.body.statements.size() == 2);

  const auto &outerLoop = function.body.statements[0];
  REQUIRE(outerLoop.kind == hir::StatementKind::Loop);
  REQUIRE(outerLoop.loop->value == 0);
  REQUIRE(outerLoop.arguments[0]->resolvedName->kind == hir::ResolvedNameKind::Local);
  REQUIRE(outerLoop.arguments[0]->resolvedName->id == 0);

  const auto &innerLoop = outerLoop.body->statements[0];
  REQUIRE(innerLoop.kind == hir::StatementKind::Loop);
  REQUIRE(innerLoop.loop->value == 1);
  REQUIRE(innerLoop.arguments[0]->resolvedName->id == outerLoop.loopBindings[0].value);
  const auto &innerNext = innerLoop.body->statements[0];
  REQUIRE(innerNext.nextTarget->kind == hir::NextTargetKind::Loop);
  REQUIRE(innerNext.nextTarget->id == 1);

  const auto &outerNext = outerLoop.body->statements[1];
  REQUIRE(outerNext.nextTarget->kind == hir::NextTargetKind::Loop);
  REQUIRE(outerNext.nextTarget->id == 0);

  const auto &tailNext = function.body.statements[1];
  REQUIRE(tailNext.kind == hir::StatementKind::Next);
  REQUIRE(tailNext.nextTarget->kind == hir::NextTargetKind::Function);
  REQUIRE(tailNext.nextTarget->id == function.id.value);
}

TEST_CASE("vNext resolver rejects assignment to immutable lexical bindings", "[vNext][HIR][Resolver]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun entry() { let value = 1; value := 2; }");
  try
  {
    static_cast<void>(hir::Resolver{}.resolve(syntaxUnit));
    FAIL("expected immutable assignment to fail");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot assign to immutable binding `value`");
    REQUIRE(error.span.begin == 29);
  }
}

TEST_CASE("vNext resolver rejects unknown names with a source span", "[vNext][HIR][Resolver]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun entry() { missing }");
  try
  {
    static_cast<void>(hir::Resolver{}.resolve(syntaxUnit));
    FAIL("expected resolver to reject an unknown name");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "unresolved name `missing`");
    REQUIRE(error.span.begin == 14);
    REQUIRE(error.span.end == 21);
  }
}

TEST_CASE("vNext resolver rejects duplicate module and lexical declarations", "[vNext][HIR][Resolver]")
{
  const auto duplicateModule = syntax::parseSourceUnit("fun same() { } fun same() { }");
  try
  {
    static_cast<void>(hir::Resolver{}.resolve(duplicateModule));
    FAIL("expected duplicate module declaration to fail");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate module declaration `same`");
    REQUIRE(error.span.begin == 15);
  }

  const auto duplicateLocal = syntax::parseSourceUnit("fun entry() { let value = 1; let value = 2; }");
  try
  {
    static_cast<void>(hir::Resolver{}.resolve(duplicateLocal));
    FAIL("expected duplicate lexical binding to fail");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate local binding `value`");
    REQUIRE(error.span.begin == 29);
  }
}
