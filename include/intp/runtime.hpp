#pragma once

#include <algorithm>
#include <ast.hpp>
#include <cstddef>
#include <cstdint>
#include <debug.hpp>
#include <functional>
#include <memory>
#include <runtime/buffer_runtime.hpp>
#include <utility>

namespace NG::runtime
{
    struct NGObject;
    struct NGType;
    struct NGModule;
    struct NGContext;
    struct RuntimeEnv;
    struct NGStructuralObject;
    struct NGTaggedValue;
    struct NumeralBase;

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

    using NGSelf = RuntimeRef<NGObject>; ///< Alias for a `RuntimeRef` to the current object (`self`).
    using NGCtx = RuntimeRef<NGContext>; ///< Alias for a `RuntimeRef` to the current context.
    using NGEnv = RuntimeRef<RuntimeEnv>; ///< Alias for a runtime dispatch environment.
    using NGArgs = Vec<RuntimeRef<NGObject>>;

    [[nodiscard]] auto make_runtime_env(const RuntimeRef<NGContext> &context = nullptr) -> NGEnv;
    [[nodiscard]] auto fork_runtime_env(const NGEnv &env) -> NGEnv;
    void runtime_env_set_state(const NGEnv &env, Str name, std::shared_ptr<void> value);
    [[nodiscard]] auto runtime_env_get_state(const NGEnv &env, const Str &name) -> std::shared_ptr<void>;

    [[nodiscard]] auto runtime_value_show(const RuntimeRef<NGObject> &value) -> Str;
    [[nodiscard]] auto runtime_value_bool(const RuntimeRef<NGObject> &value) -> bool;
    [[nodiscard]] auto runtime_value_respond(const RuntimeRef<NGObject> &value, const Str &member, const NGEnv &env,
                                             const NGArgs &args) -> RuntimeRef<NGObject>;
    [[nodiscard]] auto structural_runtime_type(const NGStructuralObject &structural) -> RuntimeRef<NGType>;
    [[nodiscard]] auto structural_runtime_show(const NGStructuralObject &structural) -> Str;
    [[nodiscard]] auto tagged_runtime_type(const NGTaggedValue &tagged) -> RuntimeRef<NGType>;
    [[nodiscard]] auto tagged_runtime_show(const NGTaggedValue &tagged) -> Str;
    [[nodiscard]] auto numeral_runtime_show(const NumeralBase &numeral) -> Str;
    [[nodiscard]] auto numeral_runtime_bool(const NumeralBase &numeral) -> bool;

    using LayoutKind = NG::buffer_runtime::LayoutKind;
    using StorageClass = NG::buffer_runtime::StorageClass;
    using FieldLayout = NG::buffer_runtime::FieldLayout;
    using VariantLayout = NG::buffer_runtime::VariantLayout;
    using TypeLayout = NG::buffer_runtime::TypeLayout;
    using FieldSpec = NG::buffer_runtime::FieldSpec;
    using VariantSpec = NG::buffer_runtime::VariantSpec;
    using LayoutRegistry = NG::buffer_runtime::LayoutRegistry;
    using HeapStore = NG::buffer_runtime::HeapStore;
    using CellRef = NG::buffer_runtime::CellRef;
    using NativeHandle = NG::buffer_runtime::NativeHandle;

    struct StorageCell : NG::buffer_runtime::FrameSlot
    {
        RuntimeRef<NGObject> boxedValue;
        RuntimeRef<NGType> runtimeType;
        uint64_t ownerScopeId = 0;
        bool marked = false;
    };

    struct CallFrame
    {
        Str functionName;
        RuntimeRef<StorageCell> receiver;
        Vec<RuntimeRef<StorageCell>> params;
        Vec<RuntimeRef<StorageCell>> locals;
        Vec<RuntimeRef<StorageCell>> temporaries;
        RuntimeRef<StorageCell> returnSlot;
    };

    /**
     * @brief Represents a callable runtime entity.
     */
    using NGCallable = std::function<RuntimeRef<NGObject>(const NGSelf &self, const NGEnv &env, const NGArgs &args)>;

