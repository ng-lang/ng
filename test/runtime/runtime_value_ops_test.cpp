#include "../test.hpp"
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/array_layout_access.hpp>
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

  auto answerSlot = make_boxed_storage_cell(makert<NGIntegral<int32_t>>(42), StorageClass::GLOBAL);
  answerSlot->name = "answer";
  root->objectSlots["answer"] = answerSlot;
  root->functions["unit"] = [](const NGSelf &, const NGEnv &, const NGArgs &) -> RuntimeRef<NGObject> {
    return makert<NGUnit>();
  };
  root->types["Sample"] = sampleType;

  auto env = make_runtime_env(root);
  auto global = std::dynamic_pointer_cast<NumeralBase>(root->objectSlots.at("answer")->boxedValue);
  REQUIRE(global != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(global.get()) == 42);
  REQUIRE(answerSlot->storageClass == StorageClass::GLOBAL);
  REQUIRE(root->functions.contains("unit"));
  REQUIRE(root->types.at("Sample") == sampleType);
  REQUIRE(runtime_value_respond(makert<NGModule>(root), "answer", env, {}) != nullptr);
}

TEST_CASE("symbol root enumeration tracks global slots and modules", "[RuntimeTest][GC]")
{
  auto root = makert<RuntimeSymbolTable>();
  auto leaf = makert<NGString>("leaf");
  auto leafSlot = make_boxed_storage_cell(leaf, StorageClass::GLOBAL);
  leafSlot->name = "leaf";
  root->objectSlots["leaf"] = leafSlot;
  root->modules["sample"] = makert<NGModule>();

  auto roots = enumerate_symbol_roots(root);
  REQUIRE(std::find(roots.begin(), roots.end(), leaf) != roots.end());
  REQUIRE(std::find(roots.begin(), roots.end(), root->modules.at("sample")) != roots.end());
}

TEST_CASE("value_ops handles direct equality and collection mutation", "[RuntimeTest][ValueOps]")
{
  auto lhs = makert<NGArray>(Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(1), makert<NGString>("two")});
  auto rhs = makert<NGArray>(Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(1), makert<NGString>("two")});
  auto tupleA = makert<NGTuple>(Vec<RuntimeRef<NGObject>>{makert<NGBoolean>(true), makert<NGIntegral<int32_t>>(3)});
  auto tupleB = makert<NGTuple>(Vec<RuntimeRef<NGObject>>{makert<NGBoolean>(true), makert<NGIntegral<int32_t>>(3)});

  REQUIRE(value_equals(lhs, rhs));
  REQUIRE(value_equals(tupleA, tupleB));

  auto lhsSlot = lhs->element_slot(0);
  REQUIRE(lhsSlot != nullptr);
  runtime_sync_storage_cell(lhsSlot, makert<NGIntegral<int32_t>>(8));
  REQUIRE_FALSE(value_equals(lhs, rhs));

  auto tupleSlot = tupleA->element_slot(1);
  REQUIRE(tupleSlot != nullptr);
  runtime_sync_storage_cell(tupleSlot, makert<NGIntegral<int32_t>>(9));
  REQUIRE_FALSE(value_equals(tupleA, tupleB));

  auto appended = std::dynamic_pointer_cast<NGArray>(value_lshift(lhs, makert<NGIntegral<int32_t>>(9)));
  REQUIRE(appended != nullptr);
  REQUIRE(array_length(*appended) == 3);

  auto text = std::dynamic_pointer_cast<NGString>(value_add(makert<NGString>("ng"), makert<NGIntegral<int32_t>>(33)));
  REQUIRE(text != nullptr);
  REQUIRE(text->payload_value() == "ng33");

  REQUIRE(value_less_than(makert<NGIntegral<int32_t>>(1), makert<NGIntegral<int32_t>>(2)));
  REQUIRE(value_greater_than(makert<NGIntegral<int32_t>>(4), makert<NGIntegral<int32_t>>(2)));
}

