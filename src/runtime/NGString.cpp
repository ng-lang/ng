
#include <intp/runtime.hpp>

namespace NG::runtime {
    using InvCtx = NGInvocationContext;


    Str NGString::show() {
        return "\"" + value + "\"";
    }

    bool NGString::opEquals(NGObject *other) const {
        if (auto otherString = dynamic_cast<NGString *>(other); otherString != nullptr) {
            return otherString->value == value;
        }
        return false;
    }

    bool NGString::boolValue() {
        return !value.empty();
    }

    NGType *NGString::type() {
        return NGString::stringType();
    }

    NGType *NGString::stringType() {
        static NGType stringType{
                .memberFunctions = {
                        {"size",   [](NGObject &self, NGContext &context, InvCtx &invCtx) {
                            auto &str = dynamic_cast<NGString &>(self);

                            context.retVal = new NGInteger(str.value.size());
                        }},
                        {"charAt", [](NGObject &self, NGContext &context, InvCtx &invCtx) {
                            auto &str = dynamic_cast<NGString &>(self);
                            auto index = dynamic_cast<NGInteger *>(                            invCtx.params[0]);

                            context.retVal = new NGInteger(str.value[index->value]);
                        }}
                }
        };

        return &stringType;
    }

    NGObject *NGString::opPlus(NGObject *other) const {
        if (auto str = dynamic_cast<NGString *>(other); str != nullptr) {


            return new NGString{value + str->value};
        }

        throw IllegalTypeException("Not a string");
    }

}