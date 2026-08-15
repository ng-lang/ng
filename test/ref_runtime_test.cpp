// AI-generated code; reviewed for this repository's vNext rewrite.
#include "test.hpp"
#include "bytecode.hpp"
#include "driver.hpp"
#include "flowir.hpp"
#include "hir.hpp"
#include "syntax/module_parser.hpp"
#include "typecheck.hpp"
#include "value.hpp"

#include <filesystem>
#include <sstream>

namespace bytecode = NG::bytecode;
namespace flowir = NG::flowir;
namespace hir = NG::hir;
namespace syntax = NG::syntax;
namespace typecheck = NG::typecheck;

namespace
{
  void check(std::string_view source)
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    static_cast<void>(typecheck::TypeChecker{}.check(module));
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

  [[nodiscard]] auto expectMainValue(std::string_view source, std::string_view value, std::string_view caseName) -> void
  {
    std::string output;
    std::string errors;
    INFO(caseName);
    REQUIRE(run(source, output, errors) == 0);
    INFO("errors: " << errors);
    REQUIRE(errors.empty());
    REQUIRE(output.find(std::string{"with value "} + std::string{value}) != std::string::npos);
  }

  [[nodiscard]] auto decode(std::string_view source) -> std::vector<bytecode::DecodedInstruction>
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    const auto typed = typecheck::TypeChecker{}.check(module);
    const auto function = bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(module.functions.front(), typed));
    return bytecode::Decoder{}.decode(function);
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
} // namespace

TEST_CASE("vNext generic swap through mutable references executes end to end", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun swap<T>(a: T ref mut, b: T ref mut) { let tmp = *a; *a := *b; *b := tmp; } "
                  "fun main() -> i64 { let mut x = 1; let mut y = 2; swap(ref mut x, ref mut y); return x * 10 + y; }",
                  "21", "swap");
}

TEST_CASE("vNext references read and write array elements in place", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun main() -> i64 { let mut values = [10, 20, 30]; let second = ref mut values[1]; *second := 99; return values[1]; }",
                  "99", "array element");
}

TEST_CASE("vNext references read and write struct fields in place", "[vNext][Ref][Runtime]")
{
  expectMainValue("struct Box { value: i64 } "
                  "fun main() -> i64 { let mut box = Box { value: 20 }; let field = ref mut box.value; *field := 21; return box.value; }",
                  "21", "struct field");
}

TEST_CASE("vNext references read and write tuple projections in place", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun main() -> i64 { let mut pair = (1, true); let second = ref mut pair.1; *second := false; "
                  "if (pair.1) { return 1; } return 7; }",
                  "7", "tuple projection");
}

TEST_CASE("vNext references follow binding rebinds through the shared cell", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun main() -> i64 { let mut value = 1; let write = ref mut value; value := 5; *write := 6; return value; }",
                  "6", "rebind");
}

TEST_CASE("vNext compound writes through a dereferenced reference path", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun main() -> i64 { let mut values = [[1, 2], [3, 4]]; let row = ref mut values[0]; (*row)[0] := 9; return values[0][0]; }",
                  "9", "nested path");
}

TEST_CASE("vNext bindings deep-copy aggregates instead of aliasing", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun main() -> i64 { let mut original = [1, 2, 3]; let mut copied = original; copied[0] := 9; "
                  "if (original[0] != 1) { return 0; } return copied[0]; }",
                  "9", "local copy");
}

TEST_CASE("vNext call arguments and returns deep-copy aggregates", "[vNext][Ref][Runtime]")
{
  expectMainValue("fun mutate(values: array<i64>) -> i64 { let mut local = values; local[0] := 7; return local[0]; } "
                  "fun main() -> i64 { let original = [1, 2, 3]; let changed = mutate(original); "
                  "if (original[0] != 1) { return 0; } return changed; }",
                  "7", "parameter copy");
}

TEST_CASE("vNext type checker rejects assignment through immutable references", "[vNext][Ref][Typecheck]")
{
  try
  {
    check("fun main() { let mut value = 1; let read = ref value; *read := 2; }");
    FAIL("expected an immutable reference assignment error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot assign through an immutable reference");
  }
}

TEST_CASE("vNext type checker rejects references to non-places", "[vNext][Ref][Typecheck]")
{
  try
  {
    check("fun main() { let r = ref (1 + 2); }");
    FAIL("expected a non-place reference error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "reference operand is not a place");
  }
}

TEST_CASE("vNext type checker rejects mutable references to immutable bindings", "[vNext][Ref][Typecheck]")
{
  try
  {
    check("fun main() { let value = 1; let r = ref mut value; }");
    FAIL("expected an immutable binding error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot create a mutable reference to an immutable binding");
  }
}

TEST_CASE("vNext type checker rejects assigning through non-reference values", "[vNext][Ref][Typecheck]")
{
  try
  {
    check("fun main() { let mut value = 1; *value := 2; }");
    FAIL("expected a non-reference assignment error");
  }
  catch (const typecheck::TypeError &error)
  {
    REQUIRE(std::string{error.what()} == "cannot assign through value of type i64");
  }
}

TEST_CASE("vNext bytecode encodes reference creation, load, and place assignment", "[vNext][Ref][Bytecode]")
{
  const auto instructions = decode(
      "fun main() -> i64 { let mut value = 1; let write = ref mut value; let read = *write; *write := 2; return read + value; }");
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction) {
            return instruction.opcode == bytecode::Opcode::MakeRef;
          }) == 1);
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction) {
            return instruction.opcode == bytecode::Opcode::LoadRef;
          }) == 1);
  REQUIRE(std::ranges::count_if(instructions, [](const auto &instruction) {
            return instruction.opcode == bytecode::Opcode::AssignPlace;
          }) == 1);
  const auto makeRef = std::find_if(instructions.begin(), instructions.end(), [](const auto &instruction) {
    return instruction.opcode == bytecode::Opcode::MakeRef;
  });
  REQUIRE(makeRef != instructions.end());
  REQUIRE(makeRef->operands[2] == 1); // mutable
}

TEST_CASE("vNext ref example files run end to end through ngi", "[vNext][Ref][Examples]")
{
  std::string output;
  std::string errors;
  REQUIRE(runExample("example/ref_swap.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 21") != std::string::npos);

  REQUIRE(runExample("example/ref_places.ng", output, errors) == 0);
  INFO("errors: " << errors);
  REQUIRE(errors.empty());
  REQUIRE(output.find("with value 32") != std::string::npos);
}

TEST_CASE("vNext runtime values deep-copy aggregates while references share their root cell", "[vNext][Ref][Value]")
{
  const auto original = NG::Value::array({NG::Value::integer(1), NG::Value::integer(2)});
  auto copied = original.deepCopy();
  copied.asArrayMut()[0] = NG::Value::integer(9);
  REQUIRE(original.asArray()[0] == 1);
  REQUIRE(copied.asArray()[0] == 9);

  auto cell = std::make_shared<NG::Value>(NG::Value::integer(3));
  const auto first = NG::Value::reference(cell, {}, true);
  const auto second = first.deepCopy();
  REQUIRE(second.isReference());
  REQUIRE(second.asReference().root == cell);
}
