
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>
#include <runtime/string_layout_access.hpp>
namespace NG::runtime
{
  NGString::NGString(Str str) : value{std::move(str)}
  {
    sync_header_backing();
  }

  void NGString::sync_header_backing() const
  {
    if (!headerRef.valid())
    {
      headerRef = buffer_runtime::allocate_string_header(headerStore, value.size());
    }
    if (!payloadRef.valid())
    {
      payloadRef = buffer_runtime::allocate_string_payload(headerStore, value);
    }
    const_cast<NGString *>(this)->value = buffer_runtime::read_string_payload(headerStore, payloadRef);

    const auto &layout = headerStore.get(headerRef).layout;
    buffer_runtime::write_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "length"), value.size());
    buffer_runtime::write_native_handle_field(
        headerStore, headerRef, *buffer_runtime::find_field(layout, "data"),
        NativeHandle{
            .typeName = "String.payload",
            .address = reinterpret_cast<uintptr_t>(headerStore.get(payloadRef).bytes.data()),
            .owning = false,
        });
  }

  auto NGString::header_cell() const -> CellRef
  {
    sync_header_backing();
    return headerRef;
  }

  auto NGString::header_length() const -> uint64_t
  {
    sync_header_backing();
    const auto &layout = headerStore.get(headerRef).layout;
    return buffer_runtime::read_u64_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "length"));
  }

  auto NGString::header_data_handle() const -> NativeHandle
  {
    sync_header_backing();
    const auto &layout = headerStore.get(headerRef).layout;
    return buffer_runtime::read_native_handle_field(headerStore, headerRef, *buffer_runtime::find_field(layout, "data"));
  }

  auto NGString::payload_cell() const -> CellRef
  {
    sync_header_backing();
    return payloadRef;
  }

  auto NGString::payload_value() const -> Str
  {
    sync_header_backing();
    return buffer_runtime::read_string_payload(headerStore, payloadRef);
  }

  void NGString::replace_payload_value(Str nextValue)
  {
    if (!payloadRef.valid() || headerStore.get(payloadRef).bytes.size() != nextValue.size())
    {
      payloadRef = buffer_runtime::allocate_string_payload(headerStore, nextValue);
    }
    else
    {
      buffer_runtime::write_string_payload(headerStore, payloadRef, nextValue);
    }
    value = std::move(nextValue);
    sync_header_backing();
  }

  auto NGString::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    sync_header_backing();
    if (auto otherString = std::dynamic_pointer_cast<NGString>(other); otherString != nullptr)
    {
      otherString->sync_header_backing();
      return otherString->payload_value() == payload_value();
    }
    return false;
  }

  auto NGString::stringType() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> stringType = makert<NGType>(NGType{
      .name = "String",
      .layout = buffer_runtime::make_string_header_layout(),
      .showHandler =
          [](const NGSelf &self) {
            auto str = std::dynamic_pointer_cast<NGString>(self);
            return str ? str->payload_value() : Str{};
          },
      .boolHandler =
          [](const NGSelf &self) {
            auto str = std::dynamic_pointer_cast<NGString>(self);
            return str && string_length(*str) != 0;
          },
      .memberFunctions = {
        {"size",
         [](const RuntimeRef<NGObject> &self, const NGEnv &env, const NGArgs &args)
              -> RuntimeRef<NGObject>
           {
              auto str = std::dynamic_pointer_cast<NGString>(self);
              return makert<NGIntegral<uint32_t>>(static_cast<uint32_t>(string_length(*str)));
            }},
        {"charAt",
         [](const RuntimeRef<NGObject> &self, const NGEnv &env, const NGArgs &args)
              -> RuntimeRef<NGObject>
           {
              auto str = std::dynamic_pointer_cast<NGString>(self);
             auto numeral = std::dynamic_pointer_cast<NumeralBase>(args[0]);

             auto index = NGIntegral<int32_t>::valueOf(numeral.get());
             if (index < 0)
             {
               throw RuntimeException("Index out of bounds: " + std::to_string(index));
             }
             return string_read_code_unit(*str, static_cast<size_t>(index));
            }},
        {"append",
         [](const RuntimeRef<NGObject> &self, const NGEnv &env, const NGArgs &args)
              -> RuntimeRef<NGObject>
           {
             auto str = std::dynamic_pointer_cast<NGString>(self);
              auto extra = runtime_value_show(args[0]);
              return makert<NGString>(str->payload_value() + extra);
            }}}});

    return stringType;
  }

  auto NGString::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
  {
    sync_header_backing();
    return makert<NGString>(payload_value() + runtime_value_show(other));
  }

} // namespace NG::runtime