    /**
     * @brief Represents a type in the runtime.
     */
    struct NGType
    {
        Str name; ///< The name of the type.
        TypeLayout layout; ///< Runtime layout metadata for value/storage representation.

        Vec<Str> properties; ///< The properties of the type.

        Str variantName; ///< Tagged union variant name when this type describes a variant constructor.

        int32_t variantIndex = -1; ///< Tagged union variant index when this type describes a variant constructor.

        Map<Str, NGCallable> memberFunctions; ///< The member functions of the type.
        std::function<RuntimeRef<NGObject>(const NGSelf &self, const Str &member, const NGEnv &env,
                                           const NGArgs &args)>
            respondHandler; ///< Optional type-driven member resolution before generic member functions.
        std::function<Str(const NGSelf &self)> showHandler; ///< Optional type-driven show protocol.
        std::function<bool(const NGSelf &self)> boolHandler; ///< Optional type-driven truthiness protocol.

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

    struct RuntimeSymbolTable
    {
        Map<Str, RuntimeRef<StorageCell>> objectSlots;
        Map<Str, NGCallable> functions;
        Map<Str, RuntimeRef<NGType>> types;
        Map<Str, RuntimeRef<NGType>> variantTypes;
        Map<Str, RuntimeRef<NGModule>> modules;
        Vec<Str> exports;
        Vec<Str> imported;
    };

    struct RuntimeEnv
    {
        RuntimeRef<RuntimeSymbolTable> symbols;
        Map<Str, std::shared_ptr<void>> runtimeState;
    };

    /**
     * @brief Represents the execution context.
     */
    struct NGContext
    {
        NGContext();
        ~NGContext();

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
        auto get_slot(Str name) -> RuntimeRef<StorageCell>;
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
        void define_function(Str name, NGCallable value);
        /**
         * @brief Defines a new type in the context.
         *
         * @param name The name of the type.
         * @param type The type.
         */
        void define_type(Str name, RuntimeRef<NGType> type);
        void define_variant_type(Str name, RuntimeRef<NGType> type);
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
        auto get_function(Str name) -> NGCallable;
        /**
         * @brief Gets a type from the context.
         *
         * @param name The name of the type.
         * @return A `RuntimeRef` to the type.
         */
        auto get_type(Str name) -> RuntimeRef<NGType>;
        auto get_variant_type(Str name) -> RuntimeRef<NGType>;
        /**
         * @brief Gets a module from the context.
         *
         * @param name The name of the module.
         * @return A `RuntimeRef` to the module.
         */
        auto get_module(Str name) -> RuntimeRef<NGModule>;

        auto symbol_table() -> RuntimeRef<RuntimeSymbolTable>;
        void adopt_symbol_table(RuntimeRef<RuntimeSymbolTable> nextSymbols) { symbols = std::move(nextSymbols); }
        auto parent_context() const -> NGContext * { return parent; }

        /**
         * @brief Prints a summary of the context.
         */
        void summary();

        Map<Str, RuntimeRef<StorageCell>> objectSlots; ///< Compatibility storage cells for local bindings.
        Set<Str> locals;                        ///< The local variables.

      private:
        RuntimeRef<RuntimeSymbolTable> symbols;
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
        virtual auto respond(const RuntimeRef<NGObject> &self, const Str &member, NGCtx context,
                             const NGArgs &args) -> RuntimeRef<NGObject> = 0;

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

        [[nodiscard]] auto boolValue() const -> bool override;

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
        virtual auto compareTo(const NGObject *other) const -> Orders { return Orders::UNORDERED; };

