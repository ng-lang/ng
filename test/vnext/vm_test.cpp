// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/bytecode.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"
#include "vnext/vm.hpp"

namespace bytecode = NG::vnext::bytecode;
namespace flowir = NG::vnext::flowir;
namespace hir = NG::vnext::hir;
namespace syntax = NG::vnext::syntax;
namespace typecheck = NG::vnext::typecheck;
namespace vm = NG::vnext::vm;

namespace
{
  [[nodiscard]] auto compile(std::string_view source) -> bytecode::Function
  {
    const auto syntaxUnit = syntax::parseSourceUnit(source);
    const auto module = hir::Resolver{}.resolve(syntaxUnit);
    typecheck::TypeChecker{}.check(module);
    return bytecode::Compiler{}.compile(flowir::Lowerer{}.lower(module.functions.front()));
  }
} // namespace

TEST_CASE("vNext VM executes verified return control flow", "[vNext][VM]")
{
  const auto function = compile("fun done() { return; }");
  const auto result = vm::VM{}.run(function);
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.executedInstructions == 1);
  REQUIRE(result.tailRecursions == 0);
}

TEST_CASE("vNext VM materializes integer literals and reads bound locals", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { let value = 42; return value; }");
  const auto result = vm::VM{}.run(function);
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.returnValue == 42);
}

TEST_CASE("vNext VM tail recursion reuses the active frame until fuel exhaustion", "[vNext][VM]")
{
  const auto function = compile("fun recur(value: i64) { next (value); }");
  const auto result = vm::VM{}.run(function, 1000);
  REQUIRE(result.reason == vm::HaltReason::FuelExhausted);
  REQUIRE(result.executedInstructions == 1000);
  REQUIRE(result.tailRecursions == 500);
}

TEST_CASE("vNext VM dispatches loop backedges without host recursion", "[vNext][VM]")
{
  const auto function = compile("fun step(seed: i64) { loop (state = seed) { next (state); } }");
  const auto result = vm::VM{}.run(function, 1000);
  REQUIRE(result.reason == vm::HaltReason::FuelExhausted);
  REQUIRE(result.executedInstructions == 1000);
  REQUIRE(result.tailRecursions == 0);
}
