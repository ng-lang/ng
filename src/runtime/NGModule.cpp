
#include <intp/runtime.hpp>

namespace NG::runtime {
    NGObject *NGModule::respond(const Str &member, NGContext *context, NGInvocationContext *invocationContext) {
        if (this->functions.contains(member)) {
            auto &&fns = this->functions;
            NGContext newContext{*context};
            newContext.objects["self"] = this;
            fns[member](*this, newContext, *invocationContext);

            return newContext.retVal;
        } else if (this->objects.contains(member)) {
            return this->objects[member];
        }
        return NGObject::respond(member, context, invocationContext);
    }

    NGModule::~NGModule() = default;
}