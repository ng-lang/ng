// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/syntax/module_parser.hpp"

namespace syntax = NG::vnext::syntax;

namespace
{
  [[nodiscard]] auto asFunction(const syntax::ModuleItemPtr &item) -> const syntax::FunctionDeclaration &
  {
    const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(item.get());
    REQUIRE(function != nullptr);
    return *function;
  }

  [[nodiscard]] auto asLet(const syntax::StatementPtr &statement) -> const syntax::LetStatement &
  {
    const auto *let = dynamic_cast<const syntax::LetStatement *>(statement.get());
    REQUIRE(let != nullptr);
    return *let;
  }
} // namespace

TEST_CASE("vNext module parser accepts generic enum declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("enum Result<T, E> { Ok(value: T), Err(error: E) } fun ok() -> Result<i64, string> { return Result.Ok(7); }");
  const auto *result = dynamic_cast<const syntax::EnumDeclaration *>(source.items[0].get());
  REQUIRE(result != nullptr);
  REQUIRE(result->genericParameters == std::vector<std::string>{"T", "E"});
  REQUIRE(result->variants[0].payloadType != nullptr);
  REQUIRE(result->variants[1].payloadType != nullptr);
}

TEST_CASE("vNext module parser accepts enum declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("enum Result { Ok(i64), Error(string), Empty } fun empty() -> Result { return Result.Empty; }");
  REQUIRE(source.items.size() == 2);
  const auto *result = dynamic_cast<const syntax::EnumDeclaration *>(source.items[0].get());
  REQUIRE(result != nullptr);
  REQUIRE(result->name == "Result");
  REQUIRE(result->variants.size() == 3);
  REQUIRE(result->variants[0].payloadType != nullptr);
  REQUIRE(result->variants[2].payloadType == nullptr);
}

TEST_CASE("vNext module parser accepts struct declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("struct Point { x: i64, label: string } fun origin() -> Point { return Point { x: 0, label: \"origin\" }; }");
  REQUIRE(source.items.size() == 2);
  const auto *point = dynamic_cast<const syntax::StructDeclaration *>(source.items[0].get());
  REQUIRE(point != nullptr);
  REQUIRE(point->name == "Point");
  REQUIRE(point->fields.size() == 2);
  REQUIRE(point->fields[0].name == "x");
  REQUIRE(point->fields[1].name == "label");
}

TEST_CASE("vNext module parser accepts declarations and keeps bindings inside blocks", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("fun compute() { let result = 2 * 3; result } fun empty() { }");
  REQUIRE(source.items.size() == 2);

  const auto &compute = asFunction(source.items[0]);
  REQUIRE(compute.name == "compute");
  REQUIRE(compute.body.statements.size() == 1);
  REQUIRE(asLet(compute.body.statements[0]).name == "result");
  REQUIRE(compute.body.tailExpression != nullptr);

  const auto &empty = asFunction(source.items[1]);
  REQUIRE(empty.name == "empty");
  REQUIRE(empty.body.statements.empty());
  REQUIRE(empty.body.tailExpression == nullptr);
}

TEST_CASE("vNext module parser rejects a local statement at module scope", "[vNext][Syntax][Module]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("let value = 1;"));
    FAIL("expected module parser to reject a top-level let binding");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "expected a module declaration");
    REQUIRE(error.span().begin == 0);
    REQUIRE(error.span().end == 3);
  }
}

TEST_CASE("vNext module parser rejects local declarations in a function block", "[vNext][Syntax][Module]")
{
  try
  {
    static_cast<void>(syntax::parseSourceUnit("fun outer() { fun inner() { } }"));
    FAIL("expected block parser to reject a local declaration");
  }
  catch (const syntax::ParseError &error)
  {
    REQUIRE(std::string{error.what()} == "module declarations are not permitted in a block");
    REQUIRE(error.span().begin == 14);
    REQUIRE(error.span().end == 17);
  }
}
