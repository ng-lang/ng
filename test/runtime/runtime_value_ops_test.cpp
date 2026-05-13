#include "../test.hpp"
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/index_layout_access.hpp>
#include <runtime/native_marshaling.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/string_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_access.hpp>
#include <runtime/value_ops.hpp>

using namespace NG::runtime;
using namespace NG::runtime::native;
using namespace NG::runtime::ops;

TEST_CASE("runtime symbol tables back module globals and env dispatch", "[RuntimeTest][Runtime]")
{
  auto root = makert<RuntimeSymbolTable>();
  auto sampleType = makert<NGType>();
  sampleType->name = "Sample";

  auto answerSlot = numeral_cell_from_value<int32_t>(42);
  answerSlot->storageClass = StorageClass::GLOBAL;
  answerSlot->name = "answer";
  root->objectSlots["answer"] = answerSlot;
  root->functions["unit"] = [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
    return unit_cell();
  };
  root->types["Sample"] = sampleType;

  auto env = make_runtime_env(root);
  REQUIRE(read_inline_cell_bytes<int32_t>(root->objectSlots.at("answer")) == 42);
  REQUIRE(answerSlot->storageClass == StorageClass::GLOBAL);
  REQUIRE(root->functions.contains("unit"));
  REQUIRE(root->types.at("Sample") == sampleType);
  auto module = make_runtime_module(root);
  REQUIRE(runtime_module_object_slots(module).contains("answer"));
  REQUIRE(runtime_value_respond(module, "answer", env, {}) != nullptr);
}

TEST_CASE("symbol root enumeration tracks global slots and modules", "[RuntimeTest][GC]")
{
  auto root = makert<RuntimeSymbolTable>();
  auto leafSlot = make_runtime_string("leaf", StorageClass::GLOBAL);
  leafSlot->name = "leaf";
  root->objectSlots["leaf"] = leafSlot;
  root->modules["sample"] = make_runtime_module();

  auto roots = enumerate_symbol_roots(root);
  REQUIRE(std::find(roots.cells.begin(), roots.cells.end(), leafSlot) != roots.cells.end());
  REQUIRE(std::find(roots.cells.begin(), roots.cells.end(), root->modules.at("sample")) != roots.cells.end());
}

TEST_CASE("value_ops handles direct equality and collection mutation", "[RuntimeTest][ValueOps]")
{
  auto lhs = make_runtime_array_cell({numeral_cell_from_value<int32_t>(1), make_runtime_string("two")});
  auto rhs = make_runtime_array_cell({numeral_cell_from_value<int32_t>(1), make_runtime_string("two")});
  auto tupleA = make_runtime_tuple_cell({make_runtime_boolean(true), numeral_cell_from_value<int32_t>(3)});
  auto tupleB = make_runtime_tuple_cell({make_runtime_boolean(true), numeral_cell_from_value<int32_t>(3)});

  REQUIRE(value_equals(lhs, rhs));
  REQUIRE(value_equals(tupleA, tupleB));

  auto lhsSlot = runtime_cell_slot_ref(lhs, 0);
  REQUIRE(lhsSlot != nullptr);
  runtime_copy_storage_cell(lhsSlot, numeral_cell_from_value<int32_t>(8));
  REQUIRE_FALSE(value_equals(lhs, rhs));

  auto tupleSlot = runtime_cell_slot_ref(tupleA, 1);
  REQUIRE(tupleSlot != nullptr);
  runtime_copy_storage_cell(tupleSlot, numeral_cell_from_value<int32_t>(9));
  REQUIRE_FALSE(value_equals(tupleA, tupleB));

  auto leftCell = numeral_cell_from_value<int32_t>(4);
  auto rightCell = numeral_cell_from_value<int32_t>(2);
  REQUIRE(value_greater_than(leftCell, rightCell));
  auto summedCells = dispatch_binary_operator(leftCell, RuntimeBinaryOperator::Add, rightCell);
  REQUIRE(summedCells != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(summedCells) == 6);

  auto appended =
      value_lshift(lhs, numeral_cell_from_value<int32_t>(9));
  REQUIRE(runtime_is_array_value(appended));
  REQUIRE(runtime_array_length(appended) == 3);

  auto text = dispatch_binary_operator(make_runtime_string("ng"), RuntimeBinaryOperator::Add, numeral_cell_from_value<int32_t>(33));
  REQUIRE(text != nullptr);
  REQUIRE(runtime_string_value(text) == "ng33");

  REQUIRE(value_less_than(numeral_cell_from_value<int32_t>(1), numeral_cell_from_value<int32_t>(2)));
  REQUIRE(value_greater_than(numeral_cell_from_value<int32_t>(4), numeral_cell_from_value<int32_t>(2)));
}

