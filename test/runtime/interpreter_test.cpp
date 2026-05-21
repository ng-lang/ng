#include "../test.hpp"
#include <intp/intp.hpp>
#include <typecheck/typecheck.hpp>

using namespace NG;
using namespace NG::parsing;
using namespace NG::intp;
using namespace NG::ast;

static void interpret(const Str &source)
{
  Interpreter *intp = NG::intp::stupid();

  auto ast = parse(source);

  REQUIRE(ast != nullptr);

  ast->accept(intp);

  // intp->summary();

  delete intp;
  destroyast(ast);
}

TEST_CASE("interpreter should accept simple definitions", "[InterpreterTest]")
{

  interpret(R"(
        val x = 1;
        val y = 2;
        val name = "ng";
        fun hello() {
          return "fizz";
        }
        val z = x + y;

        val g = hello();

        fun times(a, b) {
            return a * b;
        }

        val h = times(x, y);

        assert(z == 3, h == 2);
    )");
}

TEST_CASE("interpreter should run statements", "[InterpreterTest]")
{

  interpret(R"(
        fun max(a, b) {
            if (a > b) {
                return a;
            }
            return b;
        }

        val x = max(1, 2);
        val y = max(5, 4);
        val g = max(x, y);
        val h = max(g, 10);

        assert(x == 2, y == 5, g == 5, h == 10);
    )");
}

TEST_CASE("interpreter should run recursion", "[InterpreterTest]")
{

  interpret(R"(
        fun fact(x) {
            if (x > 0) {
                return x * fact(x-1);
            }
            return 1;
        }

        val z = fact(5);

        assert(z == 120);
    )");
}

TEST_CASE("interpreter should run complex recursion", "[InterpreterTest]")
{

  interpret(R"(
        fun gcd(a, b) {
            val c = a % b;
            if (c == 0) {
                return b;
            }
            return gcd(b, c);
        }

        val g = gcd(60, 33);

        assert(g == 3);
    )");
}

TEST_CASE("shoud be able to interpret array literal", "[InterpreterTest]")
{

  interpret(R"(
        val arr = [1, 2, 3, 4, 5];

        assert(arr[4] == 5);
    )");
}

TEST_CASE("shoud be able to interpret array index", "[InterpreterTest]")
{

  interpret(R"(
        val arr = [1, 2, 3, 4, 5];


        assert(arr[3] == 4);

        assert(arr[4] == 5);
        arr[4] := 6;
        assert(arr[4] == 6);
    )");
}

TEST_CASE("should be able interpret string member function", "[InterpreterTest]")
{
  interpret(R"(
        val x = "123";

        val y = x.size();

        assert(y == 3);

        assert(x.charAt(1) == 50);
    )");
}

TEST_CASE("should be able interpret simple type definition", "[InterpreterTest]")
{
  interpret(R"(
type Person {
    property firstName;
    property lastName;
    property kid;

    fun name(self: ref<Self>) {
        return self.firstName + " " + self.lastName;
    }
}

val person = new Person {
    firstName: "Kimmy",
    lastName: "Leo",
    kid: new Person {
        firstName: "Tiny",
        lastName: "Leo"
    }
};

assert(person.name().size() == 9);

assert(person.kid.name() == "Tiny Leo");

assert(person.firstName == "Kimmy");
)");
}

TEST_CASE("should resolve unqualified member properties via frame receiver", "[InterpreterTest]")
{
  interpret(R"(
type Counter {
    property value;

    fun bump(self: ref<Self>, delta) {
        self.value := self.value + delta;
        return self.value;
    }
}

val counter = new Counter {
    value: 10
};

assert(counter.bump(5) == 15);
assert(counter.value == 15);
)");
}

TEST_CASE("function frames should resolve globals without local context fallback", "[InterpreterTest]")
{
  interpret(R"(
val total = 1;

fun bump() {
    total := total + 1;
}

bump();

assert(total == 2);
)");
}

TEST_CASE("default arguments should resolve earlier parameters through call frame slots", "[InterpreterTest]")
{
  interpret(R"(
fun choose(x, y = x + 1) {
    return y;
}

assert(choose(4) == 5);
assert(choose(4, 9) == 9);
)");
}

TEST_CASE("top level frame should publish root bindings before nested function calls", "[InterpreterTest]")
{
  interpret(R"(
val total = 1;

fun readTotal() {
    assert(total == 3);
}

{
    total := 3;
    readTotal();
}

assert(total == 3);
)");
}

