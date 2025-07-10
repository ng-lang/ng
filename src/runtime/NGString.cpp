
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
namespace NG::runtime
{
    using InvCtx = NGInvocationContext;

    auto NGString::show() const -> Str
    {
        return value;
    }

    auto NGString::opEquals(RuntimeRef<NGObject> other) const -> bool
    {
        if (auto otherString = std::dynamic_pointer_cast<NGString>(other); otherString != nullptr)
        {
            return otherString->value == value;
        }
        return false;
    }

    auto NGString::boolValue() const -> bool
    {
        return !value.empty();
    }

    auto NGString::type() const -> RuntimeRef<NGType>
    {
        return NGString::stringType();
    }

    auto NGString::stringType() -> RuntimeRef<NGType>
    {
        static RuntimeRef<NGType> stringType = makert<NGType>(NGType{
            .memberFunctions = {
                {"size", [](const RuntimeRef<NGObject> &self, const RuntimeRef<NGContext> &context, const RuntimeRef<InvCtx> &invCtx)
                 {
                     auto str = std::dynamic_pointer_cast<NGString>(self);

                     context->retVal = makert<NGIntegral<uint32_t>>(str->value.size());
                 }},
                {"charAt", [](const RuntimeRef<NGObject> &self, const RuntimeRef<NGContext> &context, const RuntimeRef<InvCtx> &invCtx)
                 {
                     auto str = std::dynamic_pointer_cast<NGString>(self);
                     auto numeral = std::dynamic_pointer_cast<NumeralBase>(invCtx->params[0]);

                     auto index = NGIntegral<int32_t>::valueOf(numeral.get());

                     context->retVal = makert<NGIntegral<int32_t>>(str->value[index]);
                 }}}});

        return stringType;
    }

    auto NGString::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        if (auto str = std::dynamic_pointer_cast<NGString>(other); str != nullptr)
        {
            return makert<NGString>(value + str->value);
        }

        throw IllegalTypeException("Not a string");
    }

}