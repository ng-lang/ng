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

TEST_CASE("NGCompiler: compile function with local variables", "[compiler]") {
    const char* source = R"(
        fun complex_calc(x, y, z) {
            val a = x + y;
            val b = a * z;
            val c = b - x;
            return c;
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
    REQUIRE(module->functions[0].params.size() == 3);
}

TEST_CASE("NGCompiler integration: function with local variables execution", "[compiler][integration]") {
    const char* source = R"(
        fun calc(x, y) {
            val tmp = x * 2;
            val res = tmp + y;
            return res;
        }
        
        print(calc(5, 3));
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
    
    REQUIRE(print_value == 13); // 5*2 + 3 = 13
}

TEST_CASE("NGCompiler: compile nested if statements", "[compiler]") {
    const char* source = R"(
        fun nested_if(x) {
            if (x > 10) {
                if (x > 20) {
                    return 2;
                }
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

TEST_CASE("NGCompiler integration: nested if execution", "[compiler][integration]") {
    const char* source = R"(
        fun nested_if(x) {
            if (x > 10) {
                if (x > 20) {
                    return 2;
                }
                return 1;
            }
            return 0;
        }
        
        print(nested_if(15));
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

TEST_CASE("NGCompiler: compile multiple functions", "[compiler]") {
    const char* source = R"(
        fun add(a, b) {
            return a + b;
        }
        
        fun sub(a, b) {
            return a - b;
        }
        
        fun mul(a, b) {
            return a * b;
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
    REQUIRE(module->functions.size() == 3);
}

TEST_CASE("NGCompiler: compile empty function", "[compiler]") {
    const char* source = R"(
        fun empty() {
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

TEST_CASE("NGCompiler: compile module with only statements", "[compiler]") {
    const char* source = R"(
        print(1);
        print(2);
        print(3);
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
    REQUIRE(module->start_block != nullptr);
}

TEST_CASE("NGCompiler: compile complex expressions", "[compiler]") {
    const char* source = R"(
        val result = (5 + 3) * (10 - 2) / 4;
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
    REQUIRE(module->variables.size() == 1);
}

TEST_CASE("NGCompiler integration: multiple imports", "[compiler][integration]") {
    const char* source = R"(
        print(42);
        println(43);
        log(44);
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
    REQUIRE(module->imports.size() >= 3);
}

TEST_CASE("NGCompiler: compile if statement with else-if", "[compiler]") {
    const char* source = R"(
        fun check_range(x) {
            if (x < 0) {
                return -1;
            } else {
                if (x > 100) {
                    return 1;
                } else {
                    return 0;
                }
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

TEST_CASE("NGCompiler integration: function parameters used multiple times", "[compiler][integration]") {
    const char* source = R"(
        fun calc_expr(x) {
            val sq = x * x;
            val res = sq + x;
            return res;
        }
        
        print(calc_expr(5));
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
    
    REQUIRE(print_value == 30); // 5*5 + 5 = 30
}

TEST_CASE("NGCompiler: compile multiple module-level values", "[compiler]") {
    const char* source = R"(
        val a = 1;
        val b = 2;
        val c = 3;
        val d = 4;
        val e = 5;
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
    REQUIRE(module->variables.size() == 5);
    REQUIRE(module->constants.size() >= 5);
}

TEST_CASE("NGCompiler integration: chained comparisons", "[compiler][integration]") {
    const char* source = R"(
        fun in_range(x) {
            if (x > 10) {
                if (x < 20) {
                    return 1;
                }
            }
            return 0;
        }
        
        print(in_range(15));
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

TEST_CASE("NGCompiler: compile function calling another function", "[compiler]") {
    const char* source = R"(
        fun helper(x) {
            return x + 10;
        }
        
        fun caller(y) {
            val tmp = helper(y);
            return tmp * 2;
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
    REQUIRE(module->functions.size() == 2);
}

TEST_CASE("NGCompiler integration: function calling another function execution", "[compiler][integration]") {
    const char* source = R"(
        fun helper(x) {
            return x + 10;
        }
        
        fun caller(y) {
            return helper(y) * 2;
        }
        
        print(caller(5));
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
    
    REQUIRE(print_value == 30); // (5 + 10) * 2 = 30
}

TEST_CASE("NGCompiler: compile with module name", "[compiler]") {
    const char* source = R"(
        module mymodule;
        
        fun get_num() {
            return 42;
        }
    )";
    
    auto ast = parse(source, "mymodule");
    REQUIRE(ast != nullptr);
    
    NG::typecheck::TypeIndex typeIndex;
    try {
        typeIndex = NG::typecheck::type_check(ast);
    } catch (...) {}
    
    NGCompiler compiler(typeIndex);
    auto module = compiler.compile(ast);
    
    REQUIRE(module != nullptr);
    REQUIRE(module->name == "mymodule");
}

TEST_CASE("NGCompiler: compile function with no parameters", "[compiler]") {
    const char* source = R"(
        fun get_const() {
            return 42;
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
    REQUIRE(module->functions[0].params.size() == 0);
}

TEST_CASE("NGCompiler integration: zero parameters function", "[compiler][integration]") {
    const char* source = R"(
        fun get_value() {
            return 99;
        }
        
        print(get_value());
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
    
    REQUIRE(print_value == 99);
}

TEST_CASE("NGCompiler: compile with negative numbers", "[compiler]") {
    const char* source = R"(
        val neg = -42;
        val pos = 42;
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

TEST_CASE("NGCompiler integration: negative number execution", "[compiler][integration]") {
    const char* source = R"(
        val num = -50;
        print(num);
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
    
    REQUIRE(print_value == -50);
}

TEST_CASE("NGCompiler: compile floating point literals", "[compiler]") {
    const char* source = R"(
        val pi = 3.14;
        val e = 2.71;
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

TEST_CASE("NGCompiler: compile if without else", "[compiler]") {
    const char* source = R"(
        fun check(x) {
            if (x > 10) {
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

TEST_CASE("NGCompiler: compile multiple return statements", "[compiler]") {
    const char* source = R"(
        fun multi_return(x) {
            if (x < 0) {
                return -1;
            }
            if (x == 0) {
                return 0;
            }
            return 1;
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

TEST_CASE("NGCompiler integration: early return execution", "[compiler][integration]") {
    const char* source = R"(
        fun check_sign(x) {
            if (x < 0) {
                return -1;
            }
            return 1;
        }
        
        print(check_sign(-5));
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
    
    REQUIRE(print_value == -1);
}

TEST_CASE("NGCompiler: compile with logical not", "[compiler]") {
    const char* source = R"(
        val t = !false;
        val f = !true;
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

TEST_CASE("NGCompiler: compile function with many parameters", "[compiler]") {
    const char* source = R"(
        fun sum5(a, b, c, d, e) {
            return a + b + c + d + e;
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
    REQUIRE(module->functions[0].params.size() == 5);
}

TEST_CASE("NGCompiler integration: many parameters execution", "[compiler][integration]") {
    const char* source = R"(
        fun sum3(a, b, c) {
            return a + b + c;
        }
        
        print(sum3(10, 20, 30));
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
    
    REQUIRE(print_value == 60);
}

TEST_CASE("NGCompiler: compile deep expression nesting", "[compiler]") {
    const char* source = R"(
        val res = ((1 + 2) * (3 + 4)) - ((5 - 2) * (6 - 4));
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
    REQUIRE(module->variables.size() == 1);
}

TEST_CASE("NGCompiler integration: deep expression execution", "[compiler][integration]") {
    const char* source = R"(
        val res = (2 + 3) * (4 - 1);
        print(res);
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
    
    REQUIRE(print_value == 15); // (2+3) * (4-1) = 5 * 3 = 15
}

TEST_CASE("NGCompiler: compile function with only return", "[compiler]") {
    const char* source = R"(
        fun constant() {
            return 42;
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

TEST_CASE("NGCompiler: compile boolean operations", "[compiler]") {
    const char* source = R"(
        val a = true == true;
        val b = false != true;
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

TEST_CASE("NGCompiler: compile chained addition", "[compiler]") {
    const char* source = R"(
        val sum = 1 + 2 + 3 + 4 + 5;
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
    REQUIRE(module->variables.size() == 1);
}

TEST_CASE("NGCompiler integration: chained operations execution", "[compiler][integration]") {
    const char* source = R"(
        val sum = 1 + 2 + 3;
        print(sum);
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
    
    REQUIRE(print_value == 6);
}

TEST_CASE("NGCompiler: compile mixed arithmetic", "[compiler]") {
    const char* source = R"(
        val calc = 10 + 5 * 2 - 8 / 4;
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
    REQUIRE(module->variables.size() == 1);
}

TEST_CASE("NGCompiler: compile zero value", "[compiler]") {
    const char* source = R"(
        val zero = 0;
        val result = zero + zero;
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

TEST_CASE("NGCompiler integration: zero value execution", "[compiler][integration]") {
    const char* source = R"(
        val zero = 0;
        print(zero);
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
    
    REQUIRE(print_value == 0);
}

TEST_CASE("NGCompiler: compile large numbers", "[compiler]") {
    const char* source = R"(
        val big = 1000000;
        val huge = 999999999;
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

TEST_CASE("NGCompiler: compile simple loop", "[compiler]") {
    const char* source = R"(
        fun loop_once() {
            loop {
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

TEST_CASE("NGCompiler: compile identity operations", "[compiler]") {
    const char* source = R"(
        val same = 5 + 0;
        val also_same = 5 * 1;
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
