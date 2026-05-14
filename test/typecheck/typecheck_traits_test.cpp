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

TEST_CASE("supertrait bound should expose inherited methods", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Counter { value: i32; }

    trait Eq {
      fun same(self: ref<Self>, other: ref<Self>) -> bool;
    }

    trait Ord: Eq {
      fun less(self: ref<Self>, other: ref<Self>) -> bool;
    }

    impl Ord for Counter {
      fun same(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value == other.value;
      }

      fun less(self: ref<Self>, other: ref<Self>) -> bool {
        return self.value < other.value;
      }
    }

    fun equal<T: Ord>(left: ref<T>, right: ref<T>) -> bool {
      return left.same(right);
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("equal"));
  check_type_tag(*index["Ord"], typeinfo_tag::TRAIT);

  destroyast(ast);
}

TEST_CASE("qualified trait calls should resolve ambiguous trait method names", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Person { name: string; }

    trait Display {
      fun text(self: ref<Self>) -> string;
    }

    trait Debug {
      fun text(self: ref<Self>) -> string;
    }

    impl Display for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Debug for Person {
      fun text(self: ref<Self>) -> string {
        return "debug " + self.name;
      }
    }

    val ada = new Person { name: "Ada" };
    val display = ada.Display::text();
    val debugText = Debug::text(ref ada);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("display"));
  REQUIRE(index.contains("debugText"));
  check_type_tag(*index["display"], typeinfo_tag::STRING);
  check_type_tag(*index["debugText"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("ambiguous unqualified trait method call should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    type Person { name: string; }

    trait Display {
      fun text(self: ref<Self>) -> string;
    }

    trait Debug {
      fun text(self: ref<Self>) -> string;
    }

    impl Display for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    impl Debug for Person {
      fun text(self: ref<Self>) -> string {
        return self.name;
      }
    }

    val ada = new Person { name: "Ada" };
    val text = ada.text();
  )", "Ambiguous trait method call");
}

TEST_CASE("inherent method should take precedence over same-named trait method", "[TypeCheck][Traits]")
{
  auto ast = parse(R"(
    type Box {
      value: i32;

      fun label(self: ref<Self>) -> string {
        return "inherent";
      }
    }

    trait Label {
      fun label(self: ref<Self>) -> string;
    }

    impl Label for Box {
      fun label(self: ref<Self>) -> string {
        return "trait";
      }
    }

    val box = new Box { value: 1 };
    val inherent = box.label();
    val qualified = box.Label::label();
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  check_type_tag(*index["inherent"], typeinfo_tag::STRING);
  check_type_tag(*index["qualified"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("cyclic trait inheritance should fail", "[TypeCheck][Traits][Failure]")
{
  typecheck_failure(R"(
    trait A: B {
      fun a(self: ref<Self>) -> unit;
    }

    trait B: A {
      fun b(self: ref<Self>) -> unit;
    }
  )", "Cyclic trait inheritance");
}