        [[nodiscard]] auto opGreaterThan(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opLessThan(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opGreaterEqual(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opLessEqual(RuntimeRef<NGObject> other) const -> bool override;

        auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;

        auto opRShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;
        // Meta-Object function
        auto respond(const RuntimeRef<NGObject> &self, const Str &member, NGCtx context,
                     const NGArgs &args) -> RuntimeRef<NGObject> override;
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

        auto compareTo(const NGObject *other) const -> Orders override { return T::comparator(this, other); }

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
        Map<Str, NGCallable> functions;         ///< The functions in the module.
        Map<Str, RuntimeRef<NGType>> types;     ///< The types in the module.
        Map<Str, NGCallable> native_functions;  ///< The native functions in the module.
        Map<Str, std::shared_ptr<void>> native_state; ///< Runtime-owned native module state.

        NGModule(RuntimeRef<NGContext> ctx);
        static auto moduleType() -> RuntimeRef<NGType>;
        void set_native_state(Str name, std::shared_ptr<void> value);
        [[nodiscard]] auto get_native_state(const Str &name) const -> std::shared_ptr<void>;
        void clear_native_state(const Str &name);

        /**
         * @brief Returns the size of the module.
         *
         * @return The size of the module.
         */
        [[nodiscard]] auto size() const -> size_t { return objects.size() + functions.size() + types.size(); }

        ~NGModule() override;
    };

    /**
     * @brief Represents an array in the runtime.
     */
    struct NGArray : NGObject
    {
        mutable Vec<RuntimeRef<StorageCell>> elementSlots;
        mutable HeapStore headerStore;
        mutable CellRef headerRef;
        mutable CellRef payloadRef;
        mutable size_t payloadCapacity = 0;

        explicit NGArray(const Vec<RuntimeRef<NGObject>> &vec = {});

        static auto arrayType() -> RuntimeRef<NGType>;

        void sync_header_backing() const;
        void sync_element_slots() const;
        [[nodiscard]] auto header_cell() const -> CellRef;
        [[nodiscard]] auto header_store() const -> const HeapStore & { return headerStore; }
        [[nodiscard]] auto header_length() const -> uint64_t;
        [[nodiscard]] auto header_capacity() const -> uint64_t;
        [[nodiscard]] auto header_data_handle() const -> NativeHandle;
        [[nodiscard]] auto payload_cell() const -> CellRef;
        [[nodiscard]] auto payload_items() const -> Vec<RuntimeRef<NGObject>>;
        void replace_payload_items(const Vec<RuntimeRef<NGObject>> &values, size_t capacityHint = 0);
        [[nodiscard]] auto element_slot(size_t index) const -> RuntimeRef<StorageCell>;

        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        auto opLShift(RuntimeRef<NGObject> other) -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Represents an tuple in the runtime.
     */
    struct NGTuple : NGObject
    {
        mutable Vec<RuntimeRef<StorageCell>> elementSlots;
        mutable HeapStore payloadStore;
        mutable CellRef payloadRef;

        explicit NGTuple(const Vec<RuntimeRef<NGObject>> &vec = {});
        static auto tupleType() -> RuntimeRef<NGType>;
        void sync_payload_backing() const;
        void sync_element_slots() const;
        [[nodiscard]] auto payload_cell() const -> CellRef;
        [[nodiscard]] auto payload_store() const -> const HeapStore & { return payloadStore; }
        [[nodiscard]] auto payload_items() const -> Vec<RuntimeRef<NGObject>>;
        void replace_payload_items(const Vec<RuntimeRef<NGObject>> &values);
        [[nodiscard]] auto element_slot(size_t index) const -> RuntimeRef<StorageCell>;

        [[nodiscard]] auto opIndex(RuntimeRef<NGObject> index) const -> RuntimeRef<NGObject> override;

        auto opIndex(RuntimeRef<NGObject> index, RuntimeRef<NGObject> newValue) -> RuntimeRef<NGObject> override;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;
    };

    /**
     * @brief Represents a boolean in the runtime.
     */
    struct NGBoolean final : NGObject
    {
        bool value; ///< The value of the boolean.

        explicit NGBoolean(bool value = false) : value{value} {}

        static auto booleanType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;
    };

    /**
     * @brief Represents a string in the runtime.
     */
    struct NGString final : NGObject
    {
        Str value; ///< The value of the string.
        mutable HeapStore headerStore;
        mutable CellRef headerRef;
        mutable CellRef payloadRef;

        /**
         * @brief Returns the type of the string.
         *
         * @return A `RuntimeRef` to the type of the string.
         */
        static auto stringType() -> RuntimeRef<NGType>;
        void sync_header_backing() const;
        [[nodiscard]] auto header_cell() const -> CellRef;
        [[nodiscard]] auto header_store() const -> const HeapStore & { return headerStore; }
        [[nodiscard]] auto header_length() const -> uint64_t;
        [[nodiscard]] auto header_data_handle() const -> NativeHandle;
        [[nodiscard]] auto payload_cell() const -> CellRef;
        [[nodiscard]] auto payload_value() const -> Str;
        void replace_payload_value(Str nextValue);

        explicit NGString(Str str);

        [[nodiscard]] auto opEquals(RuntimeRef<NGObject> other) const -> bool override;

        [[nodiscard]] auto opPlus(RuntimeRef<NGObject> other) const -> RuntimeRef<NGObject> override;
    };

    /**
     * @brief Represents a structural object in the runtime.
     */
    struct NGStructuralObject : NGObject
    {
        RuntimeRef<NGType> customizedType;         ///< The customized type of the object.
        mutable Map<Str, RuntimeRef<StorageCell>> propertySlots; ///< Dynamic/string-keyed properties backed by storage cells.
        mutable Vec<RuntimeRef<StorageCell>> fieldSlots;
        mutable HeapStore payloadStore;
        mutable CellRef payloadRef;

        Map<Str, NGCallable> selfMemberFunctions; ///< The member functions of the object.

        void sync_payload_backing() const;
        void sync_field_slots() const;
        [[nodiscard]] auto payload_cell() const -> CellRef;
        [[nodiscard]] auto payload_store() const -> const HeapStore & { return payloadStore; }
        [[nodiscard]] auto payload_fields() const -> Vec<RuntimeRef<NGObject>>;
        void replace_payload_fields(const Vec<RuntimeRef<NGObject>> &values);
        [[nodiscard]] auto field_slot(size_t index) const -> RuntimeRef<StorageCell>;
        [[nodiscard]] auto property_slot(const Str &name) const -> RuntimeRef<StorageCell>;
        [[nodiscard]] auto property_slot_or_create(const Str &name) const -> RuntimeRef<StorageCell>;

    };

    struct NGUnit : NGObject
    {
        static auto unitType() -> RuntimeRef<NGType>;
    };

    struct NGMovedObject final : NGObject
    {
        [[nodiscard]] static auto movedType() -> RuntimeRef<NGType>;
    };

    struct NGReference final : NGObject
    {
        using MarkHook = std::function<void()>;

        RuntimeRef<StorageCell> targetCell;
        MarkHook markHook;
        Str debugName;

        NGReference(RuntimeRef<StorageCell> targetCell, Str debugName, MarkHook markHook = nullptr);

        [[nodiscard]] static auto referenceType() -> RuntimeRef<NGType>;

        [[nodiscard]] auto read() const -> RuntimeRef<NGObject>;
        void write(const RuntimeRef<NGObject> &value) const;
        void mark_referenced_heap() const;
        [[nodiscard]] auto storage_cell() const -> RuntimeRef<StorageCell> { return targetCell; }
    };

    /**
     * @brief Represents a newtype wrapper in the runtime.
     *
     * A newtype wraps a value with a distinct nominal type identity.
     * The wrapped value can only be accessed via explicit cast (unwrap).
     */
    struct NGNewType : NGObject
    {
        RuntimeRef<NGType> newType;     ///< The nominal type of this newtype.
        RuntimeRef<NGObject> wrapped;   ///< The wrapped value.

        NGNewType(RuntimeRef<NGType> type, RuntimeRef<NGObject> value)
            : newType(std::move(type)), wrapped(std::move(value)) {}

        [[nodiscard]] auto type() const -> RuntimeRef<NGType> override { return newType; }
        [[nodiscard]] auto show() const -> Str override
        {
            if (newType && newType->showHandler)
            {
                return newType->showHandler(RuntimeRef<NGObject>(const_cast<NGNewType *>(this), [](NGObject *) {}));
            }
            return runtime_value_show(wrapped);
        }
        [[nodiscard]] auto boolValue() const -> bool override
        {
            if (newType && newType->boolHandler)
            {
                return newType->boolHandler(RuntimeRef<NGObject>(const_cast<NGNewType *>(this), [](NGObject *) {}));
            }
            return runtime_value_bool(wrapped);
        }

        auto respond(const RuntimeRef<NGObject> &self, const Str &member, NGCtx context,
                     const NGArgs &args) -> RuntimeRef<NGObject> override
        {
            if (newType && (newType->respondHandler || newType->memberFunctions.contains(member)))
            {
                auto dispatchSelf = self && self.get() == this ? self
                                                               : RuntimeRef<NGObject>(this, [](NGObject *) {});
                return NGObject::respond(dispatchSelf, member, context, args);
            }
            return runtime_value_respond(wrapped, member, make_runtime_env(context), args);
        }
    };

    /**
     * @brief A tagged value in a tagged union (e.g. Ok(42), Err("not found")).
     */
    struct NGTaggedValue : NGObject
    {
        Str unionName;                       ///< The name of the tagged union type.
        Str variantName;                     ///< The name of this variant.
        int32_t variantIndex;                ///< The index of this variant.
        Vec<Str> payloadNames;               ///< Named fields for the payload (e.g. {"value"} for Ok(value: i32)).
        mutable Vec<RuntimeRef<StorageCell>> payloadSlots;
        mutable HeapStore payloadStore;
        mutable CellRef payloadRef;

        NGTaggedValue(Str unionName, Str variantName, int32_t variantIndex,
                      Vec<RuntimeRef<NGObject>> payload, Vec<Str> payloadNames = {})
            : unionName(std::move(unionName)), variantName(std::move(variantName)),
              variantIndex(variantIndex), payloadNames(std::move(payloadNames))
        {
            replace_payload_items(payload);
        }

        void sync_payload_backing() const;
        void sync_payload_slots() const;
        [[nodiscard]] auto payload_cell() const -> CellRef;
        [[nodiscard]] auto payload_store() const -> const HeapStore & { return payloadStore; }
        [[nodiscard]] auto payload_items() const -> Vec<RuntimeRef<NGObject>>;
        void replace_payload_items(const Vec<RuntimeRef<NGObject>> &values);
        [[nodiscard]] auto payload_slot(size_t index) const -> RuntimeRef<StorageCell>;

    };

    /**
     * @brief Registers a native library.
     *
     * @param moduleId The ID of the module.
     * @param handlers The handlers for the native functions.
     */
    void register_native_library(Str moduleId, Map<Str, NGCallable> handlers);
    void bind_native_library_handlers(const RuntimeRef<NGModule> &module, const Map<Str, NGCallable> &handlers);
    [[nodiscard]] auto current_native_module(const NGEnv &env) -> RuntimeRef<NGModule>;

    using GCRootProvider = std::function<Vec<RuntimeRef<NGObject>>()>;

    [[nodiscard]] auto make_storage_cell(const TypeLayout &layout, StorageClass storageClass = StorageClass::TEMPORARY,
                                         const RuntimeRef<NGObject> &boxedValue = nullptr, Str name = {},
                                         const RuntimeRef<NGType> &runtimeType = nullptr)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto make_boxed_storage_cell(const RuntimeRef<NGObject> &value,
                                               StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto moved_object() -> RuntimeRef<NGObject>;
    [[nodiscard]] auto is_moved_object(const RuntimeRef<NGObject> &value) -> bool;
    void ensure_usable_value(const RuntimeRef<NGObject> &value);
    [[nodiscard]] auto auto_deref_value(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>;
    [[nodiscard]] auto clone_value(const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>;
    [[nodiscard]] auto materialize_value(const RuntimeRef<NGObject> &value, bool moved) -> RuntimeRef<NGObject>;
    [[nodiscard]] auto allocate_heap_object(const RuntimeRef<NGObject> &value, const Str &debugName) -> RuntimeRef<NGReference>;
    [[nodiscard]] auto enumerate_context_roots(const RuntimeRef<NGContext> &context) -> Vec<RuntimeRef<NGObject>>;
    [[nodiscard]] auto register_gc_root_provider(GCRootProvider provider) -> size_t;
    void unregister_gc_root_provider(size_t providerId);
    void collect_managed_heap();
    [[nodiscard]] auto managed_heap_size() -> size_t;
} // namespace NG::runtime
