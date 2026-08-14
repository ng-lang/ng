// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/driver.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

#include <sstream>

namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

namespace
{
  void check(std::string_view source)
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
  }

  auto run(const std::vector<std::string_view> &arguments, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::vnext::runDriver(arguments, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext type checker evaluates const if and records branch selection", "[vNext][Typecheck][ConstIf]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun main() -> i64 { const if (1 < 2) { return 1; } else { return 2; } }");
  const auto module = hir::Resolver{}.resolve(syntaxUnit);
  const auto result = typecheck::TypeChecker{}.check(module);
  const auto &statement = module.functions.front().body.statements.front();
  REQUIRE(statement.kind == hir::StatementKind::ConstIf);
  REQUIRE(result.constIfSelections.at(&statement));

  const auto falseUnit = syntax::parseSourceUnit("fun main() -> i64 { const if (2 < 1) { return 1; } else { return 2; } }");
  const auto falseModule = hir::Resolver{}.resolve(falseUnit);
  const auto falseResult = typecheck::TypeChecker{}.check(falseModule);
  const auto &falseStatement = falseModule.functions.front().body.statements.front();
  REQUIRE_FALSE(falseResult.constIfSelections.at(&falseStatement));
}

TEST_CASE("vNext type checker evaluates arithmetic, comparisons, and logical const if conditions", "[vNext][Typecheck][ConstIf]")
{
  REQUIRE_NOTHROW(check("fun main() -> i64 { const if ((2 + 3) * 4 > 10 && !false) { return 1; } return 2; }"));
  REQUIRE_NOTHROW(check("fun main() -> i64 { const if (false || 7 / 2 == 3) { return 1; } return 2; }"));
  REQUIRE_NOTHROW(check("fun main() -> i64 { const if (\"left\" != \"right\") { return 1; } return 2; }"));
}

TEST_CASE("vNext type checker ignores type errors in inactive const if branches", "[vNext][Typecheck][ConstIf]")
{
  REQUIRE_NOTHROW(check("fun main() -> i64 { const if (true) { return 1; } else { let wrong: i64 = \"text\"; } }"));
  REQUIRE_NOTHROW(check("fun main() -> i64 { const if (false) { let wrong: i64 = \"text\"; } else { return 2; } }"));
}

TEST_CASE("vNext type checker still rejects syntax and resolution errors in inactive const if branches", "[vNext][Typecheck][ConstIf]")
{
  try
  {
    check("fun main() -> i64 { const if (true) { return 1; } else { let = 3; } }");
    FAIL("expected syntax error in inactive const if branch");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected a binding name after `let`");
  }

  try
  {
    check("fun main() -> i64 { const if (true) { return 1; } else { return missing; } }");
    FAIL("expected resolution error in inactive const if branch");
  }
  catch (const hir::ResolutionError &error)
  {
    REQUIRE(std::string{error.what()} == "unresolved name `missing`");
  }
}

TEST_CASE("vNext type checker requires typed bool const if conditions", "[vNext][Typecheck][ConstIf]")
{
  try
  {
    check("fun main() { const if (1 + 2) { return; } }");
    FAIL("expected non-bool const if condition");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const if condition type mismatch: expected bool, got i64");
  }
}

TEST_CASE("vNext type checker rejects non-constant const if conditions", "[vNext][Typecheck][ConstIf]")
{
  try
  {
    check("fun main() -> bool { let flag = true; const if (flag) { return true; } return false; }");
    FAIL("expected non-constant const if condition");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const if condition is not a compile-time constant expression");
  }
}

TEST_CASE("vNext type checker reports const evaluation errors in const if conditions", "[vNext][Typecheck][ConstIf]")
{
  REQUIRE_THROWS_WITH(check("fun main() { const if (1 / 0 == 0) { return; } }"), "const integer division by zero");
  REQUIRE_THROWS_WITH(check("fun main() { const if (9223372036854775807 + 1 > 0) { return; } }"),
                      "const integer `+` overflow");
}

TEST_CASE("vNext type checker defers per-instance const if in const-generic functions", "[vNext][Typecheck][ConstIf]")
{
  try
  {
    check("fun choose<const N: i64>() -> i64 { const if (N > 0) { return 1; } return 2; }");
    FAIL("expected per-instance const if rejection");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "per-instance `const if` inside a const-generic function is not yet supported");
  }
}

TEST_CASE("vNext ngi driver lowers only the selected const if branch", "[vNext][Driver][ConstIf]")
{
  std::string output;
  std::string errors;
  REQUIRE(run({"--source", "fun main() -> i64 { const if (1 < 2) { return 1; } else { return 2; } }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 2 instruction(s) with value 1\n");
  REQUIRE(errors.empty());

  REQUIRE(run({"--source", "fun main() -> i64 { const if (2 < 1) { return 1; } else { return 2; } }"}, output, errors) == 0);
  REQUIRE(output == "compiled 1 vNext function(s); main returned after 2 instruction(s) with value 2\n");
  REQUIRE(errors.empty());
}
