
#include <intp/runtime.hpp>

namespace NG::runtime {

    RuntimeRef<NGType> NGStructuralObject::type() const {
        return this->customizedType;
    }

    RuntimeRef<NGObject> NGStructuralObject::respond(const Str &member, RuntimeRef<NGContext> context,
                                          RuntimeRef<NGInvocationContext> invocationContext) {

        if (selfMemberFunctions.find(member) != selfMemberFunctions.end()) {
            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            RuntimeRef<NGStructuralObject> self = std::dynamic_pointer_cast<NGStructuralObject>(invocationContext->target);
            context->objects["self"] = self;
            selfMemberFunctions[member](self, newContext, invocationContext);

            return newContext->retVal;
        } else if (properties.find(member) != properties.end()) {
            return properties[member];
        } else {
            return NGObject::respond(member, context, invocationContext);
        }

    }

    Str NGStructuralObject::show() const {
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