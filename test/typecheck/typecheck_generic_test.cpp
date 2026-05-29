#include "typecheck_utils.hpp"
#include <module.hpp>

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

TEST_CASE("generic function call should record canonical and mangled instance names",
          "[TypeCheck][Generic][Mangling]")
{
  auto ast = parse(R"(
    fun id<T>(x: T) -> T = x;
    val result = id(42);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("result"));

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[1]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  auto call = dynamic_ast_cast<FunCallExpression>(valStmt->value);
  REQUIRE(call != nullptr);
  REQUIRE(call->genericInstanceName == "id<i32>");
  REQUIRE(call->mangledCalleeName == "$NG2:v11:F7:default2:id1:13:i32");
  REQUIRE(call->resolvedCalleeName == call->mangledCalleeName);

  destroyast(ast);
}

TEST_CASE("generic function mangling uses definition module and module-qualified nominal args",
          "[TypeCheck][Generic][Mangling]")
{
  auto ast = parse(R"(
    type User {
      property id: i32;
    }

    fun id<T>(x: T) -> T = x;

    val user = new User { id: 1 };
    val result = id(user);
  )", "app.main");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("result"));

  auto compileUnit = dynamic_ast_cast<CompileUnit>(ast);
  REQUIRE(compileUnit != nullptr);
  auto valDef = dynamic_ast_cast<ValDef>(compileUnit->module->definitions[3]);
  REQUIRE(valDef != nullptr);
  auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body);
  REQUIRE(valStmt != nullptr);
  auto call = dynamic_ast_cast<FunCallExpression>(valStmt->value);
  REQUIRE(call != nullptr);
  REQUIRE(call->genericInstanceName == "id<ref<User>>");
  REQUIRE(call->mangledCalleeName == "$NG2:v11:F8:app.main2:id1:119:ref<app.main::User>");

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

TEST_CASE("concrete recursive union helper should expose tagged-union param types", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Node<T> = Cell(content: T, _next: ref<Node<T>>) | Empty;
    val empty: Node<i32> = Empty();
    val first = Cell(1, ref empty);

    fun f(node: Node<i32>) {
      return;
    }
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);

  REQUIRE(index.contains("f"));
  REQUIRE(index.contains("first"));

  auto *funType = dynamic_cast<FunctionType *>(&*index["f"]);
  REQUIRE(funType != nullptr);
  REQUIRE(funType->parametersType.size() == 1);
  REQUIRE(funType->parametersType[0]->tag() == typeinfo_tag::TAGGED_UNION);
  auto *firstVariant = dynamic_cast<VariantType *>(&*index["first"]);
  REQUIRE(firstVariant != nullptr);
  REQUIRE(firstVariant->unionName == "Node<i32>");

  destroyast(ast);
}

