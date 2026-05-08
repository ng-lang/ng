
#include "intp/runtime.hpp"
#include "intp/runtime_numerals.hpp"

namespace NG::runtime
{

  auto NGTaggedValue::type() const -> RuntimeRef<NGType>
  {
    return makert<NGType>(NGType{.name = unionName});
  }

  auto NGTaggedValue::show() const -> Str
  {
    Str result = variantName + "(";
    for (size_t i = 0; i < payload.size(); ++i)
    {
      if (i > 0) result += ", ";
      result += payload[i]->show();
    }
    result += ")";
    return result;
  }

  auto NGTaggedValue::respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject>
  {
    // Support .field access by name (e.g. result.value)
    if (!payloadNames.empty())
    {
      for (size_t i = 0; i < payloadNames.size(); ++i)
      {
        if (payloadNames[i] == member && i < payload.size())
        {
          return payload[i];
        }
      }
    }
    // Support .0, .1, etc. positional access
    // Also support .tag to get variant name, .payload to get all values
    if (member == "tag")
    {
      return makert<NGString>(variantName);
    }
    if (member == "index")
    {
      return makert<NGIntegral<int32_t>>(variantIndex);
    }
    return NGObject::respond(member, context, invocationContext);
  }

} // namespace NG::runtime
