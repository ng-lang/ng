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

TEST_CASE("should reject ref on reference value", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(R"(
    type Box { value: i32; }
    val box = new Box { value: 1 };
    val ptr = ref box;
  )", "cannot take a reference value");
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

TEST_CASE("should track partial moves from object fields", "[TypeCheck][RefMove][PartialMove]")
{
  auto ast = parse(R"(
    type Box {
      left: i32;
      right: i32;

      fun total(self: ref<Self>) -> i32 {
        return self.left + self.right;
      }
    }

    {
      val box = new Box { left: 1, right: 2 };
      val moved = move box.left;
      val sibling: i32 = box.right;
      box.left := moved;
      val total: i32 = box.total();
    }
  )");

  REQUIRE(ast != nullptr);
  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("should reject reads and method calls on partially moved objects", "[TypeCheck][RefMove][PartialMove][Failure]")
{
  typecheck_failure(
      R"(
        type Box { left: i32; right: i32; }
        {
          val box = new Box { left: 1, right: 2 };
          val moved = move box.left;
          val again = box.left;
        }
      )",
      "Use after move: box.left");

  typecheck_failure(
      R"(
        type Box { left: i32; right: i32; }
        {
          val box = new Box { left: 1, right: 2 };
          val moved = move box.left;
          val whole = box;
        }
      )",
      "Use after partial move: box");

  typecheck_failure(
      R"(
        type Box {
          left: i32;
          right: i32;
          fun total(self: ref<Self>) -> i32 { return self.left + self.right; }
        }
        {
          val box = new Box { left: 1, right: 2 };
          val moved = move box.left;
          val total = box.total();
        }
      )",
      "Use after partial move: box");
}

TEST_CASE("should track partial moves from tuple constant indexes", "[TypeCheck][RefMove][PartialMove]")
{
  auto ast = parse(R"(
    {
      val tuple = (1, "right");
      val moved = move tuple[0];
      val sibling: string = tuple[1];
      tuple[0] := 2;
      val restored: (i32, string) = tuple;
    }
  )");

  REQUIRE(ast != nullptr);
  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("should reject invalid tuple and array partial moves", "[TypeCheck][RefMove][PartialMove][Failure]")
{
  typecheck_failure(
      R"(
        {
          val tuple = (1, "right");
          val moved = move tuple[0];
          val again = tuple[0];
        }
      )",
      "Use after move: tuple[0]");

  typecheck_failure(
      R"(
        {
          val tuple = (1, "right");
          val moved = move tuple[0];
          val whole = tuple;
        }
      )",
      "Use after partial move: tuple");

  typecheck_failure(
      R"(
        {
          val arr = [1, 2, 3];
          val moved = move arr[0];
        }
      )",
      "Move from indexed place only supports tuple constant indexes");
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

TEST_CASE("should reject object construction with missing declared property", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        type Box {
          value: i32;
          other: i32;
        }
        val box = new Box { value: 1 };
      )",
      "Missing property 'other' for type Box");
}

TEST_CASE("should reject tagged variant calls with invalid payload arity or type", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        type Result = Ok(value: i32) | Err(msg: string);
        val result = Ok();
      )",
      "Invalid payload arity for variant Ok");

  typecheck_failure(
      R"(
        type Result = Ok(value: i32) | Err(msg: string);
        val result = Ok("bad");
      )",
      "Payload type mismatch for variant Ok");
}

TEST_CASE("should reject generic tagged variant calls with invalid payload arity", "[TypeCheck][RefMove][Failure]")
{
  typecheck_failure(
      R"(
        type Option<T> = Some(value: T) | None;
        val option: Option<i32> = Some();
      )",
      "Invalid payload arity for variant Some");
}