TEST_CASE("native marshaling supports boolean packs and sum types", "[RuntimeTest][Native]")
{
  NGArgs boolArgs{makert<NGBoolean>(true), makert<NGBoolean>(false)};
  auto booleans = require_all_args_as<NGBoolean>("assert", boolArgs, "a boolean");
  REQUIRE(booleans.size() == 2);
  REQUIRE(booleans[0]->value);
  REQUIRE_FALSE(booleans[1]->value);

  NGArgs stringArgs{makert<NGString>("hello")};
  auto lenArg = require_arg_as_one_of<NGArray, NGString>("len", stringArgs, 0, "an array or string");
  REQUIRE(std::holds_alternative<RuntimeRef<NGString>>(lenArg));
  REQUIRE(std::get<RuntimeRef<NGString>>(lenArg)->payload_value() == "hello");

  NGArgs badArgs{makert<NGIntegral<int32_t>>(1)};
  REQUIRE_THROWS_WITH(([]() {
                        return require_arg_as_one_of<NGArray, NGString>(
                            "len", NGArgs{makert<NGIntegral<int32_t>>(1)}, 0, "an array or string");
                      }()),
                      Catch::Matchers::ContainsSubstring("len() requires an array or string at argument 1"));
}

TEST_CASE("native marshaling can read slot-backed native arg views", "[RuntimeTest][Native]")
{
  auto env = make_runtime_env();
  NGArgs args{makert<NGIntegral<int32_t>>(1), makert<NGString>("hello")};

  auto slots = bind_native_arg_slots(env, args);
  REQUIRE(slots != nullptr);
  REQUIRE(slots->size() == 2);

  auto view = native_args_view(env, args);
  REQUIRE(view.size() == 2);
  REQUIRE(view.slot_at(0) == slots->at(0));

  auto number = require_arg_as<NGIntegral<int32_t>>("slotTest", view, 0, "an integer");
  REQUIRE(number != nullptr);
  REQUIRE(number->value == 1);

  runtime_sync_storage_cell(slots->at(1), makert<NGString>("world"));
  auto text = require_arg_as<NGString>("slotTest", view, 1, "a string");
  REQUIRE(text != nullptr);
  REQUIRE(text->payload_value() == "world");
}

TEST_CASE("runtime value access adapts storage cells and boxed objects", "[RuntimeTest][Runtime]")
{
  auto number = makert<NGIntegral<int32_t>>(7);
  auto zero = makert<NGIntegral<int32_t>>(0);
  auto slot = make_boxed_storage_cell(number, StorageClass::TEMPORARY);
  slot->name = "value";

  REQUIRE(runtime_value_bool(number));
  REQUIRE_FALSE(runtime_value_bool(zero));
  REQUIRE(runtime_value_bool(slot));
  REQUIRE(runtime_value_show(number) == "7");
  REQUIRE(runtime_value_show(zero) == "0");
  REQUIRE(runtime_value_show(slot) == "7");
  REQUIRE(runtime_value_type(slot)->name == "Int");

  runtime_sync_storage_cell(slot, nullptr);
  REQUIRE(runtime_value_type(slot)->name == "Int");
}

TEST_CASE("runtime value respond prefers type-handle member dispatch", "[RuntimeTest][Runtime]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "WrappedInt";
  nominalType->memberFunctions["kind"] = [](const NGSelf &, const NGEnv &, const NGArgs &) {
    return makert<NGString>("nominal");
  };

  auto nominal = makert<NGNewType>(nominalType, makert<NGIntegral<int32_t>>(3));
  auto env = make_runtime_env();

  auto objectResult = std::dynamic_pointer_cast<NGString>(runtime_value_respond(nominal, "kind", env, {}));
  REQUIRE(objectResult != nullptr);
  REQUIRE(objectResult->payload_value() == "nominal");

  auto slot = make_boxed_storage_cell(nominal, StorageClass::TEMPORARY);
  runtime_sync_storage_cell(slot, nullptr, nominalType);
  auto slotResult = std::dynamic_pointer_cast<NGString>(runtime_value_respond(slot, "kind", env, {}));
  REQUIRE(slotResult != nullptr);
  REQUIRE(slotResult->payload_value() == "nominal");
}

