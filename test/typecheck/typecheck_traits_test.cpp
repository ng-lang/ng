#include "typecheck_utils.hpp"

TEST_CASE("trait definition and concrete impl should type check", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter {
      label: string;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("Show"));
  check_type_tag(*index["Show"], typeinfo_tag::TRAIT);

  destroyast(ast);
}

TEST_CASE("generic trait bound should allow bounded method calls", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter {
      label: string;
    }

    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    impl Show for Counter {
      fun show(self: ref<Self>) -> string {
        return self.label;
      }
    }

    fun render<T: Show>(x: ref<T>) -> string {
      return x.show();
    }

    val c = new Counter { label: "three" };
    val s = render(ref c);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("s"));
  check_type_tag(*index["s"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("trait impl missing method should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    type Counter { value: i32; }
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
    impl Show for Counter {
    }
  )", "missing method");
}

TEST_CASE("ref trait object should fail in phase 1", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }
    fun render(x: ref<Show>) -> string {
      return x.show();
    }
  )", "ref<Trait>");
}
