#include "../test.hpp"
#include <compiler/ngcompiler.hpp>
#include <typecheck/typecheck.hpp>
#include <orgasm/interpreter.hpp>

using namespace NG;
using namespace NG::compiler;
using namespace ng::orgasm;

TEST_CASE("NGCompiler should compile simple function", "[compiler]") {
    const char* source = R"(
        fun id(n) {
            return n;
        }
    )";
    
    // Parse the source
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    // Type check (may not complete fully without full type inference, but we can still compile)
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {
        // Continue with empty type index
    }
    
    // Compile
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->name == "default");
    REQUIRE(module->functions.size() > 0);
    REQUIRE(module->functions[0].name == "id");
}

TEST_CASE("NGCompiler should compile function with constants", "[compiler]") {
    const char* source = R"(
        fun add(a, b) {
            return a + b;
        }
    )";
    
    // Parse the source
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    // Type check (may not complete fully without full type inference, but we can still compile)
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {
        // Continue with empty type index
    }
    
    // Compile
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() > 0);
    REQUIRE(module->functions[0].name == "add");
    REQUIRE(module->functions[0].params.size() == 2);
}

TEST_CASE("NGCompiler should compile val definitions", "[compiler]") {
    const char* source = R"(
        val x = 42;
    )";
    
    // Parse the source
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    // Type check
    auto typeIndex = NG::typecheck::type_check(ast);
    
    // Compile
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() > 0);
    REQUIRE(module->constants.size() > 0);
}

TEST_CASE("NGCompiler should handle module structure", "[compiler]") {
    const char* source = R"(
        fun twice(x) {
            return x + x;
        }
        
        val result = twice(21);
    )";
    
    // Parse the source
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    // Type check (may not complete fully without full type inference, but we can still compile)
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {
        // Continue with empty type index
    }
    
    // Compile
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->name == "default");
    REQUIRE(module->functions.size() > 0);
    REQUIRE(module->variables.size() > 0);
}

// Integration tests: compile NG to ORGASM and execute
TEST_CASE("NGCompiler integration: execute compiled identity function", "[compiler][integration]") {
    const char* source = R"(
        fun id(n) {
            return n;
        }
        
        print(id(42));
    )";
    
    // Parse and compile
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    // Execute with interpreter
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    bool print_called = false;
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        print_called = true;
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_called);
    REQUIRE(print_value == 42);
}

TEST_CASE("NGCompiler integration: execute arithmetic operations", "[compiler][integration]") {
    const char* source = R"(
        val result = 42;
        print(result);
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 42);
}

TEST_CASE("NGCompiler integration: if statement execution", "[compiler][integration]") {
    const char* source = R"(
        fun max(a, b) {
            if (a > b) {
                return a;
            }
            return b;
        }
        
        print(max(15, 25));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 25);
}

TEST_CASE("NGCompiler: compile binary expressions", "[compiler]") {
    const char* source = R"(
        val sum = 5 + 3;
        val diff = 10 - 4;
        val prod = 6 * 7;
        val quot = 20 / 4;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() == 4);
    if (module->start_block) {
        REQUIRE(module->start_block->instructions.size() > 0);
    }
}

TEST_CASE("NGCompiler: compile comparison operations", "[compiler]") {
    const char* source = R"(
        val gt = 5 > 3;
        val lt = 2 < 4;
        val eq = 7 == 7;
        val ne = 5 != 3;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() == 4);
}

TEST_CASE("NGCompiler: compile string literals", "[compiler]") {
    const char* source = R"(
        val msg = "hello";
        print(msg);
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->strings.size() == 1);
    REQUIRE(module->strings[0].value == "hello");
}

TEST_CASE("NGCompiler: compile boolean literals", "[compiler]") {
    const char* source = R"(
        val t = true;
        val f = false;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->constants.size() >= 2); // At least true and false constants
}

TEST_CASE("NGCompiler: compile nested function calls", "[compiler]") {
    const char* source = R"(
        fun inc(x) {
            return x + 1;
        }
        
        fun dec(x) {
            return x - 1;
        }
        
        val result = inc(dec(10));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 2);
    REQUIRE(module->variables.size() == 1);
}

TEST_CASE("NGCompiler integration: nested function calls execution", "[compiler][integration]") {
    const char* source = R"(
        fun inc(x) {
            return x + 1;
        }
        
        fun dec(x) {
            return x - 1;
        }
        
        print(inc(dec(10)));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 10); // dec(10) = 9, inc(9) = 10
}