TEST_CASE("nested returns should propagate through return slots only", "[InterpreterTest]")
{
  interpret(R"(
type Result = Ok(value: i32) | Err(msg: string);

fun choose(flag) {
    if (flag) {
        switch (Ok(7)) {
            case Ok(value) {
                return value;
            }
            case Err(msg) {
                return 0;
            }
        }
    }
    return -1;
}

assert(choose(true) == 7);
assert(choose(false) == -1);
)");
}

TEST_CASE("should be able interpret integral values", "[InterpreterTest]")
{
  interpret(R"(
val x = 1i8;

val y = 2u16;

val z = x + y;

assert(z == 3);

)");
}

TEST_CASE("should be able interpret integral & floating values", "[InterpreterTest]")
{
  interpret(R"(
val x = 1f32;

val y = 2f64;

val a = 3i32;

val z = x + y + a;

assert(z == 6.0);
)");
}

TEST_CASE("basic loop (single variable)", "[InterpreterTest]")
{
  interpret(R"(
fun sum(n) {
  val s = 0;
  loop i = 0 {
    s := s + i;
    if (i < n) {
      next i + 1;
    }
  }
  return s;
}

val result = sum(10);

assert(result == 55);
)");
}

TEST_CASE("basic tail recursion (with default parameters)", "[InterpreterTest]")
{
  interpret(R"(

fun sum(i, n = 0) {
  if (i == 0) {
    return n;
  }
  next i - 1, n + i;
}

val result = sum(10);

assert(result == 55);
)");
}

TEST_CASE("interpreter should tail-call self recursion with spread arguments", "[InterpreterTest]")
{
  interpret(R"(
fun drain<T...>(args: T...) {
  if (args.size == 0) {
    return 0;
  }

  val (head, ...tail) = args;
  return drain(...tail);
}

assert(drain(1, 2, 3) == 0);
)");
}

TEST_CASE("block shadowing should honor frame-local scope ownership", "[InterpreterTest]")
{
  interpret(R"(
fun scoped() {
  val x = 1;
  if (true) {
    val x = 2;
    x := x + 1;
    assert(x == 3);
  }
  assert(x == 1);
  return x;
}

assert(scoped() == 1);
)");
}

TEST_CASE("switch case bindings should stay scoped without context forks", "[InterpreterTest]")
{
  interpret(R"(
type Result = Ok(value: i32) | Err(msg: string);

fun readOk() {
  val value = 1;
  switch (Ok(7)) {
    case Ok(value) {
      assert(value == 7);
    }
    case Err(msg) {
      assert(false);
    }
  }
  return value;
}

fun readErr() {
  val value = 1;
  switch (Err("nope")) {
    case Ok(value) {
      assert(false);
    }
    case Err(msg) {
      assert(msg == "nope");
    }
  }
  return value;
}

assert(readOk() == 1);
assert(readErr() == 1);
)");
}

TEST_CASE("type checking", "[InterpreterTestChecking]")
{
  interpret(R"(
type SomeType {}

type OtherType {}

val some_obj = new SomeType {};

assert(some_obj is SomeType);
assert(not(some_obj is OtherType));
)");
}

TEST_CASE("object property update", "[InterpreterTestChecking]")
{
  interpret(R"(
type SomeType {
  property a;
}

val some_obj = new SomeType { a: 1 };

assert(some_obj.a == 1);

some_obj.a := 2;

assert(some_obj.a == 2);
)");
}

TEST_CASE("interpreter should support prelude string and file helpers", "[InterpreterTest][Prelude]")
{
  interpret(R"(
        val content = "hello,world";
        val parts = split(content, ",");
        val reversed = reverse(parts);

        assert(content == "hello,world");
        assert(len(parts) == 2);
        assert(parts[0] == "hello");
        assert(parts[1] == "world");
        assert(join(parts, "-") == "hello-world");
        assert(trim("  hi  ") == "hi");
        assert(contains(content, "lo,wo"));
        assert(replace(content, "world", "ng") == "hello,ng");
        assert(startsWith(content, "hello"));
        assert(endsWith(content, "world"));
        assert(toUpper("Ng") == "NG");
        assert(toLower("Ng") == "ng");
        assert(reversed[0] == "world");
        assert(reversed[1] == "hello");
    )");
}

