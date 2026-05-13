#include "../test.hpp"
#include <algorithm>
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
void append_u16(Vec<uint8_t> &code, uint16_t value)
{
  code.push_back(static_cast<uint8_t>(value & 0xFFU));
  code.push_back(static_cast<uint8_t>((value >> 8U) & 0xFFU));
}

void emit_u16(Vec<uint8_t> &code, OpCode op, uint16_t value)
{
  code.push_back(static_cast<uint8_t>(op));
  append_u16(code, value);
}

void emit_u16_u16(Vec<uint8_t> &code, OpCode op, uint16_t first, uint16_t second)
{
  code.push_back(static_cast<uint8_t>(op));
  append_u16(code, first);
  append_u16(code, second);
}
} // namespace

static auto result_i32(const RuntimeRef<StorageCell> &result) -> int32_t
{
  return read_numeric_cell_as<int32_t>(result);
}

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

  REQUIRE(result_i32(result) == 7);

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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 7);

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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 30);

  destroyast(ast);
}

TEST_CASE("compiler and vm should copy array bindings by default", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        fun main() {
            val arr = [1];
            val copy = arr;
            copy[0] := 2;
            if (arr[0] == 1) {
                return copy[0];
            }
            return 0;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 2);

  destroyast(ast);
}

TEST_CASE("compiler and vm should alias heap object bindings from new", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        type Box {
            value: i32;
        }

        fun main() {
            val box = new Box { value: 1 };
            val copy = box;
            copy.value := 2;
            if (box.value == 2) {
                return copy.value;
            }
            return 0;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 2);

  destroyast(ast);
}

TEST_CASE("compiler and vm should support ref swap with move dereference", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        fun swap(a: i32 ref, b: i32 ref) {
            val tmp = move *a;
            *a := move *b;
            *b := move tmp;
        }

        fun main() {
            val x = 1;
            val y = 2;
            swap(ref x, ref y);
            if (x == 2) {
                return y;
            }
            return 0;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 1);

  destroyast(ast);
}

TEST_CASE("compiler and vm should support refs to globals", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        val x = 1;

        fun set_it(target: i32 ref) {
            *target := 9;
        }

        fun main() {
            set_it(ref x);
            return x;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 9);

  destroyast(ast);
}

TEST_CASE("compiler and vm should support refs to object properties", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        type Box {
            property value;
        }

        fun main() {
            val box = new Box { value: 1 };
            val ptr = ref box.value;
            *ptr := 7;
            return box.value;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 7);

  destroyast(ast);
}

