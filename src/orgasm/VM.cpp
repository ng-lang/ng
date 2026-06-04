#include <orgasm/vm.hpp>
#include <orgasm/compiler.hpp>
#include <algorithm>
#include <bit>
#include <functional>
#include <iostream>
#include <intp/runtime_numerals.hpp>
#include <cstring>
#include <limits>
#include <module.hpp>
#include <runtime/array_layout_access.hpp>
#include <runtime/index_layout_access.hpp>
#include <runtime/struct_layout_access.hpp>
#include <runtime/tuple_layout_access.hpp>
#include <runtime/value_access.hpp>
#include <runtime/value_ops.hpp>

namespace NG::orgasm
{
    using namespace NG::runtime::ops;
    namespace
    {
        constexpr uint16_t SWITCH_DEFAULT_TAG = std::numeric_limits<uint16_t>::max();

        template <typename UInt>
        auto read_le_bytes(const Vec<uint8_t> &code, size_t &ip) -> UInt
        {
            UInt value = 0;
            for (size_t i = 0; i < sizeof(UInt); ++i)
            {
                value |= static_cast<UInt>(code[ip++]) << (i * 8U);
            }
            return value;
        }

        auto read_byte_checked(const Vec<uint8_t> &code, size_t &ip) -> uint8_t
        {
            if (ip >= code.size())
            {
                throw RuntimeException("VM error: unexpected end of bytecode");
            }
            return code[ip++];
        }

        template <typename UInt>
        auto read_le_bytes_checked(const Vec<uint8_t> &code, size_t &ip) -> UInt
        {
            if (ip + sizeof(UInt) > code.size())
            {
                throw RuntimeException("VM error: unexpected end of bytecode reading " +
                                       std::to_string(sizeof(UInt)) + " bytes");
            }
            UInt value = 0;
            for (size_t i = 0; i < sizeof(UInt); ++i)
            {
                value |= static_cast<UInt>(code[ip++]) << (i * 8U);
            }
            return value;
        }

        // Clone a value slot for stack operations.
        auto clone_value_slot(const RuntimeRef<StorageCell> &source, const Str &name = "stack") -> RuntimeRef<StorageCell>
        {
            return clone_runtime_storage_cell(source, StorageClass::TEMPORARY, name);
        }

        // Move a slot: copy value to a new temporary cell, mark original as moved.
        auto move_slot(const RuntimeRef<StorageCell> &source) -> RuntimeRef<StorageCell>
        {
            ensure_usable_cell(source);
            auto moved = make_storage_cell(source->layout, StorageClass::TEMPORARY, "stack", source->runtimeType);
            runtime_copy_storage_cell(moved, source);
            mark_moved_storage_cell(source);
            return moved;
        }

        auto ensure_slot(Vec<RuntimeRef<StorageCell>> &slots, size_t index, const Str &prefix,
                         StorageClass storageClass = StorageClass::FRAME) -> RuntimeRef<StorageCell>
        {
            if (index >= slots.size())
            {
                slots.resize(index + 1);
            }
            if (!slots[index])
            {
                slots[index] = unit_cell(storageClass);
                slots[index]->name = prefix + std::to_string(index);
            }
            return slots[index];
        }

        void append_slot_roots(GCRootSet &roots, const Vec<RuntimeRef<StorageCell>> &slots)
        {
            for (const auto &slot : slots)
            {
                if (slot)
                {
                    roots.cells.push_back(slot);
                }
            }
        }

        auto drop_target_for_cell(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
        {
            if (!cell || runtime_cell_is_moved(cell) || !runtime_cell_has_value(cell))
            {
                return nullptr;
            }
            if (runtime_is_trait_object_ref(cell))
            {
                return nullptr;
            }
            if (runtime_is_reference_value(cell))
            {
                return nullptr;
            }
            return cell;
        }
    }

    void VM::register_native_raw(const Str &name, NativeFunction func)
    {
        native_functions[name] = std::move(func);
    }

    auto VM::run(const BytecodeModule &module) -> RuntimeRef<StorageCell>
    {
        current_module = &module;
        root_symbols = makert<RuntimeSymbolTable>();
        auto gcRootProviderId = register_gc_root_provider([this]() {
            auto roots = enumerate_symbol_roots(root_symbols);
            append_slot_roots(roots, globals);
            append_slot_roots(roots, stack);
            for (const auto &frame : call_stack)
            {
                append_slot_roots(roots, frame.locals);
            }
            return roots;
        });
        struct RootProviderGuard
        {
            size_t id;
            ~RootProviderGuard() { unregister_gc_root_provider(id); }
        } rootProviderGuard{gcRootProviderId};
        gcFinalizerId = register_gc_finalizer([this, &module](const RuntimeRef<StorageCell> &cell) {
            std::function<void(const RuntimeRef<StorageCell> &)> finalizeCell;
            finalizeCell = [this, &module, &finalizeCell](const RuntimeRef<StorageCell> &targetCell) {
                if (!targetCell || runtime_cell_is_moved(targetCell) || !runtime_cell_has_value(targetCell) ||
                    !targetCell->dropArmed || targetCell->lifecycleDropped || targetCell->dropInProgress)
                {
                    return;
                }
                auto type = runtime_value_type(targetCell);
                if (!type)
                {
                    return;
                }
                auto dropChildren = [&]() {
                    for (const auto &slot : runtime_cell_slot_refs(targetCell))
                    {
                        finalizeCell(slot);
                    }
                    for (const auto &[_, slot] : runtime_cell_named_slot_refs(targetCell))
                    {
                        finalizeCell(slot);
                    }
                };
                if (type->dropCellHandler)
                {
                    type->dropCellHandler(targetCell);
                    targetCell->lifecycleDropped = true;
                    targetCell->dropArmed = false;
                    dropChildren();
                    return;
                }
                auto dropIt = std::find_if(module.functions.begin(), module.functions.end(), [&](const Function &fun) {
                    return fun.name == type->name + ".Drop::drop";
                });
                if (dropIt == module.functions.end())
                {
                    targetCell->lifecycleDropped = true;
                    targetCell->dropArmed = false;
                    dropChildren();
                    return;
                }
                targetCell->dropInProgress = true;
                try
                {
                    execute_slots(module, *dropIt, {make_runtime_reference_cell(targetCell, "arg:self")});
                    targetCell->lifecycleDropped = true;
                    targetCell->dropArmed = false;
                    dropChildren();
                    targetCell->dropInProgress = false;
                }
                catch (...)
                {
                    targetCell->dropInProgress = false;
                    throw;
                }
            };
            finalizeCell(cell);
        });
        struct FinalizerGuard
        {
            size_t &id;
            ~FinalizerGuard()
            {
                if (id != 0)
                {
                    unregister_gc_finalizer(id);
                    id = 0;
                }
            }
        } finalizerGuard{gcFinalizerId};
        // Size globals dynamically based on module needs
        size_t maxGlobal = 0;
        for (const auto &fun : module.functions) {
            for (size_t i = 0; i < fun.code.size(); ++i) {
                OpCode op = static_cast<OpCode>(fun.code[i]);
                if (op == OpCode::STORE_GLOBAL || op == OpCode::LOAD_GLOBAL) {
                    if (i + 2 < fun.code.size()) {
                        uint16_t idx = static_cast<uint16_t>(fun.code[i + 1]) | (static_cast<uint16_t>(fun.code[i + 2]) << 8);
                        maxGlobal = std::max(maxGlobal, static_cast<size_t>(idx) + 1);
                    }
                }
            }
        }
        globals.resize(std::max(maxGlobal, size_t{1}));
        for (size_t i = 0; i < globals.size(); ++i)
        {
            ensure_slot(globals, i, "global:", StorageClass::GLOBAL);
        }
        
        // Register built-ins
        root_symbols->functions["not"] = [](const NGSelf &, const NGEnv &,
                                            const NGArgs &args) -> RuntimeRef<StorageCell> {
            if (args.empty()) throw RuntimeException("not expects 1 arg");
            return make_runtime_boolean(!runtime_value_bool(args[0]));
        };

        for (const auto &type : module.types) {
            auto ngType = makert<NGType>();
            ngType->name = type.name;
            ngType->properties = type.properties;
            if (std::ranges::find(type.derivedTraits, Str{"Clone"}) != type.derivedTraits.end())
            {
                NGCallable cloneMember = [](const NGSelf &self, const NGEnv &, const NGArgs &) -> RuntimeRef<StorageCell> {
                    if (!self) return unit_cell();
                    return clone_runtime_storage_cell(self, StorageClass::TEMPORARY, "clone");
                };
                ngType->memberFunctions["Clone::clone"] = cloneMember;
                ngType->memberFunctions["clone"] = std::move(cloneMember);
            }
            root_types[type.name] = ngType;
            root_symbols->types[type.name] = ngType;
        }

        if (!module.functions.empty())
        {
            const Function* target = &module.functions[0];
            if (auto it = std::find_if(module.functions.begin(), module.functions.end(),
                                       [](const Function &fun) { return fun.name == "main"; });
                it != module.functions.end())
            {
                target = &(*it);
            }
            if (target->name != "__start__" && module.functions[0].name == "__start__") {
                execute_slots(module, module.functions[0], {});
            }
            return execute_slots(module, *target, {});
        }
        return unit_cell();
    }

