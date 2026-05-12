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
    struct NGType;
    struct RuntimeEnv;
    struct RuntimeSymbolTable;
    struct StorageCell;
    enum class Orders : int8_t;

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
        Vec<RuntimeRef<StorageCell>> slotValues; ///< Values to be passed to the next iteration.
        explicit NextIteration(Vec<RuntimeRef<StorageCell>> slotValues) : slotValues(slotValues) {};
    };

    /**
     * @brief Exception used to signal the end of an iteration.
     */
    struct StopIteration : public std::exception
    {
    };

    using NGSelf = RuntimeRef<StorageCell>; ///< Alias for the current slot-backed receiver (`self`).
    using NGEnv = RuntimeRef<RuntimeEnv>; ///< Alias for a runtime dispatch environment.
    using NGSymbols = RuntimeRef<RuntimeSymbolTable>; ///< Alias for shared runtime symbol tables.
    using NGArgs = Vec<RuntimeRef<StorageCell>>;

    [[nodiscard]] auto make_runtime_env(const NGSymbols &symbols = nullptr) -> NGEnv;
    [[nodiscard]] auto fork_runtime_env(const NGEnv &env) -> NGEnv;
    void runtime_env_set_state(const NGEnv &env, Str name, std::shared_ptr<void> value);
    [[nodiscard]] auto runtime_env_get_state(const NGEnv &env, const Str &name) -> std::shared_ptr<void>;

    [[nodiscard]] auto runtime_value_show(const RuntimeRef<StorageCell> &cell) -> Str;
    [[nodiscard]] auto runtime_value_bool(const RuntimeRef<StorageCell> &cell) -> bool;
    [[nodiscard]] auto runtime_value_respond_slot(const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
                                                  const NGArgs &args) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_value_respond(const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
                                             const NGArgs &args) -> RuntimeRef<StorageCell>;

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
        Map<Str, std::shared_ptr<void>> namedRefs;
        RuntimeRef<NGType> runtimeType;
        uint64_t ownerScopeId = 0;
        bool initialized = false;
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
     * @brief Represents a callable runtime entity.
     */
    using NGCallable = std::function<RuntimeRef<StorageCell>(const NGSelf &self, const NGEnv &env, const NGArgs &args)>;
    enum class RuntimeBinaryOperator : uint8_t
    {
        Add,
        Subtract,
        Multiply,
        Divide,
        Modulus,
        LShift,
        RShift,
    };
    using NGCellShowHandler = std::function<Str(const RuntimeRef<StorageCell> &cell)>;
    using NGCellBoolHandler = std::function<bool(const RuntimeRef<StorageCell> &cell)>;
    using NGCellRespondHandler =
        std::function<RuntimeRef<StorageCell>(const RuntimeRef<StorageCell> &cell, const Str &member, const NGEnv &env,
                                              const NGArgs &args)>;
    using NGCellBinaryOperatorHandler =
        std::function<RuntimeRef<StorageCell>(const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other)>;
    using NGCellOrderOperatorHandler =
        std::function<Orders(const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other)>;

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
        NGCellShowHandler showCellHandler; ///< Optional cell-native show protocol.
        NGCellBoolHandler boolCellHandler; ///< Optional cell-native truthiness protocol.
        NGCellRespondHandler respondCellHandler; ///< Optional cell-native member resolution before materialization.
        Map<RuntimeBinaryOperator, NGCellBinaryOperatorHandler> cellBinaryOperators; ///< Optional cell-native binary ops.
        NGCellOrderOperatorHandler cellOrderHandler; ///< Optional cell-native ordering/equality handler.

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
            if (cellBinaryOperators.size() != other.cellBinaryOperators.size())
            {
                return false;
            }
            if (static_cast<bool>(cellOrderHandler) != static_cast<bool>(other.cellOrderHandler) ||
                static_cast<bool>(showCellHandler) != static_cast<bool>(other.showCellHandler) ||
                static_cast<bool>(boolCellHandler) != static_cast<bool>(other.boolCellHandler) ||
                static_cast<bool>(respondCellHandler) != static_cast<bool>(other.respondCellHandler))
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
            for (const auto &kv : cellBinaryOperators)
            {
                if (!other.cellBinaryOperators.contains(kv.first))
                {
                    return false;
                }
            }
            return true;
        }
    };

    [[nodiscard]] inline auto runtime_object_type() -> RuntimeRef<NGType>
    {
        static RuntimeRef<NGType> OBJECT_TYPE = makert<NGType>(NGType{
            .name = "Object",
            .layout = TypeLayout{.name = "Object", .kind = LayoutKind::DYNAMIC},
            .showCellHandler = [](const RuntimeRef<StorageCell> &) { return Str{"[Object]"}; },
            .boolCellHandler = [](const RuntimeRef<StorageCell> &) { return true; },
        });
        return OBJECT_TYPE;
    }

    struct RuntimeSymbolTable
    {
        Map<Str, RuntimeRef<StorageCell>> objectSlots;
        Map<Str, NGCallable> functions;
        Map<Str, RuntimeRef<NGType>> types;
        Map<Str, RuntimeRef<NGType>> variantTypes;
        Map<Str, RuntimeRef<StorageCell>> modules;
        Vec<Str> exports;
        Vec<Str> imported;
    };

    struct RuntimeEnv
    {
        RuntimeRef<RuntimeSymbolTable> symbols;
        Map<Str, std::shared_ptr<void>> runtimeState;
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
     * @brief Represents a definition in the runtime.
     */
    struct NGDefinition
    {
        Str name;                  ///< The name of the definition.
        NG::ast::ASTNode *defbody; ///< The body of the definition.
    };

    [[nodiscard]] auto module_runtime_type() -> RuntimeRef<NGType>;

    [[nodiscard]] auto array_runtime_type() -> RuntimeRef<NGType>;
    [[nodiscard]] auto make_runtime_array_cell(const Vec<RuntimeRef<StorageCell>> &slots,
                                               size_t capacityHint = 0,
                                               StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_array_value(const RuntimeRef<StorageCell> &cell) -> bool;
    [[nodiscard]] auto runtime_array_length(const RuntimeRef<StorageCell> &cell) -> size_t;
    [[nodiscard]] auto runtime_array_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>;

    [[nodiscard]] auto tuple_runtime_type() -> RuntimeRef<NGType>;
    [[nodiscard]] auto make_runtime_tuple_cell(const Vec<RuntimeRef<StorageCell>> &slots,
                                               StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_tuple_value(const RuntimeRef<StorageCell> &cell) -> bool;
    [[nodiscard]] auto runtime_tuple_length(const RuntimeRef<StorageCell> &cell) -> size_t;
    [[nodiscard]] auto runtime_tuple_slots(const RuntimeRef<StorageCell> &cell) -> Vec<RuntimeRef<StorageCell>>;

    [[nodiscard]] auto boolean_runtime_type() -> RuntimeRef<NGType>;

    [[nodiscard]] auto string_runtime_type() -> RuntimeRef<NGType>;
    [[nodiscard]] auto make_runtime_string(Str value,
                                           StorageClass storageClass = StorageClass::TEMPORARY) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_string_value(const RuntimeRef<StorageCell> &cell) -> bool;
    [[nodiscard]] auto runtime_string_value(const RuntimeRef<StorageCell> &cell) -> Str;

    [[nodiscard]] auto make_runtime_structural_cell(const RuntimeRef<NGType> &type,
                                                    const Vec<RuntimeRef<StorageCell>> &fields = {},
                                                    const Map<Str, RuntimeRef<StorageCell>> &properties = {},
                                                    StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_structural_value(const RuntimeRef<StorageCell> &value) -> bool;
    [[nodiscard]] auto runtime_structural_type(const RuntimeRef<StorageCell> &value) -> RuntimeRef<NGType>;
    [[nodiscard]] auto runtime_structural_field_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>;
    [[nodiscard]] auto runtime_structural_field_slot(const RuntimeRef<StorageCell> &value, size_t index) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_structural_field_index(const RuntimeRef<StorageCell> &value, const Str &name)
        -> std::optional<size_t>;
    [[nodiscard]] auto runtime_structural_property_slots(const RuntimeRef<StorageCell> &value)
        -> Map<Str, RuntimeRef<StorageCell>>;
    [[nodiscard]] auto runtime_structural_property_slot(const RuntimeRef<StorageCell> &value, const Str &name)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_structural_property_slot_or_create(const RuntimeRef<StorageCell> &value, const Str &name)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_structural_read_member(const RuntimeRef<StorageCell> &value, const Str &member)
        -> RuntimeRef<StorageCell>;
    void runtime_structural_write_member(const RuntimeRef<StorageCell> &value, const Str &member,
                                         const RuntimeRef<StorageCell> &nextValue);
    void runtime_structural_replace_field_slots(const RuntimeRef<StorageCell> &value,
                                                const Vec<RuntimeRef<StorageCell>> &slots);

    [[nodiscard]] auto unit_runtime_type() -> RuntimeRef<NGType>;
    [[nodiscard]] auto reference_runtime_type() -> RuntimeRef<NGType>;

    [[nodiscard]] auto make_runtime_newtype_cell(const RuntimeRef<NGType> &type, const RuntimeRef<StorageCell> &wrapped,
                                                 StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;

    [[nodiscard]] auto make_runtime_tagged_cell(const RuntimeRef<NGType> &type,
                                                const Vec<RuntimeRef<StorageCell>> &payloadSlots,
                                                StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto make_runtime_tagged_cell(Str unionName, Str variantName, int32_t variantIndex,
                                                Vec<RuntimeRef<StorageCell>> payloadSlots,
                                                Vec<Str> payloadNames = {},
                                                StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_tagged_value(const RuntimeRef<StorageCell> &value) -> bool;
    [[nodiscard]] auto runtime_tagged_type(const RuntimeRef<StorageCell> &value) -> RuntimeRef<NGType>;
    [[nodiscard]] auto runtime_tagged_union_name(const RuntimeRef<StorageCell> &value) -> Str;
    [[nodiscard]] auto runtime_tagged_variant_name(const RuntimeRef<StorageCell> &value) -> Str;
    [[nodiscard]] auto runtime_tagged_variant_index(const RuntimeRef<StorageCell> &value) -> int32_t;
    [[nodiscard]] auto runtime_tagged_payload_names(const RuntimeRef<StorageCell> &value) -> Vec<Str>;
    [[nodiscard]] auto runtime_tagged_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>;
    [[nodiscard]] auto runtime_tagged_slot(const RuntimeRef<StorageCell> &value, size_t index) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_tagged_payload_index(const RuntimeRef<StorageCell> &value, const Str &member)
        -> std::optional<size_t>;
    [[nodiscard]] auto runtime_tagged_read_member(const RuntimeRef<StorageCell> &value, const Str &member)
        -> RuntimeRef<StorageCell>;

    /**
     * @brief Registers a native library.
     *
     * @param moduleId The ID of the module.
     * @param handlers The handlers for the native functions.
     */
    void register_native_library(Str moduleId, Map<Str, NGCallable> handlers);
    void bind_native_library_handlers(const RuntimeRef<StorageCell> &module, const Map<Str, NGCallable> &handlers);
    [[nodiscard]] auto current_native_module(const NGEnv &env) -> RuntimeRef<StorageCell>;

    struct GCRootSet
    {
        Vec<RuntimeRef<StorageCell>> cells;
    };

    using GCRootProvider = std::function<GCRootSet()>;

    [[nodiscard]] auto make_storage_cell(const TypeLayout &layout, StorageClass storageClass = StorageClass::TEMPORARY,
                                         Str name = {}, const RuntimeRef<NGType> &runtimeType = nullptr)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto make_value_storage_cell(const RuntimeRef<StorageCell> &value,
                                               StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto moved_runtime_type() -> RuntimeRef<NGType>;
    void mark_moved_storage_cell(const RuntimeRef<StorageCell> &cell);
    void clear_storage_cell(const RuntimeRef<StorageCell> &cell);
    [[nodiscard]] auto make_runtime_reference_cell(const RuntimeRef<StorageCell> &targetCell, Str debugName = {},
                                                   StorageClass storageClass = StorageClass::TEMPORARY)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_reference_value(const RuntimeRef<StorageCell> &cell) -> bool;
    [[nodiscard]] auto runtime_reference_target(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_read_reference(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>;
    void runtime_write_reference(const RuntimeRef<StorageCell> &cell, const RuntimeRef<StorageCell> &nextValue);
    [[nodiscard]] auto allocate_heap_cell(const RuntimeRef<StorageCell> &value, const Str &debugName) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto make_runtime_module(const NGSymbols &symbols = nullptr) -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_is_module_value(const RuntimeRef<StorageCell> &value) -> bool;
    [[nodiscard]] auto runtime_module_slots(const RuntimeRef<StorageCell> &value) -> Vec<RuntimeRef<StorageCell>>;
    [[nodiscard]] auto runtime_module_object_slots(const RuntimeRef<StorageCell> &value)
        -> Map<Str, RuntimeRef<StorageCell>>;
    [[nodiscard]] auto runtime_module_slot_named(const RuntimeRef<StorageCell> &value, const Str &name)
        -> RuntimeRef<StorageCell>;
    [[nodiscard]] auto runtime_module_functions(const RuntimeRef<StorageCell> &value) -> Map<Str, NGCallable>;
    [[nodiscard]] auto runtime_module_types(const RuntimeRef<StorageCell> &value) -> Map<Str, RuntimeRef<NGType>>;
    [[nodiscard]] auto runtime_module_native_functions(const RuntimeRef<StorageCell> &value) -> Map<Str, NGCallable>;
    [[nodiscard]] auto runtime_module_imports(const RuntimeRef<StorageCell> &value) -> Set<Str>;
    [[nodiscard]] auto runtime_module_exports(const RuntimeRef<StorageCell> &value) -> Set<Str>;
    void runtime_module_set_native_function(const RuntimeRef<StorageCell> &value, const Str &name, NGCallable handler);
    [[nodiscard]] auto runtime_module_type_named(const RuntimeRef<StorageCell> &value, const Str &name) -> RuntimeRef<NGType>;
    void runtime_module_set_native_state(const RuntimeRef<StorageCell> &value, Str name, std::shared_ptr<void> state);
    [[nodiscard]] auto runtime_module_get_native_state(const RuntimeRef<StorageCell> &value, const Str &name)
        -> std::shared_ptr<void>;
    void runtime_module_clear_native_state(const RuntimeRef<StorageCell> &value, const Str &name);
    [[nodiscard]] auto enumerate_symbol_roots(const NGSymbols &symbols) -> GCRootSet;
    [[nodiscard]] auto register_gc_root_provider(GCRootProvider provider) -> size_t;
    void unregister_gc_root_provider(size_t providerId);
    void collect_managed_heap();
    [[nodiscard]] auto managed_heap_size() -> size_t;
} // namespace NG::runtime
