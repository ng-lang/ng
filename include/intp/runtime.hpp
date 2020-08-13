
#ifndef __NG_INTP_RUNTIME_HPP
#define __NG_INTP_RUNTIME_HPP

#include <fwd.hpp>
#include <functional>
#include "common.hpp"

namespace NG::runtime {


    struct NGInvocationContext {
        Vec<NGObject *> params;
    };

    using NGInvocationHandler = std::function<void(NGObject &self, NGContext &ctx, NGInvocationContext &invCtx)>;
    struct NGType {
        Str name;

        Vec<Str> properties;

        Map<Str, NGInvocationHandler> memberFunctions;
    };

    struct NGContext {
        Map<Str, NGObject *> objects;
        Map<Str, NGInvocationHandler> functions;
        Map<Str, NGType *> types;

        Map<Str, NGModule *> modules;

        NGObject *retVal;

        ~NGContext();
    };

    struct IOverloadedOperators {
        virtual NGObject *opIndex(NGObject *index) const = 0;

        // obj[index] = newValue
        virtual NGObject *opIndex(NGObject *index, NGObject *newValue) = 0;

        // obj == other

        virtual NGObject *opPlus(NGObject *other) const = 0;

        virtual NGObject *opMinus(NGObject *other) const = 0;

        virtual NGObject *opTimes(NGObject *other) const = 0;

        virtual NGObject *opDividedBy(NGObject *other) const = 0;

        virtual NGObject *opModulus(NGObject *other) const = 0;

        virtual bool opEquals(NGObject *other) const = 0;

        virtual bool opNotEqual(NGObject *other) const = 0;

        virtual bool opGreaterThan(NGObject *other) const = 0;

        virtual bool opLessThan(NGObject *other) const = 0;

        virtual bool opGreaterEqual(NGObject *other) const = 0;

        virtual bool opLessEqual(NGObject *other) const = 0;

        virtual NGObject *opLShift(NGObject *other) = 0;

        virtual NGObject *opRShift(NGObject *other) = 0;

        // Meta-Object function
        virtual NGObject *respond(const Str &member, NGContext *context, NGInvocationContext *invocationContext) = 0;

        virtual ~IOverloadedOperators() noexcept = 0;
    };

    struct IBasicObject {
        virtual Str show() = 0;

        virtual bool boolValue() = 0;

        virtual NGType *type() = 0;

        virtual ~IBasicObject() = 0;
    };

    struct NGObject : public virtual IOverloadedOperators, public virtual IBasicObject {

        NGObject() = default;

        static NGObject *boolean(bool boolean);

        static NGType *objectType();

        bool boolValue() override {
            return true;
        }

        Str show() override;

        NGType *type() override;

        ~NGObject() override;

        // TODO: reference counting
//        virtual void acquire() = 0;
//
//        virtual void release() = 0;

        // Operators overloading

        // obj[index]
        NGObject *opIndex(NGObject *index) const override;

        // obj[index] = newValue
        NGObject *opIndex(NGObject *index, NGObject *newValue) override;

        // obj == other
        bool opEquals(NGObject *other) const override;

        bool opNotEqual(NGObject *other) const override;

        NGObject *opPlus(NGObject *other) const override;

        NGObject *opMinus(NGObject *other) const override;

        NGObject *opTimes(NGObject *other) const override;

        NGObject *opDividedBy(NGObject *other) const override;

        NGObject *opModulus(NGObject *other) const override;

        bool opGreaterThan(NGObject *other) const override;

        bool opLessThan(NGObject *other) const override;

        bool opGreaterEqual(NGObject *other) const override;

        bool opLessEqual(NGObject *other) const override;

        NGObject *opLShift(NGObject *other) override;

        NGObject *opRShift(NGObject *other) override;

        // Meta-Object function
        NGObject *respond(const Str &member, NGContext *context, NGInvocationContext *invocationContext) override;
    };

    enum class Orders {
        GT, EQ, LT,
        UNORDERED
    };

    template<class T>
    struct ThreeWayComparable : public virtual NGObject {

        bool opEquals(NGObject *other) const override {
            return T::comparator(this, other) == Orders::EQ;
        }

        bool opNotEqual(NGObject *other) const override {
            return !this->opEquals(other);
        }

        bool opLessThan(NGObject *other) const override {
            return T::comparator(this, other) == Orders::LT;
        }

        bool opGreaterThan(NGObject *other) const override {
            return T::comparator(this, other) == Orders::GT;
        }

        bool opLessEqual(NGObject *other) const override {
            return !this->opGreaterThan(other);
        }

        bool opGreaterEqual(NGObject *other) const override {
            return !this->opLessThan(other);
        }
    };

    struct NGInteger final : ThreeWayComparable<NGInteger> {
        long long value = 0;

        static Orders comparator(const NGObject *left, const NGObject *right);

        explicit NGInteger(long long value = 0) : value{value} {}

        Str show() override {
            return std::to_string(value);
        }

        bool boolValue() override;

        NGObject *opPlus(NGObject *other) const override;

        NGObject *opMinus(NGObject *other) const override;

        NGObject *opTimes(NGObject *other) const override;

        NGObject *opDividedBy(NGObject *other) const override;

        NGObject *opModulus(NGObject *other) const override;
    };

    struct NGDefinition {
        Str name;
        NG::AST::ASTNode *defbody;
    };

    struct NGModule {
        Map<Str, NGDefinition *> defs;
    };

    struct NGArray : NGObject {
        Vec<NGObject *> items;

        explicit NGArray(const Vec<NGObject *> &vec = {}) : items{vec} {}

        NGObject *opIndex(NGObject *index) const override;

        NGObject *opIndex(NGObject *index, NGObject *newValue) override;

        Str show() override;

        bool boolValue() override;

        bool opEquals(NGObject *other) const override;

        NGObject *opLShift(NGObject *other) override;
    };

    struct NGBoolean final : NGObject {
        bool value;

        explicit NGBoolean(bool value = false) : value{value} {}

        Str show() override;

        bool opEquals(NGObject *other) const override;

        bool boolValue() override;
    };

    struct NGString final : NGObject {
        Str value;

        static NGType *stringType();

        NGType *type() override;

        explicit NGString(const Str &str) : value{str} {}

        Str show() override;

        bool boolValue() override;

        bool opEquals(NGObject *other) const override;

        NGObject *opPlus(NGObject *other) const override;
    };


    struct NGStructuralObject : NGObject {
        NGType *customizedType{};
        Map<Str, NGObject *> properties{};

        Map<Str, NGInvocationHandler> selfMemberFunctions{};

        NGObject *respond(const Str &member, NGContext *context, NGInvocationContext *invocationContext) override;

        NGType *type() override;
    };

    template<class T>
    using NGRef = T *;

}

#endif // __NG_RUNTIME_HPP
