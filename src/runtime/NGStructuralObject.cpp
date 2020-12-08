
#include <intp/runtime.hpp>

namespace NG::runtime {

    NGType *NGStructuralObject::type() {
        return this->customizedType;
    }

    NGObject *NGStructuralObject::respond(const Str &member, NGContext *context,
                                          NGInvocationContext *invocationContext) {

        if (selfMemberFunctions.find(member) != selfMemberFunctions.end()) {
            NGContext newContext{*context};
            context->objects["self"] = this;
            selfMemberFunctions[member](*this, newContext, *invocationContext);

            return newContext.retVal;
        } else if (properties.find(member) != properties.end()) {
            return properties[member];
        } else {
            return NGObject::respond(member, context, invocationContext);
        }

    }

    Str NGStructuralObject::show() {
        Str repr{};
        for (const auto &[name, value] : properties) {
            if (!repr.empty()) {
                repr += ", ";
            }

            repr += (name + ": " + value->show());
        }

        return "{ " + repr + " }";
    }
}