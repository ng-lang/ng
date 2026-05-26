#include "../test.hpp"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
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

struct SourceModuleFixture
{
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("ng_orgasm_module_artifact_" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  SourceModuleFixture()
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
    std::filesystem::create_directories(root);
  }

  ~SourceModuleFixture()
  {
    NG::module::clear_module_loader_cache();
    NG::module::get_module_registry().clear();
    std::filesystem::remove_all(root);
  }

  void write(const std::filesystem::path &relative, const Str &source) const
  {
    auto target = root / relative;
    std::filesystem::create_directories(target.parent_path());
    std::ofstream out{target};
    REQUIRE(out.good());
    out << source;
  }

  auto paths() const -> Vec<Str>
  {
    return {root.string()};
  }
};
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

TEST_CASE("compiler and vm should call imported source module through canonical id",
          "[OrgasmTest][ModuleArtifact]")
{
  SourceModuleFixture fixture;
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> i32 {
      return 41;
    }
  )");
  auto ast = parse(R"(
    import pkg.math (*);
    fun main() -> i32 {
      return answer() + 1;
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast, {}, fixture.paths());

  Compiler compiler{fixture.paths()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE(bytecode.imports.size() == 1);
  REQUIRE(bytecode.imports.front().moduleName == "pkg.math");

  VM vm{fixture.paths()};
  auto result = vm.run(bytecode);
  REQUIRE(result_i32(result) == 42);

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

TEST_CASE("compiler should use bare type names for generic impl methods", "[OrgasmTest][Traits]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: i32;
    }

    trait Value {
      fun get(self: ref<Self>) -> i32;
    }

    impl<T> Value for Box<T> {
      fun get(self: ref<Self>) -> i32 {
        return self.value;
      }
    }
  )");
  REQUIRE(ast != nullptr);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  REQUIRE(std::ranges::any_of(bytecode.functions,
                              [](const Function &function) { return function.name == "Box.Value::get"; }));
  REQUIRE_FALSE(std::ranges::any_of(bytecode.functions,
                                    [](const Function &function) { return function.name == "Box<T>.Value::get"; }));

  destroyast(ast);
}

