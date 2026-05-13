#include "../test.hpp"
#include <runtime/buffer_runtime.hpp>

#include <limits>

using namespace NG::buffer_runtime;

TEST_CASE("LayoutRegistry stores and returns layouts", "[RuntimeTest][BufferRuntime]")
{
  LayoutRegistry registry;
  TypeLayout layout{
      .name = "Pair",
      .kind = LayoutKind::INLINE_VALUE,
      .size = 8,
      .alignment = 4,
      .fields = {FieldLayout{.name = "left", .offset = 0, .size = 4},
                 FieldLayout{.name = "right", .offset = 4, .size = 4}},
      .triviallyCopyable = true,
      .triviallyMovable = true,
  };

  auto id = registry.register_layout(layout);
  const auto &stored = registry.get_layout(id);

  REQUIRE(stored.name == "Pair");
  REQUIRE(stored.size == 8);
  REQUIRE(stored.fields.size() == 2);
  REQUIRE(stored.fields[1].offset == 4);
}

TEST_CASE("HeapStore allocates and reads raw cell bytes", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  TypeLayout layout{.name = "i32", .kind = LayoutKind::INLINE_VALUE, .size = 4, .alignment = 4};

  auto ref = heap.allocate(layout);
  REQUIRE(ref.valid());

  heap.write(ref, 0, Vec<uint8_t>{1, 2, 3, 4});
  auto bytes = heap.read(ref, 0, 4);

  REQUIRE(bytes == Vec<uint8_t>{1, 2, 3, 4});
}

TEST_CASE("HeapStore rejects out-of-bounds and overflowing byte ranges", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  TypeLayout layout{.name = "bytes", .kind = LayoutKind::DYNAMIC, .size = 4, .alignment = 1};
  auto ref = heap.allocate(layout);

  REQUIRE_THROWS_AS(heap.read(ref, 5, 1), std::out_of_range);
  REQUIRE_THROWS_AS(heap.write(ref, 2, Vec<uint8_t>{1, 2, 3}), std::out_of_range);
  REQUIRE_THROWS_AS(heap.read(ref, std::numeric_limits<size_t>::max(), 1), std::out_of_range);
  REQUIRE_THROWS_AS(heap.write(CellRef{.cellId = ref.cellId, .offset = 3}, 1, Vec<uint8_t>{1}), std::out_of_range);
  REQUIRE_THROWS_AS(heap.read(CellRef{}, 0, 0), std::out_of_range);
  REQUIRE_THROWS_AS(heap.get(CellRef{.cellId = 999}), std::out_of_range);
}

TEST_CASE("HeapStore honors CellRef offsets for subrange reads and writes", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  TypeLayout layout{.name = "bytes", .kind = LayoutKind::DYNAMIC, .size = 6, .alignment = 1};
  auto ref = heap.allocate(layout);
  heap.write(ref, 0, Vec<uint8_t>{0, 1, 2, 3, 4, 5});

  auto subRef = CellRef{.cellId = ref.cellId, .offset = 2};
  REQUIRE(heap.read(subRef, 0, 3) == Vec<uint8_t>{2, 3, 4});
  heap.write(subRef, 1, Vec<uint8_t>{9, 8});
  REQUIRE(heap.read(ref, 0, 6) == Vec<uint8_t>{0, 1, 2, 9, 8, 5});
}

TEST_CASE("make_slot sizes frame slots from layout", "[RuntimeTest][BufferRuntime]")
{
  TypeLayout layout{.name = "Result", .kind = LayoutKind::TAGGED_UNION, .size = 24, .alignment = 8};
  auto slot = make_slot("ret", layout);

  REQUIRE(slot.name == "ret");
  REQUIRE(slot.layout.name == "Result");
  REQUIRE(slot.bytes.size() == 24);
}

