#include "../test.hpp"
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <intp/runtime_numerals.hpp>

using namespace NG;
using namespace NG::ast;
using namespace NG::orgasm;
using namespace NG::runtime;

TEST_CASE("Orgasm: newtype wrap and unwrap", "[OrgasmTest][Nominal]")
{
  auto ast = parse(R"(
        type UserId wraps i64;
        fun main() {
            val x = cast<UserId>(42);
            return 42;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(read_numeric_cell_as<int32_t>(result) == 42);

  destroyast(ast);
}

TEST_CASE("Orgasm: type alias transparent", "[OrgasmTest][Nominal]")
{
  auto ast = parse(R"(
        type Meters = f64;
        fun main() {
            val x: Meters = 3.14;
            return 42;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(read_numeric_cell_as<int32_t>(result) == 42);

  destroyast(ast);
}

TEST_CASE("Orgasm: multiple newtypes", "[OrgasmTest][Nominal]")
{
  auto ast = parse(R"(
        type UserId wraps i64;
        type OrderId wraps i64;
        fun main() {
            val uid = cast<UserId>(1);
            val oid = cast<OrderId>(2);
            return 1 + 2;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(read_numeric_cell_as<int32_t>(result) == 3);

  destroyast(ast);
}

TEST_CASE("Orgasm: newtype show delegates to wrapped", "[OrgasmTest][Nominal]")
{
  auto ast = parse(R"(
        type Wrapper wraps i32;
        fun main() {
            val x = cast<Wrapper>(42);
            print(x);
            return 42;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(read_numeric_cell_as<int32_t>(result) == 42);

  destroyast(ast);
}