TEST_CASE("compiler should register inherited defaults under implemented trait names", "[OrgasmTest][Traits]")
{
  auto ast = parse(R"(
    type Box {}

    trait Parent {
      fun label(self: ref<Self>) -> string {
        return "parent";
      }
    }

    trait Child: Parent {}

    impl Child for Box {}
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  REQUIRE(std::ranges::any_of(bytecode.functions,
                              [](const Function &function) { return function.name == "Box.Parent::label"; }));
  REQUIRE(std::ranges::any_of(bytecode.functions,
                              [](const Function &function) { return function.name == "Box.Child::label"; }));
  REQUIRE(std::ranges::any_of(bytecode.functions,
                              [](const Function &function) { return function.name == "Box.label"; }));

  destroyast(ast);
}

TEST_CASE("compiler should qualify calls inside inherited trait default bodies", "[OrgasmTest][Traits]")
{
  auto ast = parse(R"(
    type Box {}

    trait Parent {
      fun prefix(self: ref<Self>) -> string {
        return "parent";
      }

      fun labelViaSelf(self: ref<Self>) -> string {
        return self.prefix();
      }
    }

    trait Child: Parent {}

    impl Child for Box {}
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  REQUIRE(std::ranges::find(bytecode.strings, "prefix") != bytecode.strings.end());

  destroyast(ast);
}

TEST_CASE("compiler should emit supertrait aliases for provided impl methods", "[OrgasmTest][Traits]")
{
  auto ast = parse(R"(
    type Counter {
      value: i32;
    }

    trait Eq {
      fun same(self: ref<Self>, other: ref<Self>) -> bool;

      fun not_same(self: ref<Self>, other: ref<Self>) -> bool {
        return !self.same(other);
      }
    }

    trait Ord: Eq {}

    impl Ord for Counter {
      fun same(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value == other.value;
      }
    }

    fun main() -> bool {
      val one = new Counter { value: 1 };
      val two = new Counter { value: 2 };
      return one.not_same(two);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  auto hasFunction = [&bytecode](const Str &name) {
    return std::ranges::any_of(bytecode.functions, [&name](const Function &function) { return function.name == name; });
  };

  REQUIRE(hasFunction("Counter.Eq::same"));
  REQUIRE(hasFunction("Counter.Ord::same"));
  REQUIRE(hasFunction("Counter.same"));
  REQUIRE(hasFunction("Counter.Eq::not_same"));
  REQUIRE(hasFunction("Counter.Ord::not_same"));

  VM vm;
  auto result = vm.run(bytecode);
  REQUIRE(runtime_value_bool(result));

  destroyast(ast);
}

TEST_CASE("compiler should emit one trait ref wrapper for direct trait-ref parameters", "[OrgasmTest][Traits]")
{
  auto ast = parse(R"(
    type Box {}

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Box {
      fun show(self: ref<Self>) -> string {
        return "box";
      }
    }

    fun take(value: ref<Show>) -> string {
      return value.show();
    }

    fun main() -> string {
      val box = new Box {};
      return take(box);
    }
  )");
  REQUIRE(ast != nullptr);

  NG::typecheck::type_check(ast);

  Compiler compiler;
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));

  auto mainIt = std::ranges::find_if(bytecode.functions, [](const Function &function) { return function.name == "main"; });
  REQUIRE(mainIt != bytecode.functions.end());
  auto makeTraitRefCount = static_cast<size_t>(
      std::ranges::count(mainIt->code, static_cast<uint8_t>(OpCode::MAKE_TRAIT_REF)));
  REQUIRE(makeTraitRefCount == 1);

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

            fun bump(self: ref<Self>, delta) {
                self.value := self.value + delta;
                return self.value;
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

TEST_CASE("compiler and vm should fold const if from prelude is_ref predicate", "[const_if][OrgasmTest][Generics]")
{
  auto ast = parse(R"(
        fun value_kind<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 100;
          } else {
            return 0;
          }
        }

        fun ref_kind<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 10;
          } else {
            return 1;
          }
        }

        fun main() -> i32 {
          val value = 1;
          return value_kind(value) + ref_kind(ref value);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE(std::count_if(bytecode.functions.begin(), bytecode.functions.end(),
                        [](const Function &function) { return function.name.starts_with("$NG"); }) == 2);

  VM vm;
  NG::library::prelude::register_vm_natives(vm);
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 10);

  destroyast(ast);
}

TEST_CASE("compiler and vm should keep const if decisions per generic instance",
          "[const_if][OrgasmTest][Generics]")
{
  auto ast = parse(R"(
        fun classify<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 1;
          } else {
            return 0;
          }
        }

        fun main() -> i32 {
          val value = 1;
          return classify(value) + classify(ref value);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE(std::none_of(bytecode.functions.begin(), bytecode.functions.end(),
                       [](const Function &function) { return function.name == "classify"; }));
  REQUIRE(std::count_if(bytecode.functions.begin(), bytecode.functions.end(),
                        [](const Function &function) { return function.name.starts_with("$NG"); }) == 2);

  VM vm;
  NG::library::prelude::register_vm_natives(vm);
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 1);

  destroyast(ast);
}

TEST_CASE("compiler and vm should collect nested generic function instances",
          "[const_if][OrgasmTest][Generics]")
{
  auto ast = parse(R"(
        fun inner<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 2;
          } else {
            return 3;
          }
        }

        fun outer<T>(value: T) -> i32 {
          return inner(value);
        }

        fun main() -> i32 {
          val value = 1;
          return outer(value) + outer(ref value);
        }
    )");
  REQUIRE(ast != nullptr);

  Compiler compiler{{}, NG::library::prelude::native_function_names()};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE(std::none_of(bytecode.functions.begin(), bytecode.functions.end(),
                       [](const Function &function) { return function.name == "inner" || function.name == "outer"; }));
  REQUIRE(std::count_if(bytecode.functions.begin(), bytecode.functions.end(),
                        [](const Function &function) { return function.name.starts_with("$NG"); }) == 4);

  VM vm;
  NG::library::prelude::register_vm_natives(vm);
  auto result = vm.run(bytecode);

  REQUIRE(result_i32(result) == 5);

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
            assert(regexMatch("fun main", "fun"));
            assert(regexMatch("fun main", "^type") == false);
            assert(len(currentExecutablePath()) > 0);
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

            fun bump(self: ref<Self>, delta: i32) -> i32 {
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

TEST_CASE("vm should lazy load imported bytecode modules from ngo artifacts", "[OrgasmTest][VM][Ngo]")
{
  SourceModuleFixture fixture;

  BytecodeModule imported;
  imported.name = "pkg.math";
  Function answer;
  answer.name = "answer";
  answer.num_locals = 0;
  answer.num_params = 0;
  answer.code.push_back(static_cast<uint8_t>(OpCode::PUSH_I32));
  answer.code.push_back(42);
  answer.code.push_back(0);
  answer.code.push_back(0);
  answer.code.push_back(0);
  answer.code.push_back(static_cast<uint8_t>(OpCode::RETURN));
  imported.exports["answer"] = 0;
  imported.functions.push_back(std::move(answer));

  auto artifactPath = fixture.root / "pkg" / "math.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(imported, artifactPath.string());

  BytecodeModule entry;
  entry.name = "entry";
  entry.imports.push_back(ExternalSymbol{.moduleName = "pkg.math", .symbolName = "answer"});
  Function main;
  main.name = "main";
  main.num_locals = 0;
  main.num_params = 0;
  emit_u16_u16(main.code, OpCode::CALL_IMPORT, 0, 0);
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));
  entry.functions.push_back(std::move(main));

  VM vm{fixture.paths()};
  auto result = vm.run(entry);
  REQUIRE(result_i32(result) == 42);
}