TEST_CASE("aggregate storage cells keep slot-backed state without object cache", "[RuntimeTest][ValueOps][Buffered]")
{
  auto arrayCell = make_runtime_array_cell({
      numeral_cell_from_value<int32_t>(1),
      numeral_cell_from_value<int32_t>(2),
  });
  auto tupleCell = make_runtime_tuple_cell({
      make_runtime_boolean(true),
      numeral_cell_from_value<int32_t>(3),
  });

  auto pairType = makert<NGType>();
  pairType->name = "Pair";
  pairType->properties = {"left", "right"};
  auto structuralCell = make_runtime_structural_cell(
      pairType,
      {
          numeral_cell_from_value<int32_t>(4),
          numeral_cell_from_value<int32_t>(5),
      });
  auto taggedCell = make_runtime_tagged_cell(
      "Result", "Ok", 0,
      {
          numeral_cell_from_value<int32_t>(9),
      },
      {"value"});

  REQUIRE(runtime_value_show(arrayCell) == "[1, 2]");
  REQUIRE(runtime_value_show(tupleCell) == "(true, 3)");
  REQUIRE(runtime_value_show(structuralCell) == "{ left: 4, right: 5 }");
  REQUIRE(runtime_value_show(taggedCell) == "Ok(9)");

  auto arrayCellCopy = clone_runtime_storage_cell(arrayCell);
  auto tupleCellCopy = clone_runtime_storage_cell(tupleCell);
  auto structuralCellCopy = clone_runtime_storage_cell(structuralCell);
  auto taggedCellCopy = clone_runtime_storage_cell(taggedCell);

  REQUIRE(value_equals(arrayCell, arrayCellCopy));
  REQUIRE(value_equals(tupleCell, tupleCellCopy));
  REQUIRE(value_equals(structuralCell, structuralCellCopy));
  REQUIRE(value_equals(taggedCell, taggedCellCopy));
}

TEST_CASE("string storage cells keep payload bytes without object cache", "[RuntimeTest][ValueOps][Buffered]")
{
  auto left = make_runtime_string("hello");
  auto right = make_runtime_string("world");
  REQUIRE(runtime_value_show(left) == "hello");
  REQUIRE(runtime_value_bool(left));
  REQUIRE(value_less_than(left, right));

  auto appended = value_add(left, right);
  REQUIRE(runtime_string_value(appended) == "helloworld");
}

TEST_CASE("reference and moved storage cells avoid object cache", "[RuntimeTest][ValueOps][Buffered]")
{
  auto target = numeral_cell_from_value<int32_t>(7);
  auto refCell = make_runtime_reference_cell(target, "target");
  REQUIRE(runtime_value_show(refCell) == "ref(target)");
  REQUIRE(runtime_reference_target(refCell) == target);

  auto movedCell = make_storage_cell(TypeLayout{}, StorageClass::TEMPORARY);
  mark_moved_storage_cell(movedCell);
  REQUIRE(runtime_value_show(movedCell) == "<moved>");
  REQUIRE_FALSE(runtime_value_bool(movedCell));
}

TEST_CASE("native marshaling supports boolean packs and sum types", "[RuntimeTest][Native]")
{
  NGArgs boolArgs{make_runtime_boolean(true), make_runtime_boolean(false)};
  auto nativeView = make_native_args_view(boolArgs);
  auto first = runtime_boolean_value(nativeView.slot_at(0));
  auto second = runtime_boolean_value(nativeView.slot_at(1));
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  REQUIRE(*first);
  REQUIRE_FALSE(*second);

  NGArgs stringArgs{make_runtime_string("hello")};
  auto stringView = make_native_args_view(stringArgs);
  auto lenArg = require_arg_slot("len", stringView, 0, "an array or string");
  REQUIRE(runtime_is_string_value(lenArg));
  REQUIRE(runtime_string_value(lenArg) == "hello");

  REQUIRE_THROWS_WITH(([]() {
                        return require_array_arg_slot(
                            "len",
                            make_native_args_view(
                                NGArgs{numeral_cell_from_value<int32_t>(1)}),
                            0, "an array or string");
                      }()),
                      Catch::Matchers::ContainsSubstring("len() requires an array or string at argument 1"));
}

TEST_CASE("native marshaling can read slot-backed native arg views", "[RuntimeTest][Native]")
{
  auto env = make_runtime_env();
  NGArgs args{numeral_cell_from_value<int32_t>(1),
              make_runtime_string("hello")};

  auto slots = bind_native_arg_slots(env, args);
  REQUIRE(slots != nullptr);
  REQUIRE(slots->size() == 2);

  auto view = native_args_view(env, args);
  REQUIRE(view.size() == 2);
  REQUIRE(view.slot_at(0) == slots->at(0));

  REQUIRE(require_numeric_arg<int32_t>("slotTest", view, 0, "an integer") == 1);

  runtime_copy_storage_cell(slots->at(1), make_runtime_string("world"));
  REQUIRE(require_string_arg("slotTest", view, 1, "a string") == "world");
}

TEST_CASE("runtime value access adapts storage cells and boxed objects", "[RuntimeTest][Runtime]")
{
  auto number = numeral_cell_from_value<int32_t>(7);
  auto zero = numeral_cell_from_value<int32_t>(0);
  auto slot = clone_runtime_storage_cell(number, StorageClass::TEMPORARY);
  slot->name = "value";

  REQUIRE(runtime_value_bool(number));
  REQUIRE_FALSE(runtime_value_bool(zero));
  REQUIRE(runtime_value_bool(slot));
  REQUIRE(runtime_value_show(number) == "7");
  REQUIRE(runtime_value_show(zero) == "0");
  REQUIRE(runtime_value_show(slot) == "7");
  REQUIRE(runtime_value_type(slot)->name == "i32");

  clear_storage_cell(slot);
  REQUIRE(runtime_value_type(slot)->name == "i32");
}

