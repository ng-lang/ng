
#include "intp/runtime.hpp"
#include <debug.hpp>
#include <typeinfo>

namespace NG::runtime {

    NGObject *NGObject::boolean(bool boolean) {
        return new NGBoolean{boolean};
    }

    Str NGObject::show() {
        return "[NGObject]";
    }

    bool NGObject::opEquals(NGObject *other) const {
        return false;
    }

    NGObject *NGObject::opIndex(NGObject *index) const {
        throw IllegalTypeException("Not index-accessible");
    }

    NGObject *NGObject::opIndex(NGObject *accessor, NGObject *newValue) {
        throw IllegalTypeException("Not index-accessible");
    }

    NGObject *NGObject::opPlus(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opMinus(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opTimes(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opDividedBy(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opModulus(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterThan(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opLessThan(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterEqual(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opLessEqual(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::respond(const Str &member, NGContext *context, NGInvocationContext *invocationContext) {
        Map<Str, NGInvocationHandler> &fns = this->type()->memberFunctions;
        if (fns.find(member) != fns.end()) {
            NGContext newContext{*context};
            newContext.objects["self"] = this;
            fns[member](*this, newContext, *invocationContext);

            return newContext.retVal;
        }

        throw NotImplementedException();
    }

    bool NGObject::opNotEqual(NGObject *other) const {
        return !opEquals(other);
    }

    NGObject *NGObject::opLShift(NGObject *other) {
        throw NotImplementedException();
    }

    NGObject *NGObject::opRShift(NGObject *other) {
        throw NotImplementedException();
    }

    NGType *NGObject::objectType() {
        static NGType objectType{};
        return &objectType;
    }

    NGType *NGObject::type() {
        return objectType();
    }

    NGObject::~NGObject() = default;

    NGContext::~NGContext() = default;
}