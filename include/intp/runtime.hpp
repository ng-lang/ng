#pragma once

#include <algorithm>
#include <ast.hpp>
#include <functional>
#include <memory>
#include <debug.hpp>
#include <utility>

namespace NG::runtime
{

    /**
     * @brief Alias for std::shared_ptr for runtime objects.
     *
     * @tparam T The type of the runtime object.
     */
    template <class T>
    using RuntimeRef = std::shared_ptr<T>;

    /**
     * @brief Creates a `RuntimeRef` (std::shared_ptr) for a runtime object.
     *
     * @tparam T The type of the runtime object to create.
     * @tparam Args The types of the arguments for the constructor of T.
     * @param args The arguments for the constructor of T.
     * @return A `RuntimeRef<T>` to the newly created object.
     */
    template <class T, class... Args>
    [[nodiscard]] inline auto makert(Args &&...args) -> RuntimeRef<T>
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief Represents the context of a function invocation.
     */
    struct NGInvocationContext
    {
        RuntimeRef<NGObject> target;      ///< The target object of the invocation.
        Vec<RuntimeRef<NGObject>> params; ///< The parameters of the invocation.
    };

    /**
     * @brief Exception used to signal the next iteration of a loop.
     */
    struct NextIteration : public std::exception
    {
        Vec<RuntimeRef<NGObject>> slotValues; ///< Values to be passed to the next iteration.
        explicit NextIteration(Vec<RuntimeRef<NGObject>> slotValues) : slotValues(slotValues) {};
    };

    /**
     * @brief Exception used to signal the end of an iteration.
     */
    struct StopIteration : public std::exception
    {
    };

    using NGSelf = RuntimeRef<NGObject>;              ///< Alias for a `RuntimeRef` to the current object (`self`).
    using NGCtx = RuntimeRef<NGContext>;              ///< Alias for a `RuntimeRef` to the current context.
    using NGInvCtx = RuntimeRef<NGInvocationContext>; ///< Alias for a `RuntimeRef` to the current invocation context.

    /**
     * @brief Represents an invocable entity (e.g., a function).
     */
    using NGInvocable = std::function<void(NGSelf self, NGCtx ctx, NGInvCtx invCtx)>;

    /**
     * @brief Represents a type in the runtime.
     */
    struct NGType
    {
        Str name; ///< The name of the type.

        Vec<Str> properties; ///< The properties of the type.

        Map<Str, NGInvocable> memberFunctions; ///< The member functions of the type.

        auto operator==(const NGType &other) const -> bool
        {
            if (this == &other)
            {
                return true;
            }
            if (name != other.name || properties != other.properties)
            {
                return false;
            }
            if (memberFunctions.size() != other.memberFunctions.size())
            {
                return false;
            }
            for (const auto &kv : memberFunctions)
            {
                if (!other.memberFunctions.contains(kv.first))
                {
                    return false;
                }
            }
            return true;
        }
    };

    /**
     * @brief Represents the execution context.
     */
    struct NGContext
    {

        RuntimeRef<NGObject> retVal; ///< The return value of the current function.

        /**
         * @brief Creates a new child context.
         *
         * @return A `RuntimeRef` to the new child context.
         */
        auto fork() -> RuntimeRef<NGContext>;

        /**
         * @brief Gets an object from the context.
         *
         * @param name The name of the object.
         * @return A `RuntimeRef` to the object.
         */
        auto get(Str name) -> RuntimeRef<NGObject>;
        /**
         * @brief Sets an object in the context.
         *
         * @param name The name of the object.
         * @param value The value of the object.
         */
        void set(Str name, RuntimeRef<NGObject> value);

        /**
         * @brief Defines a new object in the context.
         *
         * @param name The name of the object.
         * @param value The value of the object.
         */
        void define(Str name, RuntimeRef<NGObject> value);
        /**
         * @brief Defines a new function in the context.
         *
         * @param name The name of the function.
         * @param value The function.
         */
        void define_function(Str name, NGInvocable value);
        /**
         * @brief Defines a new type in the context.
         *
         * @param name The name of the type.
         * @param type The type.
         */
        void define_type(Str name, RuntimeRef<NGType> type);
        /**
         * @brief Defines a new module in the context.
         *
         * @param name The name of the module.
         * @param module The module.
         */
        void define_module(Str name, RuntimeRef<NGModule> module);

        /**
         * @brief Checks if an object exists in the context.
         *
         * @param name The name of the object.
         * @param global Whether to search in the global scope.
         * @return `true` if the object exists, `false` otherwise.
         */
        auto has_object(Str name, bool global = false) -> bool;
        /**
         * @brief Checks if a function exists in the context.
         *
         * @param name The name of the function.
         * @param global Whether to search in the global scope.
         * @return `true` if the function exists, `false` otherwise.
         */
        auto has_function(Str name, bool global = false) -> bool;
        /**
         * @brief Checks if a type exists in the context.
         *
         * @param name The name of the type.
         * @param global Whether to search in the global scope.
         * @return `true` if the type exists, `false` otherwise.
         */
        auto has_type(Str name, bool global = false) -> bool;
        /**
         * @brief Checks if a module exists in the context.
         *
         * @param name The name of the module.
         * @param global Whether to search in the global scope.
         * @return `true` if the module exists, `false` otherwise.
         */
        auto has_module(Str name, bool global = false) -> bool;