TEST_CASE("runtime value access handles null and cleared cells without materialization", "[RuntimeTest][Runtime][Failure]")
{
  REQUIRE(runtime_value_type(nullptr)->name == "Object");
  REQUIRE(runtime_value_layout(nullptr).size == 0);
  REQUIRE(runtime_value_show(nullptr) == "unit");
  REQUIRE_FALSE(runtime_value_bool(nullptr));
  REQUIRE_FALSE(runtime_cell_has_value(nullptr));
  REQUIRE_FALSE(runtime_cell_is_moved(nullptr));
  REQUIRE_THROWS_WITH(runtime_value_respond_slot(nullptr, "missing", make_runtime_env(), {}),
                      Catch::Matchers::ContainsSubstring("Cannot respond to member 'missing' on null storage cell"));

  auto slot = numeral_cell_from_value<int32_t>(12);
  runtime_copy_storage_cell(slot, nullptr);
  REQUIRE_FALSE(runtime_cell_has_value(slot));
  REQUIRE(slot->runtimeType == nullptr);
  REQUIRE(slot->bytes.empty());

  auto fallback = clone_runtime_storage_cell(nullptr, StorageClass::GLOBAL, "fallback");
  REQUIRE(runtime_value_type(fallback)->name == "unit");
  REQUIRE(fallback->storageClass == StorageClass::GLOBAL);
  REQUIRE(fallback->name == "fallback");
}

TEST_CASE("runtime value respond prefers type-handle member dispatch", "[RuntimeTest][Runtime]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "WrappedInt";
  nominalType->memberFunctions["kind"] = [](const NGSelf &, const NGEnv &, const NGArgs &) {
    return make_runtime_string("nominal");
  };

  auto nominal = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(3));
  auto env = make_runtime_env();

  auto slotResult = runtime_value_respond(nominal, "kind", env, {});
  REQUIRE(slotResult != nullptr);
  REQUIRE(runtime_string_value(slotResult) == "nominal");
}

TEST_CASE("runtime show and bool can use nominal type handlers", "[RuntimeTest][Runtime]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "Flagged";
  nominalType->showCellHandler = [](const RuntimeRef<StorageCell> &) { return "flagged"; };
  nominalType->boolCellHandler = [](const RuntimeRef<StorageCell> &) { return false; };

  auto slot = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(1));
  REQUIRE(runtime_value_show(slot) == "flagged");
  REQUIRE_FALSE(runtime_value_bool(slot));
}

TEST_CASE("nominal storage cells keep wrapped slots without object cache", "[RuntimeTest][Runtime][Buffered]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "WrappedInt";

  auto slot = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(3));
  REQUIRE(runtime_value_type(slot)->name == "WrappedInt");
  REQUIRE(runtime_value_show(slot) == "3");
  REQUIRE(runtime_value_bool(slot));

  auto wrappedSlot = runtime_cell_slot_ref(slot, 0);
  REQUIRE(wrappedSlot != nullptr);
  REQUIRE(runtime_value_type(wrappedSlot)->name == "i32");

  REQUIRE(read_inline_cell_bytes<int32_t>(wrappedSlot) == 3);
}

TEST_CASE("nominal cell show and bool can read wrapped slot without materializing", "[RuntimeTest][Runtime][Buffered]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "WrappedFlag";

  auto slot = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(7));
  REQUIRE(runtime_value_show(slot) == "7");
  REQUIRE(runtime_value_bool(slot));
}

TEST_CASE("nominal cell ops can use wrapped slots without materializing", "[RuntimeTest][Runtime][Buffered]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "Meters";

  auto left = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(1));
  auto right = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(2));
  REQUIRE(value_less_than(left, right));
  REQUIRE_FALSE(value_equals(left, right));

  auto added = value_add(left, right);
  REQUIRE(read_inline_cell_bytes<int32_t>(added) == 3);
}

TEST_CASE("nominal cell member dispatch can use wrapped slot without materializing", "[RuntimeTest][Runtime][Buffered]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "WrappedTuple";

  auto slot = make_runtime_newtype_cell(
      nominalType,
      make_runtime_tuple_cell({numeral_cell_from_value<int32_t>(1), numeral_cell_from_value<int32_t>(2)}));
  auto env = make_runtime_env();
  auto size = runtime_value_respond(slot, "size", env, {});
  REQUIRE(size != nullptr);
  REQUIRE(read_inline_cell_bytes<uint32_t>(size) == 2);
}

TEST_CASE("runtime value ops prefer type-driven operator handlers", "[RuntimeTest][Runtime]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "Meters";
  nominalType->cellBinaryOperators[RuntimeBinaryOperator::Add] =
      [](const RuntimeRef<StorageCell> &, const RuntimeRef<StorageCell> &) -> RuntimeRef<StorageCell> {
    return make_runtime_string("meters-add");
  };
  nominalType->cellOrderHandler = [](const RuntimeRef<StorageCell> &, const RuntimeRef<StorageCell> &) {
    return Orders::LT;
  };

  auto left = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(1));
  auto right = make_runtime_newtype_cell(nominalType, numeral_cell_from_value<int32_t>(2));

  auto added = value_add(left, right);
  REQUIRE(runtime_string_value(added) == "meters-add");
  REQUIRE(value_less_than(left, right));
}