TEST_CASE("make_inline_layout computes field offsets and trailing padding", "[RuntimeTest][BufferRuntime]")
{
  LayoutRegistry registry;
  auto boolId = registry.register_layout(
      TypeLayout{.name = "bool", .kind = LayoutKind::INLINE_VALUE, .size = 1, .alignment = 1, .triviallyCopyable = true,
                 .triviallyMovable = true});
  auto i32Id = registry.register_layout(
      TypeLayout{.name = "i32", .kind = LayoutKind::INLINE_VALUE, .size = 4, .alignment = 4, .triviallyCopyable = true,
                 .triviallyMovable = true});

  auto layout = make_inline_layout("Pair", {FieldSpec{.name = "left", .layoutId = i32Id},
                                            FieldSpec{.name = "flag", .layoutId = boolId}},
                                   registry);

  REQUIRE(layout.kind == LayoutKind::INLINE_VALUE);
  REQUIRE(layout.fields.size() == 2);
  REQUIRE(layout.fields[0].offset == 0);
  REQUIRE(layout.fields[1].offset == 4);
  REQUIRE(layout.size == 8);
  REQUIRE(layout.alignment == 4);
}

TEST_CASE("make_tagged_union_layout reserves a shared payload area", "[RuntimeTest][BufferRuntime]")
{
  LayoutRegistry registry;
  auto i32Id = registry.register_layout(
      TypeLayout{.name = "i32", .kind = LayoutKind::INLINE_VALUE, .size = 4, .alignment = 4, .triviallyCopyable = true,
                 .triviallyMovable = true});
  auto i64Id = registry.register_layout(
      TypeLayout{.name = "i64", .kind = LayoutKind::INLINE_VALUE, .size = 8, .alignment = 8, .triviallyCopyable = true,
                 .triviallyMovable = true});

  auto layout = make_tagged_union_layout(
      "Value",
      {
          VariantSpec{.name = "Int", .fields = {FieldSpec{.name = "value", .layoutId = i32Id}}},
          VariantSpec{.name = "Wide", .fields = {FieldSpec{.name = "value", .layoutId = i64Id}}},
      },
      registry);

  REQUIRE(layout.kind == LayoutKind::TAGGED_UNION);
  REQUIRE(layout.variants.size() == 2);
  REQUIRE(layout.alignment == 8);
  REQUIRE(layout.variants[0].payloadOffset == 8);
  REQUIRE(layout.variants[0].fields[0].offset == 8);
  REQUIRE(layout.variants[1].fields[0].offset == 8);
  REQUIRE(layout.size == 16);
}

TEST_CASE("reference and native handle layouts use runtime carrier sizes", "[RuntimeTest][BufferRuntime]")
{
  auto refLayout = make_reference_layout();
  auto handleLayout = make_native_handle_layout("native<Window>");

  REQUIRE(refLayout.kind == LayoutKind::REFERENCE);
  REQUIRE(refLayout.size == sizeof(CellRef));
  REQUIRE(refLayout.alignment == alignof(CellRef));
  REQUIRE(refLayout.triviallyCopyable);

  REQUIRE(handleLayout.kind == LayoutKind::NATIVE_HANDLE);
  REQUIRE(handleLayout.size == sizeof(NativeHandle));
  REQUIRE(handleLayout.alignment == alignof(NativeHandle));
  REQUIRE_FALSE(handleLayout.triviallyCopyable);
}

TEST_CASE("array and string header layouts describe dynamic payload headers", "[RuntimeTest][BufferRuntime]")
{
  auto arrayLayout = make_array_header_layout();
  auto stringLayout = make_string_header_layout();

  REQUIRE(arrayLayout.kind == LayoutKind::DYNAMIC);
  REQUIRE(arrayLayout.fields.size() == 3);
  REQUIRE(arrayLayout.fields[0].name == "length");
  REQUIRE(arrayLayout.fields[1].name == "capacity");
  REQUIRE(arrayLayout.fields[2].name == "data");
  REQUIRE(arrayLayout.containsPointers);

  REQUIRE(stringLayout.kind == LayoutKind::DYNAMIC);
  REQUIRE(stringLayout.fields.size() == 2);
  REQUIRE(stringLayout.fields[0].name == "length");
  REQUIRE(stringLayout.fields[1].name == "data");
  REQUIRE(stringLayout.containsPointers);
}