TEST_CASE("typechecker should import exported function metadata from ngo artifacts",
          "[OrgasmTest][ModuleArtifact][Ngo][TypeCheck]")
{
  SourceModuleFixture fixture;

  auto importedAst = parse(R"(
    module pkg.math exports *;
    fun answer() -> i32 {
      return 42;
    }
  )", "pkg/math.ng");
  REQUIRE(importedAst != nullptr);
  NG::typecheck::type_check(importedAst, {}, fixture.paths());
  Compiler compiler{fixture.paths()};
  auto importedBytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(importedAst));
  auto artifactPath = fixture.root / "pkg" / "math.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(importedBytecode, artifactPath.string());
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> bool {
      return false;
    }
  )");

  NG::module::clear_module_loader_cache();
  NG::module::get_module_registry().clear();

  auto entryAst = parse(R"(
    import pkg.math (*);
    val result: i32 = answer();
  )");
  REQUIRE(entryAst != nullptr);
  auto index = NG::typecheck::type_check(entryAst, {}, fixture.paths());
  REQUIRE(index.contains("result"));
  REQUIRE(index["result"]->tag() == NG::typecheck::typeinfo_tag::I32);

  destroyast(importedAst);
  destroyast(entryAst);
}

TEST_CASE("module loader should fall back to source when ngo source hash is stale",
          "[OrgasmTest][ModuleArtifact][Ngo][TypeCheck]")
{
  SourceModuleFixture fixture;

  BytecodeModule staleBytecode;
  staleBytecode.name = "pkg.math";
  staleBytecode.exports["answer"] = 0;
  staleBytecode.exportTypeReprs["answer"] = "fun () -> i32";
  Function staleAnswer;
  staleAnswer.name = "answer";
  staleAnswer.code = {static_cast<uint8_t>(OpCode::PUSH_I32), 1, 0, 0, 0, static_cast<uint8_t>(OpCode::RETURN)};
  staleBytecode.functions.push_back(std::move(staleAnswer));
  auto artifactPath = fixture.root / "pkg" / "math.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(staleBytecode, artifactPath.string(), "stale-source-hash");
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> bool {
      return true;
    }
  )");

  auto entryAst = parse(R"(
    import pkg.math (*);
    val result: bool = answer();
  )");
  REQUIRE(entryAst != nullptr);
  auto index = NG::typecheck::type_check(entryAst, {}, fixture.paths());
  REQUIRE(index.contains("result"));
  REQUIRE(index["result"]->tag() == NG::typecheck::typeinfo_tag::BOOL);

  destroyast(entryAst);
}

TEST_CASE("typechecker should import exported trait metadata from ngo artifacts",
          "[OrgasmTest][ModuleArtifact][Ngo][TypeCheck][Traits]")
{
  SourceModuleFixture fixture;

  auto importedAst = parse(R"(
    module pkg.show exports *;

    type Counter {
      label: string;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }

    fun make() -> ref<Counter> {
      return new Counter { label: "ngo" };
    }
  )", "pkg/show.ng");
  REQUIRE(importedAst != nullptr);
  NG::typecheck::type_check(importedAst, {}, fixture.paths());
  Compiler compiler{fixture.paths()};
  auto importedBytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(importedAst));
  auto artifactPath = fixture.root / "pkg" / "show.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(importedBytecode, artifactPath.string());

  NG::module::clear_module_loader_cache();
  NG::module::get_module_registry().clear();

  auto entryAst = parse(R"(
    import pkg.show (*);

    val counter = make();
    val text = counter.show();
  )");
  REQUIRE(entryAst != nullptr);
  auto index = NG::typecheck::type_check(entryAst, {}, fixture.paths());
  REQUIRE(index.contains("text"));
  REQUIRE(index["text"]->tag() == NG::typecheck::typeinfo_tag::STRING);

  destroyast(importedAst);
  destroyast(entryAst);
}

