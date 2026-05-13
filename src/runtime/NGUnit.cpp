
#include <intp/runtime.hpp>
#include <runtime/value_access.hpp>
namespace NG::runtime
{

  auto unit_runtime_type() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> unitType = makert<NGType>(NGType{
      .name = "unit",
      .layout = TypeLayout{.name = "unit", .kind = LayoutKind::INLINE_VALUE, .triviallyCopyable = true,
                           .triviallyMovable = true},
      .memberFunctions = {},
      .showCellHandler = [](const RuntimeRef<StorageCell> &) { return Str{"unit"}; },
      .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return false; },
      .cellOrderHandler = [](const RuntimeRef<StorageCell> &, const RuntimeRef<StorageCell> &) { return Orders::EQ; },
    });
    return unitType;
  }

} // namespace NG::runtime
