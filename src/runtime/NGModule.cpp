
#include <intp/runtime.hpp>
#include <debug.hpp>
namespace NG::runtime
{
    RuntimeRef<NGObject> NGModule::respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext)
    {
        if (this->functions.contains(member))
        {
            auto &&fns = this->functions;
            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            RuntimeRef<NGObject> self = invocationContext->target;
            newContext->objects["self"] = self;
            fns[member](self, newContext, invocationContext);

            return newContext->retVal;
        }
        else if (this->objects.contains(member))
        {
            return this->objects[member];
        }
        return NGObject::respond(member, context, invocationContext);
    }

    NGModule::~NGModule() = default;
}