TEST_CASE("runtime show and bool can use nominal type handlers", "[RuntimeTest][Runtime]")
{
  auto nominalType = makert<NGType>();
  nominalType->name = "Flagged";
  nominalType->showHandler = [](const NGSelf &) { return "flagged"; };
  nominalType->boolHandler = [](const NGSelf &) { return false; };

  auto nominal = makert<NGNewType>(nominalType, makert<NGIntegral<int32_t>>(1));
  REQUIRE(runtime_value_show(nominal) == "flagged");
  REQUIRE_FALSE(runtime_value_bool(nominal));

  auto slot = make_boxed_storage_cell(nominal, StorageClass::TEMPORARY);
  runtime_sync_storage_cell(slot, nullptr, nominalType);
  REQUIRE(runtime_value_show(slot) == "flagged");
  REQUIRE_FALSE(runtime_value_bool(slot));
}

TEST_CASE("runtime value respond uses type-descriptor handlers for aggregate and module members", "[RuntimeTest][Runtime]")
{
  auto env = make_runtime_env();

  auto tuple = makert<NGTuple>(Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(7), makert<NGString>("two")});
  auto tupleSize = std::dynamic_pointer_cast<NumeralBase>(runtime_value_respond(tuple, "size", env, {}));
  REQUIRE(tupleSize != nullptr);
  REQUIRE(NGIntegral<uint32_t>::valueOf(tupleSize.get()) == 2);

  auto tagged = makert<NGTaggedValue>("Result", "Ok", 0, Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(42)},
                                      Vec<Str>{"value"});
  auto tag = std::dynamic_pointer_cast<NGString>(runtime_value_respond(tagged, "tag", env, {}));
  REQUIRE(tag != nullptr);
  REQUIRE(tag->payload_value() == "Ok");

  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};
  auto structural = makert<NGStructuralObject>();
  structural->customizedType = objectType;
  structural->replace_payload_fields({makert<NGIntegral<int32_t>>(1), makert<NGIntegral<int32_t>>(2)});
  REQUIRE(runtime_value_type(structural)->name == "Pair");
  REQUIRE(runtime_value_show(structural) == "{ left: 1, right: 2 }");
  auto left = std::dynamic_pointer_cast<NumeralBase>(runtime_value_respond(structural, "left", env, {}));
  REQUIRE(left != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(left.get()) == 1);

  auto module = makert<NGModule>();
  module->objects["hello"] = makert<NGString>("world");
  auto hello = std::dynamic_pointer_cast<NGString>(runtime_value_respond(module, "hello", env, {}));
  REQUIRE(hello != nullptr);
  REQUIRE(hello->payload_value() == "world");
}

TEST_CASE("direct NGObject protocol calls fall through to type-descriptor handlers", "[RuntimeTest][Runtime]")
{
  RuntimeRef<NGObject> tuple = makert<NGTuple>(Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(7), makert<NGString>("two")});
  REQUIRE(tuple->show() == "(7, two)");
  REQUIRE(tuple->boolValue());
  REQUIRE(tuple->type()->name == "Tuple");

  auto pairType = makert<NGType>();
  pairType->name = "Pair";
  pairType->properties = {"left", "right"};
  RuntimeRef<NGObject> structural = makert<NGStructuralObject>();
  auto structuralObject = std::dynamic_pointer_cast<NGStructuralObject>(structural);
  REQUIRE(structuralObject != nullptr);
  structuralObject->customizedType = pairType;
  structuralObject->replace_payload_fields({makert<NGIntegral<int32_t>>(1), makert<NGIntegral<int32_t>>(2)});
  REQUIRE(structural->show() == "{ left: 1, right: 2 }");
  REQUIRE(structural->type()->name == "Pair");

  RuntimeRef<NGObject> tagged =
      makert<NGTaggedValue>("Result", "Ok", 0, Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(42)}, Vec<Str>{"value"});
  REQUIRE(tagged->show() == "Ok(42)");
  REQUIRE(tagged->boolValue());
  REQUIRE(tagged->type()->name == "Result");
}

