#include "../test.hpp"

#include <typecheck/typecheck.hpp>
#include <typecheck/typeinfo.hpp>

#include "typecheck_utils.hpp"

using namespace NG::typecheck;

TEST_CASE("should parse union type annotations", "[TypeCheck][Union]")
{
  auto ast = parse(R"(
            val x: i32 | string = 1;
            val y: i32 | string = "hello";
            val z: bool | i32 | f32 = true;
        )");

  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("should parse union type in function params and return", "[TypeCheck][Union]")
{
  auto ast = parse(R"(
            fun foo(x: i32 | string) -> i32 | bool = 1;
        )");

  REQUIRE(ast != nullptr);
  destroyast(ast);
}

TEST_CASE("should type check union type with matching value", "[TypeCheck][Union]")
{
  auto ast = parse(R"(
            val x: i32 | string = 1;
            val y: i32 | string = "hello";
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.count("x") == 1);
  REQUIRE(index.count("y") == 1);

  destroyast(ast);
}

TEST_CASE("should reject non-matching value for union type", "[TypeCheck][Union][Failure]")
{
  typecheck_failure("val x: i32 | string = true;", "");
  typecheck_failure("val x: i32 | string = 1.5;", "");
  typecheck_failure("val x: bool | f32 = \"hello\";", "");
}

TEST_CASE("switch exhaustive with all variants covered", "[TypeCheck][Switch][Exhaustive]")
{
  auto ast = parse(R"(
            type Result = Ok(value: i32) | Err(msg: string);
            val x = Ok(42);
            switch (x) {
                case Ok(v) { val a = v; }
                case Err(m) { val b = m; }
            }
        )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  destroyast(ast);
}

TEST_CASE("switch exhaustive with otherwise branch", "[TypeCheck][Switch][Otherwise]")
{
  auto ast = parse(R"(
            type Result = Ok(value: i32) | Err(msg: string);
            val x = Ok(42);
            switch (x) {
                case Ok(v) { val a = v; }
                otherwise { val b = 0; }
            }
        )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  destroyast(ast);
}

TEST_CASE("switch non-exhaustive fails without otherwise", "[TypeCheck][Switch][Exhaustive][Failure]")
{
  typecheck_failure(R"(
            type Result = Ok(value: i32) | Err(msg: string);
            val x = Ok(42);
            switch (x) {
                case Ok(v) { val a = v; }
            }
        )", "Non-exhaustive switch");
}

TEST_CASE("switch with unknown variant fails", "[TypeCheck][Switch][Failure]")
{
  typecheck_failure(R"(
            type Result = Ok(value: i32) | Err(msg: string);
            val x = Ok(42);
            switch (x) {
                case Ok(v) { val a = v; }
                case Err(m) { val b = m; }
                case NotFound(z) { val c = z; }
            }
        )", "Unknown variant");
}

TEST_CASE("switch with otherwise covers missing variants", "[TypeCheck][Switch][Otherwise]")
{
  auto ast = parse(R"(
            type Shape = Circle(radius: f32) | Rectangle(width: f32, height: f32) | Triangle(base: f32, height: f32);
            val s = Circle(5.0);
            switch (s) {
                case Circle(r) { val a = r; }
                otherwise { val b = 0; }
            }
        )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  destroyast(ast);
}

TEST_CASE("self recursive tagged union payloads should resolve enclosing union", "[TypeCheck][Union][Recursive]")
{
  auto ast = parse(R"(
            type Node = Cell(content: i32, _next: ref<Node>) | Empty;

            val tail = Empty();
            val head = Cell(1, ref tail);
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.count("tail") == 1);
  REQUIRE(index.count("head") == 1);

  auto *tailType = dynamic_cast<VariantType *>(&*index["tail"]);
  REQUIRE(tailType != nullptr);
  REQUIRE(tailType->unionName == "Node");

  auto *headType = dynamic_cast<VariantType *>(&*index["head"]);
  REQUIRE(headType != nullptr);
  REQUIRE(headType->unionName == "Node");

  destroyast(ast);
}

TEST_CASE("new should type check tagged union variants as heap refs", "[TypeCheck][Union][Recursive]")
{
  auto ast = parse(R"(
            type Node = Cell(content: i32, _next: ref<Node>) | Empty;

            val empty: ref<Node> = new Empty {};
            val head: ref<Node> = new Cell { content: 1, _next: new Empty {} };
        )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.count("empty") == 1);
  REQUIRE(index.count("head") == 1);
  REQUIRE(index["empty"]->tag() == typeinfo_tag::REFERENCE);
  REQUIRE(index["head"]->tag() == typeinfo_tag::REFERENCE);

  auto *emptyRef = dynamic_cast<ReferenceType *>(&*index["empty"]);
  REQUIRE(emptyRef != nullptr);
  REQUIRE(emptyRef->referencedType->tag() == typeinfo_tag::TAGGED_UNION);

  auto *headRef = dynamic_cast<ReferenceType *>(&*index["head"]);
  REQUIRE(headRef != nullptr);
  REQUIRE(headRef->referencedType->tag() == typeinfo_tag::TAGGED_UNION);

  destroyast(ast);
}
