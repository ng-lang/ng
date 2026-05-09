#include "typecheck_utils.hpp"

TEST_CASE("should type check reference and move expressions", "[TypeCheck][RefMove]")
{
  auto ast = parse(R"(
    val x: i32 = 1;
    val rx: i32 ref = ref x;
    val ax: ref<i32> = &x;
    val y: i32 = *rx;
    val z: i32 = move *rx;
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  auto rxType = std::dynamic_pointer_cast<ReferenceType>(index["rx"]);
  REQUIRE(rxType != nullptr);
  check_type_tag(*rxType->referencedType, typeinfo_tag::I32);

  auto axType = std::dynamic_pointer_cast<ReferenceType>(index["ax"]);
  REQUIRE(axType != nullptr);
  check_type_tag(*axType->referencedType, typeinfo_tag::I32);

  check_type_tag(*index["y"], typeinfo_tag::I32);
  check_type_tag(*index["z"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("should type check deref assignment and move from ref parameters", "[TypeCheck][RefMove]")
{
  auto ast = parse(R"(
    fun swap(a: i32 ref, b: i32 ref) -> i32 {
      val tmp = move *a;
      *a := move *b;
      *b := move tmp;
      return move *a;
    }
  )");

  REQUIRE(ast != nullptr);
  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("should reject ref on rvalue", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure("val x = ref 1;", "Reference operator requires an lvalue");
}

TEST_CASE("should reject deref on non-reference", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure("val x: i32 = *1;", "Cannot dereference non-reference type");
}

TEST_CASE("should reject move on non-movable expression", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure("val x: i32 = 1; val y = move (x + 1);", "Move operator requires a movable place");
}

TEST_CASE("should reject use after move", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure("val x: i32 = 1; val y = move x; val z = x;", "Use after move: x");
}

TEST_CASE("should allow reassignment after move", "[TypeCheck][RefMove]")
{
  auto ast = parse(R"(
    {
      val x: i32 = 1;
      val y = move x;
      x := 2;
      val z: i32 = x;
    }
  )");

  REQUIRE(ast != nullptr);
  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("should reject read after branch-local move", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        {
          val x: i32 = 1;
          if (1 == 1) { val y = move x; } else { val z = 0; }
          val w = x;
        }
      )",
      "Use after move: x");
}

TEST_CASE("should reject use after move through function call argument", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        fun take(x: i32) -> i32 = x;
        {
          val x: i32 = 1;
          val y = take(move x);
          val z = x;
        }
      )",
      "Use after move: x");
}

TEST_CASE("should reject use after move through generic call argument", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        fun id<T>(x: T) -> T = x;
        {
          val x: i32 = 1;
          val y = id(move x);
          val z = x;
        }
      )",
      "Use after move: x");
}

TEST_CASE("should reject read after switch-branch move", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        type Result = Ok(value: i32) | Err(msg: string);
        {
          val x: i32 = 1;
          val r = Ok(0);
          switch (r) {
            case Ok(v) { val y = move x; }
            case Err(m) { val z = 0; }
          }
          val w = x;
        }
      )",
      "Use after move: x");
}
