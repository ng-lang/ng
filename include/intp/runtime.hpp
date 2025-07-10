
#ifndef __NG_INTP_RUNTIME_HPP
#define __NG_INTP_RUNTIME_HPP

#include <fwd.hpp>
#include <functional>
#include "common.hpp"
#include <memory>
#include <debug.hpp>

namespace NG::runtime
{

    template <class T>
    using RuntimeRef = std::shared_ptr<T>;

    template <class T, class... Args>
    inline RuntimeRef<T> makert(Args &&...args)
    {
        return std::make_shared<T>(std::move(args)...);
    }

    struct NGInvocationContext
    {
        RuntimeRef<NGObject> target;
        Vec<RuntimeRef<NGObject>> params;
    };

    using NGInvocationHandler = std::function<void(RuntimeRef<NGObject> self, RuntimeRef<NGContext> ctx, RuntimeRef<NGInvocationContext> invCtx)>;
    struct NGType
    {
        Str name;

        Vec<Str> properties;

        Map<Str, NGInvocationHandler> memberFunctions;
    };

    struct NGContext
    {
        Str currentModuleName{};
        Vec<Str> modulePaths{};
        Map<Str, RuntimeRef<NGObject>> objects;
        Map<Str, NGInvocationHandler> functions;
        Map<Str, RuntimeRef<NGType>> types;

        RuntimeRef<NGModule> currentModule;

        Map<Str, RuntimeRef<NGModule>> modules;

        RuntimeRef<NGObject> retVal;

        void appendModulePath(const Str &path)
        {
            if (std::find(std::begin(modulePaths), std::end(modulePaths), path) == std::end(modulePaths))
            {
                modulePaths.push_back(path);
            }
        }

        ~NGContext();
    };

    struct OperatorsBase
    {
        virtual RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index) const = 0;

