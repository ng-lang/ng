// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/bytecode.hpp"
#include "vnext/driver.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"

#include <filesystem>
#include <sstream>

namespace bytecode = NG::vnext::bytecode;
namespace flowir = NG::vnext::flowir;
namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;

namespace
{
  constexpr std::string_view ShapeSource =
      "enum Shape { Circle(radius: i64), Square(side: i64), Empty }";

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
    const int status = NG::vnext::runDriver({"--source", source}, outputStream, errorStream);
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
    const int status = NG::vnext::runDriver({path}, outputStream, errorStream);
    output = std::move(outputStream).str();
    errors = std::move(errorStream).str();
    return status;
  }
} // namespace

TEST_CASE("vNext parser builds switch statements with case patterns and otherwise", "[vNext][Switch][Syntax]")
{
  const auto unit = syntax::parseSourceUnit(std::string{ShapeSource} +
                                            " fun main() { let s = Shape.Circle(1); switch (s) { "
                                            "case Circle(r) { r; } case Square(w) { w; } otherwise { } } }");
  const auto function = std::find_if(unit.items.begin(), unit.items.end(), [](const auto &item) {
    return dynamic_cast<const syntax::FunctionDeclaration *>(item.get()) != nullptr;
  });
  REQUIRE(function != unit.items.end());
  const auto &declaration = *static_cast<const syntax::FunctionDeclaration *>(function->get());
  const auto &body = declaration.body;
  REQUIRE(body.statements.size() == 2);
  const auto *switchStatement = dynamic_cast<const syntax::SwitchStatement *>(body.statements[1].get());
  REQUIRE(switchStatement != nullptr);
  REQUIRE(switchStatement->cases.size() == 2);
  REQUIRE(switchStatement->cases[0].pattern.variantName == "Circle");
  REQUIRE(switchStatement->cases[0].pattern.bindingName == std::optional<std::string>{"r"});
  REQUIRE(switchStatement->cases[1].pattern.variantName == "Square");
  REQUIRE(switchStatement->cases[1].pattern.bindingName == std::optional<std::string>{"w"});
  REQUIRE(switchStatement->otherwise != nullptr);
}

TEST_CASE("vNext resolver scopes switch payload bindings to each case body", "[vNext][Switch][Hir]")
{
  const auto module = resolve(std::string{ShapeSource} +
                              " fun main() -> i64 { let s = Shape.Circle(1); let mut total = 0; switch (s) { "
                              "case Circle(r) { total := r; } case Square(w) { total := w; } case Empty { total := 0; } } "
                              "switch (s) { case Circle(r) { total := r; } case Square(w) { total := w; } case Empty { total := 0; } } "
                              "return total; }");
  const auto &function = module.functions.back();
  const auto firstSwitch = std::find_if(function.body.statements.begin(), function.body.statements.end(),
                                        [](const auto &statement) { return statement.kind == hir::StatementKind::Switch; });
  REQUIRE(firstSwitch != function.body.statements.end());
  REQUIRE(firstSwitch->switchCases.size() == 3);
  REQUIRE(firstSwitch->switchCases[0].binding.has_value());
  REQUIRE(firstSwitch->switchCases[1].binding.has_value());
  // Reused binding names across cases must resolve to distinct local ids.
  const auto secondSwitch = std::find_if(std::next(firstSwitch), function.body.statements.end(),
                                         [](const auto &statement) { return statement.kind == hir::StatementKind::Switch; });
  REQUIRE(secondSwitch != function.body.statements.end());
  REQUIRE(secondSwitch->switchCases[0].binding->value != firstSwitch->switchCases[0].binding->value);
}

TEST_CASE("vNext type checker validates switch exhaustiveness and duplicate variants", "[vNext][Switch][Typecheck]")
{
  try
  {
    check(std::string{ShapeSource} +
          " fun main() { let s = Shape.Circle(1); switch (s) { case Circle(r) { r; } } }");
    FAIL("expected a non-exhaustive switch error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "switch is not exhaustive: missing variant `Square`");
  }

  try
  {
    check(std::string{ShapeSource} +
          " fun main() { let s = Shape.Circle(1); switch (s) { case Circle(r) { r; } case Circle(x) { x; } case Square(w) { w; } case Empty { } } }");
    FAIL("expected a duplicate variant error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "duplicate variant `Circle` in switch");
  }

  try
  {
    check(std::string{ShapeSource} +
          " fun main() { let s = Shape.Circle(1); switch (s) { case Nope { } otherwise { } } }");
    FAIL("expected an unknown variant error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "unknown variant `Nope` for enum `Shape`");
  }

  try
  {
    check(std::string{ShapeSource} +
          " fun main() { let s = Shape.Circle(1); switch (s) { case Empty(value) { value; } otherwise { } } }");
    FAIL("expected a payloadless binding error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "variant `Empty` has no payload to bind");
  }
}

TEST_CASE("vNext FlowIR lowers switch dispatch chains over enum variants", "[vNext][Switch][FlowIR]")
{
  const auto module = resolve(std::string{ShapeSource} +
                              " fun main() -> i64 { let s = Shape.Circle(1); let mut total = 0; switch (s) { "
                              "case Circle(r) { total := r; } case Square(w) { total := w; } case Empty { total := 0; } } "
                              "return total; }");
  const auto typed = typecheck::TypeChecker{}.check(module);
  const auto function = flowir::Lowerer{}.lower(module.functions.back(), typed);
  size_t variantIndexes{};
  size_t payloadExtracts{};
  for (const auto &block : function.blocks)
  {
    for (const auto &instruction : block.instructions)
    {
      if (instruction.kind == flowir::InstructionKind::EnumVariantIndex) ++variantIndexes;
      if (instruction.kind == flowir::InstructionKind::ExtractEnumPayload) ++payloadExtracts;
    }
  }
  REQUIRE(variantIndexes == 1);
  REQUIRE(payloadExtracts == 2);
  REQUIRE_NOTHROW(flowir::Verifier{}.verify(function));
  REQUIRE_NOTHROW(bytecode::Verifier{}.verify(bytecode::Compiler{}.compile(function)));
}

TEST_CASE("vNext switch executes payload bindings, payloadless cases, and otherwise", "[vNext][Switch][Runtime]")
{
  std::string output;
  std::string errors;
  REQUIRE(run(std::string{ShapeSource} +
                  " fun main() -> i64 { let s = Shape.Circle(5); let mut total = 0; switch (s) { "
                  "case Circle(r) { total := r; } case Square(w) { total := -1; } case Empty { total := -2; } } "
                  "return total; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 5") != std::string::npos);

  REQUIRE(run(std::string{ShapeSource} +
                  " fun main() -> i64 { let s = Shape.Empty; let mut total = 0; switch (s) { "
                  "case Circle(r) { total := -1; } otherwise { total := 9; } } "
                  "return total; }",
              output, errors) == 0);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 9") != std::string::npos);
}

TEST_CASE("vNext switch example file runs end to end through ngi", "[vNext][Switch][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/vnext/enum_match.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 50") != std::string::npos);
}
