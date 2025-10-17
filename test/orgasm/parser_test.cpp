#include <catch2/catch_test_macros.hpp>
#include "orgasm/parser.hpp"

using namespace ng::orgasm;

TEST_CASE("ORGASM parser should parse empty module", "[orgasm][parser]") {
  std::string source = ".module test\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module != nullptr);
  REQUIRE(module->name == "test");
}

TEST_CASE("ORGASM parser should parse symbols", "[orgasm][parser]") {
  std::string source = ".module test\n.symbols [id, n, print]\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->symbols.size() == 3);
  REQUIRE(module->symbols[0] == "id");
  REQUIRE(module->symbols[1] == "n");
  REQUIRE(module->symbols[2] == "print");
}

TEST_CASE("ORGASM parser should parse constants", "[orgasm][parser]") {
  std::string source = ".module test\n.const i32 42\n.const f64 3.14\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->constants.size() == 2);
  REQUIRE(module->constants[0].type == PrimitiveType::I32);
  REQUIRE(module->constants[1].type == PrimitiveType::F64);
}

TEST_CASE("ORGASM parser should parse string literals", "[orgasm][parser]") {
  std::string source = ".module test\n.str [hello world]\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->strings.size() == 1);
  REQUIRE(module->strings[0].value == "hello world");
}

TEST_CASE("ORGASM parser should parse imports", "[orgasm][parser]") {
  std::string source = ".module test\n.import print, core\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->imports.size() == 1);
  REQUIRE(module->imports[0].symbol_name == "print");
  REQUIRE(module->imports[0].module_name == "core");
}

TEST_CASE("ORGASM parser should parse variables", "[orgasm][parser]") {
  std::string source = ".module test\n.val i32 0\n.val addr 0\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->variables.size() == 2);
  REQUIRE(module->variables[0].type == PrimitiveType::I32);
  REQUIRE(module->variables[1].type == PrimitiveType::ADDR);
}

TEST_CASE("ORGASM parser should parse arrays", "[orgasm][parser]") {
  std::string source = ".module test\n.array i32 [1, 2, 3]\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->arrays.size() == 1);
  REQUIRE(module->arrays[0].element_type == PrimitiveType::I32);
  REQUIRE(module->arrays[0].elements.size() == 3);
}

TEST_CASE("ORGASM parser should parse exports", "[orgasm][parser]") {
  std::string source = ".module test\n.export main\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->exports.size() == 1);
  REQUIRE(module->exports[0].symbol_name == "main");
}

TEST_CASE("ORGASM parser should parse float constants", "[orgasm][parser]") {
  std::string source = ".module test\n.const f32 3.14\n.const f64 2.71\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->constants.size() == 2);
  REQUIRE(module->constants[0].type == PrimitiveType::F32);
  REQUIRE(module->constants[1].type == PrimitiveType::F64);
}

TEST_CASE("ORGASM parser should parse boolean constants", "[orgasm][parser]") {
  std::string source = ".module test\n.const bool true\n.const bool false\n.endmodule";
  Parser parser(source);

  auto module = parser.parse_module();
  REQUIRE(module->constants.size() == 2);
  REQUIRE(module->constants[0].type == PrimitiveType::BOOL);
  REQUIRE(module->constants[1].type == PrimitiveType::BOOL);
}