        // obj[index] = newValue
        virtual RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) = 0;

        // obj == other

        virtual RuntimeRef<NGObject> opPlus(RuntimeRef<NGObject> other) const = 0;

        virtual RuntimeRef<NGObject> opMinus(RuntimeRef<NGObject> other) const = 0;

        virtual RuntimeRef<NGObject> opTimes(RuntimeRef<NGObject> other) const = 0;

        virtual RuntimeRef<NGObject> opDividedBy(RuntimeRef<NGObject> other) const = 0;

        virtual RuntimeRef<NGObject> opModulus(RuntimeRef<NGObject> other) const = 0;

        virtual bool opEquals(RuntimeRef<NGObject> other) const = 0;

        virtual bool opNotEqual(RuntimeRef<NGObject> other) const = 0;

        virtual bool opGreaterThan(RuntimeRef<NGObject> other) const = 0;

        virtual bool opLessThan(RuntimeRef<NGObject> other) const = 0;

        virtual bool opGreaterEqual(RuntimeRef<NGObject> other) const = 0;

        virtual bool opLessEqual(RuntimeRef<NGObject> other) const = 0;

        virtual RuntimeRef<NGObject> opLShift(RuntimeRef<NGObject> other) = 0;

        virtual RuntimeRef<NGObject> opRShift(RuntimeRef<NGObject> other) = 0;

        // Meta-Object function
        virtual RuntimeRef<NGObject> respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) = 0;

        virtual ~OperatorsBase() noexcept = 0;
    };

    struct ObjectBase
    {
        virtual Str show() const = 0;

        virtual bool boolValue() const = 0;

        virtual RuntimeRef<NGType> type() const = 0;

        virtual ~ObjectBase() = 0;
    };

    enum class Orders
    {
        GT,
        EQ,
        LT,
        UNORDERED
    };

    struct NGObject : public virtual OperatorsBase, public virtual ObjectBase
    {

        NGObject() = default;

        static RuntimeRef<NGObject> boolean(bool boolean);

        static RuntimeRef<NGType> objectType();

        bool boolValue() const override
        {
            return true;
        }

        Str show() const override;

        RuntimeRef<NGType> type() const override;

        ~NGObject() override;

        // TODO: reference counting
        //        virtual void acquire() = 0;
        //
        //        virtual void release() = 0;

        // Operators overloading

        // obj[index]
        RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index) const override;

        // obj[index] = newValue
        RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) override;

        // obj == other
        bool opEquals(RuntimeRef<NGObject> other) const override;

        bool opNotEqual(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opPlus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opMinus(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opTimes(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opDividedBy(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opModulus(RuntimeRef<NGObject> other) const override;

        virtual Orders compareTo(const NGObject *other) const
        {
            return Orders::UNORDERED;
        };

        bool opGreaterThan(RuntimeRef<NGObject> other) const override;

        bool opLessThan(RuntimeRef<NGObject> other) const override;

        bool opGreaterEqual(RuntimeRef<NGObject> other) const override;

        bool opLessEqual(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opLShift(RuntimeRef<NGObject> other) override;

        RuntimeRef<NGObject> opRShift(RuntimeRef<NGObject> other) override;

        // Meta-Object function
        RuntimeRef<NGObject> respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) override;
    };

    inline Orders negate(Orders order)
    {
        switch (order)
        {
        case Orders::GT:
            return Orders::LT;
        case Orders::LT:
            return Orders::GT;
        default:
            return order;
        }
    }

    template <class T>
    struct ThreeWayComparable : public virtual NGObject
    {

        Orders compareTo(const NGObject *other) const override
        {
            return T::comparator(this, other);
        }

        bool opEquals(RuntimeRef<NGObject> other) const override
        {
            return T::comparator(this, other.get()) == Orders::EQ;
        }

        bool opNotEqual(RuntimeRef<NGObject> other) const override
        {
            return !this->opEquals(other);
        }

        bool opLessThan(RuntimeRef<NGObject> other) const override
        {
            return T::comparator(this, other.get()) == Orders::LT;
        }

        bool opGreaterThan(RuntimeRef<NGObject> other) const override
        {
            return T::comparator(this, other.get()) == Orders::GT;
        }

        bool opLessEqual(RuntimeRef<NGObject> other) const override
        {
            return !this->opGreaterThan(other);
        }

        bool opGreaterEqual(RuntimeRef<NGObject> other) const override
        {
            return !this->opLessThan(other);
        }
    };

    struct NGDefinition
    {
        Str name;
        NG::ast::ASTNode *defbody;
    };

    struct NGModule : public virtual NGObject
    {
        Vec<Str> imports;
        Vec<Str> exports;

        Map<Str, RuntimeRef<NGObject>> objects;
        Map<Str, NGInvocationHandler> functions;
        Map<Str, RuntimeRef<NGType>> types;

        size_t size() const
        {
            return objects.size() + functions.size() + types.size();
        }

        RuntimeRef<NGObject> respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) override;

        ~NGModule() override;
    };

    struct NGArray : NGObject
    {
        RuntimeRef<Vec<RuntimeRef<NGObject>>> items;

        explicit NGArray(const Vec<RuntimeRef<NGObject>> &vec = {}) : items{makert<Vec<RuntimeRef<NGObject>>>(vec)} {}

        RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index) const override;

        RuntimeRef<NGObject> opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) override;

        Str show() const override;

        bool boolValue() const override;

        bool opEquals(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opLShift(RuntimeRef<NGObject> other) override;
    };

    struct NGBoolean final : NGObject
    {
        bool value;

        explicit NGBoolean(bool value = false) : value{value} {}

        Str show() const override;

        bool opEquals(RuntimeRef<NGObject> other) const override;

        bool boolValue() const override;
    };

    struct NGString final : NGObject
    {
        Str value;

        static RuntimeRef<NGType> stringType();

        RuntimeRef<NGType> type() const override;

        explicit NGString(const Str &str) : value{str} {}

        Str show() const override;

        bool boolValue() const override;

        bool opEquals(RuntimeRef<NGObject> other) const override;

        RuntimeRef<NGObject> opPlus(RuntimeRef<NGObject> other) const override;
    };

    struct NGStructuralObject : NGObject
    {
        RuntimeRef<NGType> customizedType{};
        Map<Str, RuntimeRef<NGObject>> properties{};

        Map<Str, NGInvocationHandler> selfMemberFunctions{};

        RuntimeRef<NGObject> respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) override;

        RuntimeRef<NGType> type() const override;

        Str show() const override;
    };

}

#endif // __NG_RUNTIME_HPP
