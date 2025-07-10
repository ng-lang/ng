
#include "intp/runtime.hpp"
#include <debug.hpp>
#include <typeinfo>

namespace NG::runtime
{

    auto NGObject::boolean(bool boolean) -> RuntimeRef<NGObject>
    {
        return makert<NGBoolean>(boolean);
    }

    auto NGObject::show() const -> Str
    {
        return "[NGObject]";
    }

    auto NGObject::opEquals(RuntimeRef<NGObject> other) const -> bool
    {
        return false;
    }

    auto NGObject::opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject>
    {
        throw IllegalTypeException("Not index-accessible");
    }

    auto NGObject::opIndex(RuntimeRef<NGObject> accessor, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject>
    {
        throw IllegalTypeException("Not index-accessible");
    }

    auto NGObject::opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opGreaterThan(RuntimeRef<NGObject> other) const -> bool
    {
        throw NotImplementedException();
    }

    auto NGObject::opLessThan(RuntimeRef<NGObject> other) const -> bool
    {
        throw NotImplementedException();
    }

    auto NGObject::opGreaterEqual(RuntimeRef<NGObject> other) const -> bool
    {
        throw NotImplementedException();
    }

    auto NGObject::opLessEqual(RuntimeRef<NGObject> other) const -> bool
    {
        throw NotImplementedException();
    }

    auto NGObject::respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject>
    {
        Map<Str, NGInvocationHandler> &fns = this->type()->memberFunctions;
        if (fns.contains(member))
        {
            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            RuntimeRef<NGObject> self = invocationContext->target;
            newContext->objects["self"] = self;
            fns[member](self, newContext, invocationContext);

            return newContext->retVal;
        }

        throw NotImplementedException();
    }

    auto NGObject::opNotEqual(RuntimeRef<NGObject> other) const -> bool
    {
        return !opEquals(other);
    }

    auto NGObject::opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject>
    {
        throw NotImplementedException();
    }

    auto NGObject::objectType() -> RuntimeRef<NGType>
    {
        static RuntimeRef<NGType> objectType = makert<NGType>();
        return objectType;
    }

    auto NGObject::type() const -> RuntimeRef<NGType>
    {
        return objectType();
    }

    NGObject::~NGObject() = default;

    NGContext::~NGContext() = default;
}