        /**
         * @brief Gets a function from the context.
         *
         * @param name The name of the function.
         * @return The function.
         */
        auto get_function(Str name) -> NGInvocable;
        /**
         * @brief Gets a type from the context.
         *
         * @param name The name of the type.
         * @return A `RuntimeRef` to the type.
         */
        auto get_type(Str name) -> RuntimeRef<NGType>;
        /**
         * @brief Gets a module from the context.
         *
         * @param name The name of the module.
         * @return A `RuntimeRef` to the module.
         */
        auto get_module(Str name) -> RuntimeRef<NGModule>;

        /**
         * @brief Prints a summary of the context.
         */
        void summary();

        Map<Str, RuntimeRef<NGObject>> objects; ///< The objects in the context.
        Map<Str, NGInvocable> functions;        ///< The functions in the context.
        Map<Str, RuntimeRef<NGType>> types;     ///< The types in the context.
        Map<Str, RuntimeRef<NGModule>> modules; ///< The modules in the context.
        Vec<Str> exports;                       ///< The exported symbols.
        Vec<Str> imported;                      ///< The imported symbols.
        Set<Str> locals;                        ///< The local variables.

    private:
        NGContext *parent = nullptr; ///< The parent context.
    };

    /**
     * @brief Base class for operator overloading.
     */
    struct OperatorsBase // NOLINT(cppcoreguidelines-special-member-functions)
    {
        /**
         * @brief Index operator.
         *
         * @param index The index.
         * @return The value at the index.
         */
        [[nodiscard]] virtual auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Index assignment operator.
         *
         * @param index The index.
         * @param newValue The new value.
         * @return The new value.
         */
        virtual auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Addition operator.
         *
         * @param other The other operand.
         * @return The result of the addition.
         */
        [[nodiscard]] virtual auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Subtraction operator.
         *
         * @param other The other operand.
         * @return The result of the subtraction.
         */
        [[nodiscard]] virtual auto opMinus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Multiplication operator.
         *
         * @param other The other operand.
         * @return The result of the multiplication.
         */
        [[nodiscard]] virtual auto opTimes(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Division operator.
         *
         * @param other The other operand.
         * @return The result of the division.
         */
        [[nodiscard]] virtual auto opDividedBy(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Modulus operator.
         *
         * @param other The other operand.
         * @return The result of the modulus operation.
         */
        [[nodiscard]] virtual auto opModulus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Equality operator.
         *
         * @param other The other operand.
         * @return `true` if the objects are equal, `false` otherwise.
         */
        [[nodiscard]] virtual auto opEquals(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Inequality operator.
         *
         * @param other The other operand.
         * @return `true` if the objects are not equal, `false` otherwise.
         */
        [[nodiscard]] virtual auto opNotEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Greater than operator.
         *
         * @param other The other operand.
         * @return `true` if this object is greater than the other, `false` otherwise.
         */
        [[nodiscard]] virtual auto opGreaterThan(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Less than operator.
         *
         * @param other The other operand.
         * @return `true` if this object is less than the other, `false` otherwise.
         */
        [[nodiscard]] virtual auto opLessThan(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Greater than or equal to operator.
         *
         * @param other The other operand.
         * @return `true` if this object is greater than or equal to the other, `false` otherwise.
         */
        [[nodiscard]] virtual auto opGreaterEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Less than or equal to operator.
         *
         * @param other The other operand.
         * @return `true` if this object is less than or equal to the other, `false` otherwise.
         */
        [[nodiscard]] virtual auto opLessEqual(RuntimeRef<NGObject> other) const -> bool = 0;

        /**
         * @brief Left shift operator.
         *
         * @param other The other operand.
         * @return The result of the left shift.
         */
        virtual auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Right shift operator.
         *
         * @param other The other operand.
         * @return The result of the right shift.
         */
        virtual auto opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> = 0;

        /**
         * @brief Meta-object function for responding to messages.
         *
         * @param member The name of the member to invoke.
         * @param context The current context.
         * @param invocationContext The invocation context.
         * @return The result of the invocation.
         */
        virtual auto respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject> = 0;

        virtual ~OperatorsBase() noexcept = 0;
    };

    /**
     * @brief Base class for all runtime objects.
     */
    struct ObjectBase : NonCopyable // NOLINT(cppcoreguidelines-special-member-functions)
    {
        /**
         * @brief Returns a string representation of the object.
         *
         * @return A string representation of the object.
         */
        [[nodiscard]] virtual auto show() const -> Str = 0;

        /**
         * @brief Returns the boolean value of the object.
         *
         * @return The boolean value of the object.
         */
        [[nodiscard]] virtual auto boolValue() const -> bool = 0;

        /**
         * @brief Returns the type of the object.
         *
         * @return A `RuntimeRef` to the type of the object.
         */
        [[nodiscard]] virtual auto type() const -> RuntimeRef<NGType> = 0;

        virtual ~ObjectBase() = 0;
    };

    /**
     * @brief Represents the result of a three-way comparison.
     */
    enum class Orders : int8_t
    {
        GT,       ///< Greater than
        EQ,       ///< Equal to
        LT,       ///< Less than
        UNORDERED ///< Unordered
    };

    /**
     * @brief The base class for all NG objects.
     */
    struct NGObject : // NOLINT(cppcoreguidelines-special-member-functions)
                      public virtual OperatorsBase,
                      public virtual ObjectBase
    {

        NGObject() = default;

        /**
         * @brief Creates a boolean object.
         *
         * @param boolean The boolean value.
         * @return A `RuntimeRef` to the boolean object.
         */
        static auto boolean(bool boolean) -> RuntimeRef<NGObject>;

        /**
         * @brief Returns the type of the object.
         *
         * @return A `RuntimeRef` to the type of the object.
         */
        static auto objectType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto boolValue() const -> bool override
        {
            return true;
        }

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        ~NGObject() override;

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

        /**
         * @brief Compares this object with another object.
         *
         * @param other The other object.
         * @return The result of the comparison.
         */
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
        auto respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Negates an `Orders` value.
     *
     * @param order The `Orders` value to negate.
     * @return The negated `Orders` value.
     */
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

    /**
     * @brief A mixin for three-way comparable objects.
     *
     * @tparam T The type of the object.
     */
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

    /**
     * @brief Represents a definition in the runtime.
     */
    struct NGDefinition
    {
        Str name;                  ///< The name of the definition.
        NG::ast::ASTNode *defbody; ///< The body of the definition.
    };

    /**
     * @brief Represents a module in the runtime.
     */
    struct NGModule : public virtual NGObject // NOLINT(cppcoreguidelines-special-member-functions)
    {
        Set<Str> imports; ///< The imported modules.
        Set<Str> exports; ///< The exported symbols.

        Map<Str, RuntimeRef<NGObject>> objects; ///< The objects in the module.
        Map<Str, NGInvocable> functions;        ///< The functions in the module.
        Map<Str, RuntimeRef<NGType>> types;     ///< The types in the module.
        Map<Str, NGInvocable> native_functions; ///< The native functions in the module.

        NGModule(RuntimeRef<NGContext> ctx);

        /**
         * @brief Returns the size of the module.
         *
         * @return The size of the module.
         */
        [[nodiscard]] auto size() const -> size_t
        {
            return objects.size() + functions.size() + types.size();
        }

        auto respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject> override;

        ~NGModule() override;
    };

    /**
     * @brief Represents an array in the runtime.
     */
    struct NGArray : NGObject
    {
        RuntimeRef<Vec<RuntimeRef<NGObject>>> items; ///< The items in the array.

        explicit NGArray(const Vec<RuntimeRef<NGObject>> &vec = {}) : items{makert<Vec<RuntimeRef<NGObject>>>(vec)} {}

        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto boolValue() const -> bool override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Represents an tuple in the runtime.
     */
    struct NGTuple : NGObject
    {
        RuntimeRef<Vec<RuntimeRef<NGObject>>> items; ///< The items in the tuple.

        explicit NGTuple(const Vec<RuntimeRef<NGObject>> &vec = {}) : items{makert<Vec<RuntimeRef<NGObject>>>(vec)} {}

        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto boolValue() const -> bool override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        auto respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Represents a boolean in the runtime.
     */
    struct NGBoolean final : NGObject
    {
        bool value; ///< The value of the boolean.

        explicit NGBoolean(bool value = false) : value{value} {}

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto boolValue() const -> bool override;
    };

    /**
     * @brief Represents a string in the runtime.
     */
    struct NGString final : NGObject
    {
        Str value; ///< The value of the string.

        /**
         * @brief Returns the type of the string.
         *
         * @return A `RuntimeRef` to the type of the string.
         */
        static auto stringType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        explicit NGString(Str str) : value{std::move(str)} {}

        [[nodiscard]] auto show() const -> Str override;

        [[nodiscard]] auto boolValue() const -> bool override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Represents a structural object in the runtime.
     */
    struct NGStructuralObject : NGObject
    {
        RuntimeRef<NGType> customizedType;         ///< The customized type of the object.
        Map<Str, RuntimeRef<NGObject>> properties; ///< The properties of the object.

        Map<Str, NGInvocable> selfMemberFunctions; ///< The member functions of the object.

        auto respond(const Str &member, NGCtx context, NGInvCtx invocationContext) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;

        [[nodiscard]] auto show() const -> Str override;
    };

    struct NGUnit : NGObject
    {
        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override;
        [[nodiscard]] auto show() const -> Str override;
    };

    /**
     * @brief Registers a native library.
     *
     * @param moduleId The ID of the module.
     * @param handlers The handlers for the native functions.
     */
    void
    register_native_library(Str moduleId, Map<Str, NGInvocable> handlers);
}
