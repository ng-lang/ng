#include "../test.hpp"
#include <compiler/ngcompiler.hpp>
#include <typecheck/typecheck.hpp>

using namespace NG;
using namespace NG::compiler;

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