TEST_CASE("native library binding injects owning module context and state", "[RuntimeTest][Native]")
{
  auto root = makert<RuntimeSymbolTable>();
  auto module = makert<NGModule>(root);

  bind_native_library_handlers(
      module, {{"remember",
                [](const NGSelf &, const NGEnv &context, const NGArgs &) -> RuntimeRef<NGObject> {
                  auto nativeModule = current_native_module(context);
                  REQUIRE(nativeModule != nullptr);
                  nativeModule->set_native_state("sentinel", std::make_shared<int>(42));
                  return makert<NGUnit>();
                }}});

  REQUIRE(current_native_module(make_runtime_env(root)) == nullptr);

  auto result = module->native_functions.at("remember")(nullptr, make_runtime_env(root), {});
  REQUIRE(result != nullptr);

  auto stored = std::static_pointer_cast<int>(module->get_native_state("sentinel"));
  REQUIRE(stored != nullptr);
  REQUIRE(*stored == 42);
  REQUIRE(current_native_module(make_runtime_env(root)) == nullptr);
}

TEST_CASE("heap references are backed by heap storage cells", "[RuntimeTest][GC]")
{
  auto ref = allocate_heap_object(makert<NGIntegral<int32_t>>(3), "heap:value");
  auto cell = ref->storage_cell();

  REQUIRE(cell != nullptr);
  REQUIRE(cell->storageClass == StorageClass::HEAP);

  auto value = std::dynamic_pointer_cast<NumeralBase>(ref->read());
  REQUIRE(value != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(value.get()) == 3);

  ref->write(makert<NGIntegral<int32_t>>(9));
  auto updated = std::dynamic_pointer_cast<NumeralBase>(cell->boxedValue);
  REQUIRE(updated != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(updated.get()) == 9);
}

TEST_CASE("managed heap traces referenced storage cells instead of boxed object identity", "[RuntimeTest][GC]")
{
  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);

  auto shared = makert<NGString>("shared");
  auto keep = allocate_heap_object(shared, "heap:keep");
  auto drop = allocate_heap_object(shared, "heap:drop");

  auto providerId = register_gc_root_provider([keep]() -> Vec<RuntimeRef<NGObject>> { return {keep}; });
  collect_managed_heap();
  unregister_gc_root_provider(providerId);

  REQUIRE(managed_heap_size() == 1);
  REQUIRE_NOTHROW(keep->read());
  REQUIRE_THROWS_WITH(drop->read(), Catch::Matchers::ContainsSubstring("Dangling heap reference"));

  collect_managed_heap();
  REQUIRE(managed_heap_size() == 0);
}

TEST_CASE("struct layout access reads and writes typed fields", "[RuntimeTest][LayoutObjects]")
{
  auto objectType = makert<NGType>();
  objectType->name = "Pair";
  objectType->properties = {"left", "right"};

  auto object = makert<NGStructuralObject>();
  object->customizedType = objectType;
  object->replace_payload_fields({makert<NGIntegral<int32_t>>(1), makert<NGIntegral<int32_t>>(2)});

  auto right = std::dynamic_pointer_cast<NumeralBase>(structural_read_member(object, "right"));
  REQUIRE(right != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(right.get()) == 2);

  structural_write_member(object, "left", makert<NGIntegral<int32_t>>(9));
  auto fields = object->payload_fields();
  auto left = std::dynamic_pointer_cast<NumeralBase>(fields[0]);
  REQUIRE(left != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(left.get()) == 9);
  REQUIRE(object->payload_fields().size() == 2);
  REQUIRE(object->payload_store().get(object->payload_cell()).layout.name == "Structural.payload");

  auto leftSlot = object->field_slot(0);
  REQUIRE(leftSlot != nullptr);
  REQUIRE(structural_member_slot(object, "left") == leftSlot);
  REQUIRE(std::static_pointer_cast<StorageCell>(object->payload_store().get(object->payload_cell()).opaqueRefs[0]) == leftSlot);
  runtime_sync_storage_cell(leftSlot, makert<NGIntegral<int32_t>>(11));

  auto slotted = std::dynamic_pointer_cast<NumeralBase>(structural_read_member(object, "left"));
  REQUIRE(slotted != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(slotted.get()) == 11);
  REQUIRE(NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(object->payload_fields()[0]).get()) == 11);
}