TEST_CASE("invalid unary operator usage", "[InterpreterTestChecking]")
{
  // operator query not implemented
  REQUIRE_THROWS_MATCHES(interpret("val x = ?5;"), NotImplementedException,
                         MessageMatches(ContainsSubstring("not implemented yet")));

  // cannot negate non-number
  REQUIRE_THROWS_MATCHES(interpret("val x = -false;"), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot negate a non-number")));

  // cannot negate unsigned number
  REQUIRE_THROWS_MATCHES(interpret("val x = -1u8;"), RuntimeException,
                         MessageMatches(ContainsSubstring("Cannot negate unsigned integers")));
}

TEST_CASE("unary operator usage", "[InterpreterTestChecking]")
{
  interpret(R"(
            val x = 1;
            val y = -1;
            assert(x == -y);
            assert(x != y);
            assert(!false);
            assert(!(!true));
            assert(0.0 > -1.0);
        )");
}

TEST_CASE("interpreter should copy array bindings by default", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        val original = [1, 2, 3];
        val copy = original;
        copy[0] := 9;
        assert(original[0] == 1);
        assert(copy[0] == 9);
    )");
}

TEST_CASE("interpreter should alias heap object bindings from new", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        type Box {
          property value;
        }

        val original = new Box { value: 1 };
        val copy = original;
        copy.value := 9;
        assert(original.value == 9);
        assert(copy.value == 9);
    )");
}

TEST_CASE("interpreter should support ref swap with move dereference", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        fun swap(a, b) {
          val tmp = move *a;
          *a := move *b;
          *b := move tmp;
        }

        val x = 1;
        val y = 2;
        swap(ref x, ref y);
        assert(x == 2);
        assert(y == 1);
    )");
}

TEST_CASE("interpreter should reject use after move at runtime", "[InterpreterTest][RefMove]")
{
  REQUIRE_THROWS_MATCHES(interpret(R"(
        val x = 1;
        val y = move x;
        val z = x;
    )"),
                         RuntimeException, MessageMatches(ContainsSubstring("Use after move")));
}

TEST_CASE("interpreter should copy function arguments by default", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        fun mutate(arr) {
          arr[0] := 9;
        }

        val source = [1, 2, 3];
        mutate(source);
        assert(source[0] == 1);
    )");
}

TEST_CASE("interpreter should copy return values by default", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        fun identity(value) {
          return value;
        }

        val source = [1, 2, 3];
        val copy = identity(source);
        copy[0] := 9;
        assert(source[0] == 1);
        assert(copy[0] == 9);
    )");
}

TEST_CASE("interpreter should support references to object properties", "[InterpreterTest][RefMove]")
{
  interpret(R"(
        type Box {
          property value;
        }

        val box = new Box { value: 1 };
        val ptr = ref box.value;
        *ptr := 7;
        assert(box.value == 7);
    )");
}

TEST_CASE("managed heap should sweep unreachable interpreter cycles", "[InterpreterTest][RefMove][GC]")
{
  NG::runtime::collect_managed_heap();
  REQUIRE(NG::runtime::managed_heap_size() == 0);

  auto ast = parse(R"(
        type Node {
          property link;
        }

        val node = new Node {};
        node.link := node;
    )");
  REQUIRE(ast != nullptr);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);
  REQUIRE(NG::runtime::managed_heap_size() == 1);

  delete intp;
  destroyast(ast);

  NG::runtime::collect_managed_heap();
  REQUIRE(NG::runtime::managed_heap_size() == 0);
}

TEST_CASE("Tuples", "[InterpreterTestChecking]")
{
  interpret(R"(
        val tup = (1, false, "hello");
        assert(tup.0 == 1);
        assert(tup.1 == false);
        assert(tup.2 == "hello");

        val (a, b, c) = tup;

        val (d, ...e) = tup;

        val f = unit;

        assert(f is unit);

        assert(a == d);

        val [x, ...y] = [1, 2, 3, 4, 5];

        assert(e.size == 2);
        assert(y[0] == 2);

        val [z, ...] = y;

        assert(z == 2);
        )");
}

TEST_CASE("interpreter should reject unpacking arity mismatches at runtime", "[InterpreterTestChecking][Failure]")
{
  REQUIRE_THROWS_MATCHES(interpret("val (a, b, c) = (1, 2);"), RuntimeException,
                         MessageMatches(ContainsSubstring("Tuple unpacking arity mismatch")));
  REQUIRE_THROWS_MATCHES(interpret("val [a, b] = [1];"), RuntimeException,
                         MessageMatches(ContainsSubstring("Array unpacking arity mismatch")));
}

