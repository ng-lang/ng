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

TEST_CASE("vNext VM executes direct calls through module frames", "[vNext][VM]")
{
  const auto syntaxUnit = syntax::parseSourceUnit("fun helper(value: i64) -> i64 { return value + 1; } fun main() -> i64 { return helper(41); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  typecheck::TypeChecker{}.check(hirModule);
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions) flows.push_back(flowir::Lowerer{}.lower(function));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  const auto result = vm::VM{}.run(module, hir::DefId{1});
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.returnValue == 42);
}

TEST_CASE("vNext VM materializes integer literals and reads bound locals", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { let value = 42; return value; }");
  const auto result = vm::VM{}.run(function);
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.returnValue == 42);
}

TEST_CASE("vNext VM executes mutable local assignment", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { let mut total = 1; total := total + 41; return total; }");
  REQUIRE(vm::VM{}.run(function).returnValue == 42);
}

TEST_CASE("vNext VM executes prefix and logical boolean operations", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { if !(1 > 2) && (2 < 3) { return -42; } else { return 0; } }");
  REQUIRE(vm::VM{}.run(function).returnValue == -42);
}

TEST_CASE("vNext VM executes binary arithmetic and comparison-driven branches", "[vNext][VM]")
{
  const auto arithmetic = compile("fun main() -> i64 { return 6 * 7 + 1; }");
  REQUIRE(vm::VM{}.run(arithmetic).returnValue == 43);

  const auto branch = compile("fun main() -> i64 { if 1 < 2 { return 7; } else { return 9; } }");
  REQUIRE(vm::VM{}.run(branch).returnValue == 7);
}

TEST_CASE("vNext VM executes terminating loop state transitions", "[vNext][VM]")
{
  const auto function = compile(
      "fun main(seed: i64) -> i64 { loop (state = seed) { if state < 3 { next (state + 1); } return state; } }");
  const auto result = vm::VM{}.run(function, std::vector<int64_t>{0});
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.returnValue == 3);
  REQUIRE(result.tailRecursions == 0);
}

TEST_CASE("vNext VM tail recursion reuses the active frame until fuel exhaustion", "[vNext][VM]")
{
  const auto function = compile("fun recur(value: i64) { next (value); }");
  const auto result = vm::VM{}.run(function, std::vector<int64_t>{1}, 1000);
  REQUIRE(result.reason == vm::HaltReason::FuelExhausted);
  REQUIRE(result.executedInstructions == 1000);
  REQUIRE(result.tailRecursions == 500);
}

TEST_CASE("vNext VM dispatches loop backedges without host recursion", "[vNext][VM]")
{
  const auto function = compile("fun step(seed: i64) { loop (state = seed) { next (state); } }");
  const auto result = vm::VM{}.run(function, std::vector<int64_t>{1}, 1000);
  REQUIRE(result.reason == vm::HaltReason::FuelExhausted);
  REQUIRE(result.executedInstructions == 1000);
  REQUIRE(result.tailRecursions == 0);
}
