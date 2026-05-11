
#include <intp/runtime.hpp>
namespace NG::runtime
{

  auto NGUnit::unitType() -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> unitType = makert<NGType>(NGType{
      .name = "unit",
      .layout = TypeLayout{.name = "unit", .kind = LayoutKind::INLINE_VALUE, .triviallyCopyable = true,
                           .triviallyMovable = true},
      .memberFunctions = {},
      .showHandler = [](const NGSelf &) { return Str{"unit"}; },
    });
    return unitType;
  }

} // namespace NG::runtime
