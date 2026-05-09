#include "typecheck_utils.hpp"

TEST_CASE("generic function definition should type check", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    fun id<T>(x: T) -> T = x;
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  // The generic function should be registered as a GenericDefType
  REQUIRE(index.contains("id"));
  check_type_tag(*index["id"], typeinfo_tag::GENERIC_DEF);

  destroyast(ast);
}

TEST_CASE("generic function call should infer types", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    fun id<T>(x: T) -> T = x;
    val result = id(42);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  // result should be inferred as i32 (since 42 is i32)
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic function with multiple type params", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    fun pair<A, B>(a: A, b: B) -> A = a;
    val result = pair(42, "hello");
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  // result should be inferred as i32 (return type is A = i32)
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic function with array type param", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    fun first<T>(arr: [T]) -> T = arr[0];
    val result = first([1, 2, 3]);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  // result should be inferred as i32
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic function preserves non-generic context", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    val x: int = 10;
    fun add<T>(a: T, b: T) -> T = a;
    val result = add(1, 2);
    val check = x + 1;
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);
  REQUIRE(index.contains("check"));
  check_type_tag(*index["check"], typeinfo_tag::I32);

  destroyast(ast);
}

// --- Tests using prelude functions (len, print, assert) ---

TEST_CASE("generic function should work with prelude len()", "[TypeCheck][Generic][Prelude]")
{
  auto prelude_types = build_prelude_type_index();

  auto ast = parse(R"(
    fun count<T...>(args: T...) -> u32 {
      return args.size;
    }
    val c = count(1, "two", 3.0, true);
    val n = len([1, 2, 3]);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast, prelude_types);

  // count returns u32
  REQUIRE(index.contains("c"));
  check_type_tag(*index["c"], typeinfo_tag::U32);

  // len returns u32
  REQUIRE(index.contains("n"));
  check_type_tag(*index["n"], typeinfo_tag::U32);

  destroyast(ast);
}

TEST_CASE("generic function with print should type check", "[TypeCheck][Generic][Prelude]")
{
  auto prelude_types = build_prelude_type_index();

  auto ast = parse(R"(
    fun greet<T...>(args: T...) {
      print(...args);
    }
    val result = greet("hello", 42);
  )");

  REQUIRE(ast != nullptr);

  // Should not throw — print accepts any type
  auto index = type_check(ast, prelude_types);
  REQUIRE(index.contains("result"));

  destroyast(ast);
}

TEST_CASE("generic function with assert should type check", "[TypeCheck][Generic][Prelude]")
{
  auto prelude_types = build_prelude_type_index();

  auto ast = parse(R"(
    fun checkEqual<T>(a: T, b: T) {
      assert(a == b);
    }
    val result = checkEqual(42, 42);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast, prelude_types);
  REQUIRE(index.contains("result"));

  destroyast(ast);
}

TEST_CASE("len() should work with array literal", "[TypeCheck][Prelude][len]")
{
  auto prelude_types = build_prelude_type_index();

  auto ast = parse(R"(
    val arr = [1, 2, 3, 4, 5];
    val n = len(arr);
    val m = len(["a", "b"]);
    val s = len("hello");
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast, prelude_types);

  REQUIRE(index.contains("n"));
  check_type_tag(*index["n"], typeinfo_tag::U32);

  REQUIRE(index.contains("m"));
  check_type_tag(*index["m"], typeinfo_tag::U32);

  REQUIRE(index.contains("s"));
  check_type_tag(*index["s"], typeinfo_tag::U32);

  destroyast(ast);
}

TEST_CASE("generic object type declaration should instantiate from annotation", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    val box: ref<Box<i32>> = new Box<i32> { value: 42 };
    val n = box.value;
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("box"));
  REQUIRE(index["box"]->repr() == "ref<Box<i32>>");
  REQUIRE(index.contains("n"));
  check_type_tag(*index["n"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic type alias and newtype declarations should instantiate", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Pair<T> = (T, T);
    type Id<T> wraps T;

    val pair: Pair<string> = ("a", "b");
    val raw = cast<Id<i32>>(42);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("pair"));
  REQUIRE(index["pair"]->repr() == "Pair<string>");
  REQUIRE(index.contains("raw"));
  REQUIRE(index["raw"]->repr() == "Id<i32>");

  destroyast(ast);
}

TEST_CASE("generic tagged union constructors should infer instantiated type", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Result<T> = Ok(value: T) | Err(msg: string);
    val success = Ok(42);
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("success"));
  REQUIRE(index["success"]->tag() == typeinfo_tag::VARIANT);
  auto *variant = dynamic_cast<VariantType *>(&*index["success"]);
  REQUIRE(variant != nullptr);
  REQUIRE(variant->unionName.find("Result<i32>") != Str::npos);

  destroyast(ast);
}

TEST_CASE("generic unit variants should use expected type for instantiation", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Node<T> = Cell(content: T, _next: ref<Node<T>>) | Empty;
    val empty: Node<i32> = Empty();
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("empty"));
  REQUIRE(index["empty"]->tag() == typeinfo_tag::TAGGED_UNION);

  auto *emptyType = dynamic_cast<TaggedUnionType *>(&*index["empty"]);
  REQUIRE(emptyType != nullptr);
  REQUIRE(emptyType->name == "Node<i32>");

  destroyast(ast);
}

TEST_CASE("recursive generic ref traversal should type check", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
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
        case Cell(first, rest) {
          printList(rest);
        }
      }
    }

    printList(ref first);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("first"));
  destroyast(ast);
}

TEST_CASE("generic type annotation should require explicit type arguments", "[TypeCheck][Generic][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    val box: Box = unit;
  )", "requires type arguments");
}

TEST_CASE("non generic type should reject generic arguments", "[TypeCheck][Generic][Failure]")
{
  typecheck_failure(R"(
    type Plain {
      property value: i32;
    }

    val box: Plain<i32> = unit;
  )", "is not generic");
}

TEST_CASE("generic instantiated types should compose in function signatures", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    fun unwrap(box: Box<i32>) -> i32 {
      return box.value;
    }
  )");

  REQUIRE(ast != nullptr);

  auto index = type_check(ast);

  REQUIRE(index.contains("unwrap"));
  auto *funType = dynamic_cast<FunctionType *>(&*index["unwrap"]);
  REQUIRE(funType != nullptr);
  REQUIRE(funType->parametersType.size() == 1);
  REQUIRE(funType->parametersType[0]->repr() == "Box<i32>");
  check_type_tag(*funType->returnType, typeinfo_tag::I32);

  destroyast(ast);
}
