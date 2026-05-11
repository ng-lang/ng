#pragma once

#include <common.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace NG::buffer_runtime
{
  enum class LayoutKind : uint8_t
  {
    INLINE_VALUE,
    TAGGED_UNION,
    REFERENCE,
    DYNAMIC,
    NATIVE_HANDLE
  };

  enum class StorageClass : uint8_t
  {
    FRAME,
    HEAP,
    GLOBAL,
    TEMPORARY,
    NATIVE
  };

  struct FieldLayout
  {
    Str name;
    uint32_t layoutId = 0;
    size_t offset = 0;
    size_t size = 0;
    size_t alignment = 1;
  };

  struct FieldSpec
  {
    Str name;
    uint32_t layoutId = 0;
  };

  struct VariantLayout
  {
    Str name;
    uint32_t tag = 0;
    size_t payloadOffset = 0;
    size_t payloadSize = 0;
    size_t alignment = 1;
    Vec<FieldLayout> fields;
  };

  struct VariantSpec
  {
    Str name;
    Vec<FieldSpec> fields;
  };

  struct TypeLayout
  {
    uint32_t id = 0;
    Str name;
    LayoutKind kind = LayoutKind::DYNAMIC;
    size_t size = 0;
    size_t alignment = alignof(std::max_align_t);
    Vec<FieldLayout> fields;
    Vec<VariantLayout> variants;
    bool containsPointers = false;
    bool triviallyCopyable = false;
    bool triviallyMovable = false;
  };

  struct NativeHandle
  {
    Str typeName;
    uintptr_t address = 0;
    bool owning = false;
  };

  struct CellRef
  {
    uint64_t cellId = 0;
    size_t offset = 0;

    [[nodiscard]] auto valid() const -> bool { return cellId != 0; }
  };

  struct HeapCell
  {
    uint64_t id = 0;
    StorageClass storageClass = StorageClass::HEAP;
    TypeLayout layout;
    Vec<uint8_t> bytes;
    Map<size_t, NativeHandle> nativeHandles;
    Vec<std::shared_ptr<void>> opaqueRefs;
  };

  struct FrameSlot
  {
    Str name;
    StorageClass storageClass = StorageClass::FRAME;
    TypeLayout layout;
    Vec<uint8_t> bytes;
  };

  struct CallFrame
  {
    Str functionName;
    Vec<FrameSlot> parameters;
    Vec<FrameSlot> locals;
    Vec<FrameSlot> temporaries;
    std::optional<FrameSlot> receiver;
    std::optional<FrameSlot> returnSlot;
  };

  class LayoutRegistry
  {
  public:
    auto register_layout(TypeLayout layout) -> uint32_t;
    auto get_layout(uint32_t layoutId) const -> const TypeLayout &;

  private:
    uint32_t nextId = 1;
    Map<uint32_t, TypeLayout> layouts;
  };

  class HeapStore
  {
  public:
    auto allocate(const TypeLayout &layout, StorageClass storageClass = StorageClass::HEAP) -> CellRef;
    auto get(CellRef ref) -> HeapCell &;
    auto get(CellRef ref) const -> const HeapCell &;
    void write(CellRef ref, size_t offset, const Vec<uint8_t> &data);
    [[nodiscard]] auto read(CellRef ref, size_t offset, size_t size) const -> Vec<uint8_t>;

  private:
    uint64_t nextId = 1;
    Map<uint64_t, HeapCell> cells;
  };

  [[nodiscard]] auto make_slot(Str name, const TypeLayout &layout,
                               StorageClass storageClass = StorageClass::FRAME) -> FrameSlot;
  [[nodiscard]] auto make_inline_layout(Str name, const Vec<FieldSpec> &fields,
                                        const LayoutRegistry &registry) -> TypeLayout;
  [[nodiscard]] auto make_tagged_union_layout(Str name, const Vec<VariantSpec> &variants,
                                              const LayoutRegistry &registry) -> TypeLayout;
  [[nodiscard]] auto make_reference_layout(Str name = "ref") -> TypeLayout;
  [[nodiscard]] auto make_native_handle_layout(Str name) -> TypeLayout;
  [[nodiscard]] auto make_byte_buffer_layout(size_t size, Str name = "bytes") -> TypeLayout;
  [[nodiscard]] auto make_array_header_layout(Str name = "Array") -> TypeLayout;
  [[nodiscard]] auto make_string_header_layout(Str name = "String") -> TypeLayout;
  [[nodiscard]] auto find_field(const TypeLayout &layout, const Str &name) -> const FieldLayout *;
  void write_u64_field(HeapStore &heap, CellRef ref, const FieldLayout &field, uint64_t value);
  [[nodiscard]] auto read_u64_field(const HeapStore &heap, CellRef ref, const FieldLayout &field) -> uint64_t;
  void write_native_handle_field(HeapStore &heap, CellRef ref, const FieldLayout &field, const NativeHandle &value);
  [[nodiscard]] auto read_native_handle_field(const HeapStore &heap, CellRef ref, const FieldLayout &field) -> NativeHandle;
  [[nodiscard]] auto allocate_array_header(HeapStore &heap, uint64_t length, uint64_t capacity) -> CellRef;
  [[nodiscard]] auto allocate_string_header(HeapStore &heap, uint64_t length) -> CellRef;
  [[nodiscard]] auto allocate_string_payload(HeapStore &heap, const Str &value) -> CellRef;
  void write_string_payload(HeapStore &heap, CellRef ref, const Str &value);
  [[nodiscard]] auto read_string_payload(const HeapStore &heap, CellRef ref) -> Str;
} // namespace NG::buffer_runtime