TEST_CASE("module loader should fall back to source when ngo export metadata is incomplete",
          "[OrgasmTest][ModuleArtifact][Ngo][TypeCheck]")
{
  SourceModuleFixture fixture;

  BytecodeModule incompleteBytecode;
  incompleteBytecode.name = "pkg.math";
  incompleteBytecode.exports["answer"] = 0;
  Function answer;
  answer.name = "answer";
  answer.code = {static_cast<uint8_t>(OpCode::PUSH_I32), 1, 0, 0, 0, static_cast<uint8_t>(OpCode::RETURN)};
  incompleteBytecode.functions.push_back(std::move(answer));
  auto artifactPath = fixture.root / "pkg" / "math.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(incompleteBytecode, artifactPath.string());
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> bool {
      return true;
    }
  )");

  auto entryAst = parse(R"(
    import pkg.math (*);
    val result: bool = answer();
  )");
  REQUIRE(entryAst != nullptr);
  auto index = NG::typecheck::type_check(entryAst, {}, fixture.paths());
  REQUIRE(index.contains("result"));
  REQUIRE(index["result"]->tag() == NG::typecheck::typeinfo_tag::BOOL);

  destroyast(entryAst);
}

TEST_CASE("module loader should fall back to source when ngo is incompatible",
          "[OrgasmTest][ModuleArtifact][Ngo][TypeCheck]")
{
  SourceModuleFixture fixture;

  BytecodeModule wrongModule;
  wrongModule.name = "pkg.other";
  auto artifactPath = fixture.root / "pkg" / "math.ngo";
  std::filesystem::create_directories(artifactPath.parent_path());
  write_bytecode_module(wrongModule, artifactPath.string());
  fixture.write("pkg/math.ng", R"(
    module pkg.math exports *;
    fun answer() -> bool {
      return true;
    }
  )");

  auto entryAst = parse(R"(
    import pkg.math (*);
    val result = answer();
  )");
  REQUIRE(entryAst != nullptr);
  auto index = NG::typecheck::type_check(entryAst, {}, fixture.paths());
  REQUIRE(index.contains("result"));
  REQUIRE(index["result"]->tag() == NG::typecheck::typeinfo_tag::BOOL);

  destroyast(entryAst);
}

TEST_CASE("vm should retag existing trait refs for destination trait coercions", "[OrgasmTest][VM][Traits]")
{
  BytecodeModule module;
  module.name = "trait-retag";
  module.strings = {"Ord", "Eq", "check_trait"};

  Function main;
  main.name = "main";
  main.num_locals = 1;
  main.num_params = 0;
  main.code.push_back(static_cast<uint8_t>(OpCode::PUSH_I32));
  main.code.push_back(7);
  main.code.push_back(0);
  main.code.push_back(0);
  main.code.push_back(0);
  emit_u16(main.code, OpCode::STORE_LOCAL, 0);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));
  emit_u16(main.code, OpCode::MAKE_LOCAL_REF, 0);
  emit_u16(main.code, OpCode::MAKE_TRAIT_REF, 0);
  emit_u16(main.code, OpCode::MAKE_TRAIT_REF, 1);
  emit_u16_u16(main.code, OpCode::NATIVE_CALL, 2, 1);
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));
  module.functions.push_back(std::move(main));

  bool sawEq = false;
  VM vm;
  vm.register_native_raw("check_trait", [&sawEq](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
    sawEq = args.size() == 1 && runtime_is_trait_object_ref(args[0]) && runtime_trait_object_name(args[0]) == "Eq";
    return unit_cell();
  });

  auto result = vm.run(module);

  REQUIRE(sawEq);
  REQUIRE(runtime_value_show(result) == "unit");
}

