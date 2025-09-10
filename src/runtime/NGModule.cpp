
#include <intp/runtime.hpp>
#include <debug.hpp>
namespace NG::runtime
{

    NGModule::NGModule(RuntimeRef<NGContext> ctx)
        : objects(ctx->objects), functions(ctx->functions),
          types(ctx->types), exports(ctx->exports.begin(), ctx->exports.end()),
          imports(ctx->imported.begin(), ctx->imported.end())
    {
    }
    auto NGModule::respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject>
    {
        if (this->functions.contains(member))
        {
            auto &&fns = this->functions;
            RuntimeRef<NGContext> newContext = context->fork();
            RuntimeRef<NGObject> self = invocationContext->target;
            newContext->define("self", self);
            fns[member](self, newContext, invocationContext);

            return newContext->retVal;
        }
        if (this->objects.contains(member))
        {
            return this->objects[member];
        }
        return NGObject::respond(member, context, invocationContext);
    }

    NGModule::~NGModule() = default;
}