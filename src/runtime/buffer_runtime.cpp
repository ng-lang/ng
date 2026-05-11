#include <runtime/buffer_runtime.hpp>

#include <cstring>

namespace NG::buffer_runtime
{
  namespace
  {
    [[nodiscard]] auto align_to(size_t value, size_t alignment) -> size_t
    {
      if (alignment <= 1)
      {
        return value;
      }
      auto remainder = value % alignment;
      return remainder == 0 ? value : value + (alignment - remainder);
    }
  } // namespace

  auto LayoutRegistry::register_layout(TypeLayout layout) -> uint32_t
  {
    uint32_t assignedId = layout.id;
    if (layout.id == 0)
    {
      assignedId = nextId++;
      layout.id = assignedId;
    }
    else
    {
      nextId = std::max(nextId, layout.id + 1);
    }
    layouts.insert_or_assign(layout.id, std::move(layout));
    return assignedId;
  }

  auto LayoutRegistry::get_layout(uint32_t layoutId) const -> const TypeLayout &
  {
    auto it = layouts.find(layoutId);
    if (it == layouts.end())
    {
      throw std::out_of_range("Unknown layout id");
    }
    return it->second;
  }

  auto HeapStore::allocate(const TypeLayout &layout, StorageClass storageClass) -> CellRef
  {
    HeapCell cell;
    cell.id = nextId++;
    cell.storageClass = storageClass;
    cell.layout = layout;
    cell.bytes.resize(layout.size);
    cells.insert_or_assign(cell.id, cell);
    return CellRef{.cellId = cell.id, .offset = 0};
  }

  auto HeapStore::get(CellRef ref) -> HeapCell &
  {
    auto it = cells.find(ref.cellId);
    if (it == cells.end())
    {
      throw std::out_of_range("Unknown heap cell");
    }
    return it->second;
  }

  auto HeapStore::get(CellRef ref) const -> const HeapCell &
  {
    auto it = cells.find(ref.cellId);
    if (it == cells.end())
    {
      throw std::out_of_range("Unknown heap cell");
    }
    return it->second;
  }

  void HeapStore::write(CellRef ref, size_t offset, const Vec<uint8_t> &data)
  {
    auto &cell = get(ref);
    if (offset + data.size() > cell.bytes.size())
    {
      throw std::out_of_range("Write exceeds cell bounds");
    }
    std::copy(data.begin(), data.end(), cell.bytes.begin() + static_cast<ptrdiff_t>(offset));
  }

  auto HeapStore::read(CellRef ref, size_t offset, size_t size) const -> Vec<uint8_t>
  {
    const auto &cell = get(ref);
    if (offset + size > cell.bytes.size())
    {
      throw std::out_of_range("Read exceeds cell bounds");
    }
    return Vec<uint8_t>{cell.bytes.begin() + static_cast<ptrdiff_t>(offset),
                        cell.bytes.begin() + static_cast<ptrdiff_t>(offset + size)};
  }

  auto make_slot(Str name, const TypeLayout &layout, StorageClass storageClass) -> FrameSlot
  {
    FrameSlot slot;
    slot.name = std::move(name);
    slot.storageClass = storageClass;
    slot.layout = layout;
    slot.bytes.resize(layout.size);
    return slot;
  }

  auto make_inline_layout(Str name, const Vec<FieldSpec> &fields, const LayoutRegistry &registry) -> TypeLayout
  {
    TypeLayout layout;
    layout.name = std::move(name);
    layout.kind = LayoutKind::INLINE_VALUE;
    layout.alignment = 1;
    layout.triviallyCopyable = true;
    layout.triviallyMovable = true;

    size_t offset = 0;
    for (const auto &fieldSpec : fields)
    {
      const auto &fieldType = registry.get_layout(fieldSpec.layoutId);
      const auto fieldAlignment = std::max<size_t>(fieldType.alignment, 1);
      offset = align_to(offset, fieldAlignment);

      layout.fields.push_back(FieldLayout{
          .name = fieldSpec.name,
          .layoutId = fieldType.id,
          .offset = offset,
          .size = fieldType.size,
          .alignment = fieldAlignment,
      });

      offset += fieldType.size;
      layout.alignment = std::max(layout.alignment, fieldAlignment);
      layout.containsPointers = layout.containsPointers || fieldType.containsPointers;
      layout.triviallyCopyable = layout.triviallyCopyable && fieldType.triviallyCopyable;
      layout.triviallyMovable = layout.triviallyMovable && fieldType.triviallyMovable;
    }

    layout.size = align_to(offset, layout.alignment);
    return layout;
  }

