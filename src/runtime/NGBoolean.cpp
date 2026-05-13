
#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>

namespace NG::runtime
{

  auto boolean_runtime_type() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Bool",
        .layout = TypeLayout{.name = "Bool", .kind = LayoutKind::INLINE_VALUE, .size = sizeof(bool), .alignment = alignof(bool),
                             .containsPointers = false, .triviallyCopyable = true, .triviallyMovable = true},
        .showCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return (!cell->bytes.empty() && cell->bytes[0] != 0) ? Str{"true"} : Str{"false"};
            },
        .boolCellHandler =
            [](const RuntimeRef<StorageCell> &cell) {
              return !cell->bytes.empty() && cell->bytes[0] != 0;
            },
        .cellOrderHandler =
            [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) {
              auto lhs = !self->bytes.empty() && self->bytes[0] != 0;
              auto rhs = !other->bytes.empty() && other->bytes[0] != 0;
              if (lhs == rhs) return Orders::EQ;
              return lhs ? Orders::GT : Orders::LT;
            },
    });
    return type;
  }
} // namespace NG::runtime