TEST_CASE("heap store allocates typed array and string headers", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;

  auto arrayRef = allocate_array_header(heap, 3, 8);
  auto stringRef = allocate_string_header(heap, 11);

  const auto &arrayLayout = heap.get(arrayRef).layout;
  const auto &stringLayout = heap.get(stringRef).layout;

  auto *arrayLength = find_field(arrayLayout, "length");
  auto *arrayCapacity = find_field(arrayLayout, "capacity");
  auto *arrayData = find_field(arrayLayout, "data");
  auto *stringLength = find_field(stringLayout, "length");
  auto *stringData = find_field(stringLayout, "data");

  REQUIRE(arrayLength != nullptr);
  REQUIRE(arrayCapacity != nullptr);
  REQUIRE(arrayData != nullptr);
  REQUIRE(stringLength != nullptr);
  REQUIRE(stringData != nullptr);

  REQUIRE(read_u64_field(heap, arrayRef, *arrayLength) == 3);
  REQUIRE(read_u64_field(heap, arrayRef, *arrayCapacity) == 8);
  REQUIRE(read_u64_field(heap, stringRef, *stringLength) == 11);
  REQUIRE(read_native_handle_field(heap, arrayRef, *arrayData).address == 0);
  REQUIRE(read_native_handle_field(heap, stringRef, *stringData).address == 0);
}

TEST_CASE("heap store preserves native handle side-table fields", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  auto ref = allocate_string_header(heap, 4);
  const auto &layout = heap.get(ref).layout;
  auto *data = find_field(layout, "data");

  REQUIRE(data != nullptr);

  write_native_handle_field(heap, ref, *data, NativeHandle{
                                              .typeName = "char[]",
                                              .address = 0x1234,
                                              .owning = false,
                                          });

  auto handle = read_native_handle_field(heap, ref, *data);
  REQUIRE(handle.typeName == "char[]");
  REQUIRE(handle.address == 0x1234);
  REQUIRE_FALSE(handle.owning);
}

TEST_CASE("heap native handle fields respect CellRef offsets", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  TypeLayout layout{.name = "handles", .kind = LayoutKind::DYNAMIC, .size = 32, .alignment = 8};
  auto ref = heap.allocate(layout);
  auto subRef = CellRef{.cellId = ref.cellId, .offset = 8};
  FieldLayout field{.name = "handle", .offset = 4, .size = sizeof(NativeHandle)};

  write_native_handle_field(heap, subRef, field, NativeHandle{
                                                    .typeName = "sub",
                                                    .address = 0xCAFE,
                                                    .owning = true,
                                                });

  REQUIRE(read_native_handle_field(heap, ref, FieldLayout{.name = "absolute", .offset = 12}).typeName == "sub");
  REQUIRE(read_native_handle_field(heap, subRef, field).address == 0xCAFE);
  REQUIRE(read_native_handle_field(heap, ref, field).address == 0);
}

TEST_CASE("heap store allocates and reads string payload cells", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  auto ref = allocate_string_payload(heap, "hello");

  REQUIRE(heap.get(ref).layout.name == "String.payload");
  REQUIRE(read_string_payload(heap, ref) == "hello");

  write_string_payload(heap, ref, "world");
  REQUIRE(read_string_payload(heap, ref) == "world");
  REQUIRE(read_string_payload(heap, CellRef{.cellId = ref.cellId, .offset = 1}) == "orld");
  REQUIRE_THROWS_AS(write_string_payload(heap, ref, "too long"), std::out_of_range);
  REQUIRE_THROWS_AS(read_string_payload(heap, CellRef{.cellId = ref.cellId, .offset = 99}), std::out_of_range);
}

TEST_CASE("heap field helpers validate u64 field size and default native handles", "[RuntimeTest][BufferRuntime]")
{
  HeapStore heap;
  TypeLayout layout{.name = "small", .kind = LayoutKind::INLINE_VALUE, .size = 4, .alignment = 4};
  auto ref = heap.allocate(layout);
  FieldLayout smallField{.name = "small", .offset = 0, .size = 4};
  FieldLayout absentHandle{.name = "handle", .offset = 0, .size = sizeof(NativeHandle)};

  REQUIRE_THROWS_AS(write_u64_field(heap, ref, smallField, 1), std::out_of_range);
  REQUIRE_THROWS_AS(read_u64_field(heap, ref, smallField), std::out_of_range);
  REQUIRE(read_native_handle_field(heap, ref, absentHandle).address == 0);
}