TEST_CASE("struct layout access keeps dynamic properties in slots", "[RuntimeTest][LayoutObjects]")
{
  auto object = makert<NGStructuralObject>();

  structural_write_member(object, "dyn", makert<NGIntegral<int32_t>>(7));
  auto slot = object->property_slot("dyn");
  REQUIRE(slot != nullptr);
  REQUIRE(structural_member_slot(object, "dyn") == slot);
  REQUIRE(slot->boxedValue != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(slot->boxedValue).get()) == 7);

  auto reference = makert<NGReference>(slot, "dyn");
  reference->write(makert<NGIntegral<int32_t>>(9));

  auto value = std::dynamic_pointer_cast<NumeralBase>(structural_read_member(object, "dyn"));
  REQUIRE(value != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(value.get()) == 9);
}

TEST_CASE("array layout access reads and writes indexed elements", "[RuntimeTest][LayoutObjects]")
{
  NGArray array{Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(4), makert<NGIntegral<int32_t>>(5)}};
  REQUIRE(array_length(array) == 2);

  auto value = std::dynamic_pointer_cast<NumeralBase>(array_read_element(array, 1));
  REQUIRE(value != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(value.get()) == 5);
  REQUIRE(array_element_slot(array, 1) != nullptr);

  array_write_element(array, 0, makert<NGIntegral<int32_t>>(9));
  auto updated = std::dynamic_pointer_cast<NumeralBase>(array_read_element(array, 0));
  REQUIRE(updated != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(updated.get()) == 9);

  auto firstSlot = array.element_slot(0);
  REQUIRE(firstSlot != nullptr);
  REQUIRE(array_element_slot(array, 0) == firstSlot);
  REQUIRE(std::static_pointer_cast<StorageCell>(array.header_store().get(array.payload_cell()).opaqueRefs[0]) == firstSlot);
  runtime_sync_storage_cell(firstSlot, makert<NGIntegral<int32_t>>(12));

  auto slotted = std::dynamic_pointer_cast<NumeralBase>(array_read_element(array, 0));
  REQUIRE(slotted != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(slotted.get()) == 12);
  REQUIRE(NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(array.payload_items()[0]).get()) == 12);
}

TEST_CASE("tuple layout access reads members and indexed elements", "[RuntimeTest][LayoutObjects]")
{
  NGTuple tuple{Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(7), makert<NGString>("two")}};
  REQUIRE(tuple_length(tuple) == 2);
  REQUIRE(tuple.payload_items().size() == 2);
  REQUIRE(tuple.payload_store().get(tuple.payload_cell()).layout.name == "Tuple.payload");

  auto first = std::dynamic_pointer_cast<NumeralBase>(tuple_read_element(tuple, 0));
  REQUIRE(first != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(first.get()) == 7);
  REQUIRE(tuple_element_slot(tuple, 0) != nullptr);

  auto size = std::dynamic_pointer_cast<NumeralBase>(tuple_read_member(tuple, "size"));
  REQUIRE(size != nullptr);
  REQUIRE(NGIntegral<uint32_t>::valueOf(size.get()) == 2);

  tuple_write_element(tuple, 0, makert<NGIntegral<int32_t>>(9));
  auto updated = std::dynamic_pointer_cast<NumeralBase>(tuple_read_member(tuple, "0"));
  REQUIRE(updated != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(updated.get()) == 9);
  REQUIRE(tuple.payload_items().size() == 2);

  auto firstSlot = tuple.element_slot(0);
  REQUIRE(firstSlot != nullptr);
  REQUIRE(tuple_element_slot(tuple, 0) == firstSlot);
  REQUIRE(std::static_pointer_cast<StorageCell>(tuple.payload_store().get(tuple.payload_cell()).opaqueRefs[0]) == firstSlot);
  runtime_sync_storage_cell(firstSlot, makert<NGIntegral<int32_t>>(12));

  auto slotted = std::dynamic_pointer_cast<NumeralBase>(tuple_read_element(tuple, 0));
  REQUIRE(slotted != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(slotted.get()) == 12);
  REQUIRE(NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(tuple.payload_items()[0]).get()) == 12);
}

