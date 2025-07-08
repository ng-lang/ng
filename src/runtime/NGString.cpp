
#include <intp/runtime.hpp>

namespace NG::runtime {
    using InvCtx = NGInvocationContext;


    Str NGString::show() {
        return "\"" + value + "\"";
    }

    bool NGString::opEquals(RuntimeRef<NGObject> other) const {
        if (auto otherString = std::dynamic_pointer_cast<NGString>(other); otherString != nullptr) {
            return otherString->value == value;
        }
        return false;
    }

    bool NGString::boolValue() {
        return !value.empty();
    }

    RuntimeRef<NGType> NGString::type() {
        return NGString::stringType();
    }

    RuntimeRef<NGType> NGString::stringType() {
        static RuntimeRef<NGType> stringType = makert<NGType>(NGType{
            .memberFunctions = {
                    {"size",   [](RuntimeRef<NGObject> self, RuntimeRef<NGContext> context, RuntimeRef<InvCtx> invCtx) {
                        auto str = std::dynamic_pointer_cast<NGString>(self);

                        context->retVal = makert<NGInteger>(str->value.size());
                    }},
                    {"charAt", [](RuntimeRef<NGObject> self, RuntimeRef<NGContext> context, RuntimeRef<InvCtx> invCtx) {
                        auto str = std::dynamic_pointer_cast<NGString>(self);
                        auto index = std::dynamic_pointer_cast<NGInteger>(invCtx->params[0]);

                        context->retVal = makert<NGInteger>(str->value[index->asSize()]);
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