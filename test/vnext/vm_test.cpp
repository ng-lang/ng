// AI-generated code; reviewed for this repository's vNext rewrite.
#include "../test.hpp"
#include "vnext/bytecode.hpp"
#include "vnext/flowir.hpp"
#include "vnext/hir.hpp"
#include "vnext/syntax/module_parser.hpp"
#include "vnext/typecheck.hpp"
#include "vnext/vm.hpp"

#include <limits>

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
    static_cast<void>(typecheck::TypeChecker{}.check(module));
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
  static_cast<void>(typecheck::TypeChecker{}.check(hirModule));
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

TEST_CASE("vNext VM materializes dynamic and fixed homogeneous arrays", "[vNext][VM]")
{
  const auto dynamic = vm::VM{}.run(compile("fun values() -> array<i64> { return [1, 2, 3]; }"));
  REQUIRE(dynamic.returnValue->isArray());
  REQUIRE(dynamic.returnValue->asArray().size() == 3);
  REQUIRE(dynamic.returnValue->asArray()[0] == 1);
  REQUIRE(dynamic.returnValue->asArray()[2] == 3);

  const auto fixed = vm::VM{}.run(compile("fun values() -> array<i64, 3> { return [4, 5, 6]; }"));
  REQUIRE(fixed.returnValue->isArray());
  REQUIRE(fixed.returnValue->asArray().size() == 3);
  REQUIRE(fixed.returnValue->asArray()[1] == 5);
}

TEST_CASE("vNext VM passes string values through direct module calls", "[vNext][VM]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun greeting() -> string { return \"hello\"; } fun main() -> string { return greeting() + \" world\"; }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  const auto typed = typecheck::TypeChecker{}.check(hirModule);
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions) flows.push_back(flowir::Lowerer{}.lower(function, typed));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  REQUIRE(vm::VM{}.run(module, hir::DefId{1}).returnValue == NG::vnext::Value::string("hello world"));
}

TEST_CASE("vNext VM executes typed string constants and concatenation", "[vNext][VM]")
{
  const auto function = compile("fun greeting() -> string { return \"hello\" + \" world\"; }");
  REQUIRE(vm::VM{}.run(function).returnValue == NG::vnext::Value::string("hello world"));
}

TEST_CASE("vNext VM executes prefix and logical boolean operations", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { if !(1 > 2) && (2 < 3) { return -42; } else { return 0; } }");
  REQUIRE(vm::VM{}.run(function).returnValue == -42);
}

TEST_CASE("vNext VM rejects checked i64 arithmetic overflow", "[vNext][VM]")
{
  const auto add = compile("fun main(value: i64) -> i64 { return value + 1; }");
  REQUIRE_THROWS_WITH(vm::VM{}.run(add, std::vector<int64_t>{std::numeric_limits<int64_t>::max()}),
                      "integer addition overflow");

  const auto multiply = compile("fun main(value: i64) -> i64 { return value * 2; }");
  REQUIRE_THROWS_WITH(vm::VM{}.run(multiply, std::vector<int64_t>{std::numeric_limits<int64_t>::max()}),
                      "integer multiplication overflow");

  const auto negate = compile("fun main(value: i64) -> i64 { return -value; }");
  REQUIRE_THROWS_WITH(vm::VM{}.run(negate, std::vector<int64_t>{std::numeric_limits<int64_t>::min()}),
                      "integer negation overflow");

  const auto divide = compile("fun main(value: i64) -> i64 { return value / -1; }");
  REQUIRE_THROWS_WITH(vm::VM{}.run(divide, std::vector<int64_t>{std::numeric_limits<int64_t>::min()}),
                      "integer division overflow");
}

TEST_CASE("vNext VM short-circuits logical operations", "[vNext][VM]")
{
  const auto andFunction = compile("fun main() -> i64 { if false && (1 / 0 == 0) { return 1; } return 42; }");
  REQUIRE(vm::VM{}.run(andFunction).returnValue == 42);

  const auto orFunction = compile("fun main() -> i64 { if true || (1 / 0 == 0) { return 42; } return 1; }");
  REQUIRE(vm::VM{}.run(orFunction).returnValue == 42);
}

TEST_CASE("vNext VM executes else-if control-flow chains", "[vNext][VM]")
{
  const auto function = compile(
      "fun main(first: bool, second: bool) -> i64 { if first { return 1; } else if second { return 2; } else { return 3; } }");
  REQUIRE(vm::VM{}.run(function, std::vector<int64_t>{0, 1}).returnValue == 2);
  REQUIRE(vm::VM{}.run(function, std::vector<int64_t>{0, 0}).returnValue == 3);
}

TEST_CASE("vNext VM executes i64 bitwise and shift operations", "[vNext][VM]")
{
  const auto function = compile("fun main() -> i64 { return ((12 & 10) | 1) ^ (1 << 3) >> 1; }");
  REQUIRE(vm::VM{}.run(function).returnValue == 13);
}

TEST_CASE("vNext VM rejects zero divisors and invalid shift counts", "[vNext][VM]")
{
  REQUIRE_THROWS_WITH(vm::VM{}.run(compile("fun main() -> i64 { return 1 / 0; }")), "integer division by zero");
  REQUIRE_THROWS_WITH(vm::VM{}.run(compile("fun main() -> i64 { return 1 << -1; }")), "integer shift count is out of range");
}

TEST_CASE("vNext VM executes binary arithmetic and comparison-driven branches", "[vNext][VM]")
{
  const auto arithmetic = compile("fun main() -> i64 { return 6 * 7 + 1; }");
  REQUIRE(vm::VM{}.run(arithmetic).returnValue == 43);

  const auto branch = compile("fun main() -> i64 { if 1 < 2 { return 7; } else { return 9; } }");
  REQUIRE(vm::VM{}.run(branch).returnValue == 7);
}

TEST_CASE("vNext VM executes terminating stateful tail recursion without host recursion", "[vNext][VM]")
{
  const auto syntaxUnit = syntax::parseSourceUnit(
      "fun count(value: i64) -> i64 { if value == 0 { return 0; } next (value - 1); } fun main() -> i64 { return count(3); }");
  const auto hirModule = hir::Resolver{}.resolve(syntaxUnit);
  static_cast<void>(typecheck::TypeChecker{}.check(hirModule));
  std::vector<flowir::Function> flows;
  for (const auto &function : hirModule.functions) flows.push_back(flowir::Lowerer{}.lower(function));
  const auto module = bytecode::ModuleCompiler{}.compile(flows);
  const auto result = vm::VM{}.run(module, hir::DefId{1});
  REQUIRE(result.reason == vm::HaltReason::Return);
  REQUIRE(result.returnValue == 0);
  REQUIRE(result.tailRecursions == 3);
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