TEST_CASE("runtime value ops cover unsupported and aggregate mismatch paths", "[RuntimeTest][Runtime][ValueOps][Failure]")
{
  REQUIRE(dispatch_binary_operator(nullptr, RuntimeBinaryOperator::Add, numeral_cell_from_value<int32_t>(1)) == nullptr);
  REQUIRE_THROWS_WITH(value_subtract(make_runtime_string("a"), make_runtime_string("b")),
                      Catch::Matchers::ContainsSubstring("Unsupported binary operator"));
  REQUIRE_THROWS_WITH(value_less_than(make_runtime_array_cell({}), make_runtime_array_cell({})),
                      Catch::Matchers::ContainsSubstring("Unsupported binary operator"));

  REQUIRE_FALSE(value_equals(make_runtime_array_cell({numeral_cell_from_value<int32_t>(1)}),
                             make_runtime_array_cell({numeral_cell_from_value<int32_t>(1),
                                                      numeral_cell_from_value<int32_t>(2)})));
  REQUIRE_FALSE(value_equals(make_runtime_tuple_cell({numeral_cell_from_value<int32_t>(1)}),
                             make_runtime_tuple_cell({numeral_cell_from_value<int32_t>(1),
                                                      numeral_cell_from_value<int32_t>(2)})));
  REQUIRE_FALSE(value_equals(make_runtime_tagged_cell("Result", "Ok", 0, {numeral_cell_from_value<int32_t>(1)}, {"value"}),
                             make_runtime_tagged_cell("Result", "Err", 1, {make_runtime_string("bad")}, {"message"})));

  auto objectType = makert<NGType>();
  objectType->name = "Record";
  objectType->properties = {"value"};
  auto left = make_runtime_structural_cell(objectType, {numeral_cell_from_value<int32_t>(1)}, {{"extra", unit_cell()}});
  auto right = make_runtime_structural_cell(objectType, {numeral_cell_from_value<int32_t>(1)});
  REQUIRE_FALSE(value_equals(left, right));
}

TEST_CASE("runtime value ops can dispatch from larger right-hand layouts", "[RuntimeTest][Runtime][ValueOps]")
{
  auto narrowType = makert<NGType>();
  narrowType->name = "Narrow";
  narrowType->layout = TypeLayout{.name = "Narrow", .kind = LayoutKind::INLINE_VALUE, .size = 1};

  auto wideType = makert<NGType>();
  wideType->name = "Wide";
  wideType->layout = TypeLayout{.name = "Wide", .kind = LayoutKind::INLINE_VALUE, .size = 16};
  wideType->cellBinaryOperators[RuntimeBinaryOperator::Add] =
      [](const RuntimeRef<StorageCell> &, const RuntimeRef<StorageCell> &) {
        return make_runtime_string("right-dispatch");
      };
  wideType->cellOrderHandler = [](const RuntimeRef<StorageCell> &, const RuntimeRef<StorageCell> &) {
    return Orders::GT;
  };

  auto narrow = make_storage_cell(narrowType->layout, StorageClass::TEMPORARY, {}, narrowType);
  narrow->initialized = true;
  auto wide = make_storage_cell(wideType->layout, StorageClass::TEMPORARY, {}, wideType);
  wide->initialized = true;

  REQUIRE(runtime_string_value(value_add(narrow, wide)) == "right-dispatch");
  REQUIRE(value_less_than(narrow, wide));
}

TEST_CASE("runtime value respond uses type-descriptor handlers for aggregate and module members", "[RuntimeTest][Runtime]")
{
  auto env = make_runtime_env();

  auto tupleCell = make_runtime_tuple_cell({
      numeral_cell_from_value<int32_t>(7),
      make_runtime_string("two"),
  });
  auto tupleSize = runtime_value_respond(tupleCell, "size", env, {});
  REQUIRE(tupleSize != nullptr);
  REQUIRE(read_inline_cell_bytes<uint32_t>(tupleSize) == 2);
  auto tupleItem = runtime_value_respond(tupleCell, "1", env, {});
  REQUIRE(tupleItem != nullptr);
  REQUIRE(runtime_string_value(tupleItem) == "two");

  auto taggedCell = make_runtime_tagged_cell(
      "Result", "Ok", 0,
      {
          numeral_cell_from_value<int32_t>(42),
      },
      {"value"});
  auto tag = runtime_value_respond(taggedCell, "tag", env, {});
  REQUIRE(tag != nullptr);
  REQUIRE(runtime_string_value(tag) == "Ok");
  auto taggedValue = runtime_value_respond(taggedCell, "value", env, {});
  REQUIRE(taggedValue != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(taggedValue) == 42);

  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};
  auto structuralCell = make_runtime_structural_cell(
      objectType,
      {
          numeral_cell_from_value<int32_t>(1),
          numeral_cell_from_value<int32_t>(2),
      });
  REQUIRE(runtime_value_type(structuralCell)->name == "Pair");
  REQUIRE(runtime_value_show(structuralCell) == "{ left: 1, right: 2 }");
  auto left = runtime_value_respond(structuralCell, "left", env, {});
  REQUIRE(left != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(left) == 1);
  auto right = runtime_value_respond(structuralCell, "right", env, {});
  REQUIRE(right != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(right) == 2);

  auto moduleSymbols = makert<RuntimeSymbolTable>();
  moduleSymbols->objectSlots["hello"] = make_runtime_string("world", StorageClass::GLOBAL);
  auto module = make_runtime_module(moduleSymbols);
  auto hello = runtime_value_respond(module, "hello", env, {});
  REQUIRE(hello != nullptr);
  REQUIRE(runtime_string_value(hello) == "world");
}