TEST_CASE("loop bindings should stay scoped to the loop", "[InterpreterTestChecking]")
{
  interpret(R"(
        val x = 10;
        loop i = x {
          assert(i == 10);
        }
        assert(x == 10);
    )");
  REQUIRE_THROWS_MATCHES(interpret(R"(
        loop i = 1 {
        }
        val leaked = i;
    )"),
                         RuntimeException, MessageMatches(ContainsSubstring("Undefined binding: i")));
}

TEST_CASE("Tagged unions", "[InterpreterTestChecking]")
{
  interpret(R"(
        type Result = Ok(value: i32) | Err(msg: string);

        val success = Ok(42);
        val failure = Err("not found");

        switch (success) {
            case Ok(value) {
                assert(value == 42);
            }
            case Err(msg) {
                assert(false);
            }
        }

        switch (failure) {
            case Ok(value) {
                assert(false);
            }
            case Err(msg) {
                assert(msg == "not found");
            }
        }
        )");
}

TEST_CASE("Recursive tagged union refs", "[InterpreterTestChecking]")
{
  interpret(R"(
        type Node = Cell(content: i32, _next: ref<Node>) | Empty;

        val tail = Empty();
        val head = Cell(1, ref tail);

        switch (head) {
            case Cell(content, nextRef) {
                assert(content == 1);

                switch (*nextRef) {
                    case Empty {
                        assert(true);
                    }
                    case Cell(other, rest) {
                        assert(false);
                    }
                }
            }
            case Empty {
                assert(false);
            }
        }
        )");
}

TEST_CASE("new should allocate tagged union variants on heap", "[InterpreterTestChecking]")
{
  interpret(R"(
        type Node = Cell(content: i32, _next: ref<Node>) | Empty;

        val empty: ref<Node> = new Empty {};
        val head: ref<Node> = new Cell { content: 1, _next: new Empty {} };

        switch (*empty) {
            case Empty {
                assert(true);
            }
            case Cell(content, nextRef) {
                assert(false);
            }
        }

        switch (*head) {
            case Empty {
                assert(false);
            }
            case Cell(content, nextRef) {
                assert(content == 1);
                switch (*nextRef) {
                    case Empty {
                        assert(true);
                    }
                    case Cell(other, rest) {
                        assert(false);
                    }
                }
            }
        }
        )");
}

TEST_CASE("interpreter should handle recursive generic ref traversal", "[InterpreterTestChecking]")
{
  interpret(R"(
        type Node<T> = Cell(content: T, _next: ref<Node<T>>) | Empty;

        val empty: Node<i32> = Empty();
        val third = Cell(3, ref empty);
        val second = Cell(2, ref third);
        val first = Cell(1, ref second);

        fun printList<T>(head: ref<Node<T>>) {
            switch(*head) {
                case Empty {
                    return;
                }
                case Cell(value, rest) {
                    printList(rest);
                }
            }
        }

        printList(ref first);
        )");
}

TEST_CASE("interpreter should handle concrete recursive helper over instantiated union", "[InterpreterTestChecking]")
{
  interpret(R"(
        type Node<T> = Cell(content: T, _next: ref<Node<T>>) | Empty;

        val empty: Node<i32> = Empty();
        val third = Cell(3, ref empty);
        val second = Cell(2, ref third);
        val first = Cell(1, ref second);

        fun printNode(node: Node<i32>) {
            switch(node) {
                case Empty {
                    return;
                }
                case Cell(value, nextRef) {
                    printNode(*nextRef);
                }
            }
        }

        printNode(first);
	        )");
}

TEST_CASE("interpreter should dispatch inherited defaults through implemented trait names",
          "[InterpreterTest][Traits]")
{
  interpret(R"(
        type Box {}

        trait Parent {
            fun label(self: ref<Self>) -> string {
                return "parent";
            }
        }

        trait Child: Parent {}

        impl Child for Box {}

        val box = new Box {};
        assert(box.Child::label() == "parent");
        assert(box.Parent::label() == "parent");
        )");
}

TEST_CASE("generic function call (interpreter)", "[InterpreterTest]")
{
  interpret(R"(
        fun identity<T>(x: T) -> T {
            return x;
        }

        val result = identity(42);
        assert(result == 42);
        )");
}

