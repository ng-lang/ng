#include "../test.hpp"

using namespace NG;
using namespace NG::ast;
using namespace NG::parsing;

TEST_CASE("parser should parse ref type annotations in prefix and suffix forms", "[Parser][RefMove][TypeAnnotation]")
{
  auto ast = parse(R"(
    fun borrow(x: i32 ref) -> ref<i32> {
      return ref x;
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  REQUIRE(compileUnit->module->definitions.size() == 1);

  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->params.size() == 1);
  REQUIRE(funDef->params[0]->annotatedType != nullptr);
  REQUIRE(funDef->params[0]->annotatedType->repr() == "ref<i32>");
  REQUIRE(funDef->returnType != nullptr);
  REQUIRE(funDef->returnType->repr() == "ref<i32>");

  auto body = dynamic_ast_cast<CompoundStatement>(funDef->body);
  REQUIRE(body != nullptr);
  REQUIRE(body->statements.size() == 1);
  auto returnStmt = dynamic_ast_cast<ReturnStatement>(body->statements[0]);
  REQUIRE(returnStmt != nullptr);
  auto refExpr = dynamic_ast_cast<UnaryExpression>(returnStmt->expression);
  REQUIRE(refExpr != nullptr);
  REQUIRE(refExpr->optr != nullptr);
  REQUIRE(refExpr->optr->type == TokenType::KEYWORD_REF);
  REQUIRE(refExpr->repr() == "ref x");

  destroyast(ast);
}

TEST_CASE("parser should parse move, deref, and address-of expressions", "[Parser][RefMove][Expression]")
{
  auto ast = parse(R"(
    fun swap<T>(a: T ref, b: T ref) {
      val tmp = move *a;
      *a := move *b;
      *b := move tmp;
      val ref_alias = &tmp;
    }
  )");
  REQUIRE(ast != nullptr);

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[0]);
  REQUIRE(funDef != nullptr);
  REQUIRE(funDef->genericParams.size() == 1);

  auto body = dynamic_ast_cast<CompoundStatement>(funDef->body);
  REQUIRE(body != nullptr);
  REQUIRE(body->statements.size() == 4);

  auto tmpDef = dynamic_ast_cast<ValDefStatement>(body->statements[0]);
  REQUIRE(tmpDef != nullptr);
  auto moveDeref = dynamic_ast_cast<UnaryExpression>(tmpDef->value);
  REQUIRE(moveDeref != nullptr);
  REQUIRE(moveDeref->optr->type == TokenType::KEYWORD_MOVE);
  REQUIRE(moveDeref->repr() == "move *a");
  auto derefA = dynamic_ast_cast<UnaryExpression>(moveDeref->operand);
  REQUIRE(derefA != nullptr);
  REQUIRE(derefA->optr->type == TokenType::TIMES);

  auto assignA = dynamic_ast_cast<SimpleStatement>(body->statements[1]);
  REQUIRE(assignA != nullptr);
  auto assignAExpr = dynamic_ast_cast<AssignmentExpression>(assignA->expression);
  REQUIRE(assignAExpr != nullptr);
  REQUIRE(assignAExpr->repr() == "*a = move *b");
  REQUIRE(dynamic_ast_cast<UnaryExpression>(assignAExpr->target) != nullptr);

  auto assignB = dynamic_ast_cast<SimpleStatement>(body->statements[2]);
  REQUIRE(assignB != nullptr);
  auto assignBExpr = dynamic_ast_cast<AssignmentExpression>(assignB->expression);
  REQUIRE(assignBExpr != nullptr);
  REQUIRE(assignBExpr->repr() == "*b = move tmp");

  auto aliasDef = dynamic_ast_cast<ValDefStatement>(body->statements[3]);
  REQUIRE(aliasDef != nullptr);
  auto addrOf = dynamic_ast_cast<UnaryExpression>(aliasDef->value);
  REQUIRE(addrOf != nullptr);
  REQUIRE(addrOf->optr->type == TokenType::AMPERSAND);
  REQUIRE(addrOf->repr() == "&tmp");

  destroyast(ast);
}

TEST_CASE("parser should reject bare ref type annotations", "[Parser][RefMove][Invalid]")
{
  parseInvalid("fun bad(x: ref) { return x; }", "Expected '<' after ref in type annotation");
}