TEST_CASE("concrete recursive union helper call should type check", "[TypeCheck][Generic]")
{
  auto ast = parse(R"(
    type Node<T> = Cell(content: T, _next: ref<Node<T>>) | Empty;
    val empty: Node<i32> = Empty();
    val first = Cell(1, ref empty);

    fun f(node: Node<i32>) {
      return;
    }

    f(first);
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

TEST_CASE("generic type partial specialization should resolve alias body", "[TypeCheck][Generic][Specialization]")
{
  auto ast = parse(R"(
    type deref<T>;
    type<T> deref<ref<T>> = T;

    val value: deref<ref<i32>> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("value"));
  check_type_tag(*index["value"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("abstract generic type alias should reject unmatched instantiations",
          "[TypeCheck][Generic][Specialization][Failure]")
{
  typecheck_failure(R"(
    type deref<T>;

    val value: deref<i32> = 1;
  )", "Abstract type alias");
}

TEST_CASE("generic type partial specialization should choose the most specific matching pattern",
          "[TypeCheck][Generic][Specialization]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    type unwrap<T> = string;
    type<T> unwrap<ref<T>> = T;
    type<T> unwrap<ref<Box<T>>> = T;

    val value: unwrap<ref<Box<i32>>> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("value"));
  check_type_tag(*index["value"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("deleted ref specialization should reject nested references", "[TypeCheck][Generic][Specialization][Failure]")
{
  typecheck_failure(R"(
    type<T> ref<ref<T>> = delete;

    type Box {
      property value: i32;
    }

    val box = new Box { value: 1 };
    val invalid: ref<ref<Box>> = ref box;
  )", "deleted");
}

TEST_CASE("deleted primary type alias should reject matching type use",
          "[TypeCheck][Generic][Delete][Failure]")
{
  typecheck_failure(R"(
    type Blocked<T> = delete;
    val value: Blocked<i32> = 1;
  )", "deleted");
}

TEST_CASE("deleted generic function overload should reject exact matching calls",
          "[TypeCheck][Generic][Delete][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    fun<T> take_box(value: Box<T>) = delete;

    val box: Box<i32> = unit;
    take_box(box);
  )", "Function overload is deleted");
}

TEST_CASE("more specific deleted generic function overload should beat valid fallback",
          "[TypeCheck][Generic][Delete][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    fun<T> accept(value: T) -> T = value;
    fun<T> accept(value: Box<T>) = delete;

    val box: Box<i32> = unit;
    val invalid = accept(box);
  )", "Function overload is deleted");
}

TEST_CASE("non matching deleted generic function overload should not block valid fallback",
          "[TypeCheck][Generic][Delete]")
{
  auto ast = parse(R"(
    fun<T> accept(value: T) -> T = value;
    fun<T> accept(value: ref<T>) = delete;

    val valid = accept(1);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("valid"));
  check_type_tag(*index["valid"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("deleted const specialization should reject matching predicate use",
          "[TypeCheck][Generic][Delete][Failure]")
{
  typecheck_failure(R"(
    type Box {
      property value: i32;
    }

    const is_bad<T>: bool = native;
    const<T> is_bad<ref<T>>: bool = delete;

    fun<T> reject_ref(value: T) -> unit where !is_bad<T> = unit;

    val box = new Box { value: 1 };
    reject_ref(box);
  )", "Const specialization is deleted");
}

TEST_CASE("native const predicate where clause should accept matching generic calls",
          "[TypeCheck][Generic][ConstPredicate]")
{
  auto ast = parse(R"(
    const is_ref<T>: bool = native;

    fun accept_value<T>(value: T) -> T where !is_ref<T> = value;
    val result = accept_value(1);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("native const predicate where clause should reject ref generic calls",
          "[TypeCheck][Generic][ConstPredicate][Failure]")
{
  typecheck_failure(R"(
    const is_ref<T>: bool = native;

    fun accept_value<T>(value: T) -> T where !is_ref<T> = value;
    val value = 1;
    val invalid = accept_value(ref value);
  )", "Where predicate is not satisfied");
}

TEST_CASE("const predicate definitions should reject non-bool native return types",
          "[TypeCheck][Generic][ConstPredicate][Failure]")
{
  typecheck_failure(R"(
    const is_ref<T>: i32 = native;
  )", "Native const predicate must return bool");
}

TEST_CASE("const predicate definitions should reject value type mismatches",
          "[TypeCheck][Generic][ConstPredicate][Failure]")
{
  typecheck_failure(R"(
    const is_ref<T>: bool = 1;
  )", "Const definition type mismatch");
}

TEST_CASE("const definitions should accept typed compile-time scalar values",
          "[TypeCheck][Generic][ConstPredicate]")
{
  auto ast = parse(R"(
    const always<T>: bool = true;
    const one<T>: i32 = 1;
    const label<T>: string = "value";
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("const functions should compute const definitions through STUPID",
          "[TypeCheck][Generic][ConstFun]")
{
  auto ast = parse(R"(
    const fun add(a: i32, b: i32) -> i32 {
      return a + b;
    }

    const answer: i32 = add(20, 22);
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("const functions should fold const if conditions",
          "[TypeCheck][Generic][ConstFun]")
{
  auto ast = parse(R"(
    const fun large(value: i32) -> bool {
      return value > 10;
    }

    const if (large(20)) {
      val ok: i32 = 1;
    } else {
      val impossible: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("const functions should satisfy where predicates",
          "[TypeCheck][Generic][ConstFun]")
{
  auto ast = parse(R"(
    const fun positive(value: i32) -> bool {
      return value > 0;
    }

    fun id<const N: i32>(value: i32) -> i32 where positive(N) = value;

    val result = id<1>(2);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const functions should support recursive STUPID evaluation",
          "[TypeCheck][Generic][ConstFun]")
{
  auto ast = parse(R"(
    const fun fact(n: i32) -> i32 {
      if (n == 0) {
        return 1;
      } else {
        return n * fact(n - 1);
      }
    }

    const six: i32 = fact(3);
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast));

  destroyast(ast);
}

TEST_CASE("const functions should allow runtime calls with non-const arguments",
          "[TypeCheck][Generic][ConstFun]")
{
  auto ast = parse(R"(
    const fun add_one(value: i32) -> i32 {
      return value + 1;
    }

    val input: i32 = 41;
    val output: i32 = add_one(input);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("output"));
  check_type_tag(*index["output"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const functions should reject non-const calls during compile-time execution",
          "[TypeCheck][Generic][ConstFun][Failure]")
{
  typecheck_failure(R"(
    fun runtime_value() -> i32 {
      return 1;
    }

    const fun invalid() -> i32 {
      return runtime_value();
    }

    const value: i32 = invalid();
  )", "Non-const function cannot be called from const function");
}

TEST_CASE("const functions should reject native declarations",
          "[TypeCheck][Generic][ConstFun][Failure]")
{
  typecheck_failure(R"(
    const fun invalid() -> i32 = native;
  )", "Const function cannot be native or deleted");
}

TEST_CASE("std tuple const predicates should evaluate through prelude",
          "[TypeCheck][Generic][EnhancedTuple]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    const if (is_tuple<(i32, string)>) {
      val ok: i32 = 1;
    } else {
      val impossible: i32 = false;
    }

    const if (tuple_size<(i32, string, bool)> == 3) {
      val ok2: i32 = 2;
    } else {
      val impossible2: i32 = false;
    }

    const if (sizeof_pack<i32, string, bool> == 3) {
      val ok3: i32 = 3;
    } else {
      val impossible3: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast, preludeTypes));

  destroyast(ast);
}

TEST_CASE("prelude tuple const predicates survive module registry clears",
          "[TypeCheck][Generic][EnhancedTuple][Prelude]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  NG::module::clear_module_loader_cache();
  NG::module::get_module_registry().clear();

  auto ast = parse(R"(
    const if (is_tuple<(i32, string)>) {
      val ok: i32 = 1;
    } else {
      val impossible: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast, preludeTypes));

  destroyast(ast);
}

TEST_CASE("std tuple module should be directly importable",
          "[TypeCheck][Generic][EnhancedTuple][Module]")
{
  auto ast = parse(R"(
    import std.tuple (*);

    const if (is_tuple<(i32, string)> && tuple_size<(i32, string)> == 2) {
      val ok: i32 = 1;
    } else {
      val impossible: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  REQUIRE_NOTHROW(type_check(ast, {}, {"lib", "../lib"}));

  destroyast(ast);
}

TEST_CASE("std tuple_element should project tuple element types through prelude",
          "[TypeCheck][Generic][EnhancedTuple]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    val first: tuple_element<(i32, string, bool), 0> = 1;
    val second: tuple_element<(i32, string, bool), 1> = "value";
  )");
  REQUIRE(ast != nullptr);
  INFO(ast->repr());

  auto index = type_check(ast, preludeTypes);
  REQUIRE(index.contains("first"));
  REQUIRE(index.contains("second"));
  check_type_tag(*index["first"], typeinfo_tag::I32);
  check_type_tag(*index["second"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("enhanced tuple pack-to-tuple return should expand parameter packs",
          "[TypeCheck][Generic][EnhancedTuple]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    fun gather<T...>(args: T...) -> (T...) {
      return (...args,);
    }

    val packed: (i32, string, bool) = gather(1, "value", true);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, preludeTypes);
  REQUIRE(index.contains("packed"));
  auto tupleType = std::dynamic_pointer_cast<TupleType>(index["packed"]);
  REQUIRE(tupleType != nullptr);
  REQUIRE(tupleType->elementTypes.size() == 3);
  check_type_tag(*tupleType->elementTypes[0], typeinfo_tag::I32);
  check_type_tag(*tupleType->elementTypes[1], typeinfo_tag::STRING);
  check_type_tag(*tupleType->elementTypes[2], typeinfo_tag::BOOL);

  destroyast(ast);
}

TEST_CASE("enhanced tuple_concat should concatenate tuple types",
          "[TypeCheck][Generic][EnhancedTuple]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    val joined: tuple_concat<(i32, string), (bool, i32)> = (1, "value", true, 2);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, preludeTypes);
  REQUIRE(index.contains("joined"));
  auto tupleType = std::dynamic_pointer_cast<TupleType>(index["joined"]);
  REQUIRE(tupleType != nullptr);
  REQUIRE(tupleType->elementTypes.size() == 4);

  destroyast(ast);
}

TEST_CASE("enhanced tuple spread should apply to fixed arity calls",
          "[TypeCheck][Generic][EnhancedTuple]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    fun middle(left: i32, value: string, ok: bool) -> string {
      return value;
    }

    val result = middle(...(1, "spread", true));
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, preludeTypes);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::STRING);

  destroyast(ast);
}

TEST_CASE("enhanced tuple spread should apply to trait-qualified calls",
          "[TypeCheck][Generic][EnhancedTuple][Traits]")
{
  auto preludeTypes = NG::typecheck::build_prelude_type_index();
  auto ast = parse(R"(
    type Accumulator {
      base: i32;
    }

    trait Combine {
      fun combine(self: ref<Self>, left: i32, right: i32) -> i32;
    }

    impl Combine for Accumulator {
      fun combine(self: ref<Self>, left: i32, right: i32) -> i32 {
        return self.base + left + right;
      }
    }

    val accumulator = new Accumulator { base: 1 };
    val args = (2, 3);
    val qualified = Combine::combine(accumulator, ...args);
    val receiverQualified = accumulator.Combine::combine(...args);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, preludeTypes);
  REQUIRE(index.contains("qualified"));
  REQUIRE(index.contains("receiverQualified"));
  check_type_tag(*index["qualified"], typeinfo_tag::I32);
  check_type_tag(*index["receiverQualified"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("prelude is_ref const predicate should fold const if by generic type",
          "[TypeCheck][Generic][ConstPredicate]")
{
  auto ast = parse(R"(
    fun value_kind<T>(value: T) -> i32 {
      const if (is_ref<T>) {
        val impossible: i32 = false;
        return 1;
      } else {
        return 0;
      }
    }

    fun ref_kind<T>(value: T) -> i32 {
      const if (is_ref<T>) {
        return 1;
      } else {
        val impossible: i32 = false;
        return 0;
      }
    }

    val value = 1;
    val value_result = value_kind(value);
    val ref_result = ref_kind(ref value);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("value_result"));
  REQUIRE(index.contains("ref_result"));
  check_type_tag(*index["value_result"], typeinfo_tag::I32);
  check_type_tag(*index["ref_result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const predicate specialization should choose the most specific matching pattern",
          "[TypeCheck][Generic][ConstPredicate][Specialization]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    const is_box_ref<T>: bool = false;
    const<T> is_box_ref<ref<T>>: bool = false;
    const<T> is_box_ref<ref<Box<T>>>: bool = true;

    fun accept_box_ref<T>(value: T) -> i32 where is_box_ref<T> = 7;

    fun marker<T>(value: T) -> i32 {
      const if (is_box_ref<T>) {
        return 11;
      } else {
        val impossible: i32 = false;
        return 0;
      }
    }

    val boxed = new Box<i32> { value: 1 };
    val accepted = accept_box_ref(boxed);
    val marked = marker(boxed);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("accepted"));
  REQUIRE(index.contains("marked"));
  check_type_tag(*index["accepted"], typeinfo_tag::I32);
  check_type_tag(*index["marked"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const predicate specialization should honor where predicates",
          "[TypeCheck][Generic][ConstPredicate][Specialization]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    const is_box<T>: bool = false;
    const<T> is_box<Box<T>>: bool = true;

    const is_box_ref<T>: bool = false;
    const<T> is_box_ref<ref<T>> where is_box<T>: bool = true;

    const is_plain_ref<T>: bool = false;
    const<T> is_plain_ref<ref<T>>: bool = true;
    const<T> is_plain_ref<ref<T>> where is_box<T>: bool = false;

    val box_category: i32 = 0;
    const if (is_box_ref<ref<Box<i32>>>) {
      box_category := 2;
    } else {
      val impossible: i32 = false;
    }

    val ref_category: i32 = 0;
    const if (is_plain_ref<ref<i32>>) {
      ref_category := 1;
    } else {
      val impossible: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("box_category"));
  REQUIRE(index.contains("ref_category"));
  check_type_tag(*index["box_category"], typeinfo_tag::I32);
  check_type_tag(*index["ref_category"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const predicate specialization should honor where trait bounds",
          "[TypeCheck][Generic][ConstPredicate][Specialization]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    trait Debug {
      fun inspect(self: ref<Self>) -> string;
    }

    type Visible {
      property value: i32;
    }

    impl Show for Visible {
      fun show(self: ref<Self>) -> string {
        return "visible";
      }
    }

    impl Debug for Visible {
      fun inspect(self: ref<Self>) -> string {
        return "visible";
      }
    }

    type Silent {
      property value: i32;
    }

    impl Show for Silent {
      fun show(self: ref<Self>) -> string {
        return "silent";
      }
    }

    const is_show_debug_ref<T>: bool = false;
    const<T> is_show_debug_ref<ref<T>> where T: Show + Debug && is_ref<ref<T>>: bool = true;

    fun classify<T>(value: T) -> i32 {
      const if (is_show_debug_ref<T>) {
        return 1;
      } else {
        return 0;
      }
    }

    val visible = new Visible { value: 1 };
    val silent = new Silent { value: 2 };
    val matched = classify(visible);
    val fallback = classify(silent);
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("matched"));
  REQUIRE(index.contains("fallback"));
  check_type_tag(*index["matched"], typeinfo_tag::I32);
  check_type_tag(*index["fallback"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("native const predicates should classify trait abstract and concrete types",
          "[TypeCheck][Generic][ConstPredicate]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    type NativeHandle = native;
    type AbstractHandle;

    type Widget {
      property value: i32;
    }

    const if (is_trait<Show>) {
      val trait_result = 1;
    } else {
      val impossible: i32 = false;
    }

    const if (!is_abstract<NativeHandle>) {
      val native_result = 1;
    } else {
      val impossible: i32 = false;
    }

    const if (is_abstract<AbstractHandle>) {
      val abstract_result = 1;
    } else {
      val impossible: i32 = false;
    }

    const if (!is_abstract<Widget>) {
      val concrete_result = 1;
    } else {
      val impossible: i32 = false;
    }
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("Show"));
  REQUIRE(index.contains("NativeHandle"));
  REQUIRE(index.contains("AbstractHandle"));
  REQUIRE(index.contains("Widget"));

  destroyast(ast);
}

TEST_CASE("native const predicates should participate in type specialization where clauses",
          "[TypeCheck][Generic][ConstPredicate][Specialization]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    type NativeHandle = native;
    type AbstractHandle;
    type Widget {
      property value: i32;
    }

    type classify<T>;
    type<T> classify<T>: where is_trait<T> = bool;
    type<T> classify<T>: where is_abstract<T> = string;
    type<T> classify<T> = i32;

    val trait_value: classify<Show> = true;
    val abstract_value: classify<AbstractHandle> = "abstract";
    val native_value: classify<NativeHandle> = 7;
    val concrete_value: classify<Widget> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("trait_value"));
  REQUIRE(index.contains("abstract_value"));
  REQUIRE(index.contains("native_value"));
  REQUIRE(index.contains("concrete_value"));
  check_type_tag(*index["trait_value"], typeinfo_tag::BOOL);
  check_type_tag(*index["abstract_value"], typeinfo_tag::STRING);
  check_type_tag(*index["native_value"], typeinfo_tag::I32);
  check_type_tag(*index["concrete_value"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic type partial specialization should honor where predicates",
          "[TypeCheck][Generic][Specialization]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    const is_box<T>: bool = false;
    const<T> is_box<Box<T>>: bool = true;

    type unwrap<T>;
    type<T> unwrap<ref<T>>: where is_box<T> = bool;
    type<T> unwrap<ref<T>> = T;

    val boxed = new Box<i32> { value: 1 };
    val box_result: unwrap<ref<Box<i32>>> = true;
    val value = 1;
    val value_result: unwrap<ref<i32>> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast);
  REQUIRE(index.contains("box_result"));
  REQUIRE(index.contains("value_result"));
  check_type_tag(*index["box_result"], typeinfo_tag::BOOL);
  check_type_tag(*index["value_result"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic type partial specialization should honor where trait bounds",
          "[TypeCheck][Generic][Specialization]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    trait Debug {
      fun inspect(self: ref<Self>) -> string;
    }

    type Visible {
      property value: i32;
    }

    impl Show for Visible {
      fun show(self: ref<Self>) -> string {
        return "visible";
      }
    }

    impl Debug for Visible {
      fun inspect(self: ref<Self>) -> string {
        return "visible";
      }
    }

    type<T> classify<T>: where T: Show + Debug && is_ref<T> = bool;
    type<T> classify<T> = i32;

    val visible = new Visible { value: 1 };
    val matched: classify<ref<Visible>> = true;
    val fallback: classify<Visible> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("matched"));
  REQUIRE(index.contains("fallback"));
  check_type_tag(*index["matched"], typeinfo_tag::BOOL);
  check_type_tag(*index["fallback"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("generic type partial specialization should reject unsatisfied where trait bounds",
          "[TypeCheck][Generic][Specialization]")
{
  auto ast = parse(R"(
    trait Show {
      fun show(self: ref<Self>) -> string;
    }

    trait Debug {
      fun inspect(self: ref<Self>) -> string;
    }

    type Visible {
      property value: i32;
    }

    impl Show for Visible {
      fun show(self: ref<Self>) -> string {
        return "visible";
      }
    }

    type<T> classify<T>: where T: Show + Debug && is_ref<T> = bool;
    type<T> classify<T> = i32;

    val visible = new Visible { value: 1 };
    val fallback: classify<ref<Visible>> = 1;
  )");
  REQUIRE(ast != nullptr);

  auto index = type_check(ast, build_prelude_type_index());
  REQUIRE(index.contains("fallback"));
  check_type_tag(*index["fallback"], typeinfo_tag::I32);

  destroyast(ast);
}

TEST_CASE("const predicate specialization should reject unsatisfied where clauses",
          "[TypeCheck][Generic][ConstPredicate][Specialization][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    const is_box_ref<T>: bool = false;
    const<T> is_box_ref<ref<T>>: bool = false;
    const<T> is_box_ref<ref<Box<T>>>: bool = true;

    fun accept_box_ref<T>(value: T) -> i32 where is_box_ref<T> = 7;

    val value = 1;
    val invalid = accept_box_ref(ref value);
  )", "Where predicate is not satisfied");
}

TEST_CASE("higher-kinded generic function should accept explicit unary type constructor",
          "[TypeCheck][Generic][HKT]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    fun accept_hkt<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = accept_hkt<Box, i32>(box);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::UNIT);

  destroyast(ast);
}

TEST_CASE("higher-kinded generic function should infer constructor and inner type from argument",
          "[TypeCheck][Generic][HKT]")
{
  auto ast = parse(R"(
    type Box<T> {
      property value: T;
    }

    fun accept_hkt<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = accept_hkt(box);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::UNIT);

  destroyast(ast);
}

TEST_CASE("higher-kinded trait declaration should type check method signatures",
          "[TypeCheck][Generic][HKT]")
{
  auto ast = parse(R"(
    trait Uses<F<_>, T> {
      fun accept(self: ref<Self>, value: F<T>) -> unit;
    }
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("Uses"));
  check_type_tag(*index["Uses"], typeinfo_tag::TRAIT);

  destroyast(ast);
}

TEST_CASE("variadic higher-kinded generic should accept matching pack constructor",
          "[TypeCheck][Generic][HKT][Pack]")
{
  auto ast = parse(R"(
    type Variadic<Head, Tail...> = native;

    fun accept<F<_, ...>>() -> unit = unit;

    val result = accept<Variadic>();
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("result"));
  check_type_tag(*index["result"], typeinfo_tag::UNIT);

  destroyast(ast);
}

TEST_CASE("variadic higher-kinded generic should reject fixed constructor",
          "[TypeCheck][Generic][HKT][Pack][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    fun accept<F<_, ...>>() -> unit = unit;

    val result = accept<Box>();
  )", "expects a type constructor with 1 fixed argument(s) and a variadic tail");
}

TEST_CASE("variadic higher-kinded application should reject too few type arguments",
          "[TypeCheck][Generic][HKT][Pack][Failure]")
{
  typecheck_failure(R"(
    fun accept<F<_, ...>>(value: F<>) -> unit = unit;
  )", "requires type arguments");
}

TEST_CASE("higher-kinded generic should reject applying first-order type parameter",
          "[TypeCheck][Generic][HKT][Failure]")
{
  typecheck_failure(R"(
    fun bad<T, U>(value: T<U>) -> unit = unit;
  )", "Type parameter 'T' is not a type constructor");
}

TEST_CASE("higher-kinded generic should reject instantiated type where constructor is expected",
          "[TypeCheck][Generic][HKT][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    fun accept_hkt<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = accept_hkt<Box<i32>, i32>(box);
  )", "expects a type constructor, not an instantiated type");
}

TEST_CASE("higher-kinded generic should reject constructor arity mismatch",
          "[TypeCheck][Generic][HKT][Failure]")
{
  typecheck_failure(R"(
    type Pair<A, B> {
      property left: A;
      property right: B;
    }

    fun accept_hkt<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val pair = new Pair<i32, string> { left: 42, right: "x" };
    val result = accept_hkt<Pair, i32>(pair);
  )", "expects a type constructor with 1 fixed argument(s), got 2 fixed argument(s)");
}

TEST_CASE("higher-kinded generic should reject argument that does not match explicit constructor",
          "[TypeCheck][Generic][HKT][Failure]")
{
  typecheck_failure(R"(
    type Box<T> {
      property value: T;
    }

    type Other<T> {
      property value: T;
    }

    fun accept_hkt<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val other = new Other<i32> { value: 42 };
    val result = accept_hkt<Box, i32>(other);
  )", "Invalid argument type for generic function");
}

TEST_CASE("generic inference should reject inconsistent repeated parameter bindings",
          "[TypeCheck][Generic][Failure]")
{
  typecheck_failure(R"(
    fun choose<T>(pair: (T, T)) -> T {
      return pair.0;
    }

    val result = choose((1, "x"));
  )", "Inconsistent bindings for generic parameter 'T'");
}

TEST_CASE("const generic parameters should instantiate distinct native types", "[TypeCheck][Generic][Const]")
{
  auto ast = parse(R"(
    type Buffer<T, const N: u32> = native;

    val a: Buffer<i32, 4> = unit;
    val b: Buffer<i32, 8> = unit;
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index["a"]->repr() == "Buffer<i32, 4:i64>");
  REQUIRE(index["b"]->repr() == "Buffer<i32, 8:i64>");
  REQUIRE_FALSE(index["a"]->match(*index["b"]));
  destroyast(ast);
}

TEST_CASE("const generic function should accept explicit const arguments", "[TypeCheck][Generic][Const]")
{
  auto ast = parse(R"(
    fun<const N: u32> id_len(value: array<i32, N>) -> array<i32, N> {
      return value;
    }

    val xs: array<i32, 3> = [1, 2, 3];
    val ys = id_len<3>(xs);
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  check_type_tag(*index["ys"], typeinfo_tag::ARRAY);
  destroyast(ast);
}

TEST_CASE("const generic arguments should reject type values and missing array length",
          "[TypeCheck][Generic][Const][Failure]")
{
  typecheck_failure(R"(
    type Buffer<T, const N: u32> = native;
    val bad: Buffer<i32, i32> = unit;
  )", "expects a compile-time constant argument");

  typecheck_failure(R"(
    type Buffer<T> = native;
    val bad: Buffer<4> = unit;
  )", "expects a type argument");

  typecheck_failure(R"(
    type Buffer<T, const N: u32> = native;
    val bad: Buffer<i32, "wide"> = unit;
  )", "expects const u32");

  typecheck_failure(R"(
    val bad: array<i32> = [1];
  )", "Fixed array type expects 2 generic arguments");
}