TEST_CASE("NGCompiler: compile unary expressions", "[compiler]") {
    const char* source = R"(
        val neg = -5;
        val notb = !true;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() == 2);
}

TEST_CASE("NGCompiler: compile multiple variables", "[compiler]") {
    const char* source = R"(
        val a = 1;
        val b = 2;
        val c = 3;
        val d = a + b + c;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() == 4);
    REQUIRE(module->constants.size() >= 3);
}

TEST_CASE("NGCompiler integration: comparison and branching", "[compiler][integration]") {
    const char* source = R"(
        fun check(x) {
            if (x == 5) {
                return 100;
            }
            return 200;
        }
        
        print(check(5));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 100);
}

// Additional tests for better coverage

TEST_CASE("NGCompiler: compile assignment expression", "[compiler]") {
    const char* source = R"(
        fun assign_test() {
            val x = 5;
            val y = x + 5;
            return y;
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}

TEST_CASE("NGCompiler: compile loop statement", "[compiler]") {
    const char* source = R"(
        fun loop_test() {
            val i = 0;
            loop {
                val j = i + 1;
                if (j > 10) {
                    next;
                }
            }
            return i;
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}

TEST_CASE("NGCompiler: compile if-else statement", "[compiler]") {
    const char* source = R"(
        fun if_else_test(x) {
            if (x < 0) {
                return -1;
            } else {
                return 1;
            }
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}

TEST_CASE("NGCompiler: compile compound statement", "[compiler]") {
    const char* source = R"(
        fun compound_test() {
            val a = 1;
            val b = 2;
            val c = 3;
            return a + b + c;
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}

TEST_CASE("NGCompiler: compile GE and LE operators", "[compiler]") {
    const char* source = R"(
        val ge = 10 >= 5;
        val le = 3 <= 7;
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->variables.size() == 2);
}

TEST_CASE("NGCompiler integration: subtraction and multiplication", "[compiler][integration]") {
    const char* source = R"(
        fun calc(x) {
            return (x - 5) * 2;
        }
        
        print(calc(10));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 10); // (10 - 5) * 2 = 10
}

TEST_CASE("NGCompiler integration: division operation", "[compiler][integration]") {
    const char* source = R"(
        fun divide(x, y) {
            return x / y;
        }
        
        print(divide(20, 4));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 5);
}

TEST_CASE("NGCompiler: compile explicit import", "[compiler]") {
    const char* source = R"(
        import external (*);
        
        fun use_external() {
            return external();
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->imports.size() >= 1);
}

TEST_CASE("NGCompiler integration: less than comparison", "[compiler][integration]") {
    const char* source = R"(
        fun is_less(a, b) {
            if (a < b) {
                return 1;
            }
            return 0;
        }
        
        print(is_less(3, 7));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 1);
}

TEST_CASE("NGCompiler integration: not equal comparison", "[compiler][integration]") {
    const char* source = R"(
        fun not_equal(a, b) {
            if (a != b) {
                return 1;
            }
            return 0;
        }
        
        print(not_equal(5, 3));
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    
    auto shared_module = std::shared_ptr<ng::orgasm::Module>(std::move(module));
    Interpreter interp(shared_module);
    
    int print_value = 0;
    interp.register_import("print", [&](const std::vector<ng::orgasm::Value> &args) -> ng::orgasm::Value {
        if (!args.empty() && args[0].type == ng::orgasm::PrimitiveType::I32) {
            print_value = std::get<int32_t>(args[0].data);
        }
        return ng::orgasm::Value();
    });
    
    interp.execute();
    
    REQUIRE(print_value == 1);
}

TEST_CASE("NGCompiler integration: GE comparison compile only", "[compiler]") {
    const char* source = R"(
        fun ge_test(a, b) {
            if (a >= b) {
                return 1;
            }
            return 0;
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}

TEST_CASE("NGCompiler integration: LE comparison compile only", "[compiler]") {
    const char* source = R"(
        fun le_test(a, b) {
            if (a <= b) {
                return 1;
            }
            return 0;
        }
    )";
    
    auto ast = parse(source);
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    REQUIRE(module != nullptr);
    REQUIRE(module->functions.size() == 1);
}