  auto make_tagged_union_layout(Str name, const Vec<VariantSpec> &variants, const LayoutRegistry &registry)
      -> TypeLayout
  {
    TypeLayout layout;
    layout.name = std::move(name);
    layout.kind = LayoutKind::TAGGED_UNION;
    layout.alignment = alignof(uint32_t);
    layout.triviallyCopyable = true;
    layout.triviallyMovable = true;

    size_t maxPayloadSize = 0;
    size_t maxPayloadAlignment = 1;

    for (size_t i = 0; i < variants.size(); ++i)
    {
      const auto &variantSpec = variants[i];
      VariantLayout variant;
      variant.name = variantSpec.name;
      variant.tag = static_cast<uint32_t>(i);
      variant.alignment = 1;

      size_t payloadSize = 0;
      for (const auto &fieldSpec : variantSpec.fields)
      {
        const auto &fieldType = registry.get_layout(fieldSpec.layoutId);
        const auto fieldAlignment = std::max<size_t>(fieldType.alignment, 1);
        payloadSize = align_to(payloadSize, fieldAlignment);

        variant.fields.push_back(FieldLayout{
            .name = fieldSpec.name,
            .layoutId = fieldType.id,
            .offset = payloadSize,
            .size = fieldType.size,
            .alignment = fieldAlignment,
        });

        payloadSize += fieldType.size;
        variant.alignment = std::max(variant.alignment, fieldAlignment);
        layout.containsPointers = layout.containsPointers || fieldType.containsPointers;
        layout.triviallyCopyable = layout.triviallyCopyable && fieldType.triviallyCopyable;
        layout.triviallyMovable = layout.triviallyMovable && fieldType.triviallyMovable;
      }

      variant.payloadSize = align_to(payloadSize, variant.alignment);
      layout.variants.push_back(variant);
      maxPayloadSize = std::max(maxPayloadSize, variant.payloadSize);
      maxPayloadAlignment = std::max(maxPayloadAlignment, variant.alignment);
    }

    layout.alignment = std::max(layout.alignment, maxPayloadAlignment);
    const auto payloadOffset = align_to(sizeof(uint32_t), maxPayloadAlignment);

    for (auto &variant : layout.variants)
    {
      variant.payloadOffset = payloadOffset;
      for (auto &field : variant.fields)
      {
        field.offset += payloadOffset;
      }
    }

    layout.size = align_to(payloadOffset + maxPayloadSize, layout.alignment);
    return layout;
  }

  auto make_reference_layout(Str name) -> TypeLayout
  {
    return TypeLayout{
        .name = std::move(name),
        .kind = LayoutKind::REFERENCE,
        .size = sizeof(CellRef),
        .alignment = alignof(CellRef),
        .containsPointers = true,
        .triviallyCopyable = true,
        .triviallyMovable = true,
    };
  }

  auto make_native_handle_layout(Str name) -> TypeLayout
  {
    return TypeLayout{
        .name = std::move(name),
        .kind = LayoutKind::NATIVE_HANDLE,
        .size = sizeof(NativeHandle),
        .alignment = alignof(NativeHandle),
        .containsPointers = true,
        .triviallyCopyable = false,
        .triviallyMovable = true,
    };
  }

  auto make_byte_buffer_layout(size_t size, Str name) -> TypeLayout
  {
    return TypeLayout{
        .name = std::move(name),
        .kind = LayoutKind::DYNAMIC,
        .size = size,
        .alignment = 1,
        .containsPointers = false,
        .triviallyCopyable = true,
        .triviallyMovable = true,
    };
  }

  auto make_array_header_layout(Str name) -> TypeLayout
  {
    auto handleLayout = make_native_handle_layout(name + ".buffer");
    auto lengthOffset = 0ZU;
    auto capacityOffset = align_to(lengthOffset + sizeof(uint64_t), alignof(uint64_t));
    auto dataOffset = align_to(capacityOffset + sizeof(uint64_t), handleLayout.alignment);
    auto alignment = std::max(alignof(uint64_t), handleLayout.alignment);

    return TypeLayout{
        .name = std::move(name),
        .kind = LayoutKind::DYNAMIC,
        .size = align_to(dataOffset + handleLayout.size, alignment),
        .alignment = alignment,
        .fields =
            {
                FieldLayout{.name = "length", .offset = lengthOffset, .size = sizeof(uint64_t), .alignment = alignof(uint64_t)},
                FieldLayout{.name = "capacity", .offset = capacityOffset, .size = sizeof(uint64_t), .alignment = alignof(uint64_t)},
                FieldLayout{.name = "data", .offset = dataOffset, .size = handleLayout.size, .alignment = handleLayout.alignment},
            },
        .containsPointers = true,
        .triviallyCopyable = false,
        .triviallyMovable = true,
    };
  }

