#include "../test.hpp"
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <intp/runtime_numerals.hpp>

using namespace NG;
using namespace NG::ast;
using namespace NG::orgasm;

TEST_CASE("compiler and vm should handle basic arithmetic", "[OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            return 1 + 2 * 3;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 7);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle const if true branch", "[const_if][OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            const if (true) {
                return 42;
            } else {
                return 0;
            }
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle const if false branch", "[const_if][OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            const if (false) {
                return 0;
            } else {
                return 42;
            }
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle const if with negation", "[const_if][OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            const if (!false) {
                return 7;
            } else {
                return 0;
            }
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 7);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle const if without else", "[const_if][OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            val x = 10;
            const if (true) {
                val y = 32;
                return x + y;
            }
            return 0;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle function parameters", "[OrgasmTest]")
{
  auto ast = parse(R"(
        fun add(a: i32, b: i32) {
            return a + b;
        }
        fun main() {
            return add(10, 20);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 30);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle tagged unions", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Result = Ok(value: i32) | Err(msg: string);

        fun main() {
            val success = Ok(42);
            switch (success) {
                case Ok(value) {
                    return value;
                }
                case Err(msg) {
                    return 0;
                }
            }
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(ast);
}