TEST_CASE("string layout access reads code units and header-backed length", "[RuntimeTest][LayoutObjects]")
{
  NGString string{"A2"};
  REQUIRE(string_length(string) == 2);
  REQUIRE(string_data_handle(string).typeName == "String.payload");
  REQUIRE(string.payload_value() == "A2");

  auto second = std::dynamic_pointer_cast<NumeralBase>(string_read_code_unit(string, 1));
  REQUIRE(second != nullptr);
  REQUIRE(NGIntegral<int32_t>::valueOf(second.get()) == 50);
}

TEST_CASE("dynamic runtime types expose header layouts", "[RuntimeTest][LayoutObjects]")
{
  auto arrayType = NGArray::arrayType();
  auto stringType = NGString::stringType();

  REQUIRE(arrayType->layout.kind == LayoutKind::DYNAMIC);
  REQUIRE(arrayType->layout.fields.size() == 3);
  REQUIRE(arrayType->layout.fields[2].name == "data");

  REQUIRE(stringType->layout.kind == LayoutKind::DYNAMIC);
  REQUIRE(stringType->layout.fields.size() == 2);
  REQUIRE(stringType->layout.fields[1].name == "data");
}

TEST_CASE("array and string runtime values sync header cells", "[RuntimeTest][LayoutObjects]")
{
  auto array = makert<NGArray>(Vec<RuntimeRef<NGObject>>{makert<NGIntegral<int32_t>>(1), makert<NGIntegral<int32_t>>(2)});
  auto string = makert<NGString>("hello");

  auto arrayRef = array->header_cell();
  auto stringRef = string->header_cell();
  const auto &arrayLayout = array->header_store().get(arrayRef).layout;
  const auto &stringLayout = string->header_store().get(stringRef).layout;
  auto *arrayLength = NG::buffer_runtime::find_field(arrayLayout, "length");
  auto *arrayCapacity = NG::buffer_runtime::find_field(arrayLayout, "capacity");
  auto *stringLength = NG::buffer_runtime::find_field(stringLayout, "length");

  REQUIRE(arrayLength != nullptr);
  REQUIRE(arrayCapacity != nullptr);
  REQUIRE(stringLength != nullptr);
  REQUIRE(NG::buffer_runtime::read_u64_field(array->header_store(), arrayRef, *arrayLength) == 2);
  REQUIRE(NG::buffer_runtime::read_u64_field(array->header_store(), arrayRef, *arrayCapacity) >= 2);
  REQUIRE(NG::buffer_runtime::read_u64_field(string->header_store(), stringRef, *stringLength) == 5);
  REQUIRE(array->header_data_handle().typeName == "Array.payload");
  REQUIRE(array->payload_items().size() == 2);
  REQUIRE(array->header_data_handle().address ==
          reinterpret_cast<uintptr_t>(array->header_store().get(array->payload_cell()).bytes.data()));
  REQUIRE(string->header_data_handle().typeName == "String.payload");
  REQUIRE(string->payload_value() == "hello");
  REQUIRE(string->header_data_handle().address ==
          reinterpret_cast<uintptr_t>(string->header_store().get(string->payload_cell()).bytes.data()));

  auto appended = std::dynamic_pointer_cast<NGArray>(array->opLShift(makert<NGIntegral<int32_t>>(3)));
  string->replace_payload_value("hello!");

  REQUIRE(appended != nullptr);
  REQUIRE(array_length(*array) == 3);
  REQUIRE(string->header_length() == 6);
  REQUIRE(string->payload_value() == "hello!");
  REQUIRE(NG::buffer_runtime::read_u64_field(array->header_store(), arrayRef, *arrayLength) == 3);
  REQUIRE(NG::buffer_runtime::read_u64_field(string->header_store(), stringRef, *stringLength) == 6);
  REQUIRE(array->payload_items().size() == 3);
  REQUIRE(array->header_data_handle().address ==
          reinterpret_cast<uintptr_t>(array->header_store().get(array->payload_cell()).bytes.data()));
  REQUIRE(string->header_data_handle().address ==
          reinterpret_cast<uintptr_t>(string->header_store().get(string->payload_cell()).bytes.data()));
}
