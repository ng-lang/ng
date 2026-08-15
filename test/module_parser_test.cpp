// AI-generated code; reviewed for this repository's vNext rewrite.
#include "syntax/module_parser.hpp"
#include "test.hpp"

namespace syntax = NG::syntax;

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

TEST_CASE("vNext module parser accepts generic function declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("fun identity<T>(value: T) -> T { return value; }");
  const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(source.items.front().get());
  REQUIRE(function != nullptr);
  REQUIRE(function->genericParameters.size() == 1);
  REQUIRE(function->genericParameters[0].kind == syntax::GenericParameterKind::Type);
  REQUIRE(function->genericParameters[0].name == "T");
}

TEST_CASE("vNext module parser accepts const generic parameter declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("fun repeat<const N: i64>(value: i64) -> array<i64, N> { }");
  const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(source.items.front().get());
  REQUIRE(function != nullptr);
  REQUIRE(function->genericParameters.size() == 1);
  REQUIRE(function->genericParameters[0].kind == syntax::GenericParameterKind::Const);
  REQUIRE(function->genericParameters[0].name == "N");
  REQUIRE(function->genericParameters[0].type != nullptr);
  const auto *lengthType = dynamic_cast<const syntax::NamedTypeSyntax *>(function->genericParameters[0].type.get());
  REQUIRE(lengthType != nullptr);
  REQUIRE(lengthType->name == "i64");
}

TEST_CASE("vNext module parser accepts mixed type and const generic parameters", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit("fun mix<T, const N: i64>(value: T) -> array<T, N> { }");
  const auto *function = dynamic_cast<const syntax::FunctionDeclaration *>(source.items.front().get());
  REQUIRE(function != nullptr);
  REQUIRE(function->genericParameters.size() == 2);
  REQUIRE(function->genericParameters[0].kind == syntax::GenericParameterKind::Type);
  REQUIRE(function->genericParameters[0].name == "T");
  REQUIRE(function->genericParameters[1].kind == syntax::GenericParameterKind::Const);
  REQUIRE(function->genericParameters[1].name == "N");
}

TEST_CASE("vNext module parser accepts generic enum declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit(
      "enum Result<T, E> { Ok(value: T), Err(error: E) } fun ok() -> Result<i64, string> { return Result.Ok(7); }");
  const auto *result = dynamic_cast<const syntax::EnumDeclaration *>(source.items[0].get());
  REQUIRE(result != nullptr);
  REQUIRE(result->genericParameters == std::vector<std::string>{"T", "E"});
  REQUIRE(result->variants[0].payloadType != nullptr);
  REQUIRE(result->variants[1].payloadType != nullptr);
}

TEST_CASE("vNext module parser accepts enum declarations", "[vNext][Syntax][Module]")
{
  const auto source = syntax::parseSourceUnit(
      "enum Result { Ok(i64), Error(string), Empty } fun empty() -> Result { return Result.Empty; }");
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
  const auto source = syntax::parseSourceUnit(
      "struct Point { x: i64, label: string } fun origin() -> Point { return Point { x: 0, label: \"origin\" }; }");
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

TEST_CASE("vNext module parser rejects malformed exports and function headers", "[vNext][Syntax][Module]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      static_cast<void>(syntax::parseSourceUnit(source));
      FAIL("expected a parse error");
    }
    catch (const syntax::ParseError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("export 5;", "expected a declaration after `export`");
  expect("fun 5() {}", "expected a function name after `fun`");
  expect("fun f(5) {}", "expected a parameter name");
  expect("fun f<T,>() {}", "expected a generic parameter after `,`");
  expect("fun f() where {}", "expected a where condition");
  expect("impl<,> Show for i64 {}", "expected a generic impl type parameter");
  expect("fun f()", "expected a function body block");
  expect("fun f() {", "expected `}` to close function body");
}

TEST_CASE("vNext module parser rejects malformed const declarations", "[vNext][Syntax][Module]")
{
  const auto expect = [](std::string_view source, std::string_view message)
  {
    try
    {
      static_cast<void>(syntax::parseSourceUnit(source));
      FAIL("expected a parse error");
    }
    catch (const syntax::ParseError &error)
    {
      REQUIRE(std::string{error.what()} == message);
    }
  };
  expect("const<const N: i64> x: bool = true;", "const parameters on const declarations are not yet supported");
  expect("const<5> x: bool = true;", "expected a const declaration generic parameter");
  expect("const f<i64: bool = true;", "expected `>` after const declaration pattern");
  expect("const x: bool = true fun main() {}", "expected `;` after const declaration body");
}

TEST_CASE("vNext module parser splits nested shift-right closers in const patterns", "[vNext][Syntax][Module]")
{
  const auto unit = syntax::parseSourceUnit("const nested<tuple<i64, i64>>: bool = true; fun main() {}");
  REQUIRE(unit.items.size() == 2);
  const auto &declaration = *static_cast<const syntax::ConstDeclaration *>(unit.items[0].get());
  REQUIRE(declaration.patternArguments.size() == 1);

  const auto deep = syntax::parseSourceUnit("const deep<array<tuple<i64>>>: bool = false; fun main() {}");
  REQUIRE(deep.items.size() == 2);
  const auto &nested = *static_cast<const syntax::ConstDeclaration *>(deep.items[0].get());
  REQUIRE(nested.patternArguments.size() == 1);
}