  auto make_string_header_layout(Str name) -> TypeLayout
  {
    auto handleLayout = make_native_handle_layout(name + ".buffer");
    auto lengthOffset = 0ZU;
    auto dataOffset = align_to(lengthOffset + sizeof(uint64_t), handleLayout.alignment);
    auto alignment = std::max(alignof(uint64_t), handleLayout.alignment);

    return TypeLayout{
        .name = std::move(name),
        .kind = LayoutKind::DYNAMIC,
        .size = align_to(dataOffset + handleLayout.size, alignment),
        .alignment = alignment,
        .fields =
            {
                FieldLayout{.name = "length", .offset = lengthOffset, .size = sizeof(uint64_t), .alignment = alignof(uint64_t)},
                FieldLayout{.name = "data", .offset = dataOffset, .size = handleLayout.size, .alignment = handleLayout.alignment},
            },
        .containsPointers = true,
        .triviallyCopyable = false,
        .triviallyMovable = true,
    };
  }

  auto find_field(const TypeLayout &layout, const Str &name) -> const FieldLayout *
  {
    for (const auto &field : layout.fields)
    {
      if (field.name == name)
      {
        return &field;
      }
    }
    return nullptr;
  }

  void write_u64_field(HeapStore &heap, CellRef ref, const FieldLayout &field, uint64_t value)
  {
    Vec<uint8_t> bytes(sizeof(uint64_t));
    std::memcpy(bytes.data(), &value, sizeof(uint64_t));
    heap.write(ref, field.offset, bytes);
  }

  auto read_u64_field(const HeapStore &heap, CellRef ref, const FieldLayout &field) -> uint64_t
  {
    auto bytes = heap.read(ref, field.offset, sizeof(uint64_t));
    uint64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(uint64_t));
    return value;
  }

  void write_native_handle_field(HeapStore &heap, CellRef ref, const FieldLayout &field, const NativeHandle &value)
  {
    auto &cell = heap.get(ref);
    cell.nativeHandles.insert_or_assign(field.offset, value);
  }

  auto read_native_handle_field(const HeapStore &heap, CellRef ref, const FieldLayout &field) -> NativeHandle
  {
    const auto &cell = heap.get(ref);
    auto it = cell.nativeHandles.find(field.offset);
    if (it == cell.nativeHandles.end())
    {
      return NativeHandle{};
    }
    return it->second;
  }

  auto allocate_array_header(HeapStore &heap, uint64_t length, uint64_t capacity) -> CellRef
  {
    auto ref = heap.allocate(make_array_header_layout());
    const auto &layout = heap.get(ref).layout;
    write_u64_field(heap, ref, *find_field(layout, "length"), length);
    write_u64_field(heap, ref, *find_field(layout, "capacity"), capacity);
    return ref;
  }

  auto allocate_string_header(HeapStore &heap, uint64_t length) -> CellRef
  {
    auto ref = heap.allocate(make_string_header_layout());
    const auto &layout = heap.get(ref).layout;
    write_u64_field(heap, ref, *find_field(layout, "length"), length);
    return ref;
  }

  auto allocate_string_payload(HeapStore &heap, const Str &value) -> CellRef
  {
    auto ref = heap.allocate(make_byte_buffer_layout(value.size(), "String.payload"));
    write_string_payload(heap, ref, value);
    return ref;
  }

  void write_string_payload(HeapStore &heap, CellRef ref, const Str &value)
  {
    auto &cell = heap.get(ref);
    if (cell.bytes.size() != value.size())
    {
      throw std::out_of_range("String payload write exceeds cell bounds");
    }
    heap.write(ref, 0, Vec<uint8_t>(value.begin(), value.end()));
  }

  auto read_string_payload(const HeapStore &heap, CellRef ref) -> Str
  {
    const auto &cell = heap.get(ref);
    auto bytes = heap.read(ref, 0, cell.bytes.size());
    return Str(bytes.begin(), bytes.end());
  }
} // namespace NG::buffer_runtime