TEST_CASE("string member handlers validate missing arguments", "[RuntimeTest][Runtime][Failure]")
{
  auto env = make_runtime_env();
  auto text = make_runtime_string("abc");

  REQUIRE_THROWS_WITH(runtime_value_respond(text, "charAt", env, {}),
                      Catch::Matchers::ContainsSubstring("requires an index argument"));
  REQUIRE_THROWS_WITH(runtime_value_respond(text, "append", env, {}),
                      Catch::Matchers::ContainsSubstring("requires a value argument"));
  REQUIRE_THROWS_WITH(runtime_value_respond(text, "charAt", env, {numeral_cell_from_value<int32_t>(3)}),
                      Catch::Matchers::ContainsSubstring("Index out of bounds"));
  auto appended = runtime_value_respond(text, "append", env, {make_runtime_string("d")});
  REQUIRE(runtime_string_value(appended) == "abcd");
}

TEST_CASE("cell protocol calls fall through to type-descriptor handlers", "[RuntimeTest][Runtime]")
{
  auto pairType = makert<NGType>();
  pairType->name = "Pair";
  pairType->properties = {"left", "right"};
  auto structural = make_runtime_structural_cell(
      pairType, {numeral_cell_from_value<int32_t>(1), numeral_cell_from_value<int32_t>(2)});
  REQUIRE(runtime_value_show(structural) == "{ left: 1, right: 2 }");
  REQUIRE(runtime_value_type(structural)->name == "Pair");

  auto tagged = make_runtime_tagged_cell("Result", "Ok", 0, {numeral_cell_from_value<int32_t>(42)}, {"value"});
  REQUIRE(runtime_value_show(tagged) == "Ok(42)");
  REQUIRE(runtime_value_bool(tagged));
  REQUIRE(runtime_value_type(tagged)->name == "Result");
}

TEST_CASE("native library binding injects owning module context and state", "[RuntimeTest][Native]")
{
  auto root = makert<RuntimeSymbolTable>();
  auto module = make_runtime_module(root);

  bind_native_library_handlers(
      module, {{"remember",
                [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<StorageCell> {
                  auto nativeModule = current_native_module(context);
                  REQUIRE(nativeModule != nullptr);
                  runtime_module_set_native_state(nativeModule, "sentinel", std::make_shared<int>(42));
                  return unit_cell();
                }}});

  REQUIRE(current_native_module(make_runtime_env(root)) == nullptr);

  auto result = runtime_module_native_functions(module).at("remember")(nullptr, make_runtime_env(root), {});
  REQUIRE(result != nullptr);

  auto stored = std::static_pointer_cast<int>(runtime_module_get_native_state(module, "sentinel"));
  REQUIRE(stored != nullptr);
  REQUIRE(*stored == 42);
  REQUIRE(current_native_module(make_runtime_env(root)) == nullptr);
}

TEST_CASE("heap references are backed by heap storage cells", "[RuntimeTest][GC]")
{
  auto refCell = allocate_heap_cell(numeral_cell_from_value<int32_t>(3), "heap:value");
  auto cell = runtime_reference_target(refCell);

  REQUIRE(cell != nullptr);
  REQUIRE(cell->storageClass == StorageClass::HEAP);

  auto value = runtime_read_reference(refCell);
  REQUIRE(read_inline_cell_bytes<int32_t>(value) == 3);

  runtime_write_reference(refCell, numeral_cell_from_value<int32_t>(9));
  REQUIRE(read_inline_cell_bytes<int32_t>(cell) == 9);
}

TEST_CASE("managed heap traces referenced storage cells instead of boxed object identity", "[RuntimeTest][GC]")
{
  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);

  auto shared = make_runtime_string("shared");
  auto keep = allocate_heap_cell(shared, "heap:keep");
  auto drop = allocate_heap_cell(shared, "heap:drop");

  auto providerId = register_gc_root_provider([keep]() -> GCRootSet {
    return {.cells = {runtime_reference_target(keep)}};
  });
  collect_managed_heap();
  unregister_gc_root_provider(providerId);

  REQUIRE(managed_heap_size() == 1);
  REQUIRE_NOTHROW(runtime_read_reference(keep));
  REQUIRE_THROWS_WITH(runtime_read_reference(drop),
                      Catch::Matchers::ContainsSubstring("Dangling heap reference"));

  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);
}

TEST_CASE("managed heap traces reference roots through storage cells", "[RuntimeTest][GC]")
{
  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);

  auto keep = allocate_heap_cell(numeral_cell_from_value<int32_t>(7), "heap:keep-ref");
  auto drop = allocate_heap_cell(numeral_cell_from_value<int32_t>(9), "heap:drop-ref");

  auto providerId = register_gc_root_provider([keep]() -> GCRootSet { return {.cells = {runtime_reference_target(keep)}}; });
  collect_managed_heap();
  unregister_gc_root_provider(providerId);

  REQUIRE(managed_heap_size() == 1);
  REQUIRE_NOTHROW(runtime_read_reference(keep));
  REQUIRE_THROWS_WITH(runtime_read_reference(drop),
                      Catch::Matchers::ContainsSubstring("Dangling heap reference"));

  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);
}

