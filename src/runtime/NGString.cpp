
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
namespace NG::runtime {
    using InvCtx = NGInvocationContext;


    Str NGString::show() const {
        return "\"" + value + "\"";
    }

    bool NGString::opEquals(RuntimeRef<NGObject> other) const {
        if (auto otherString = std::dynamic_pointer_cast<NGString>(other); otherString != nullptr) {
            return otherString->value == value;
        }
        return false;
    }

    bool NGString::boolValue() const {
        return !value.empty();
    }

    RuntimeRef<NGType> NGString::type() const {
        return NGString::stringType();
    }

    RuntimeRef<NGType> NGString::stringType() {
        static RuntimeRef<NGType> stringType = makert<NGType>(NGType{
            .memberFunctions = {
                    {"size",   [](RuntimeRef<NGObject> self, RuntimeRef<NGContext> context, RuntimeRef<InvCtx> invCtx) {
                        auto str = std::dynamic_pointer_cast<NGString>(self);

                        context->retVal = makert<NGIntegral<uint32_t>>(str->value.size());
                    }},
                    {"charAt", [](RuntimeRef<NGObject> self, RuntimeRef<NGContext> context, RuntimeRef<InvCtx> invCtx) {
                        auto str = std::dynamic_pointer_cast<NGString>(self);
                        auto numeral = std::dynamic_pointer_cast<NumeralBase>(invCtx->params[0]);

                        auto index = NGIntegral<int32_t>::valueOf(numeral.get());

                        context->retVal = makert<NGIntegral<int32_t>>(str->value[index]);
                    }}
            }
        });

        return stringType;
    }

    RuntimeRef<NGObject> NGString::opPlus(RuntimeRef<NGObject> other) const {
        if (auto str = std::dynamic_pointer_cast<NGString>(other); str != nullptr) {
            return makert<NGString>(value + str->value);
        }

        throw IllegalTypeException("Not a string");
    }

}