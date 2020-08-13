
#include "intp/runtime.hpp"

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

    NGObject *IOverloadedOperators::opIndex(NGObject *index) const {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opIndex(NGObject *index, NGObject *newValue) {
        return nullptr;
    }

    bool IOverloadedOperators::opGreaterThan(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opGreaterEqual(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opLessThan(NGObject *other) const { return false; }

    bool IOverloadedOperators::opLessEqual(NGObject *other) const { return false; }

    bool IOverloadedOperators::opEquals(NGObject *other) const { return false; }

    bool IOverloadedOperators::opNotEqual(NGObject *other) const { return false; }

    NGObject *IOverloadedOperators::opPlus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opMinus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opTimes(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opModulus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opDividedBy(NGObject *other) const { return nullptr; }


    NGObject *IOverloadedOperators::respond(const Str &member, NGContext *context,
                                            NGInvocationContext *invocationContext) {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opLShift(NGObject *object) {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opRShift(NGObject *object) {
        return nullptr;
    }

    IOverloadedOperators::~IOverloadedOperators() noexcept = default;


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

    using InvCtx = NGInvocationContext;

    NGType *NGObject::objectType() {
        static NGType objectType{};
        return &objectType;
    }

    NGType *NGObject::type() {
        return objectType();
    }

    NGObject::~NGObject() = default;

    NGContext::~NGContext() = default;

    NGObject *NGArray::opIndex(NGObject *index) const {

        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return this->items[ngInt->value];
    }

    NGObject *NGArray::opIndex(NGObject *index, NGObject *newValue) {
        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return items[ngInt->value] = newValue;
    }

    Str NGArray::show() {
        Str result{};

        for (const auto &item : this->items) {
            if (!result.empty()) {
                result += ", ";
            }

            result += item->show();
        }

        return "[" + result + "]";
    }

    bool NGArray::opEquals(NGObject *other) const {

        if (auto array = dynamic_cast<NGArray *>(other); array != nullptr) {
            if (items.size() != array->items.size()) {
                return false;
            }
            for (int i = 0; i < items.size(); ++i) {
                if (!items[i]->opEquals(array->items[i])) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    bool NGArray::boolValue() {
        return !items.empty();
    }

    NGObject *NGArray::opLShift(NGObject *other) {
        items.push_back(other);

        return this;
    }

    Str IBasicObject::show() {
        return NG::Str();
    }

    bool IBasicObject::boolValue() {
        return false;
    }

    IBasicObject::~IBasicObject() = default;

    Str NGBoolean::show() {
        return value ? "true" : "false";
    }

    bool NGBoolean::opEquals(NGObject *other) const {
        if (auto otherBoolean = dynamic_cast<NGBoolean *>(other); otherBoolean != nullptr) {
            return otherBoolean->value == value;
        }
        return false;
    }

    bool NGBoolean::boolValue() {
        return value;
    }


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

    Orders NGInteger::comparator(const NGObject *left, const NGObject *right) {
        auto leftInt = dynamic_cast<const NGInteger *>(left);
        auto rightInt = dynamic_cast<const NGInteger *>(right);

        if (leftInt == nullptr || rightInt == nullptr) {
            return Orders::UNORDERED;
        }

        long long int result = leftInt->value - rightInt->value;
        if (result > 0) {
            return Orders::GT;
        } else if (result < 0) {
            return Orders::LT;
        }
        return Orders::EQ;
    }

    NGObject *NGInteger::opPlus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value + integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opMinus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value - integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opTimes(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value * integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opDividedBy(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value / integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opModulus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger{value % integer->value};
        }
        throw IllegalTypeException("Not a number");
    }

    bool NGInteger::boolValue() {
        return value != 0;
    }

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
} // namespace NG::runtime