TEST_CASE("struct layout access reads and writes typed fields", "[RuntimeTest][LayoutObjects]")
{
  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};

  auto object = make_runtime_structural_cell(
      objectType, {numeral_cell_from_value<int32_t>(1), numeral_cell_from_value<int32_t>(2)});

  auto right = structural_read_member_slot(object, "right");
  REQUIRE(read_inline_cell_bytes<int32_t>(right) == 2);

  runtime_copy_storage_cell(structural_member_slot(object, "left"), numeral_cell_from_value<int32_t>(9));
  REQUIRE(read_inline_cell_bytes<int32_t>(structural_member_slot(object, "left")) == 9);
  REQUIRE(runtime_cell_slot_refs(object).size() == 2);

  auto leftSlot = structural_member_slot(object, "left");
  REQUIRE(leftSlot != nullptr);
  REQUIRE(structural_member_slot(object, "left") == leftSlot);
  REQUIRE(object->opaqueRefs[0] == leftSlot);
  runtime_copy_storage_cell(leftSlot, numeral_cell_from_value<int32_t>(11));

  auto slotted = structural_read_member_slot(object, "left");
  REQUIRE(read_inline_cell_bytes<int32_t>(slotted) == 11);
}

TEST_CASE("struct layout access keeps dynamic properties in slots", "[RuntimeTest][LayoutObjects]")
{
  auto slot = numeral_cell_from_value<int32_t>(7);
  auto object = make_runtime_structural_cell(nullptr, {}, {{"dyn", slot}});
  REQUIRE(slot != nullptr);
  REQUIRE(structural_member_slot(object, "dyn") == slot);
  REQUIRE(runtime_cell_has_value(slot));
  REQUIRE(read_inline_cell_bytes<int32_t>(slot) == 7);

  auto reference = make_runtime_reference_cell(slot, "dyn");
  runtime_write_reference(reference, numeral_cell_from_value<int32_t>(9));

  auto value = structural_read_member_slot(object, "dyn");
  REQUIRE(read_inline_cell_bytes<int32_t>(value) == 9);
}

TEST_CASE("struct layout access can replace typed fields with storage slots", "[RuntimeTest][LayoutObjects]")
{
  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};

  auto left = numeral_cell_from_value<int32_t>(3);
  auto right = numeral_cell_from_value<int32_t>(4);

  auto object = make_runtime_structural_cell(objectType, {left, right});

  REQUIRE(runtime_structural_field_slot(object, 0) == left);
  REQUIRE(runtime_structural_field_slot(object, 1) == right);
  REQUIRE(object->opaqueRefs[0] == left);

  runtime_copy_storage_cell(left, numeral_cell_from_value<int32_t>(8));
  auto updated = structural_read_member_slot(object, "left");
  REQUIRE(read_inline_cell_bytes<int32_t>(updated) == 8);
}

TEST_CASE("structural cells expose dynamic slot creation and reject non-structural values", "[RuntimeTest][LayoutObjects][Failure]")
{
  auto dynamic = make_runtime_structural_cell(nullptr, {});
  auto created = runtime_structural_property_slot_or_create(dynamic, "created");
  REQUIRE(created != nullptr);
  REQUIRE(created->name == "created");
  runtime_structural_write_member(dynamic, "created", make_runtime_string("slot"));
  REQUIRE(runtime_string_value(runtime_structural_read_member(dynamic, "created")) == "slot");
  REQUIRE(runtime_structural_property_slots(dynamic).contains("created"));

  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};
  auto sparse = make_runtime_structural_cell(objectType, {});
  auto left = runtime_structural_property_slot_or_create(sparse, "left");
  REQUIRE(left != nullptr);
  REQUIRE(runtime_structural_field_slots(sparse).size() == 1);
  REQUIRE(runtime_structural_field_slot(sparse, 1) == nullptr);
  REQUIRE(runtime_structural_field_index(sparse, "right").value() == 1);

  auto array = make_runtime_array_cell({});
  REQUIRE_FALSE(runtime_is_structural_value(array));
  REQUIRE_FALSE(runtime_is_structural_value(make_runtime_tuple_cell({})));
  REQUIRE_FALSE(runtime_is_structural_value(make_runtime_string("x")));
  REQUIRE_FALSE(runtime_is_structural_value(make_runtime_module()));
  REQUIRE_FALSE(runtime_is_structural_value(make_runtime_reference_cell(unit_cell())));
  REQUIRE_THROWS_WITH(runtime_structural_type(array),
                      Catch::Matchers::ContainsSubstring("Expected structural runtime value"));
  REQUIRE_THROWS_WITH(runtime_structural_replace_field_slots(array, {}),
                      Catch::Matchers::ContainsSubstring("Expected structural runtime value"));
}

TEST_CASE("array layout access reads and writes indexed elements", "[RuntimeTest][LayoutObjects]")
{
  auto array = make_runtime_array_cell({numeral_cell_from_value<int32_t>(4), numeral_cell_from_value<int32_t>(5)});
  REQUIRE(array_length(array) == 2);

  auto value = array_read_element(array, 1);
  REQUIRE(read_inline_cell_bytes<int32_t>(value) == 5);
  REQUIRE(array_element_slot(array, 1) != nullptr);

  array_write_element(array, 0, numeral_cell_from_value<int32_t>(9));
  auto updated = array_read_element(array, 0);
  REQUIRE(read_inline_cell_bytes<int32_t>(updated) == 9);

  auto firstSlot = array_element_slot(array, 0);
  REQUIRE(firstSlot != nullptr);
  REQUIRE(array_element_slot(array, 0) == firstSlot);
  REQUIRE(array->opaqueRefs[0] == firstSlot);
  runtime_copy_storage_cell(firstSlot, numeral_cell_from_value<int32_t>(12));

  auto slotted = array_read_element(array, 0);
  REQUIRE(read_inline_cell_bytes<int32_t>(slotted) == 12);
}

