
#include "intp/runtime.hpp"
#include <debug.hpp>
#include <typeinfo>

namespace NG::runtime
{

    RuntimeRef<NGObject> NGObject::boolean(bool boolean)
    {
        return makert<NGBoolean>(boolean);
    }

    Str NGObject::show() const
    {
        return "[NGObject]";
    }

    bool NGObject::opEquals(RuntimeRef<NGObject> other) const
    {
        return false;
    }

    RuntimeRef<NGObject> NGObject::opIndex(RuntimeRef<NGObject> index) const
    {
        throw IllegalTypeException("Not index-accessible");
    }

    RuntimeRef<NGObject> NGObject::opIndex(RuntimeRef<NGObject> accessor, RuntimeRef<NGObject> newValue)
    {
        throw IllegalTypeException("Not index-accessible");
    }

    RuntimeRef<NGObject> NGObject::opPlus(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::opMinus(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::opTimes(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::opDividedBy(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::opModulus(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterThan(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    bool NGObject::opLessThan(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterEqual(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    bool NGObject::opLessEqual(RuntimeRef<NGObject> other) const
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext)
    {
        Map<Str, NGInvocationHandler> &fns = this->type()->memberFunctions;
        if (fns.find(member) != fns.end())
        {
            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            RuntimeRef<NGObject> self = invocationContext->target;
            newContext->objects["self"] = self;
            fns[member](self, newContext, invocationContext);

            return newContext->retVal;
        }

        throw NotImplementedException();
    }

    bool NGObject::opNotEqual(RuntimeRef<NGObject> other) const
    {
        return !opEquals(other);
    }

    RuntimeRef<NGObject> NGObject::opLShift(RuntimeRef<NGObject> other)
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGObject> NGObject::opRShift(RuntimeRef<NGObject> other)
    {
        throw NotImplementedException();
    }

    RuntimeRef<NGType> NGObject::objectType()
    {
        static RuntimeRef<NGType> objectType = makert<NGType>();
        return objectType;
    }

    RuntimeRef<NGType> NGObject::type() const
    {
        return objectType();
    }

    NGObject::~NGObject() = default;

    NGContext::~NGContext() = default;
}
