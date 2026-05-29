
#include "../test.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

using Catch::Matchers::ContainsSubstring;

TEST_CASE("parser should parse modules", "[Parser][Module]")
{
  auto ast = parse(R"(
        module foo;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse canonical dotted module names", "[Parser][Module]")
{
  auto ast = parse(R"(
        module std.prelude exports *;
    )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module != nullptr);
  REQUIRE(compileUnit->module->name == "std.prelude");
  REQUIRE(compileUnit->module->nameDeclared);

  destroyast(ast);
}

TEST_CASE("parser should parse exports", "[Parser][Module][Export]")
{
  auto ast = parse(R"(
        // export all
        module hello exports *;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export symbol
        module hello exports (world);
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export multiple symbol
        module hello exports (a, b, c);
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export none
        module hello;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);

  ast = parse(R"(
        // export without a module name
        module exports *;
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("Should export single declaration", "[Parser][Export][Native]")
{
  auto ast = parse(R"(
        export val x = 1;

        export fun get() -> int = native;

        export type Simple {}
    )");

  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse exported imports", "[Parser][Module][Import][Export]")
{
  auto ast = parse(R"(
        export import std.string (*);
        export import std.array (reverse);
    )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module != nullptr);
  REQUIRE(compileUnit->module->imports.size() == 2);
  REQUIRE(compileUnit->module->imports[0]->exported);
  REQUIRE(compileUnit->module->imports[0]->imports == Vec<Str>{"*"});
  REQUIRE(compileUnit->module->imports[1]->exported);
  REQUIRE(compileUnit->module->imports[1]->imports == Vec<Str>{"reverse"});
  REQUIRE(std::ranges::find(compileUnit->module->exports, "reverse") != compileUnit->module->exports.end());

  destroyast(ast);
}

TEST_CASE("Should not export statement", "[Parser][Export]")
{
  parseInvalid(
      R"(
        export loop x = 1 {
        }
    )",
      "Invalid export");
}

TEST_CASE("parser should parse import as alias syntax", "[Parser][Module][Import]")
{
  auto ast = parse(R"(
        import vendor.math as math;
    )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->imports.size() == 1);
  auto importDecl = compileUnit->module->imports.front();
  REQUIRE(importDecl != nullptr);
  REQUIRE(importDecl->module == "math");
  REQUIRE(importDecl->alias == "math");
  REQUIRE(importDecl->modulePath == Vec<Str>{"vendor", "math"});

  destroyast(ast);
}