TEST_CASE("vm should run drop hooks when overwriting object properties", "[OrgasmTest][VM][Drop]")
{
  BytecodeModule module;
  module.name = "drop-property";
  module.strings = {"Droppable", "Holder", "Dynamic", "extra", "record_drop", "make_drop"};
  module.types.push_back(Type{.name = "Droppable"});
  module.types.push_back(Type{.name = "Holder", .properties = {"item"}});
  module.types.push_back(Type{.name = "Dynamic"});

  Function main;
  main.name = "main";
  main.num_locals = 3;
  main.num_params = 0;

  emit_u16_u16(main.code, OpCode::NATIVE_CALL, 5, 0);
  emit_u16_u16(main.code, OpCode::NEW_OBJECT, 1, 1);
  emit_u16(main.code, OpCode::STORE_LOCAL, 0);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));
  emit_u16(main.code, OpCode::LOAD_LOCAL, 0);
  emit_u16_u16(main.code, OpCode::NATIVE_CALL, 5, 0);
  emit_u16(main.code, OpCode::SET_PROPERTY, 0);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));

  emit_u16_u16(main.code, OpCode::NEW_OBJECT, 2, 0);
  emit_u16(main.code, OpCode::STORE_LOCAL, 1);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));
  emit_u16(main.code, OpCode::LOAD_LOCAL, 1);
  emit_u16_u16(main.code, OpCode::NATIVE_CALL, 5, 0);
  emit_u16(main.code, OpCode::SET_PROPERTY_STR, 3);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));
  emit_u16(main.code, OpCode::LOAD_LOCAL, 1);
  emit_u16_u16(main.code, OpCode::NATIVE_CALL, 5, 0);
  emit_u16(main.code, OpCode::SET_PROPERTY_STR, 3);
  main.code.push_back(static_cast<uint8_t>(OpCode::POP));
  main.code.push_back(static_cast<uint8_t>(OpCode::PUSH_UNIT));
  main.code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  Function drop;
  drop.name = "Droppable.Drop::drop";
  drop.num_locals = 1;
  drop.num_params = 1;
  emit_u16_u16(drop.code, OpCode::NATIVE_CALL, 4, 0);
  drop.code.push_back(static_cast<uint8_t>(OpCode::RETURN));

  module.functions.push_back(std::move(main));
  module.functions.push_back(std::move(drop));

  int dropCount = 0;
  auto droppableType = makert<NGType>();
  droppableType->name = "Droppable";
  droppableType->layout = TypeLayout{.name = "Droppable"};

  VM vm;
  vm.register_native_raw("make_drop", [droppableType](const Vec<RuntimeRef<StorageCell>> &) -> RuntimeRef<StorageCell> {
    return make_runtime_structural_cell(droppableType, {});
  });
  vm.register_native_raw("record_drop", [&dropCount](const Vec<RuntimeRef<StorageCell>> &) -> RuntimeRef<StorageCell> {
    ++dropCount;
    return unit_cell();
  });

  auto result = vm.run(module);

  REQUIRE(runtime_value_show(result) == "unit");
  REQUIRE(dropCount >= 2);
}

TEST_CASE("compiler and vm should register imgui natives without initialized state", "[OrgasmTest][ImGui]")
{
  auto names = NG::library::imgui::native_function_names();
  REQUIRE(std::find(names.begin(), names.end(), "init") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "cleanup") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "GetIO") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "GetStyle") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "Button") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "InputText") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "InputTextMultiline") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "TextNgHighlighted") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "BeginTable") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "PushStyleColor") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "GetWindowWidth") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "SetNextWindowBgAlpha") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "ImGuiConfigFlags_NavEnableKeyboard") != names.end());
  REQUIRE(std::find(names.begin(), names.end(), "ImGuiTableFlags_Borders") != names.end());

  VM vm;
  REQUIRE_NOTHROW(NG::library::imgui::register_vm_natives(vm));
}