TEST_CASE("generic function with multiple type params (interpreter)", "[InterpreterTest]")
{
  interpret(R"(
        fun pair<A, B>(a: A, b: B) -> (A, B) {
            return (a, b);
        }

        val p = pair(1, "hello");
        assert(true); // just make sure it runs without error
        )");
}

TEST_CASE("generic function with pack parameter (interpreter)", "[InterpreterTest]")
{
  interpret(R"(
        fun first<T...>(args: T...) -> i32 {
            return 42;
        }

        val result = first(1, "two", 3.0);
        assert(result == 42);
        )");
}

TEST_CASE("interpreter const if true branch", "[const_if][InterpreterTest]")
{
  interpret(R"(
        const if (true) {
            val x = 1;
            val y = 2;
            val z = x + y;
            assert(z == 3);
        } else {
            val z = 100;
            assert(z == 100);
        }
        )");
}

TEST_CASE("interpreter const if false branch", "[const_if][InterpreterTest]")
{
  interpret(R"(
        const if (false) {
            val z = 100;
            assert(z == 100);
        } else {
            val x = 1;
            val y = 2;
            val z = x + y;
            assert(z == 3);
        }
        )");
}

TEST_CASE("interpreter const if with negation", "[const_if][InterpreterTest]")
{
  interpret(R"(
        const if (!false) {
            val x = 42;
            assert(x == 42);
        } else {
            val x = 0;
            assert(x == 0);
        }
        )");
}

TEST_CASE("interpreter const if with equality", "[const_if][InterpreterTest]")
{
  interpret(R"(
        const if (1 == 2) {
            val x = 1;
        } else {
            val x = 2;
            assert(x == 2);
        }

        const if (1 == 1) {
            val x = 3;
            assert(x == 3);
        } else {
            val x = 4;
        }
        )");
}

TEST_CASE("interpreter const if without else", "[const_if][InterpreterTest]")
{
  interpret(R"(
        const if (true) {
            val x = 7;
            assert(x == 7);
        }

        const if (false) {
            val x = 999;
        }
        )");
}

TEST_CASE("interpreter const if should use typeof query result", "[const_if][InterpreterTest]")
{
  auto ast = parse(R"(
        type Box<T> {
          property value: T;
        }

        val box: ref<Box<i32>> = new Box<i32> { value: 42 };

        const if (typeof(box.value).name == "i32") {
            assert(box.value == 42);
        } else {
            assert(false);
        }
        )");
  REQUIRE(ast != nullptr);
  auto prelude_types = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, prelude_types);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);
  delete intp;
  destroyast(ast);
}

TEST_CASE("interpreter const if should use prelude is_ref predicate", "[const_if][InterpreterTest][Generics]")
{
  auto ast = parse(R"(
        fun value_kind<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            assert(false);
            return 1;
          } else {
            return 0;
          }
        }

        fun ref_kind<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 1;
          } else {
            assert(false);
            return 0;
          }
        }

        val value = 1;
        assert(value_kind(value) == 0);
        assert(ref_kind(ref value) == 1);
        )");
  REQUIRE(ast != nullptr);
  auto prelude_types = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, prelude_types);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);
  delete intp;
  destroyast(ast);
}

TEST_CASE("interpreter const if should keep decisions per generic instance",
          "[const_if][InterpreterTest][Generics]")
{
  auto ast = parse(R"(
        fun classify<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 1;
          } else {
            return 0;
          }
        }

        val value = 1;
        assert((classify(value) + classify(ref value)) == 1);
        )");
  REQUIRE(ast != nullptr);
  auto prelude_types = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, prelude_types);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);
  delete intp;
  destroyast(ast);
}

TEST_CASE("interpreter const if should keep decisions through nested generic calls",
          "[const_if][InterpreterTest][Generics]")
{
  auto ast = parse(R"(
        fun inner<T>(value: T) -> i32 {
          const if (is_ref<T>) {
            return 2;
          } else {
            return 3;
          }
        }

        fun outer<T>(value: T) -> i32 {
          return inner(value);
        }

        val value = 1;
        assert((outer(value) + outer(ref value)) == 5);
        )");
  REQUIRE(ast != nullptr);
  auto prelude_types = NG::typecheck::build_prelude_type_index();
  NG::typecheck::type_check(ast, prelude_types);

  Interpreter *intp = NG::intp::stupid();
  ast->accept(intp);
  delete intp;
  destroyast(ast);
}