    void VM::push_frame(const BytecodeModule &module, const Function &fun,
                        const Vec<RuntimeRef<StorageCell>> &args)
    {
        Frame frame;
        frame.module = &module;
        frame.function = &fun;
        frame.ip = 0;
        frame.locals.resize(std::max(static_cast<int32_t>(fun.num_locals), fun.num_params));
        for (size_t i = 0; i < frame.locals.size(); ++i)
        {
            ensure_slot(frame.locals, i, "local:");
        }
        for (size_t i = 0; i < args.size(); ++i)
        {
            auto target = ensure_slot(frame.locals, i, "param:");
            runtime_copy_storage_cell(target, clone_value_slot(args[i], "param:"));
        }
        
        call_stack.push_back(std::move(frame));
    }

    auto VM::execute_slots(const BytecodeModule &module, const Function &fun,
                           const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell>
    {
        const auto baseFrameDepth = call_stack.size();
        push_frame(module, fun, args);
        struct CallStackGuard
        {
            Vec<Frame> &frames;
            size_t baseDepth;
            ~CallStackGuard()
            {
                if (frames.size() > baseDepth)
                {
                    frames.resize(baseDepth);
                }
            }
        } callStackGuard{call_stack, baseFrameDepth};

        auto push_slot_copy = [this](const RuntimeRef<StorageCell> &source, const Str &name = "stack") -> RuntimeRef<StorageCell>
        {
            auto slot = clone_value_slot(source, name);
            stack.push_back(slot);
            return slot;
        };
        auto pop_slot = [this]() -> RuntimeRef<StorageCell>
        {
            if (stack.empty()) throw RuntimeException("Stack underflow");
            auto val = stack.back();
            stack.pop_back();
            return val;
        };
        auto access_target_slot = [](const RuntimeRef<StorageCell> &slot) -> RuntimeRef<StorageCell>
        {
            auto current = slot;
            while (current)
            {
                if (runtime_is_trait_object_ref(current))
                {
                    return current;
                }
                auto type = runtime_value_type(current);
                if (!type || type->name != "ref")
                {
                    return current;
                }
                auto target = runtime_reference_target(current);
                if (!target)
                {
                    throw RuntimeException("Cannot dereference non-reference value");
                }
                current = target;
            }
            return nullptr;
        };
        auto push_binary_result = [this](const RuntimeRef<StorageCell> &left, RuntimeBinaryOperator op,
                                         const RuntimeRef<StorageCell> &right) {
            auto result = dispatch_binary_operator(left, op, right);
            if (!result)
            {
                throw RuntimeException("Unsupported binary operator");
            }
            stack.push_back(result);
        };
        auto function_index_by_name = [](const BytecodeModule &lookupModule, const Str &name) -> int32_t {
            for (size_t i = 0; i < lookupModule.functions.size(); ++i)
            {
                if (lookupModule.functions[i].name == name)
                {
                    return static_cast<int32_t>(i);
                }
            }
            return -1;
        };
        auto type_dispatch_name_candidates = [](const Str &typeName) {
            Vec<Str> candidates{typeName};
            auto genericStart = typeName.find('<');
            if (genericStart != Str::npos)
            {
                auto bareName = typeName.substr(0, genericStart);
                if (bareName != typeName)
                {
                    candidates.push_back(std::move(bareName));
                }
            }
            return candidates;
        };
        auto function_base_name_matches = [](const Str &functionName, const Str &baseName) {
            if (functionName == baseName)
            {
                return true;
            }
            if (functionName.find(std::to_string(baseName.size()) + ":" + baseName) != Str::npos)
            {
                return true;
            }
            auto marker = Str{":"} + baseName;
            auto pos = functionName.find(marker);
            while (pos != Str::npos)
            {
                auto segmentStart = pos;
                auto lengthStart = functionName.rfind(':', segmentStart - 1);
                if (lengthStart != Str::npos)
                {
                    auto digitStart = functionName.rfind(':', lengthStart - 1);
                    digitStart = digitStart == Str::npos ? 0 : digitStart + 1;
                    try
                    {
                        auto length = static_cast<size_t>(std::stoul(functionName.substr(digitStart, lengthStart - digitStart)));
                        if (length == baseName.size())
                        {
                            return true;
                        }
                    }
                    catch (...)
                    {
                    }
                }
                pos = functionName.find(marker, pos + 1);
            }
            return false;
        };
        std::function<void(const BytecodeModule &, const RuntimeRef<StorageCell> &)> drop_cell_if_needed;
        drop_cell_if_needed = [this, &function_index_by_name, &drop_cell_if_needed](
                                  const BytecodeModule &dropModule, const RuntimeRef<StorageCell> &cell) {
            auto target = drop_target_for_cell(cell);
            if (!target || runtime_cell_is_moved(target) || !runtime_cell_has_value(target) ||
                !target->dropArmed || target->lifecycleDropped || target->dropInProgress)
            {
                return;
            }
            auto type = runtime_value_type(target);
            if (!type)
            {
                return;
            }
            auto dropChildren = [&]() {
                for (const auto &slot : runtime_cell_slot_refs(target))
                {
                    drop_cell_if_needed(dropModule, slot);
                }
                for (const auto &[_, slot] : runtime_cell_named_slot_refs(target))
                {
                    drop_cell_if_needed(dropModule, slot);
                }
            };
            if (type->dropCellHandler)
            {
                type->dropCellHandler(target);
                target->lifecycleDropped = true;
                target->dropArmed = false;
                dropChildren();
                return;
            }
            auto dropIndex = function_index_by_name(dropModule, type->name + ".Drop::drop");
            if (dropIndex < 0)
            {
                if (!type->memberFunctions.contains("Drop::drop"))
                {
                    target->lifecycleDropped = true;
                    target->dropArmed = false;
                    dropChildren();
                    return;
                }
                target->dropInProgress = true;
                try
                {
                    (void)runtime_value_respond_slot(target, "Drop::drop", make_runtime_env(root_symbols), {});
                    target->lifecycleDropped = true;
                    target->dropArmed = false;
                    dropChildren();
                    target->dropInProgress = false;
                }
                catch (...)
                {
                    target->dropInProgress = false;
                    throw;
                }
                return;
            }
            target->dropInProgress = true;
            try
            {
                execute_slots(dropModule, dropModule.functions[static_cast<size_t>(dropIndex)],
                              {make_runtime_reference_cell(target, "arg:self")});
                target->lifecycleDropped = true;
                target->dropArmed = false;
                dropChildren();
                target->dropInProgress = false;
            }
            catch (...)
            {
                target->dropInProgress = false;
                throw;
            }
        };
        auto drop_frame_slots = [&drop_cell_if_needed](const Frame &frameToDrop) {
            if (!frameToDrop.module)
            {
                return;
            }
            for (auto it = frameToDrop.locals.rbegin(); it != frameToDrop.locals.rend(); ++it)
            {
                drop_cell_if_needed(*frameToDrop.module, *it);
            }
        };
        auto sequence_slots = [&](const BytecodeModule &lookupModule, const RuntimeRef<StorageCell> &sequence) {
            try
            {
                return runtime_builtin_sequence_slots(sequence);
            }
            catch (const SequenceCompatibilityException &)
            {
            }

            auto target = access_target_slot(sequence);
            auto type = runtime_value_type(runtime_is_trait_object_ref(target) ? runtime_trait_object_target(target) : target);
            if (!type)
            {
                throw SequenceCompatibilityException();
            }

            auto callSequenceMember = [&](const Str &member, const Vec<RuntimeRef<StorageCell>> &args) {
                if (member == "get")
                {
                    for (const auto &function : lookupModule.functions)
                    {
                        if ((!function.name.starts_with("$NG") && function.name.find('.') != Str::npos) ||
                            !function_base_name_matches(function.name, member))
                        {
                            continue;
                        }
                        Vec<RuntimeRef<StorageCell>> callArgs;
                        callArgs.reserve(args.size() + 1);
                        callArgs.push_back(make_runtime_reference_cell(target, "arg:self"));
                        for (const auto &arg : args)
                        {
                            callArgs.push_back(clone_value_slot(arg, "arg:" + std::to_string(callArgs.size())));
                        }
                        return execute_slots(lookupModule, function, callArgs);
                    }
                }
                Vec<Str> memberCandidates{member, "Sequence::" + member};
                for (const auto &typeName : type_dispatch_name_candidates(type->name))
                {
                    for (const auto &candidateMember : memberCandidates)
                    {
                        auto functionIndex = function_index_by_name(lookupModule, typeName + "." + candidateMember);
                        if (functionIndex < 0)
                        {
                            continue;
                        }
                        const auto &function = lookupModule.functions[static_cast<size_t>(functionIndex)];
                        Vec<RuntimeRef<StorageCell>> callArgs;
                        callArgs.reserve(args.size() + 1);
                        auto selfSlot = function.explicit_receiver
                                            ? make_runtime_reference_cell(target, "arg:self")
                                            : clone_value_slot(target, "arg:self");
                        callArgs.push_back(selfSlot);
                        for (const auto &arg : args)
                        {
                            callArgs.push_back(clone_value_slot(arg, "arg:" + std::to_string(callArgs.size())));
                        }
                        return execute_slots(lookupModule, function, callArgs);
                    }
                }
                NGArgs nativeArgs;
                nativeArgs.reserve(args.size());
                for (const auto &arg : args)
                {
                    nativeArgs.push_back(clone_value_slot(arg, "arg:" + std::to_string(nativeArgs.size())));
                }
                return runtime_value_respond_slot(target, member, make_runtime_env(root_symbols), nativeArgs);
            };

            auto sizeSlot = callSequenceMember("size", {});
            auto count = read_numeric_cell_as<int64_t>(sizeSlot);
            if (count < 0)
            {
                throw RuntimeException("Sequence size cannot be negative");
            }
            Vec<RuntimeRef<StorageCell>> result;
            result.reserve(static_cast<size_t>(count));
            for (int64_t i = 0; i < count; ++i)
            {
                result.push_back(callSequenceMember("get",
                                                    {numeral_cell_from_value<int32_t>(static_cast<int32_t>(i))}));
            }
            return result;
        };

        while (call_stack.size() > baseFrameDepth)
        {
            auto &frame = call_stack.back();
            const auto &activeModule = *frame.module;
            const auto &activeFunction = *frame.function;
            current_module = &activeModule;
            const auto &code = activeFunction.code;

            if (frame.ip >= code.size())
            {
                call_stack.pop_back();
                auto result = unit_cell();
                if (call_stack.size() == baseFrameDepth)
                {
                    return result;
                }
                push_slot_copy(result);
                continue;
            }

            size_t &ip = frame.ip;
            OpCode op = static_cast<OpCode>(code[ip++]);

            auto read_u16 = [&code, &ip]() -> uint16_t
            {
                return read_le_bytes_checked<uint16_t>(code, ip);
            };

            try {
                switch (op)
                {
                // ── Stack operations ──────────────────────────────────────────
                                case OpCode::NOP:
                                    break;
                                case OpCode::PUSH_I8:
                                {
                                    int8_t val = static_cast<int8_t>(read_byte_checked(code, ip));
                                    push_slot_copy(numeral_cell_from_value<int8_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_I16:
                                {
                                    int16_t val = std::bit_cast<int16_t>(read_le_bytes_checked<uint16_t>(code, ip));
                                    push_slot_copy(numeral_cell_from_value<int16_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_I32:
                                {
                                    int32_t val = std::bit_cast<int32_t>(read_le_bytes_checked<uint32_t>(code, ip));
                                    push_slot_copy(numeral_cell_from_value<int32_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_I64:
                                {
                                    int64_t val = std::bit_cast<int64_t>(read_le_bytes_checked<uint64_t>(code, ip));
                                    push_slot_copy(numeral_cell_from_value<int64_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_U8:
                                {
                                    uint8_t val = read_byte_checked(code, ip);
                                    push_slot_copy(numeral_cell_from_value<uint8_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_U16:
                                {
                                    uint16_t val = read_le_bytes_checked<uint16_t>(code, ip);
                                    push_slot_copy(numeral_cell_from_value<uint16_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_U32:
                                {
                                    uint32_t val = read_le_bytes_checked<uint32_t>(code, ip);
                                    push_slot_copy(numeral_cell_from_value<uint32_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_U64:
                                {
                                    uint64_t val = read_le_bytes_checked<uint64_t>(code, ip);
                                    push_slot_copy(numeral_cell_from_value<uint64_t>(val));
                                    break;
                                }
                                case OpCode::PUSH_F32:
                                {
                                    float val = std::bit_cast<float>(read_le_bytes_checked<uint32_t>(code, ip));
                                    push_slot_copy(numeral_cell_from_value<float>(val));
                                    break;
                                }
                                case OpCode::PUSH_F64:
                                {
                                    double val = std::bit_cast<double>(read_le_bytes_checked<uint64_t>(code, ip));
                                    push_slot_copy(numeral_cell_from_value<double>(val));
                                    break;
                                }
                                // ── Arithmetic ──────────────────────────────────────────────
                case OpCode::ADD: {
                                    auto b = pop_slot();
                                    auto a = pop_slot();
                                    try {
                                        push_binary_result(a, RuntimeBinaryOperator::Add, b);
                                    } catch (const std::exception &ex) {
                                        auto aType = runtime_value_type(a);
                                        auto bType = runtime_value_type(b);
                                        throw RuntimeException(Str(ex.what()) + " (ADD: " +
                                                               (aType ? aType->name : Str{"?"}) + " + " +
                                                               (bType ? bType->name : Str{"?"}) + ")");
                                    }
                                    break;
                                }
                                case OpCode::SUB: { auto b = pop_slot(); auto a = pop_slot(); push_binary_result(a, RuntimeBinaryOperator::Subtract, b); break; }
                                case OpCode::MUL: { auto b = pop_slot(); auto a = pop_slot(); push_binary_result(a, RuntimeBinaryOperator::Multiply, b); break; }
                                case OpCode::DIV: { auto b = pop_slot(); auto a = pop_slot(); push_binary_result(a, RuntimeBinaryOperator::Divide, b); break; }
                                case OpCode::MOD: {
                                auto b = pop_slot(); auto a = pop_slot();
                                try { push_binary_result(a, RuntimeBinaryOperator::Modulus, b); }
                                catch (const std::exception& ex) {
                                    auto aType = runtime_value_type(a);
                                    auto bType = runtime_value_type(b);
                                    throw RuntimeException(Str(ex.what()) + " (MOD: " +
                                                           (aType ? aType->name : Str{"?"}) + " % " +
                                                           (bType ? bType->name : Str{"?"}) + ")");
                                }
                                break;
                            }
                case OpCode::LOAD_STR:
                {
                    uint16_t idx = read_u16();
                    if (idx >= current_module->strings.size()) throw RuntimeException("VM error: LOAD_STR index out of bounds");
                    stack.push_back(make_runtime_string(current_module->strings[idx]));
                    break;
                }
                case OpCode::LOAD_CONST:
                {
                    uint16_t idx = read_u16();
                    if (idx >= current_module->constants.size()) throw RuntimeException("VM error: LOAD_CONST index out of bounds");
                    push_slot_copy(numeral_cell_from_value<int64_t>(current_module->constants[idx]));
                    break;
                }
                // ── Comparison ───────────────────────────────────────────────
                case OpCode::EQ: { auto b = pop_slot(); auto a = pop_slot(); stack.push_back(make_runtime_boolean(value_equals(a, b))); break; }
                case OpCode::LT: { auto b = pop_slot(); auto a = pop_slot(); stack.push_back(make_runtime_boolean(value_less_than(a, b))); break; }
                case OpCode::GT: { auto b = pop_slot(); auto a = pop_slot(); stack.push_back(make_runtime_boolean(value_greater_than(a, b))); break; }
                case OpCode::PUSH_BOOL: stack.push_back(make_runtime_boolean(read_byte_checked(code, ip) != 0)); break;
                case OpCode::NOT: { auto val = pop_slot(); stack.push_back(make_runtime_boolean(!runtime_value_bool(val))); break; }
                case OpCode::INSTANCE_OF:
                {
                    uint16_t typeNameIdx = read_u16();
                    if (typeNameIdx >= current_module->strings.size()) throw RuntimeException("VM error: INSTANCE_OF string index out of bounds");
                    Str typeName = current_module->strings[typeNameIdx];
                    auto val = access_target_slot(pop_slot());
                    bool result = false;
                    if (auto valueType = runtime_value_type(val); valueType && valueType->name == typeName) result = true;
                    stack.push_back(make_runtime_boolean(result));
                    break;
                }
                case OpCode::NEG: {
                    auto val = pop_slot();
                    stack.push_back(negate_numeric_cell(val));
                    break;
                }
                case OpCode::RETURN: {
                    auto res = stack.empty() ? unit_cell() : pop_slot();
                    drop_frame_slots(call_stack.back());
                    call_stack.pop_back();
                    if (call_stack.size() == baseFrameDepth)
                    {
                        return res;
                    }
                    push_slot_copy(res);
                    break;
                }
                // ── Data access ──────────────────────────────────────────────
                case OpCode::LOAD_LOCAL: { push_slot_copy(ensure_slot(frame.locals, read_u16(), "local:")); break; }
                case OpCode::LOAD_PARAM: { push_slot_copy(ensure_slot(frame.locals, read_u16(), "param:")); break; }
                case OpCode::STORE_LOCAL:
                {
                    uint16_t idx = read_u16();
                    auto target = ensure_slot(frame.locals, idx, "local:");
                    drop_cell_if_needed(activeModule, target);
                    runtime_copy_storage_cell(target, stack.back());
                    break;
                }
                case OpCode::LOAD_GLOBAL: { push_slot_copy(ensure_slot(globals, read_u16(), "global:", StorageClass::GLOBAL)); break; }
                case OpCode::STORE_GLOBAL:
                {
                    uint16_t idx = read_u16();
                    auto target = ensure_slot(globals, idx, "global:", StorageClass::GLOBAL);
                    drop_cell_if_needed(activeModule, target);
                    runtime_copy_storage_cell(target, stack.back());
                    break;
                }
                case OpCode::MAKE_LOCAL_REF:
                {
                    uint16_t idx = read_u16();
                    push_slot_copy(make_runtime_reference_cell(ensure_slot(frame.locals, idx, "local:"),
                                                               "local:" + std::to_string(idx)));
                    break;
                }
                case OpCode::MAKE_GLOBAL_REF:
                {
                    uint16_t idx = read_u16();
                    push_slot_copy(make_runtime_reference_cell(ensure_slot(globals, idx, "global:", StorageClass::GLOBAL), "global:" + std::to_string(idx)));
                    break;
                }
                case OpCode::MAKE_PROPERTY_REF:
                {
                    uint16_t fieldIdx = read_u16();
                    auto target = pop_slot();
                    auto resolvePropertySlot = [fieldIdx](const RuntimeRef<StorageCell> &structural) -> RuntimeRef<StorageCell> {
                        auto slot = runtime_cell_slot_ref(structural, fieldIdx);
                        if (!slot) throw RuntimeException("Property reference is not slot-backed");
                        return slot;
                    };
                    auto structural = access_target_slot(target);
                    push_slot_copy(make_runtime_reference_cell(resolvePropertySlot(structural), "field:" + std::to_string(fieldIdx)));
                    break;
                }
                case OpCode::MAKE_PROPERTY_STR_REF:
                {
                    uint16_t nameIdx = read_u16();
                    Str propName = current_module->strings[nameIdx];
                    auto target = pop_slot();
                    auto makePropertyRef = [&propName](const RuntimeRef<StorageCell> &structural) {
                        if (auto index = structural_field_index(structural, propName))
                        {
                            if (auto slot = runtime_cell_slot_ref(structural, *index))
                            {
                                return make_runtime_reference_cell(slot, "property:" + propName);
                            }
                        }
                        if (auto slot = structural_member_slot(structural, propName))
                        {
                            return make_runtime_reference_cell(slot, "property:" + propName);
                        }
                        throw RuntimeException("Property reference is not slot-backed: " + propName);
                    };
                    auto structural = access_target_slot(target);
                    push_slot_copy(makePropertyRef(structural));
                    break;
                }
                case OpCode::MAKE_INDEX_REF:
                {
                    auto index = pop_slot();
                    auto target = pop_slot();
                    auto makeIndexRef = [&index](const RuntimeRef<StorageCell> &container) {
                        auto idx = read_numeric_cell_as<int32_t>(index);
                        if (runtime_is_tuple_value(container))
                        {
                            if (auto slot = runtime_cell_slot_ref(container, static_cast<size_t>(idx)))
                            {
                                return make_runtime_reference_cell(slot, "index");
                            }
                            throw RuntimeException("Tuple index reference is not slot-backed");
                        }
                        if (runtime_is_array_value(container))
                        {
                            if (auto slot = runtime_cell_slot_ref(container, static_cast<size_t>(idx)))
                            {
                                return make_runtime_reference_cell(slot, "index");
                            }
                            throw RuntimeException("Array index reference is not slot-backed");
                        }
                        throw RuntimeException("Cannot reference index on non-indexable value");
                    };
                    push_slot_copy(makeIndexRef(access_target_slot(target)));
                    break;
                }
                case OpCode::MAKE_TRAIT_REF:
                {
                    uint16_t traitIdx = read_u16();
                    if (traitIdx >= current_module->strings.size()) throw RuntimeException("VM error: MAKE_TRAIT_REF string index out of bounds");
                    auto targetRef = pop_slot();
                    if (runtime_is_trait_object_ref(targetRef))
                    {
                        push_slot_copy(make_runtime_trait_object_ref(runtime_trait_object_target_ref(targetRef),
                                                                     current_module->strings[traitIdx], "trait-ref"));
                        break;
                    }
                    if (!runtime_is_reference_value(targetRef))
                    {
                        throw RuntimeException("Trait object requires a reference value");
                    }
                    push_slot_copy(make_runtime_trait_object_ref(targetRef, current_module->strings[traitIdx], "trait-ref"));
                    break;
                }
                case OpCode::LOAD_REF:
                {
                    auto reference = pop_slot();
                    if (!runtime_is_reference_value(reference)) throw RuntimeException("Cannot dereference non-reference value");
                    auto target = runtime_reference_target(reference);
                    if (!target) throw RuntimeException("Cannot dereference non-reference value");
                    ensure_usable_cell(target);
                    push_slot_copy(target);
                    break;
                }
                case OpCode::STORE_REF:
                {
                    auto value = pop_slot();
                    auto reference = pop_slot();
                    if (!runtime_is_reference_value(reference)) throw RuntimeException("Cannot assign through non-reference value");
                    auto target = runtime_reference_target(reference);
                    if (!target) throw RuntimeException("Cannot assign through non-reference value");
                    drop_cell_if_needed(activeModule, target);
                    runtime_copy_storage_cell(target, value);
                    push_slot_copy(value);
                    break;
                }
                case OpCode::MOVE_LOCAL:
                {
                    uint16_t idx = read_u16();
                    stack.push_back(move_slot(ensure_slot(frame.locals, idx, "local:")));
                    break;
                }
                case OpCode::MOVE_GLOBAL:
                {
                    uint16_t idx = read_u16();
                    stack.push_back(move_slot(ensure_slot(globals, idx, "global:", StorageClass::GLOBAL)));
                    break;
                }
                case OpCode::MOVE_REF:
                {
                    auto reference = pop_slot();
                    auto slot = runtime_reference_target(reference);
                    if (!slot) throw RuntimeException("Cannot move from non-reference value");
                    stack.push_back(move_slot(slot));
                    break;
                }
                case OpCode::GET_TUPLE_ITEM:
                {
                    auto idxObj = pop_slot();
                    auto tupleObj = access_target_slot(pop_slot());
                    auto idx = read_numeric_cell_as<int32_t>(idxObj);
                    if (!runtime_is_tuple_value(tupleObj)) throw RuntimeException("Not a tuple");
                    if (auto slot = runtime_cell_slot_ref(tupleObj, static_cast<size_t>(idx)))
                    {
                        push_slot_copy(slot);
                    }
                    else
                    {
                        stack.push_back(unit_cell());
                    }
                    break;
                }
                            case OpCode::POP: pop_slot(); break;
                            case OpCode::DUP:
                            {
                                if (stack.empty()) throw RuntimeException("VM error: DUP on empty stack");
                                push_slot_copy(stack.back());
                                break;
                            }
                            case OpCode::PUSH_UNIT: stack.push_back(unit_cell()); break;
                // ── Control flow ──────────────────────────────────────────────
                case OpCode::CALL:
                {
                    uint16_t funIndex = read_u16();
                    if (funIndex >= current_module->functions.size()) throw RuntimeException("VM error: CALL function index out of bounds");
                    uint16_t numArgs = read_u16();
                    Vec<RuntimeRef<StorageCell>> callArgs;
                    callArgs.reserve(numArgs); for (int i = 0; i < numArgs; ++i) callArgs.push_back(pop_slot()); std::reverse(callArgs.begin(), callArgs.end());
                    push_frame(*current_module, current_module->functions[funIndex], callArgs);
                    break;
                }
                case OpCode::CALL_IMPORT:
                {
                    uint16_t importIdx = read_u16();
                    if (importIdx >= current_module->imports.size()) throw RuntimeException("VM error: CALL_IMPORT index out of bounds");
                    uint16_t numArgs = read_u16();
                    auto &imp = current_module->imports[importIdx];
                    
                    auto &registry = NG::module::get_module_registry();
                    auto moduleInfo = registry.queryModuleById(imp.moduleName);
                    if (!moduleInfo)
                    {
                        auto id = NG::module::module_id_from_name(imp.moduleName);
                        NG::module::FileBasedExternalModuleLoader loader{modulePaths};
                        moduleInfo = loader.load(id.pathSegments);
                        if (moduleInfo)
                        {
                            registry.addModuleInfo(moduleInfo);
                        }
                    }
                    if (!moduleInfo) throw RuntimeException("Module not found: " + imp.moduleName);

                    if (!moduleInfo->bytecodeModule && moduleInfo->moduleAst)
                    {
                        static thread_local Set<Str> compilingModules;
                        struct CompileGuard
                        {
                            Set<Str> &ids;
                            Str moduleId;

                            CompileGuard(Set<Str> &ids, Str moduleId) : ids(ids), moduleId(std::move(moduleId))
                            {
                                if (!this->ids.insert(this->moduleId).second)
                                {
                                    throw RuntimeException("Cyclic source module compilation detected: " + this->moduleId);
                                }
                            }

                            ~CompileGuard()
                            {
                                ids.erase(moduleId);
                            }
                        };
                        CompileGuard guard{compilingModules, imp.moduleName};
                        auto compileUnit = dynamic_ast_cast<NG::ast::CompileUnit>(moduleInfo->moduleAst);
                        if (!compileUnit)
                        {
                            throw RuntimeException("Failed to cast AST to CompileUnit for module " + imp.moduleName);
                        }
                        Compiler compiler{modulePaths};
                        auto bytecode = compiler.compile(compileUnit);
                        moduleInfo->bytecodeModule = std::make_shared<BytecodeModule>(std::move(bytecode));
                        registry.addModuleInfo(moduleInfo);
                    }
                    
                    if (moduleInfo->bytecodeModule) {
                        auto &otherModule = *moduleInfo->bytecodeModule;
                        int32_t funIdx = -1;
                        if (otherModule.exports.contains(imp.symbolName)) {
                            funIdx = otherModule.exports.at(imp.symbolName);
                        } else {
                             for(size_t i=0; i<otherModule.functions.size(); ++i) {
                                 if(otherModule.functions[i].name == imp.symbolName) {
                                     funIdx = (int32_t)i;
                                     break;
                                 }
                             }
                        }
                        
                        if (funIdx == -1) throw RuntimeException("Function " + imp.symbolName + " not found in module " + imp.moduleName);
                        
                        Vec<RuntimeRef<StorageCell>> callArgs;
                        callArgs.reserve(numArgs); for (int i = 0; i < numArgs; ++i) callArgs.push_back(pop_slot()); std::reverse(callArgs.begin(), callArgs.end());
                        
                        push_frame(otherModule, otherModule.functions[funIdx], callArgs);
                    } else {
                        // Try native function fallback
                        Vec<RuntimeRef<StorageCell>> callArgs;
                        callArgs.reserve(numArgs); for (int i = 0; i < numArgs; ++i) callArgs.push_back(pop_slot()); std::reverse(callArgs.begin(), callArgs.end());
                        if (native_functions.contains(imp.symbolName)) {
                            stack.push_back(native_functions[imp.symbolName](callArgs));
                        } else {
                            throw RuntimeException("Module " + imp.moduleName + " is not a bytecode module and no native function found for " + imp.symbolName);
                        }
                    }
                    break;
                }
                // ── Object/Array/Tuple ────────────────────────────────────────
                case OpCode::GET_PROPERTY:
            {
                uint16_t fieldIdx = read_u16();
                        auto target = access_target_slot(pop_slot());
                        if (runtime_is_structural_value(target)) {
                            if (auto slot = runtime_structural_field_slot(target, fieldIdx)) {
                                push_slot_copy(slot);
                            } else {
                                throw RuntimeException("Field index out of bounds: " + std::to_string(fieldIdx));
                            }
                } else {
                    throw RuntimeException("Cannot get property from non-object");
                }
                break;
            }
                case OpCode::SET_PROPERTY:
            {
                uint16_t fieldIdx = read_u16();
                auto val = pop_slot();
                auto target = access_target_slot(pop_slot());
                if (runtime_is_structural_value(target)) {
                    if (auto slot = runtime_structural_field_slot(target, fieldIdx)) {
                        drop_cell_if_needed(activeModule, slot);
                        runtime_copy_storage_cell(slot, val);
                    } else {
                        throw RuntimeException("Field index out of bounds: " + std::to_string(fieldIdx));
                    }
                } else {
                    throw RuntimeException("Cannot set property on non-object");
                }
                push_slot_copy(target);
                break;
            }
                case OpCode::GET_PROPERTY_STR:
            {
                uint16_t nameIdx = read_u16();
                Str propName = current_module->strings[nameIdx];
                auto target = access_target_slot(pop_slot());
                if (runtime_is_tuple_value(target)) {
                    if (propName == "size") {
                        push_slot_copy(numeral_cell_from_value<uint32_t>(static_cast<uint32_t>(runtime_tuple_length(target))));
                    } else {
                        try {
                            auto slot = runtime_cell_slot_ref(target, std::stoul(propName));
                            if (!slot) {
                                throw RuntimeException("Tuple has no property: " + propName);
                            }
                            push_slot_copy(slot);
                        } catch (const std::exception &) {
                            throw RuntimeException("Tuple has no property: " + propName);
                        }
                    }
                } else if (runtime_is_structural_value(target)) {
                    if (auto index = runtime_structural_field_index(target, propName)) {
                        push_slot_copy(runtime_cell_slot_ref(target, *index));
                    } else if (auto slot = runtime_structural_property_slot(target, propName)) {
                        push_slot_copy(slot);
                    } else {
                        throw RuntimeException("Property not found: " + propName);
                    }
                } else {
                    throw RuntimeException("Cannot get property from non-object");
                }
                break;
            }
                case OpCode::SET_PROPERTY_STR:
            {
                uint16_t nameIdx = read_u16();
                Str propName = current_module->strings[nameIdx];
                auto val = pop_slot();
                auto target = access_target_slot(pop_slot());
                if (runtime_is_structural_value(target)) {
                    if (auto index = runtime_structural_field_index(target, propName)) {
                        auto slot = runtime_structural_field_slot(target, *index);
                        drop_cell_if_needed(activeModule, slot);
                        runtime_copy_storage_cell(slot, val);
                    } else {
                        auto slot = runtime_structural_property_slot_or_create(target, propName);
                        drop_cell_if_needed(activeModule, slot);
                        runtime_copy_storage_cell(slot, val);
                    }
                } else {
                    throw RuntimeException("Cannot set property on non-object");
                }
                push_slot_copy(val);
                break;
            }
                case OpCode::NEW_ARRAY:
                {
                    uint16_t num = read_u16(); Vec<RuntimeRef<StorageCell>> elems;
                    for (int i = 0; i < num; ++i) elems.insert(elems.begin(), pop_slot());
                    push_slot_copy(make_runtime_array_cell(elems));
                    break;
                }
                case OpCode::NEW_TUPLE:
                {
                    uint16_t num = read_u16(); Vec<RuntimeRef<StorageCell>> elems;
                    for (int i = 0; i < num; ++i) elems.insert(elems.begin(), pop_slot());
                    push_slot_copy(make_runtime_tuple_cell(elems));
                    break;
                }
                case OpCode::GET_INDEX:
                {
                    auto idx = pop_slot();
                    auto obj = access_target_slot(pop_slot());
                    push_slot_copy(runtime_index_slot(obj, idx));
                    break;
                }
                case OpCode::SET_INDEX:
                {
                    auto val = pop_slot();
                    auto idx = pop_slot();
                    auto obj = access_target_slot(pop_slot());
                    push_slot_copy(runtime_index_write(obj, idx, val));
                    break;
                }
                case OpCode::NEW_OBJECT:
                                {
                                    uint16_t typeStrIdx = read_u16();
                                    Str typeName = current_module->strings[typeStrIdx];
                                    uint16_t numFields = read_u16();

                                    Vec<RuntimeRef<StorageCell>> fields(static_cast<size_t>(numFields));
                                    for (int i = numFields - 1; i >= 0; --i)
                                    {
                                        fields[static_cast<size_t>(i)] = pop_slot();
                                    }

                                    RuntimeRef<NGType> objectType = nullptr;
                                    if (root_types.contains(typeName)) {
                                        objectType = root_types[typeName];
                                    } else {
                                        auto genericStart = typeName.find('<');
                                        if (genericStart != Str::npos) {
                                            auto baseName = typeName.substr(0, genericStart);
                                            if (root_types.contains(baseName)) {
                                                objectType = root_types[baseName];
                                            }
                                        }
                                    }
                                    if (objectType) {
                                        stack.push_back(allocate_heap_cell(
                                            make_runtime_structural_cell(objectType, fields), "heap:" + typeName));
                                        break;
                                    }

                                    bool foundVariant = false;
                                    for (const auto &type : current_module->types) {
                                        for (size_t variantIndex = 0; variantIndex < type.variants.size(); ++variantIndex) {
                                            const auto &variant = type.variants[variantIndex];
                                            if (variant.name != typeName) {
                                                continue;
                                            }

                                            stack.push_back(allocate_heap_cell(
                                                make_runtime_tagged_cell(type.name, variant.name, static_cast<int32_t>(variantIndex),
                                                                         fields, variant.payloadFields),
                                                "heap:" + typeName));
                                            foundVariant = true;
                                            break;
                                        }
                                        if (foundVariant) {
                                            break;
                                        }
                                    }

                                    if (!foundVariant) {
                                        throw RuntimeException("Unknown type for new object: " + typeName);
                                    }
                                    break;
                                }
                case OpCode::INVOKE_MEMBER:
                {
                    uint16_t nameIdx = read_u16();
                    if (nameIdx >= current_module->strings.size()) throw RuntimeException("VM error: INVOKE_MEMBER string index out of bounds");
                    uint16_t numArgs = read_u16();
                    Str memberName = current_module->strings[nameIdx];
                    Vec<RuntimeRef<StorageCell>> callArgs;
                    callArgs.reserve(numArgs); for (int i = 0; i < numArgs; ++i) callArgs.push_back(pop_slot()); std::reverse(callArgs.begin(), callArgs.end());
                    auto targetSlot = access_target_slot(pop_slot());
                    
                    auto dispatchTarget = runtime_is_trait_object_ref(targetSlot) ? runtime_trait_object_target(targetSlot) : targetSlot;
                    Str typeName = runtime_value_type(dispatchTarget) ? runtime_value_type(dispatchTarget)->name : "Object";
                    if (runtime_is_trait_object_ref(targetSlot) && memberName.find("::") == Str::npos)
                    {
                        memberName = runtime_trait_object_name(targetSlot) + "::" + memberName;
                    }
                    Str fullFunName = typeName + "." + memberName;
                    
                    int32_t funIdx = -1;
                    for (size_t i = 0; i < current_module->functions.size(); ++i) {
                        if (current_module->functions[i].name == fullFunName) { funIdx = static_cast<int32_t>(i); break; }
                    }
                    
                    if (funIdx != -1) {
                        auto selfSlot = current_module->functions[funIdx].explicit_receiver
                                            ? make_runtime_reference_cell(dispatchTarget, "arg:self")
                                            : clone_value_slot(dispatchTarget, "arg:self");
                        selfSlot->name = "arg:self";
                        callArgs.insert(callArgs.begin(), selfSlot);
                        push_frame(*current_module, current_module->functions[funIdx], callArgs);
                    } else {
                        NGArgs memberArgs;
                        memberArgs.reserve(callArgs.size());
                        for (const auto &slot : callArgs) {
                            memberArgs.push_back(clone_value_slot(slot, "arg:" + std::to_string(memberArgs.size())));
                        }
                        push_slot_copy(runtime_value_respond_slot(targetSlot, memberName, make_runtime_env(root_symbols), memberArgs));
                    }
                    break;
                }
                case OpCode::NEW_TUPLE_SPREAD:
                case OpCode::NEW_ARRAY_SPREAD:
                {
                    uint16_t num = read_u16();
                    Vec<uint8_t> flags(num);
                    for (int i = 0; i < num; ++i) flags[i] = read_byte_checked(code, ip);
                    
                    Vec<RuntimeRef<StorageCell>> segments;
                    for (int i = 0; i < num; ++i) segments.insert(segments.begin(), pop_slot());
                    
                    Vec<RuntimeRef<StorageCell>> elems;
                    for (int i = 0; i < num; ++i) {
                        if (flags[i] == 1) { // Spread
                            auto segment = segments[i];
                            auto values = sequence_slots(activeModule, segment);
                            for (const auto &slot : values) {
                                elems.push_back(clone_value_slot(slot, "spread:" + std::to_string(elems.size())));
                            }
                        } else {
                            elems.push_back(clone_value_slot(segments[i], "spread:" + std::to_string(elems.size())));
                        }
                    }
                    
                    if (op == OpCode::NEW_TUPLE_SPREAD) push_slot_copy(make_runtime_tuple_cell(elems));
                    else push_slot_copy(make_runtime_array_cell(elems));
                    break;
                }
                case OpCode::GET_TUPLE_REST:
                {
                    auto idxObj = pop_slot();
                    auto tupleObj = pop_slot();
                    auto idx = read_numeric_cell_as<int32_t>(idxObj);
                    
                    Vec<RuntimeRef<StorageCell>> rest;
                    if (!runtime_is_tuple_value(tupleObj)) throw RuntimeException("Not a tuple");
                    auto values = runtime_tuple_slots(tupleObj);
                    if (idx >= 0 && idx < static_cast<decltype(idx)>(values.size())) {
                        for (auto it = values.begin() + idx; it != values.end(); ++it) {
                            rest.push_back(clone_value_slot(*it, "rest:" + std::to_string(rest.size())));
                        }
                    }
                    push_slot_copy(make_runtime_tuple_cell(rest));
                    break;
                }
                case OpCode::FOLD_MAP_CALL:
                case OpCode::FOLD_FILTER_CALL:
                {
                    uint16_t funIndex = read_u16();
                    auto sequence = access_target_slot(pop_slot());
                    auto items = sequence_slots(activeModule, sequence);
                    Vec<RuntimeRef<StorageCell>> elems;
                    elems.reserve(items.size());
                    for (const auto &item : items) {
                        auto mapped = execute_slots(*current_module, current_module->functions[funIndex],
                                                    {clone_value_slot(item, "fold.item")});
                        if (op == OpCode::FOLD_FILTER_CALL) {
                            if (runtime_value_bool(mapped)) {
                                elems.push_back(clone_value_slot(item, "fold.filter:" + std::to_string(elems.size())));
                            }
                        } else {
                            elems.push_back(clone_value_slot(mapped, "fold.map:" + std::to_string(elems.size())));
                        }
                    }
                    push_slot_copy(make_runtime_array_cell(elems));
                    break;
                }
                case OpCode::FOLD_LEFT_CALL:
                {
                    uint16_t funIndex = read_u16();
                    auto sequence = access_target_slot(pop_slot());
                    auto accumulator = pop_slot();
                    auto items = sequence_slots(activeModule, sequence);
                    for (const auto &item : items) {
                        accumulator = execute_slots(*current_module, current_module->functions[funIndex],
                                                    {clone_value_slot(accumulator, "fold.acc"),
                                                     clone_value_slot(item, "fold.item")});
                    }
                    push_slot_copy(accumulator);
                    break;
                }
                case OpCode::FOLD_RIGHT_CALL:
                {
                    uint16_t funIndex = read_u16();
                    auto accumulator = pop_slot();
                    auto sequence = access_target_slot(pop_slot());
                    auto items = sequence_slots(activeModule, sequence);
                    for (auto it = items.rbegin(); it != items.rend(); ++it) {
                        accumulator = execute_slots(*current_module, current_module->functions[funIndex],
                                                    {clone_value_slot(*it, "fold.item"),
                                                     clone_value_slot(accumulator, "fold.acc")});
                    }
                    push_slot_copy(accumulator);
                    break;
                }
                case OpCode::MAKE_RANGE:
                {
                    uint8_t inclusive = read_byte_checked(code, ip);
                    auto end = pop_slot();
                    auto start = pop_slot();
                    push_slot_copy(make_runtime_range_cell(start, end, inclusive != 0));
                    break;
                }
                case OpCode::SLICE_RANGE:
                {
                    auto endSlot = pop_slot();
                    auto startSlot = pop_slot();
                    auto sequence = access_target_slot(pop_slot());
                    auto slots = sequence_slots(activeModule, sequence);
                    auto bound = [&](const RuntimeRef<StorageCell> &slot, size_t defaultValue) -> size_t {
                        if (runtime_is_from_end_index(slot)) {
                            auto offset = runtime_from_end_index_value(slot);
                            if (offset < 0 || static_cast<size_t>(offset) > slots.size()) {
                                throw RuntimeException("slice range from-end bound out of range");
                            }
                            return slots.size() - static_cast<size_t>(offset);
                        }
                        auto value = read_numeric_cell_as<int32_t>(slot);
                        if (value == std::numeric_limits<int32_t>::max()) return defaultValue;
                        if (value < 0) return 0;
                        return std::min(static_cast<size_t>(value), slots.size());
                    };
                    auto start = bound(startSlot, 0);
                    auto end = bound(endSlot, slots.size());
                    if (start > end) start = end;
                    Vec<RuntimeRef<StorageCell>> result;
                    for (size_t i = start; i < end; ++i) {
                        result.push_back(clone_value_slot(slots[i], "span:" + std::to_string(result.size())));
                    }
                    if (runtime_is_tuple_value(sequence)) push_slot_copy(make_runtime_tuple_cell(result));
                    else push_slot_copy(make_runtime_span_cell(result));
                    break;
                }
                // ── Bitwise/Shift ─────────────────────────────────────────────
                case OpCode::LSHIFT: { auto b = pop_slot(); auto a = pop_slot(); push_binary_result(a, RuntimeBinaryOperator::LShift, b); break; }
                case OpCode::RSHIFT: { auto b = pop_slot(); auto a = pop_slot(); push_slot_copy(value_rshift(a, b)); break; }
                // ── Newtype ───────────────────────────────────────────────────
                case OpCode::WRAP_NEWTYPE:
                {
                    uint16_t typeIdx = read_u16();
                    if (typeIdx >= current_module->strings.size()) throw RuntimeException("VM error: WRAP_NEWTYPE string index out of bounds");
                    Str typeName = current_module->strings[typeIdx];
                    auto value = pop_slot();
                    RuntimeRef<NGType> newType;
                    if (root_types.contains(typeName)) {
                        newType = root_types[typeName];
                    } else {
                        newType = makert<NGType>();
                        newType->name = typeName;
                        root_types[typeName] = newType;
                    }
                    push_slot_copy(make_runtime_newtype_cell(newType, value));
                    break;
                }
                case OpCode::UNWRAP_NEWTYPE:
                {
                    auto value = pop_slot();
                    if (auto wrapped = runtime_cell_slot_ref(value, 0)) {
                        push_slot_copy(wrapped);
                    } else {
                        throw RuntimeException("Cannot unwrap non-newtype value");
                    }
                    break;
                }
                // ── Native/Assert ─────────────────────────────────────────────
                case OpCode::PRINT:
            {
                uint16_t numArgs = read_u16();
                Vec<RuntimeRef<StorageCell>> args_to_print;
                for (int i = 0; i < numArgs; ++i) args_to_print.push_back(pop_slot());
                for (int i = static_cast<int>(args_to_print.size()) - 1; i >= 0; --i) {
                    std::cout << runtime_value_show(args_to_print[i]) << (i == 0 ? "" : ", ");
                }
                std::cout << std::endl;
                stack.push_back(unit_cell());
                break;
            }
                case OpCode::NATIVE_CALL:
                {
                    uint16_t nameIdx = read_u16();
                    if (nameIdx >= current_module->strings.size()) throw RuntimeException("VM error: NATIVE_CALL string index out of bounds");
                    uint16_t numArgs = read_u16();
                    Str funcName = current_module->strings[nameIdx];
                    Vec<RuntimeRef<StorageCell>> callArgs;
                    callArgs.reserve(numArgs); for (int i = 0; i < numArgs; ++i) callArgs.push_back(pop_slot()); std::reverse(callArgs.begin(), callArgs.end());
                    if (!native_functions.contains(funcName)) {
                        throw RuntimeException("Native function not registered: " + funcName);
                    }
                    stack.push_back(native_functions[funcName](callArgs));
                    break;
                }
                case OpCode::ASSERT: { 
                    auto val = pop_slot(); 
                    if (!runtime_value_bool(val)) {
                        std::cerr << "Assertion Failed. Value: " << runtime_value_show(val) << std::endl;
                        throw AssertionException(); 
                    }
                    stack.push_back(unit_cell()); 
                    break; 
                }
                case OpCode::JUMP:
                {
                    int32_t target = std::bit_cast<int32_t>(read_le_bytes_checked<uint32_t>(code, ip));
                    if (target < 0 || static_cast<size_t>(target) >= code.size()) throw RuntimeException("VM error: JUMP target out of bounds");
                    ip = static_cast<size_t>(target);
                    break;
                }
                case OpCode::JUMP_IF_FALSE:
                {
                    int32_t target = std::bit_cast<int32_t>(read_le_bytes_checked<uint32_t>(code, ip));
                    if (target < 0 || static_cast<size_t>(target) >= code.size()) throw RuntimeException("VM error: JUMP_IF_FALSE target out of bounds");
                    if (!runtime_value_bool(pop_slot())) ip = static_cast<size_t>(target);
                    break;
                }

                // ── Tagged Union ──────────────────────────────────────────────
                case OpCode::CONSTRUCT_TAGGED: {
                    uint16_t typeIdx = read_u16();
                    if (typeIdx >= current_module->types.size()) throw RuntimeException("VM error: CONSTRUCT_TAGGED type index out of bounds");
                    uint16_t variantIdx = read_u16();
                    if (variantIdx >= current_module->types[typeIdx].variants.size()) throw RuntimeException("VM error: CONSTRUCT_TAGGED variant index out of bounds");
                    uint16_t numPayload = read_u16();
                    Vec<RuntimeRef<StorageCell>> payload;
                    payload.reserve(numPayload);
                    for (uint16_t i = 0; i < numPayload; ++i) {
                        payload.push_back(pop_slot());
                    }
                    std::reverse(payload.begin(), payload.end());
                    auto &type = current_module->types[typeIdx];
                    auto tagged = make_runtime_tagged_cell(
                        type.name,
                        type.variants[variantIdx].name,
                        static_cast<int32_t>(variantIdx),
                        std::move(payload),
                        type.variants[variantIdx].payloadFields);
                    push_slot_copy(std::move(tagged));
                    break;
                }

                case OpCode::GET_TAG: {
                    auto tagged = access_target_slot(pop_slot());
                    auto type = runtime_value_type(tagged);
                    if (!type || type->layout.kind != LayoutKind::TAGGED_UNION) throw IllegalTypeException("GET_TAG: not a tagged value");
                    push_slot_copy(numeral_cell_from_value<int32_t>(type->variantIndex));
                    break;
                }

                case OpCode::GET_PAYLOAD: {
                    uint16_t fieldIdx = read_u16();
                    auto tagged = access_target_slot(pop_slot());
                    auto type = runtime_value_type(tagged);
                    if (!type || type->layout.kind != LayoutKind::TAGGED_UNION) throw IllegalTypeException("GET_PAYLOAD: not a tagged value");
                    auto payload = runtime_cell_slot_refs(tagged);
                    if (fieldIdx >= payload.size()) throw IllegalTypeException("GET_PAYLOAD: index out of bounds");
                    push_slot_copy(payload[fieldIdx]);
                    break;
                }

                case OpCode::SWITCH_TAG: {
                    if (stack.empty()) throw RuntimeException("VM error: SWITCH_TAG on empty stack");
                    uint16_t numCases = read_u16();
                    // Peek at the tagged value on the stack (don't pop — case bodies need it)
                    auto taggedRef = access_target_slot(stack.back());
                    auto taggedType = runtime_value_type(taggedRef);
                    if (!taggedType || taggedType->layout.kind != LayoutKind::TAGGED_UNION) throw IllegalTypeException("SWITCH_TAG: not a tagged value");
                    int32_t tagVal = taggedType->variantIndex;
                    // Read jump table and find matching case
                    bool found = false;
                    int32_t defaultAddr = -1;
                    for (uint16_t i = 0; i < numCases; ++i) {
                        uint16_t tag = read_u16();
                        int32_t addr = std::bit_cast<int32_t>(read_le_bytes_checked<uint32_t>(code, ip));
                        if (tag == SWITCH_DEFAULT_TAG) {
                            defaultAddr = addr;
                            continue;
                        }
                        if (static_cast<int32_t>(tag) == tagVal) {
                            if (addr < 0 || static_cast<size_t>(addr) >= code.size()) throw RuntimeException("VM error: SWITCH_TAG case target out of bounds");
                            ip = static_cast<size_t>(addr);
                            found = true;
                            break;
                        }
                    }
                    if (!found && defaultAddr >= 0) {
                        if (static_cast<size_t>(defaultAddr) >= code.size()) throw RuntimeException("VM error: SWITCH_TAG default target out of bounds");
                        ip = static_cast<size_t>(defaultAddr);
                        found = true;
                    }
                    if (!found) throw IllegalTypeException("SWITCH_TAG: no matching case for tag " + std::to_string(tagVal));
                    break;
                }

                default:
                    throw RuntimeException("Unknown opcode: " + std::to_string(static_cast<int>(op)) + " at ip=" +
                                           std::to_string(ip - 1));
                }
            } catch (const std::exception& ex) {
                std::cerr << "Error at ip=" << ip-1 << " op=" << static_cast<int>(op) << " in " << activeFunction.name << ": " << ex.what() << std::endl;
                throw;
            }
        }
        return unit_cell();
    }
} // namespace NG::orgasm
