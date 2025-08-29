
#pragma once

#include <algorithm>
#include <fwd.hpp>
#include <functional>
#include "common.hpp"
#include <memory>
#include <debug.hpp>
#include <utility>
#include <unordered_set>
#include <ast.hpp>

namespace NG::runtime
{

    template <class T>
    using Set = std::unordered_set<T>;

    template <class T>
    using RuntimeRef = std::shared_ptr<T>;

    template <class T, class... Args>
    inline auto makert(Args &&...args) -> RuntimeRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
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

        auto operator==(const NGType &other) const -> bool
        {
            if (this == &other)
            {
                return true;
            }
            return this->name == other.name &&
                   this->properties == other.properties &&
                   std::all_of(begin(other.memberFunctions), end(other.memberFunctions),
                               [this](const std::pair<Str, NGInvocationHandler> &pair)
                                   -> bool
                               {
                                   return this->memberFunctions.contains(pair.first);
                               });
        }
    };

    struct NGContext
    {

        RuntimeRef<NGObject> retVal;

        void appendModulePath(const Str &path);

        auto fork() -> RuntimeRef<NGContext>;

        auto get(Str name) -> RuntimeRef<NGObject>;
        void set(Str name, RuntimeRef<NGObject> value);

        void define(Str name, RuntimeRef<NGObject> value);
        void define_function(Str name, NGInvocationHandler value);
        void define_type(Str name, RuntimeRef<NGType> type);
        void define_module(Str name, RuntimeRef<NGModule> module);

        auto has_object(Str name, bool global = false) -> bool;
        auto has_function(Str name, bool global = false) -> bool;
        auto has_type(Str name, bool global = false) -> bool;
        auto has_module(Str name, bool global = false) -> bool;

        auto get_function(Str name) -> NGInvocationHandler;
        auto get_type(Str name) -> RuntimeRef<NGType>;
        auto get_module(Str name) -> RuntimeRef<NGModule>;

        void try_save_module();
        void new_current(NG::ast::Module *e);
        auto current_module() -> RuntimeRef<NGModule>;

        void summary();

        Vec<Str> modulePaths;

        NGContext(Vec<Str> modulePaths, Map<Str, NGInvocationHandler> functions) : modulePaths(modulePaths), functions(functions) {}

    private:
        Str currentModuleName;
        Map<Str, RuntimeRef<NGObject>> objects;
        Map<Str, NGInvocationHandler> functions;
        Map<Str, RuntimeRef<NGType>> types;

        RuntimeRef<NGModule> currentModule;

        Map<Str, RuntimeRef<NGModule>> modules;

        Set<Str> locals;

        NGContext *parent = nullptr;
    };

    struct OperatorsBase // NOLINT(cppcoreguidelines-special-member-functions)
    {
        [[nodiscard]] virtual auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> = 0;

        // obj[index] = newValue
        virtual auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> = 0;

        // obj == other

        [[nodiscard]] virtual auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        [[nodiscard]] virtual auto opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        [[nodiscard]] virtual auto opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        [[nodiscard]] virtual auto opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        [[nodiscard]] virtual auto opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        [[nodiscard]] virtual auto opEquals(RuntimeRef<NGObject> other) const -> bool = 0;

        [[nodiscard]] virtual auto opNotEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        [[nodiscard]] virtual auto opGreaterThan(RuntimeRef<NGObject> other) const -> bool = 0;

        [[nodiscard]] virtual auto opLessThan(RuntimeRef<NGObject> other) const -> bool = 0;

        [[nodiscard]] virtual auto opGreaterEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        [[nodiscard]] virtual auto opLessEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        virtual auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> = 0;

        virtual auto opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> = 0;

        // Meta-Object function
        virtual auto respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject> = 0;

        virtual ~OperatorsBase() noexcept = 0;
    };

    struct ObjectBase : NonCopyable // NOLINT(cppcoreguidelines-special-member-functions)
    {
        [[nodiscard]] virtual auto show() const -> Str = 0;

        [[nodiscard]] virtual auto boolValue() const -> bool = 0;

        [[nodiscard]] virtual auto type() const -> RuntimeRef<NGType> = 0;

        virtual ~ObjectBase() = 0;
    };

    enum class Orders : int8_t
    {
        GT,
        EQ,
        LT,
        UNORDERED
    };

    struct NGObject : // NOLINT(cppcoreguidelines-special-member-functions)
                      public virtual OperatorsBase,
                      public virtual ObjectBase
    {

        NGObject() = default;

        static auto boolean(bool boolean) -> RuntimeRef<NGObject>;

        static auto objectType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto boolValue() const -> bool override
        {
            return true;
        }

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        ~NGObject() override;

        // TODO: reference counting
        //        virtual void acquire() = 0;
        //
        //        virtual void release() = 0;

        // Operators overloading

        // obj[index]
        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        // obj[index] = newValue
        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        // obj == other
        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opNotEqual(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;

        virtual auto compareTo(const NGObject *other) const -> Orders
        {
            return Orders::UNORDERED;
        };

        [[nodiscard]] auto opGreaterThan(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opLessThan(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opGreaterEqual(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opLessEqual(RuntimeRef<NGObject> other) const -> bool override;

        auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;

        auto opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;
        // Meta-Object function
        auto respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject> override;
    };

    inline auto negate(Orders order) -> Orders
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

        auto compareTo(const NGObject *other) const -> Orders override
        {
            return T::comparator(this, other);
        }

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override
        {
            return T::comparator(this, other.get()) == Orders::EQ;
        }

        [[nodiscard]] auto opNotEqual(RuntimeRef<NGObject> other) const -> bool override
        {
            return !this->opEquals(other);
        }

        [[nodiscard]] auto opLessThan(RuntimeRef<NGObject> other) const -> bool override
        {
            return T::comparator(this, other.get()) == Orders::LT;
        }

        [[nodiscard]] auto opGreaterThan(RuntimeRef<NGObject> other) const -> bool override
        {
            return T::comparator(this, other.get()) == Orders::GT;
        }

        [[nodiscard]] auto opLessEqual(RuntimeRef<NGObject> other) const -> bool override
        {
            return !this->opGreaterThan(other);
        }

        [[nodiscard]] auto opGreaterEqual(RuntimeRef<NGObject> other) const -> bool override
        {
            return !this->opLessThan(other);
        }
    };

    struct NGDefinition
    {
        Str name;
        NG::ast::ASTNode *defbody;
    };

    struct NGModule : public virtual NGObject // NOLINT(cppcoreguidelines-special-member-functions)
    {
        Vec<Str> imports;
        Vec<Str> exports;

        Map<Str, RuntimeRef<NGObject>> objects;
        Map<Str, NGInvocationHandler> functions;
        Map<Str, RuntimeRef<NGType>> types;

        [[nodiscard]] auto size() const -> size_t
        {
            return objects.size() + functions.size() + types.size();
        }

        auto respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject> override;

        ~NGModule() override;
    };

    struct NGArray : NGObject
    {
        RuntimeRef<Vec<RuntimeRef<NGObject>>> items;

        explicit NGArray(const Vec<RuntimeRef<NGObject>> &vec = {}) : items{makert<Vec<RuntimeRef<NGObject>>>(vec)} {}

        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto boolValue() const -> bool override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;
    };

    struct NGBoolean final : NGObject
    {
        bool value;

        explicit NGBoolean(bool value = false) : value{value} {}

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto boolValue() const -> bool override;
    };

    struct NGString final : NGObject
    {
        Str value;

        static auto stringType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        explicit NGString(Str str) : value{std::move(str)} {}

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto boolValue() const -> bool override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;
    };

    struct NGStructuralObject : NGObject
    {
        RuntimeRef<NGType> customizedType;
        Map<Str, RuntimeRef<NGObject>> properties;

        Map<Str, NGInvocationHandler> selfMemberFunctions;

        auto respond(const Str &member, RuntimeRef<NGContext> context, RuntimeRef<NGInvocationContext> invocationContext) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        [[nodiscard]] auto show() const -> Str override;
    };

}
