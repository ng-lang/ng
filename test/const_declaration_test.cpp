// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "driver.hpp"
#include "hir.hpp"
#include "syntax/const_expr.hpp"
#include "syntax/module_parser.hpp"
#include "syntax/type_parser.hpp"
#include "typecheck.hpp"

#include <filesystem>
#include <sstream>

namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  [[nodiscard]] auto resolve(std::string_view source) -> hir::Module
  {
    return hir::Resolver{}.resolve(syntax::parseSourceUnit(source));
  }

  void check(std::string_view source)
  {
    static_cast<void>(typecheck::TypeChecker{}.check(resolve(source)));
  }

  [[nodiscard]] auto run(std::string_view source, std::string &output, std::string &errors) -> int
  {
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({"--source", source}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  [[nodiscard]] auto runExample(std::string_view filename, std::string &output, std::string &errors) -> int
  {
    std::string path{filename};
    if (!std::filesystem::is_directory(std::filesystem::current_path() / "example")) path = std::string{"../"} + path;
    std::ostringstream outputStream;
    std::ostringstream errorStream;
    const int status = NG::runDriver({path}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }

  void expectPredicate(std::string_view declarations, std::string_view application, bool expected)
  {
    std::string output;
    std::string errors;
    const std::string source = std::string{declarations} +
                               "fun main() -> i64 { const if (" + std::string{application} +
                               ") { return 1; } return 0; }";
    REQUIRE(run(source, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(expected ? "native main exited with code 1" : "native main exited with code 0") != std::string::npos);
  }
} // namespace

TEST_CASE("vNext const declaration parser builds primary, specialization, native, and delete forms", "[vNext][ConstDecl][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(
      "const is_ref<T>: bool = false; "
      "const<T> is_ref<ref<T>>: bool = true; "
      "const builtin<T>: bool = native; "
      "const<T> forbidden<ref<T>>: bool = delete;");
  REQUIRE(unit.items.size() == 4);
  const auto &primary = *static_cast<const syntax::ConstDeclaration *>(unit.items[0].get());
  REQUIRE(primary.name == "is_ref");
  REQUIRE(primary.parameters.empty());
  REQUIRE(primary.patternArguments.size() == 1);
  REQUIRE(primary.bodyKind == syntax::ConstBodyKind::Expression);
  REQUIRE(dynamic_cast<const syntax::ConstBoolLiteral *>(primary.body.get()) != nullptr);

  const auto &specialization = *static_cast<const syntax::ConstDeclaration *>(unit.items[1].get());
  REQUIRE(specialization.parameters.size() == 1);
  REQUIRE(specialization.parameters[0].name == "T");
  REQUIRE(dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(specialization.patternArguments[0].get()) != nullptr);

  REQUIRE(static_cast<const syntax::ConstDeclaration *>(unit.items[2].get())->bodyKind == syntax::ConstBodyKind::Native);
  REQUIRE(static_cast<const syntax::ConstDeclaration *>(unit.items[3].get())->bodyKind == syntax::ConstBodyKind::Delete);
}

TEST_CASE("vNext resolver introduces primary type parameters implicitly but keeps concrete names concrete", "[vNext][ConstDecl][Hir]")
{
  const auto module = resolve(
      "struct Box { value: i64 } "
      "const is_ref<T>: bool = false; "
      "const is_i64<i64>: bool = true; "
      "const is_box<Box>: bool = true;");
  REQUIRE(module.consts.size() == 3);
  REQUIRE(module.consts[0].typeParameters == std::vector<std::string>{"T"});
  REQUIRE(module.consts[1].typeParameters.empty());
  REQUIRE(module.consts[2].typeParameters.empty());
}

TEST_CASE("vNext const expression parser handles boolean literals and comparisons", "[vNext][ConstDecl][ConstExpr]")
{
  const auto tokens = syntax::Lexer{}.lex("N > 2 && !flag");
  syntax::ConstExprParser parser{std::move(tokens)};
  const auto expression = parser.parse();
  REQUIRE(expression->kind == syntax::ConstExprKind::Binary);
  REQUIRE(renderConstExpr(*expression) != "");
  REQUIRE_NOTHROW(syntax::ConstExprParser{syntax::Lexer{}.lex("1 + 2 == 3")}.parse());
  REQUIRE_NOTHROW(syntax::ConstExprParser{syntax::Lexer{}.lex("true")}.parse());
  REQUIRE_NOTHROW(syntax::ConstExprParser{syntax::Lexer{}.lex("N > 2")}.parse());
}

TEST_CASE("vNext type parser accepts prefix ref sugar equivalent to postfix", "[vNext][ConstDecl][Type]")
{
  const auto prefix = syntax::TypeParser{syntax::Lexer{}.lex("ref<i64>")}.parse();
  const auto postfix = syntax::TypeParser{syntax::Lexer{}.lex("i64 ref")}.parse();
  REQUIRE(prefix->kind == postfix->kind);
  REQUIRE(dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(prefix.get()) != nullptr);
  REQUIRE(dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(postfix.get()) != nullptr);
  const auto mutablePrefix = syntax::TypeParser{syntax::Lexer{}.lex("ref mut<i64>")}.parse();
  const auto mutablePostfix = syntax::TypeParser{syntax::Lexer{}.lex("i64 ref mut")}.parse();
  REQUIRE(mutablePrefix->kind == mutablePostfix->kind);
  REQUIRE(dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(mutablePrefix.get())->isMutable);
  REQUIRE(dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(mutablePostfix.get())->isMutable);
}

TEST_CASE("vNext const predicates match reference patterns against concrete types", "[vNext][ConstDecl][Typecheck]")
{
  expectPredicate("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true;", "is_ref<i64>", false);
  expectPredicate("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true;", "is_ref<ref<i64>>", true);
  expectPredicate("const is_ref<T>: bool = false; const<T> is_ref<ref<T>>: bool = true;", "is_ref<array<i64>>", false);
}

TEST_CASE("vNext const predicates support repeated-parameter and exact patterns", "[vNext][ConstDecl][Typecheck]")
{
  const std::string declarations = "const equal<T, U>: bool = false; const<T> equal<T, T>: bool = true;";
  expectPredicate(declarations, "equal<i64, i64>", true);
  expectPredicate(declarations, "equal<i64, string>", false);

  expectPredicate("const is_i64<i64>: bool = true; const<T> is_i64<T>: bool = false;", "is_i64<i64>", true);
  expectPredicate("const is_i64<i64>: bool = true; const<T> is_i64<T>: bool = false;", "is_i64<string>", false);
}

TEST_CASE("vNext const predicates match nominal struct patterns", "[vNext][ConstDecl][Typecheck]")
{
  expectPredicate("struct Box { value: i64 } const is_box<T>: bool = false; const<T> is_box<Box>: bool = true;",
                  "is_box<Box>", true);
  expectPredicate("struct Box { value: i64 } const is_box<T>: bool = false; const<T> is_box<Box>: bool = true;",
                  "is_box<i64>", false);
}

TEST_CASE("vNext const predicate selection rejects deleted declarations", "[vNext][ConstDecl][Typecheck]")
{
  try
  {
    check("const<T> forbidden<ref<T>>: bool = delete; "
          "fun main() { const if (forbidden<ref<i64>>) { return; } }");
    FAIL("expected a deleted const declaration error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "const declaration `forbidden` is deleted for these type arguments");
  }
}

TEST_CASE("vNext const predicate selection rejects unregistered natives", "[vNext][ConstDecl][Typecheck]")
{
  try
  {
    check("const builtin<T>: bool = native; "
          "fun main() { const if (builtin<i64>) { return; } }");
    FAIL("expected an unregistered native error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "no const native registered for `builtin`");
  }
}

TEST_CASE("vNext const predicate evaluation reports unknown and abstract applications", "[vNext][ConstDecl][Typecheck]")
{
  try
  {
    check("fun main() { const if (missing<i64>) { return; } }");
    FAIL("expected an unknown const declaration error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown const declaration `missing`");
  }

  try
  {
    check("const is_ref<T>: bool = false; "
          "fun describe<T>() { const if (is_ref<T>) { return; } }");
    FAIL("expected an abstract type parameter error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot evaluate const declaration `is_ref` for abstract type parameter `T`");
  }
}

TEST_CASE("vNext const predicate evaluation reports ambiguous specializations", "[vNext][ConstDecl][Typecheck]")
{
  try
  {
    check("const is_ref<T>: bool = false; const<T> is_ref<T>: bool = true; "
          "fun main() { const if (is_ref<i64>) { return; } }");
    FAIL("expected an ambiguous specialization error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "ambiguous const specialization `is_ref`");
  }
}

TEST_CASE("vNext const declaration bodies fold arithmetic with const parameters", "[vNext][ConstDecl][Typecheck]")
{
  // Bodies are evaluated by the checked const evaluator; arithmetic errors
  // carry source spans.
  expectPredicate("const<T> within<T>: bool = 1 + 2 > 2;", "within<i64>", true);
}

TEST_CASE("vNext const predicates example file runs end to end through ngi", "[vNext][ConstDecl][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/const_predicates.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("native main exited with code 86") != std::string::npos);
}

TEST_CASE("vNext const declaration bodies evaluate logical operators", "[vNext][ConstDecl][ConstExpr]")
{
  expectPredicate("const<T> alwaysT<T>: bool = false || true; ", "alwaysT<i64>", true);
  expectPredicate("const<T> neverT<T>: bool = true && false; ", "neverT<i64>", false);
  expectPredicate("const<T> mixed<T>: bool = (true && false) || (false || true); ", "mixed<i64>", true);
  expectPredicate("const<T> shortT<T>: bool = true || (1 / 0 == 0); ", "shortT<i64>", true);
  expectPredicate("const<T> shortF<T>: bool = false && (1 / 0 == 0); ", "shortF<i64>", false);
}
