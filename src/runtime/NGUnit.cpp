
#include <intp/runtime.hpp>
namespace NG::runtime
{

  auto NGUnit::show() const -> Str
  {
    return "unit";
  }

  auto NGUnit::type() const -> RuntimeRef<NGType>
  {
    static RuntimeRef<NGType> unitType = makert<NGType>(NGType{
      .name = "unit",
      .memberFunctions = {},
    });
    return unitType;
  }

} // namespace NG::runtime
