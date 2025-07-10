
#include <intp/runtime.hpp>

namespace NG::runtime
{

    auto NGStructuralObject::type() const -> RuntimeRef<NGType>
    {
        return this->customizedType;
    }

    auto NGStructuralObject::respond(const Str &member, RuntimeRef<NGContext> context,
                                                     RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject>
    {

        if (selfMemberFunctions.contains(member))
        {
            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            RuntimeRef<NGStructuralObject> self = std::dynamic_pointer_cast<NGStructuralObject>(invocationContext->target);
            context->objects["self"] = self;
            selfMemberFunctions[member](self, newContext, invocationContext);

            return newContext->retVal;
        }
        if (properties.contains(member))
        {
            return properties[member];
        }
        
                    return NGObject::respond(member, context, invocationContext);
       
    }

    auto NGStructuralObject::show() const -> Str
    {
        Str repr{};
        for (const auto &[name, value] : properties)
        {
            if (!repr.empty())
            {
                repr += ", ";
            }

            repr += (name + ": " + value->show());
        }

        return "{ " + repr + " }";
    }
}