TEST_CASE("compiler and vm should invoke member functions through slot-backed call args", "[OrgasmTest][RefMove]")
{
  auto ast = parse(R"(
        type Counter {
            property value;

            fun bump(delta) {
                value := value + delta;
                return value;
            }
        }

        fun main() {
            val counter = new Counter { value: 10 };
            return counter.bump(5);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 15);

  destroyast(ast);
}

TEST_CASE("managed heap should sweep unreachable vm cycles", "[OrgasmTest][RefMove][GC]")
{
  NG::runtime::collect_managed_heap();
  REQUIRE(NG::runtime::managed_heap_size() == 0);

  auto ast = parse(R"(
        type Node {
            property link;
        }

        fun main() {
            val node = new Node { link: unit };
            node.link := node;
            return unit;
        }
    )");
  REQUIRE(ast != nullptr);

  {
    Compiler compiler;
    auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
    VM vm;
    auto result = vm.run(bytecode);
    REQUIRE(result != nullptr);
    REQUIRE(NG::runtime::managed_heap_size() == 1);
  }

  destroyast(ast);

  NG::runtime::collect_managed_heap();
  REQUIRE(NG::runtime::managed_heap_size() == 0);
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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 7);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle recursive tagged union refs", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Node = Cell(content: i32, _next: ref<Node>) | Empty;

        fun main() {
            val tail = Empty();
            val head = Cell(1, ref tail);

            switch (head) {
                case Cell(content, nextRef) {
                    switch (*nextRef) {
                        case Empty {
                            return content;
                        }
                        case Cell(other, rest) {
                            return 0;
                        }
                    }
                }
                case Empty {
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

  REQUIRE(result_i32(result) == 1);

  destroyast(ast);
}

TEST_CASE("compiler and vm should allocate tagged union variants on heap", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Node = Cell(content: i32, _next: ref<Node>) | Empty;

        fun main() {
            val head: ref<Node> = new Cell { content: 1, _next: new Empty {} };

            switch (*head) {
                case Empty {
                    return 0;
                }
                case Cell(content, nextRef) {
                    switch (*nextRef) {
                        case Empty {
                            return content;
                        }
                        case Cell(other, rest) {
                            return 0;
                        }
                    }
                }
            }
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 1);

  destroyast(ast);
}

TEST_CASE("compiler and vm should dispatch switch cases by declared variant tag", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Result = Err(msg: string) | Ok(value: i32);

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

  REQUIRE(result_i32(result) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should treat otherwise as switch default", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Result = Pending | Ok(value: i32) | Err(msg: string);

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

  REQUIRE(result_i32(result) == 7);

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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 42);

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

  REQUIRE(result_i32(result) == 42);

  destroyast(firstAst);
  destroyast(secondAst);
}

TEST_CASE("compiler and vm should call native prelude helpers", "[OrgasmTest][Prelude]")
{
  auto ast = parse(R"(
        fun main() {
            val content = trim("  hello,world  ");
            val parts = split(content, ",");
            val reversed = reverse(parts);
            val nums = range(1, 4);
            val mid = slice(nums, 1, 3);

            assert(len(parts) == 2);
            assert(parts[0] == "hello");
            assert(parts[1] == "world");
            assert(join(reversed, "-") == "world-hello");
            assert(contains(content, "lo,wo"));
            assert(replace(content, "world", "ng") == "hello,ng");
            assert(startsWith(content, "hello"));
            assert(endsWith(content, "world"));
            assert(toUpper("Ng") == "NG");
            assert(toLower("Ng") == "ng");
            assert(len(nums) == 3);
            assert(mid[0] == 2);
            assert(mid[1] == 3);
            return 42;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  NG::library::prelude::register_vm_natives(vm);
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 42);

  destroyast(ast);
}

TEST_CASE("compiler and vm should call native prelude file helpers", "[OrgasmTest][Prelude]")
{
  Map<Str, Str> files;

  auto ast = parse(R"(
        fun main() {
            writeFile("memory://ng-prelude-vm-test.txt", "hello from vm");
            return readFile("memory://ng-prelude-vm-test.txt");
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, {"readFile", "writeFile"}};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  vm.register_native_raw("writeFile", [&files](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
    files[runtime_string_value(args.at(0))] = runtime_string_value(args.at(1));
    return unit_cell();
  });
  vm.register_native_raw("readFile", [&files](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
    return make_runtime_string(files.at(runtime_string_value(args.at(0))));
  });
  auto result = vm.run(bytecode);

  REQUIRE(runtime_string_value(result) == "hello from vm");
  destroyast(ast);
}

TEST_CASE("compiler and vm should handle spread unpack property updates and member calls", "[OrgasmTest]")
{
  auto ast = parse(R"(
        type Box {
            value: i32;

            fun bump(delta: i32) -> i32 {
                self.value := self.value + delta;
                return self.value;
            }
        }

        fun main() {
            val tuple = (10, 20, 30);
            val (head, ...rest) = tuple;
            val arr = [1, rest[0], rest[1], 40];
            arr[0] := 5;

            val box = new Box { value: arr[0] };
            box.value := box.value + head;

            if (box is Box) {
                return box.bump(arr[1]);
            }
            return 0;
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 35);

  destroyast(ast);
}

TEST_CASE("compiler and vm should handle descending range and clamped slice", "[OrgasmTest][Prelude]")
{
  auto ast = parse(R"(
        fun main() {
            val down = range(3, 0);
            val window = slice(down, -5, 99);
            return window[0] + window[1] + window[2];
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  NG::library::prelude::register_vm_natives(vm);
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 6);

  destroyast(ast);
}

TEST_CASE("compiler and vm should surface native prelude argument errors", "[OrgasmTest][Prelude]")
{
  auto ast = parse(R"(
        fun main() {
            return reverse(42);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  NG::library::prelude::register_vm_natives(vm);

  REQUIRE_THROWS_AS(vm.run(bytecode), RuntimeException);

  destroyast(ast);
}

TEST_CASE("compiler and vm should trampoline deep self tail calls", "[OrgasmTest][Recursion]")
{
  auto ast = parse(R"(
        fun sum(i, n = 0) {
          if (i == 0) {
            return n;
          }
          return sum(i - 1, n + i);
        }

        fun main() {
          return sum(60000);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  VM vm;
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 1800030000);

  destroyast(ast);
}

TEST_CASE("vm should execute bytecode calls without consuming the C++ call stack", "[OrgasmTest][VM]")
{
  BytecodeModule module;
  module.name = "manual";

  Function main;
  main.name = "main";
  main.num_locals = 0;
  main.num_params = 0;
  main.code.push_back(static_cast<uint8_t>(OpCode::PUSH_I32));
  main.code.push_back(42);
  main.code.push_back(0);
  main.code.push_back(0);
  main.code.push_back(0);
  emit_u16_u16(main.code, OpCode::CALL, 1, 1);
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  Function identity;
  identity.name = "identity";
  identity.num_locals = 1;
  identity.num_params = 1;
  emit_u16(identity.code, OpCode::LOAD_PARAM, 0);
  identity.code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  module.functions.push_back(std::move(main));
  module.functions.push_back(std::move(identity));

  VM vm;
  auto result = vm.run(module);

  REQUIRE(result_i32(result) == 42);
}

TEST_CASE("vm should return unit when a nested bytecode frame reaches the end", "[OrgasmTest][VM]")
{
  BytecodeModule module;
  module.name = "manual-fallthrough";

  Function main;
  main.name = "main";
  main.num_locals = 0;
  main.num_params = 0;
  emit_u16_u16(main.code, OpCode::CALL, 1, 0);
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  Function callee;
  callee.name = "fallthrough";
  callee.num_locals = 0;
  callee.num_params = 0;

  module.functions.push_back(std::move(main));
  module.functions.push_back(std::move(callee));

  VM vm;
  auto result = vm.run(module);

  REQUIRE(runtime_value_show(result) == "unit");
}

TEST_CASE("vm should dispatch imported symbols to registered native fallback", "[OrgasmTest][VM]")
{
  auto &registry = NG::module::get_module_registry();
  registry.clear();
  NG::module::clear_module_loader_cache();

  auto moduleInfo = std::make_shared<NG::module::ModuleInfo>();
  moduleInfo->moduleId = "native_mod";
  moduleInfo->moduleName = "native_mod";
  registry.addModuleInfo(moduleInfo);

  BytecodeModule module;
  module.name = "native-import";
  module.imports.push_back(ExternalSymbol{.moduleName = "native_mod", .symbolName = "native_inc"});

  Function main;
  main.name = "main";
  main.num_locals = 0;
  main.num_params = 0;
  main.code.push_back(static_cast<uint8_t>(OpCode::PUSH_I32));
  main.code.push_back(41);
  main.code.push_back(0);
  main.code.push_back(0);
  main.code.push_back(0);
  emit_u16_u16(main.code, OpCode::CALL_IMPORT, 0, 1);
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));
  module.functions.push_back(std::move(main));

  VM vm;
  vm.register_native("native_inc", [](int32_t value) { return value + 1; });
  auto result = vm.run(module);

  REQUIRE(result_i32(result) == 42);

  registry.clear();
  NG::module::clear_module_loader_cache();
}

TEST_CASE("compiler and vm should register imgui natives without initialized state", "[OrgasmTest][ImGui]")
{
  auto names = NG::library::imgui::native_function_names();
  REQUIRE(std::find(names.begin(), names.end(), "init") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "cleanup") != names.end());

  VM vm;
  REQUIRE_NOTHROW(NG::library::imgui::register_vm_natives(vm));
}

TEST_CASE("compiler and vm should reject imgui native calls before init", "[OrgasmTest][ImGui]")
{
  for (const auto &name : NG::library::imgui::native_function_names())
  {
    if (name == "init")
    {
      continue;
    }
    BytecodeModule module;
    module.name = "imgui-native-check";
    module.strings.push_back(name);

    Function main;
    main.name = "main";
    main.num_locals = 0;
    main.num_params = 0;
    emit_u16_u16(main.code, OpCode::NATIVE_CALL, 0, 0);
    main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));
    module.functions.push_back(std::move(main));

    VM vm;
    NG::library::imgui::register_vm_natives(vm);
    REQUIRE_THROWS_AS(vm.run(module), RuntimeException);
  }
}
