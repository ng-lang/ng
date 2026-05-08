#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse a forever loop", "[Parser][Loop][Next][ControlFlow]")
{
  auto ast = parse(R"(
        loop {
            next;
        }
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should loop with simple variables ", "[Parser][Loop][Next][ControlFlow]")
{
  auto ast = parse(R"(
        loop n {
            next;
        }
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should loop with variable type annotations", "[Parser][Loop][Next][ControlFlow]")
{
  auto ast = parse(R"(
        val x = 1;
        loop n : int = x {
            next;
        }
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should loop with mixied binding types", "[Parser][Loop][Next][ControlFlow]")
{
  auto ast = parse(R"(
        val x = 1;
        loop a, b = 1, c = 2, d : int = x {
            next;
        }
    )");
  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse const if statement", "[Parser][ConstIf][ControlFlow]")
{
  auto ast = parse(R"(
        val a = 1;
        const if (true) {
            val b = 2;
        }
    )");
  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_cast<CompileUnit *>(ast.get());
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module.get();
  REQUIRE(mod != nullptr);
  // val a = 1 is parsed as a definition, const if as a statement
  REQUIRE(mod->definitions.size() == 1);
  REQUIRE(mod->statements.size() == 1);
  auto ifStmt = dynamic_cast<IfStatement *>(mod->statements[0].get());
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->isConst == true);
  REQUIRE(ifStmt->consequence != nullptr);
  REQUIRE(ifStmt->alternative == nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse const if with else", "[Parser][ConstIf][ControlFlow]")
{
  auto ast = parse(R"(
        const if (false) {
            val a = 1;
        } else {
            val b = 2;
        }
    )");
  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_cast<CompileUnit *>(ast.get());
  auto mod = compileUnit->module.get();
  REQUIRE(mod->statements.size() == 1);
  auto ifStmt = dynamic_cast<IfStatement *>(mod->statements[0].get());
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->isConst == true);
  REQUIRE(ifStmt->consequence != nullptr);
  REQUIRE(ifStmt->alternative != nullptr);
  destroyast(ast);
}

TEST_CASE("parser should parse regular if without const", "[Parser][ConstIf][ControlFlow]")
{
  auto ast = parse(R"(
        if (true) {
            val a = 1;
        }
    )");
  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_cast<CompileUnit *>(ast.get());
  auto mod = compileUnit->module.get();
  REQUIRE(mod->statements.size() == 1);
  auto ifStmt = dynamic_cast<IfStatement *>(mod->statements[0].get());
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->isConst == false);
  destroyast(ast);
}

TEST_CASE("const if repr should include const prefix", "[Parser][ConstIf][ControlFlow]")
{
  auto ast = parse(R"(
        const if (true) {
            val x = 1;
        }
    )");
  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_cast<CompileUnit *>(ast.get());
  auto mod = compileUnit->module.get();
  auto ifStmt = dynamic_cast<IfStatement *>(mod->statements[0].get());
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->isConst == true);
  auto repr = ifStmt->repr();
  REQUIRE(repr.starts_with("const if"));
  destroyast(ast);
}

TEST_CASE("parser should keep typeof const if at module statement level", "[Parser][ConstIf][TypeQuery][ControlFlow]")
{
  auto ast = parse(R"(
        val value = 42;
        const if (typeof(value).name == "i32") {
            val ok = true;
        } else {
            val ok = false;
        }
    )");
  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_cast<CompileUnit *>(ast.get());
  REQUIRE(compileUnit != nullptr);
  auto mod = compileUnit->module.get();
  REQUIRE(mod != nullptr);
  REQUIRE(mod->definitions.size() == 1);
  REQUIRE(mod->statements.size() == 1);

  auto ifStmt = dynamic_cast<IfStatement *>(mod->statements[0].get());
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->isConst);
  auto equality = dynamic_ast_cast<BinaryExpression>(ifStmt->testing);
  REQUIRE(equality != nullptr);
  auto accessor = dynamic_ast_cast<IdAccessorExpression>(equality->left);
  REQUIRE(accessor != nullptr);
  REQUIRE(dynamic_ast_cast<TypeOfExpression>(accessor->primaryExpression) != nullptr);
  REQUIRE(accessor->accessor->repr() == "name");
  destroyast(ast);
}
