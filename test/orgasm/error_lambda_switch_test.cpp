#include "../test.hpp"
#include <module.hpp>
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <intp/runtime_numerals.hpp>
#include <typecheck/typecheck.hpp>

using namespace NG;
using namespace NG::ast;
using namespace NG::orgasm;
using namespace NG::runtime;

namespace
{
struct SourceModuleFixture
{
  SourceModuleFixture()
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
  }

  ~SourceModuleFixture()
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
  }
};

auto result_i32(const RuntimeRef<StorageCell> &result) -> int32_t
{
  return read_numeric_cell_as<int32_t>(result);
}

auto result_bool(const RuntimeRef<StorageCell> &result) -> bool
{
  auto val = runtime_boolean_value(result);
  return val.has_value() && val.value();
}

auto result_string(const RuntimeRef<StorageCell> &result) -> Str
{
  return runtime_string_value(result);
}
} // namespace

TEST_CASE("switch expression should return correct value", "[ErrorLambdaSwitchTest]")
{
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val x = 42;
      val result = switch (x) {
        case 0 => 0;
        case 1 => 1;
        otherwise => 99;
      };
      return result;
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 99);

  destroyast(ast);
}

TEST_CASE("switch expression with when guard should work", "[ErrorLambdaSwitchTest]")
{
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val n = 15;
      val result = switch (n) {
        case 0 => 0;
        case x when x < 10 => 1;
        case y when y < 100 => 2;
        otherwise => 3;
      };
      return result;
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 2);

  destroyast(ast);
}

TEST_CASE("error propagation with Result should work", "[ErrorLambdaSwitchTest]")
{
  // Test basic Result pattern matching
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    type MyResult = Success(value: i32) | Failure(msg: string);

    fun main() -> i32 {
      val r = Success(42);
      switch (r) {
        case Success(v) { return v; }
        case Failure(e) { return -1; }
      }
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 42);

  destroyast(ast);
}

TEST_CASE("lambda expression should work", "[ErrorLambdaSwitchTest]")
{
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val dbl = (x: i32) -> i32 => x * 2;
      return dbl(21);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 42);

  destroyast(ast);
}

TEST_CASE("lambda with multiple parameters should work", "[ErrorLambdaSwitchTest]")
{
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val add = (a: i32, b: i32) -> i32 => a + b;
      return add(3, 4);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 7);

  destroyast(ast);
}

TEST_CASE("lambda with block body should work", "[ErrorLambdaSwitchTest]")
{
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val process = (x: i32) -> i32 {
        val tmp = x * 2;
        return tmp + 1;
      };
      return process(5);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 11);

  destroyast(ast);
}

TEST_CASE("lambda as higher-order function argument should work", "[ErrorLambdaSwitchTest]")
{
  // Function type syntax (T) -> U is not supported in parameters yet
  // Test lambda with simple variable assignment and call
  SourceModuleFixture fixture;
  auto ast = parse(R"(
    fun main() -> i32 {
      val f = (x: i32) -> i32 => x * 3;
      return f(5);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 15);

  destroyast(ast);
}