TEST_CASE("tuple layout access reads members and indexed elements", "[RuntimeTest][LayoutObjects]")
{
  auto tuple = make_runtime_tuple_cell({numeral_cell_from_value<int32_t>(7), make_runtime_string("two")});
  REQUIRE(tuple_length(tuple) == 2);
  REQUIRE(runtime_cell_slot_refs(tuple).size() == 2);

  auto first = tuple_read_element(tuple, 0);
  REQUIRE(read_inline_cell_bytes<int32_t>(first) == 7);
  REQUIRE(tuple_element_slot(tuple, 0) != nullptr);

  auto size = tuple_read_member(tuple, "size");
  REQUIRE(read_inline_cell_bytes<uint32_t>(size) == 2);

  tuple_write_element(tuple, 0, numeral_cell_from_value<int32_t>(9));
  auto updated = tuple_read_member(tuple, "0");
  REQUIRE(read_inline_cell_bytes<int32_t>(updated) == 9);
  REQUIRE(runtime_cell_slot_refs(tuple).size() == 2);

  auto firstSlot = tuple_element_slot(tuple, 0);
  REQUIRE(firstSlot != nullptr);
  REQUIRE(tuple_element_slot(tuple, 0) == firstSlot);
  REQUIRE(tuple->opaqueRefs[0] == firstSlot);
  runtime_copy_storage_cell(firstSlot, numeral_cell_from_value<int32_t>(12));

  auto slotted = tuple_read_element(tuple, 0);
  REQUIRE(read_inline_cell_bytes<int32_t>(slotted) == 12);
}

TEST_CASE("string layout access reads code units and header-backed length", "[RuntimeTest][LayoutObjects]")
{
  auto string = make_runtime_string("A2");
  REQUIRE(string_length(string) == 2);
  REQUIRE(string_data_handle(string).typeName == "String.payload");
  REQUIRE(runtime_string_value(string) == "A2");

  auto second = string_read_code_unit(string, 1);
  REQUIRE(second != nullptr);
  REQUIRE(read_inline_cell_bytes<int32_t>(second) == 50);
}

TEST_CASE("dynamic runtime types expose header layouts", "[RuntimeTest][LayoutObjects]")
{
  auto arrayType = array_runtime_type();
  auto stringType = string_runtime_type();

  REQUIRE(arrayType->layout.kind == LayoutKind::DYNAMIC);
  REQUIRE(arrayType->layout.fields.size() == 3);
  REQUIRE(arrayType->layout.fields[2].name == "data");

  REQUIRE(stringType->layout.kind == LayoutKind::DYNAMIC);
  REQUIRE(stringType->layout.fields.size() == 2);
  REQUIRE(stringType->layout.fields[1].name == "data");
}

TEST_CASE("array and string runtime values sync header cells", "[RuntimeTest][LayoutObjects]")
{
  auto array = make_runtime_array_cell({numeral_cell_from_value<int32_t>(1), numeral_cell_from_value<int32_t>(2)});
  auto string = make_runtime_string("hello");

  const auto &arrayLayout = array->layout;
  const auto &stringLayout = string->layout;
  auto *arrayLength = NG::buffer_runtime::find_field(arrayLayout, "length");
  auto *arrayCapacity = NG::buffer_runtime::find_field(arrayLayout, "capacity");
  auto *stringLength = NG::buffer_runtime::find_field(stringLayout, "length");

  REQUIRE(arrayLength != nullptr);
  REQUIRE(arrayCapacity != nullptr);
  REQUIRE(stringLength != nullptr);
  REQUIRE(runtime_cell_slot_refs(array).size() == 2);
  REQUIRE(string_data_handle(string).typeName == "String.payload");
  REQUIRE(runtime_string_value(string) == "hello");

  auto appended =
      value_lshift(array, numeral_cell_from_value<int32_t>(3));
  string = make_runtime_string("hello!");

  REQUIRE(runtime_is_array_value(appended));
  REQUIRE(runtime_array_length(appended) == 3);
  REQUIRE(string_length(string) == 6);
  REQUIRE(runtime_string_value(string) == "hello!");
  REQUIRE(runtime_cell_slot_refs(appended).size() == 3);
}

TEST_CASE("clone_runtime_storage_cell preserves slot-backed structural and tagged payloads", "[RuntimeTest][LayoutObjects]")
{
  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};

  auto structural = make_runtime_structural_cell(
      objectType,
      {
          numeral_cell_from_value<int32_t>(1),
          numeral_cell_from_value<int32_t>(2),
      });

  auto tagged = make_runtime_tagged_cell(
      "Result", "Ok", 0,
      {
          numeral_cell_from_value<int32_t>(5),
      },
      {"value"});

  auto clonedStructural = clone_runtime_storage_cell(structural);
  auto clonedTagged = clone_runtime_storage_cell(tagged);

  REQUIRE(clonedStructural != nullptr);
  REQUIRE(clonedTagged != nullptr);
  REQUIRE(runtime_cell_slot_ref(clonedStructural, 0) != runtime_cell_slot_ref(structural, 0));
  REQUIRE(runtime_cell_slot_ref(clonedTagged, 0) != runtime_cell_slot_ref(tagged, 0));
  REQUIRE(read_inline_cell_bytes<int32_t>(structural_read_member_slot(clonedStructural, "left")) == 1);
  REQUIRE(read_inline_cell_bytes<int32_t>(tagged_read_member_slot(clonedTagged, "value")) == 5);
}

