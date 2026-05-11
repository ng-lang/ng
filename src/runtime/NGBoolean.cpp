
#include <intp/runtime.hpp>

namespace NG::runtime
{

  auto NGBoolean::booleanType() -> RuntimeRef<NGType>
  {
    static auto type = makert<NGType>(NGType{
        .name = "Bool",
        .layout = TypeLayout{.name = "Bool", .kind = LayoutKind::INLINE_VALUE},
        .showHandler =
            [](const NGSelf &self) {
              auto boolean = std::dynamic_pointer_cast<NGBoolean>(self);
              return boolean && boolean->value ? "true" : "false";
            },
        .boolHandler =
            [](const NGSelf &self) {
              auto boolean = std::dynamic_pointer_cast<NGBoolean>(self);
              return boolean && boolean->value;
            },
    });
    return type;
  }

  auto NGBoolean::opEquals(RuntimeRef<NGObject> other) const -> bool
  {
    if (auto otherBoolean = std::dynamic_pointer_cast<NGBoolean>(other); otherBoolean != nullptr)
    {
      return otherBoolean->value == value;
    }
    return false;
  }

} // namespace NG::runtime
