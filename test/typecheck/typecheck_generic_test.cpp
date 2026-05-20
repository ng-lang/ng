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

    fun use<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = use<Box, i32>(box);
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

    fun use<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = use(box);
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
      fun use(self: ref<Self>, value: F<T>) -> unit;
    }
  )");

  REQUIRE(ast != nullptr);
  auto index = type_check(ast);
  REQUIRE(index.contains("Uses"));
  check_type_tag(*index["Uses"], typeinfo_tag::TRAIT);

  destroyast(ast);
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

    fun use<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val box = new Box<i32> { value: 42 };
    val result = use<Box<i32>, i32>(box);
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

    fun use<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val pair = new Pair<i32, string> { left: 42, right: "x" };
    val result = use<Pair, i32>(pair);
  )", "expects a type constructor with 1 argument(s), got 2");
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

    fun use<F<_>, T>(value: ref<F<T>>) -> unit = unit;

    val other = new Other<i32> { value: 42 };
    val result = use<Box, i32>(other);
  )", "Invalid argument type for generic function");
}
