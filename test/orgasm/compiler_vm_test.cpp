#include "../test.hpp"
#include <module.hpp>
#include <orgasm/compiler.hpp>
#include <orgasm/vm.hpp>
#include <intp/runtime_numerals.hpp>
#include <typecheck/typecheck.hpp>

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

  NG::typecheck::type_check(ast);

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

  NG::typecheck::type_check(ast);

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

  NG::typecheck::type_check(ast);

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
  NG::typecheck::type_check(ast);

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

TEST_CASE("compiler and vm should handle switch otherwise for tagged unions", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Result = Ok(value: i32) | Err(msg: string);

        fun main() {
            val failure = Err("boom");
            switch (failure) {
                case Ok(value) {
                    return value;
                }
                otherwise {
                    return 7;
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
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 7);

  destroyast(ast);
}

TEST_CASE("compiler and vm should fold const if from typeof query", "[const_if][OrgasmTest]")
{
  auto ast = parse(R"(
        fun main() {
            val value = 42;
            const if (typeof(value).name == "i32") {
                return value;
            } else {
                return 0;
            }
        }
    )");
  REQUIRE(ast != nullptr);
  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should pass arguments to imported functions", "[OrgasmTest]")
{
  auto &registry = NG::module::get_module_registry();
  registry.clear();
  NG::module::clear_module_loader_cache();

  auto importedAst = parse(R"(
        fun add(a: i32, b: i32) {
            return a + b;
        }
    )");
  REQUIRE(importedAst != nullptr);

  Compiler importedCompiler;
  auto importedBytecode = importedCompiler.compile(dynamic_ast_cast<CompileUnit>(importedAst));

  auto moduleInfo = std::make_shared<NG::module::ModuleInfo>();
  moduleInfo->moduleId = "ext";
  moduleInfo->moduleName = "ext";
  moduleInfo->moduleAst = dynamic_ast_cast<CompileUnit>(importedAst);
  moduleInfo->bytecodeModule = std::make_shared<BytecodeModule>(std::move(importedBytecode));
  registry.addModuleInfo(moduleInfo);

  auto ast = parse(R"(
        import ext (add);

        fun main() {
            return add(20, 22);
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

  registry.clear();
  NG::module::clear_module_loader_cache();
  destroyast(ast);
  destroyast(importedAst);
}

TEST_CASE("compiler should clear variant state between compile calls", "[OrgasmTest]")
{
  Compiler compiler;

  auto firstAst = parse(R"(
        type Result = Ok(value: i32);

        fun main() {
            return 0;
        }
    )");
  REQUIRE(firstAst != nullptr);
  REQUIRE_NOTHROW(compiler.compile(dynamic_ast_cast<CompileUnit>(firstAst)));

  auto secondAst = parse(R"(
        fun Ok(x: i32) -> i32 {
            return x + 1;
        }

        fun main() {
            return Ok(41);
        }
    )");
  REQUIRE(secondAst != nullptr);

  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(secondAst));
  VM vm;
  auto result = vm.run(bytecode);

  auto numeric = std::dynamic_pointer_cast<NumeralBase>(result);
  REQUIRE(numeric != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(numeric.get()) == 42);

  destroyast(firstAst);
  destroyast(secondAst);
}