TEST_CASE("typechecker should expose typed imgui native wrapper API", "[OrgasmTest][ImGui][TypeCheck]")
{
  auto ast = parse(R"(
    import std.imgui (*);

    val imguiVersion: string = GetVersion();
    val flags: i32 = ImGuiConfigFlags_NavEnableKeyboard();

    val io = GetIO();
    val style = GetStyle();
    ImGuiIO_AddConfigFlags(io, flags);
    ImGuiStyle_SetWindowRounding(style, 4.0f32);
    val input: string = InputText("name", "ng", ImGuiInputTextFlags_EnterReturnsTrue());
    val source: string = InputTextMultiline("source", input, 320.0f32, 200.0f32, ImGuiInputTextFlags_None());
    TextNgHighlighted(source);
    val selected: bool = Selectable("row", false, ImGuiSelectableFlags_SpanAllColumns(), 0.0f32, 0.0f32);
    val dragged: f32 = DragFloat("drag", 0.5f32, 0.1f32, 0.0f32, 1.0f32);
    val tableOpen: bool = BeginTable("table", 2, ImGuiTableFlags_Borders(), 0.0f32, 0.0f32, 0.0f32);
    TableSetupColumn("name", ImGuiTableColumnFlags_WidthStretch(), 0.0f32);
    TableNextRow(ImGuiTableFlags_None(), 0.0f32);
    val columnVisible: bool = TableNextColumn();
    val columns: i32 = TableGetColumnCount();
    PushStyleColor(ImGuiCol_Button(), 0.1f32, 0.2f32, 0.3f32, 1.0f32);
    PopStyleColor(1);
    PushStyleVarVec2(ImGuiStyleVar_FramePadding(), 6.0f32, 4.0f32);
    PopStyleVar(1);
    SetNextWindowBgAlpha(0.9f32);
    val windowWidth: f32 = GetWindowWidth();
    val scrollY: f32 = GetScrollY();
    TextColored(1.0f32, 0.5f32, 0.2f32, 1.0f32, "colored");
    val edited: bool = IsItemEdited();
    val anyActive: bool = IsAnyItemActive();
  )");
  REQUIRE(ast != nullptr);

  auto index = NG::typecheck::type_check(ast, {}, {NG::module::standard_library_base_path()});
  REQUIRE(index.contains("io"));
  REQUIRE(index["io"]->repr() == "ImGuiIO");
  REQUIRE(index.contains("style"));
  REQUIRE(index["style"]->repr() == "ImGuiStyle");

  destroyast(ast);
}

TEST_CASE("compiler should typecheck and compile ng ide imgui example without running it",
          "[OrgasmTest][ImGui][Example]")
{
  auto target = std::filesystem::path{"example/ng_ide.ng"};
  auto projectRoot = std::filesystem::current_path();
  if (!std::filesystem::exists(projectRoot / target))
  {
    projectRoot = projectRoot.parent_path();
  }

  std::ifstream file{projectRoot / target};
  REQUIRE(file.good());
  std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
  auto ast = parse(source, (projectRoot / target).string());
  REQUIRE(ast != nullptr);

  Vec<Str> modulePaths{(projectRoot / "lib").string(), (projectRoot / "example").string()};
  REQUIRE_NOTHROW(NG::typecheck::type_check(ast, NG::typecheck::build_prelude_type_index(), modulePaths));

  auto nativeNames = NG::library::prelude::native_function_names();
  auto imguiNativeNames = NG::library::imgui::native_function_names();
  nativeNames.insert(nativeNames.end(), imguiNativeNames.begin(), imguiNativeNames.end());

  Compiler compiler{modulePaths, nativeNames};
  auto bytecode = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE_FALSE(bytecode.functions.empty());
  REQUIRE(std::ranges::any_of(bytecode.functions, [](const Function &function) {
    return std::ranges::find(function.code, static_cast<uint8_t>(OpCode::NATIVE_CALL)) != function.code.end();
  }));

  destroyast(ast);
}

TEST_CASE("compiler should emit direct native calls for imported imgui functions", "[OrgasmTest][ImGui]")
{
  auto ast = parse(R"(
    import std.imgui (*);

    init();
    NewFrame();
    Render();
  )");
  REQUIRE(ast != nullptr);

  auto nativeNames = NG::library::prelude::native_function_names();
  auto imguiNativeNames = NG::library::imgui::native_function_names();
  nativeNames.insert(nativeNames.end(), imguiNativeNames.begin(), imguiNativeNames.end());
  Compiler compiler{{}, nativeNames};
  auto module = compiler.compile(dynamic_ast_cast<CompileUnit>(ast));
  REQUIRE_FALSE(module.functions.empty());

  auto &code = module.functions.front().code;
  auto countNativeCalls = std::ranges::count(code, static_cast<uint8_t>(OpCode::NATIVE_CALL));
  auto countImportCalls = std::ranges::count(code, static_cast<uint8_t>(OpCode::CALL_IMPORT));
  REQUIRE(countNativeCalls >= 3);
  REQUIRE(countImportCalls == 0);

  destroyast(ast);
}

TEST_CASE("compiler and vm should reject imgui native calls before init", "[OrgasmTest][ImGui]")
{
  for (const auto &name : Vec<Str>{
           "NewFrame",
           "Begin",
           "Button",
           "GetIO",
           "GetStyle",
           "StyleColorsDark",
           "SetNextWindowSize",
           "Render",
           "cleanup",
       })
  {
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