TEST_CASE("indexed runtime access uses bounds-checked array and tuple slots", "[RuntimeTest][LayoutObjects][Failure]")
{
  auto array = make_runtime_array_cell({numeral_cell_from_value<int32_t>(1)});
  auto tuple = make_runtime_tuple_cell({numeral_cell_from_value<int32_t>(2)});

  REQUIRE(read_inline_cell_bytes<int32_t>(runtime_index_read(array, numeral_cell_from_value<int32_t>(0))) == 1);
  REQUIRE(read_inline_cell_bytes<int32_t>(runtime_index_read(tuple, numeral_cell_from_value<int32_t>(0))) == 2);
  REQUIRE_THROWS_MATCHES(runtime_index_read(array, numeral_cell_from_value<int32_t>(1)), RuntimeException,
                         MessageMatches(ContainsSubstring("Index out of bounds")));
  REQUIRE_THROWS_MATCHES(runtime_index_read(tuple, numeral_cell_from_value<int32_t>(1)), RuntimeException,
                         MessageMatches(ContainsSubstring("Index out of bounds")));
  REQUIRE_THROWS_MATCHES(runtime_index_read(array, numeral_cell_from_value<int32_t>(-1)), RuntimeException,
                         MessageMatches(ContainsSubstring("Index out of bounds")));
  REQUIRE_THROWS_MATCHES(runtime_index_read(make_runtime_string("x"), numeral_cell_from_value<int32_t>(0)),
                         IllegalTypeException, MessageMatches(ContainsSubstring("Not index-accessible")));
}

TEST_CASE("runtime value type synthesizes stable metadata for raw storage cells", "[RuntimeTest][LayoutObjects]")
{
  TypeLayout layout{
      .id = 99,
      .name = "RawPair",
      .kind = LayoutKind::INLINE_VALUE,
      .size = 8,
      .alignment = 4,
  };
  auto first = make_storage_cell(layout);
  auto second = make_storage_cell(layout);

  auto firstType = runtime_value_type(first);
  auto secondType = runtime_value_type(second);

  REQUIRE(firstType != nullptr);
  REQUIRE(firstType == runtime_value_type(first));
  REQUIRE(firstType == secondType);
  REQUIRE(firstType->name == "RawPair");
  REQUIRE(firstType->layout.id == 99);
}

TEST_CASE("mixed-width non-commutative numeric operators preserve operand order", "[RuntimeTest][Numeral]")
{
  auto left = numeral_cell_from_value<int32_t>(20);
  auto right = numeral_cell_from_value<int64_t>(6);

  REQUIRE(read_inline_cell_bytes<int32_t>(value_subtract(left, right)) == 14);
  REQUIRE(read_inline_cell_bytes<int32_t>(value_divide(left, numeral_cell_from_value<int64_t>(5))) == 4);
  REQUIRE(read_inline_cell_bytes<int32_t>(value_modulus(left, right)) == 2);
  REQUIRE(read_inline_cell_bytes<int64_t>(value_add(left, right)) == 26);
  REQUIRE(read_inline_cell_bytes<int64_t>(value_multiply(left, right)) == 120);
}

TEST_CASE("native marshaling validates slot views and argument contracts", "[RuntimeTest][Native]")
{
  NativeArgsView empty;
  REQUIRE(empty.size() == 0);
  REQUIRE(empty.value_at(0) == nullptr);
  REQUIRE(empty.slot_at(0) == nullptr);

  NGArgs values{make_runtime_string("value")};
  auto slots = std::make_shared<Vec<RuntimeRef<StorageCell>>>();
  slots->push_back(make_runtime_string("slot"));
  NativeArgsView slotView{.values = &values, .slotOwner = slots};

  REQUIRE(slotView.size() == 1);
  REQUIRE(runtime_string_value(slotView.value_at(0)) == "slot");
  REQUIRE(runtime_string_value(slotView.slot_at(0)) == "slot");
  REQUIRE(require_string_arg("native", slotView, 0) == "slot");

  auto env = make_runtime_env();
  bind_native_arg_slots(env, *slots);
  auto viewFromEnv = native_args_view(env, values);
  REQUIRE(viewFromEnv.size() == 1);
  REQUIRE(runtime_string_value(viewFromEnv.slot_at(0)) == "slot");

  REQUIRE_THROWS_MATCHES(require_arg_slot("native", empty, 0, "a value"), RuntimeException,
                         MessageMatches(ContainsSubstring("requires a value")));
  REQUIRE_THROWS_MATCHES(require_string_arg("native", make_native_args_view(NGArgs{numeral_cell_from_value<int32_t>(1)}), 0),
                         RuntimeException, MessageMatches(ContainsSubstring("requires a string")));
  REQUIRE_THROWS_MATCHES(require_numeric_arg<int32_t>("native", make_native_args_view(values), 0, "an integer"),
                         RuntimeException, MessageMatches(ContainsSubstring("requires an integer")));
  REQUIRE_THROWS_MATCHES(require_arg_count("native", empty, 1), RuntimeException,
                         MessageMatches(ContainsSubstring("requires at least 1")));
  REQUIRE_THROWS_MATCHES(require_arg_count("native", make_native_args_view(values), 0, 0), RuntimeException,
                         MessageMatches(ContainsSubstring("accepts at most 0")));
}
