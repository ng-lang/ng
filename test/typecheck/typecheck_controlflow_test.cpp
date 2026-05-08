#include "typecheck_utils.hpp"

TEST_CASE("should be able to check control and result types", "[Function][TypeCheck]")
{
  auto ast = parse(R"(
        {
            val x = 1;
            val y = 2;
            x + 1;
            y + 2;
        }
        loop x = 1, y = 2 {
            next x + 1, y + 1;
        }
        if (true) {
            return 1;
        } else {
            return 2;
        }

        if (true) {
            return 1u8;
        } else {
            return 2;
        }
        
        if (false) {
            return 1;
        }
        
        fun print(x: int) -> unit = native;
        
        if (1 == 1) {
            print(1);
        } else {
            return 2;
        }

        if (2 == 1) {
            return 1;
        } else {
            print(2);
        }

        {
            if (3 > 2) {
                return 1u8;
            }

            return 2u16;
        }
        )");

  REQUIRE(ast != nullptr);

  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("should fail when check control flow with incompatible type", "[Function][TypeCheck]")
{

  typecheck_failure(
      R"(
            if (1) {}
    )",
      "Condition expression must be boolean");

  typecheck_failure(
      R"(
            if (true) { return 1; } else { return 2.0;}
    )",
      "Mismatched return types in if-else branches");

  typecheck_failure(
      R"(
            val x = 1;
            loop x {
                next;
            }
    )",
      "Next statement argument count mismatch");

  typecheck_failure(
      R"(
            val x = 1;
            loop x {
                next false;
            }
    )",
      "Next statement argument type mismatch");

  typecheck_failure(
      R"(
            loop n: int = false {
                next n + 1;
            }
    )",
      "Loop Binding Type Mismatch");

}

TEST_CASE("const if with boolean literal true", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with boolean literal false", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with negation", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (!false) {
            val x: i32 = 1;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with binary AND", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true && false) {
            val x: i32 = 1;
        } else {
            val y: i32 = 2;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if with binary OR", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false || true) {
            val x: i32 = 1;
        } else {
            val y: f64 = 2.0;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if eliminates dead branch with incompatible types", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (true) {
            return 42;
        } else {
            return 3.14;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if false eliminates then branch", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            return 42;
        } else {
            return 3;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if without else branch", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x: i32 = 1;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if false with dead code inside", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        const if (false) {
            val x = 1 + 2;
        }
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  destroyast(ast);
}

TEST_CASE("const if should evaluate typeof query properties", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        type Box<T> {
            property value: T;
        }

        fun main() {
            val box: Box<i32> = new Box<i32> { value: 1 };
            const if (typeof(box.value).name == "i32") {
                return 1;
            } else {
                return 0;
            }
        }
        )");

  REQUIRE(ast != nullptr);
  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);

  auto typeIndex = type_check(ast);
  REQUIRE(compileUnit->module->definitions.size() == 2);
  auto funDef = dynamic_ast_cast<FunctionDef>(compileUnit->module->definitions[1]);
  REQUIRE(funDef != nullptr);
  auto body = dynamic_ast_cast<CompoundStatement>(funDef->body);
  REQUIRE(body != nullptr);
  REQUIRE(body->statements.size() == 2);
  auto ifStmt = dynamic_ast_cast<IfStatement>(body->statements[1]);
  REQUIRE(ifStmt != nullptr);
  REQUIRE(ifStmt->evaluatedCondition.has_value());
  REQUIRE(ifStmt->evaluatedCondition.value());
  destroyast(ast);
}

TEST_CASE("typeof query properties should expose stable types", "[const_if][TypeCheck]")
{
  auto ast = parse(R"(
        type Result<T> = Ok(value: T) | Err(msg: string);

        val value = Ok(42);
        val type_name = typeof(value).name;
        val type_kind = typeof(value).kind;
        val field_count = typeof(value).fieldCount;
        )");

  REQUIRE(ast != nullptr);
  auto typeIndex = type_check(ast);
  REQUIRE(typeIndex.contains("type_name"));
  check_type_tag(*typeIndex["type_name"], typeinfo_tag::STRING);
  REQUIRE(typeIndex.contains("type_kind"));
  check_type_tag(*typeIndex["type_kind"], typeinfo_tag::STRING);
  REQUIRE(typeIndex.contains("field_count"));
  check_type_tag(*typeIndex["field_count"], typeinfo_tag::U32);
  destroyast(ast);
}
