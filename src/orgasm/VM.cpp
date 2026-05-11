#include <orgasm/vm.hpp>
#include <algorithm>
#include <bit>
#include <iostream>
#include <intp/runtime_numerals.hpp>
#include <cstring>
#include <limits>
#include <module.hpp>
#include <runtime/array_layout_access.hpp>
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
    }

    void VM::register_native_raw(const Str &name, NativeFunction func)
    {
        native_functions[name] = std::move(func);
    }

    auto VM::run(const BytecodeModule &module) -> RuntimeRef<NGObject>
    {
        current_module = &module;
        root_context = makert<NGContext>();
        auto gcRootProviderId = register_gc_root_provider([this]() {
            auto roots = enumerate_context_roots(root_context);
            roots.insert(roots.end(), globals.begin(), globals.end());
            roots.insert(roots.end(), stack.begin(), stack.end());
            for (const auto &frame : call_stack)
            {
                roots.insert(roots.end(), frame.locals.begin(), frame.locals.end());
                roots.insert(roots.end(), frame.args.begin(), frame.args.end());
            }
            return roots;
        });
        struct RootProviderGuard
        {
            size_t id;
            ~RootProviderGuard() { unregister_gc_root_provider(id); }
        } rootProviderGuard{gcRootProviderId};
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
        globals.resize(std::max(maxGlobal, size_t{1}), makert<NGUnit>());
        
        // Register built-ins
        root_context->define_function("not", [](const NGSelf &self, const NGCtx &ctx,
                                                const NGArgs &args) -> RuntimeRef<NGObject> {
            if (args.empty()) throw RuntimeException("not expects 1 arg");
            return NGObject::boolean(!runtime_value_bool(args[0]));
        });

        for (const auto &type : module.types) {
            auto ngType = makert<NGType>();
            ngType->name = type.name;
            ngType->properties = type.properties;
            root_types[type.name] = ngType;
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
                execute(module.functions[0], {});
            }
            return execute(*target, {});
        }
        return makert<NGUnit>();
    }

    auto VM::execute(const Function &fun, const Vec<RuntimeRef<NGObject>> &args) -> RuntimeRef<NGObject>
    {
        Frame frame;
        frame.ip = 0;
        frame.args = args;
        frame.locals.resize(std::max(static_cast<int32_t>(fun.num_locals), fun.num_params), makert<NGUnit>());
        for (size_t i = 0; i < args.size(); ++i) frame.locals[i] = args[i];
        
        call_stack.push_back(std::move(frame));
        struct FrameGuard {
            Vec<Frame> &frames;
            bool released = false;

            ~FrameGuard()
            {
                if (!released && !frames.empty()) frames.pop_back();
            }
        } guard{call_stack};
        size_t frame_idx = call_stack.size() - 1;
        
        auto pop = [this]() -> RuntimeRef<NGObject>
        {
            if (stack.empty()) throw RuntimeException("Stack underflow");
            auto val = stack.back();
            stack.pop_back();
            return val;
        };

        auto copy_out = [](const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
        {
            ensure_usable_value(value);
            return clone_value(value);
        };
        auto access_target = [](const RuntimeRef<NGObject> &value) -> RuntimeRef<NGObject>
        {
            return auto_deref_value(value);
        };

        const auto &code = fun.code;
        while (call_stack[frame_idx].ip < code.size())
        {
            size_t &ip = call_stack[frame_idx].ip;
            OpCode op = static_cast<OpCode>(code[ip++]);

            auto read_u16 = [&code, &ip]() -> uint16_t
            {
                return read_le_bytes<uint16_t>(code, ip);
            };

            try {
                switch (op)
                {
                                case OpCode::NOP:
                                    break;
                                case OpCode::PUSH_I8:
                                {
                                    int8_t val = static_cast<int8_t>(code[ip++]);
                                    stack.push_back(makert<NGIntegral<int8_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_I16:
                                {
                                    int16_t val = std::bit_cast<int16_t>(read_le_bytes<uint16_t>(code, ip));
                                    stack.push_back(makert<NGIntegral<int16_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_I32:
                                {
                                    int32_t val = std::bit_cast<int32_t>(read_le_bytes<uint32_t>(code, ip));
                                    stack.push_back(makert<NGIntegral<int32_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_I64:
                                {
                                    int64_t val = std::bit_cast<int64_t>(read_le_bytes<uint64_t>(code, ip));
                                    stack.push_back(makert<NGIntegral<int64_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U8:
                                {
                                    uint8_t val = code[ip++];
                                    stack.push_back(makert<NGIntegral<uint8_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U16:
                                {
                                    uint16_t val = read_le_bytes<uint16_t>(code, ip);
                                    stack.push_back(makert<NGIntegral<uint16_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U32:
                                {
                                    uint32_t val = read_le_bytes<uint32_t>(code, ip);
                                    stack.push_back(makert<NGIntegral<uint32_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U64:
                                {
                                    uint64_t val = read_le_bytes<uint64_t>(code, ip);
                                    stack.push_back(makert<NGIntegral<uint64_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_F32:
                                {
                                    float val = std::bit_cast<float>(read_le_bytes<uint32_t>(code, ip));
                                    stack.push_back(makert<NGFloatingPoint<float>>(val));
                                    break;
                                }
                                case OpCode::PUSH_F64:
                                {
                                    double val = std::bit_cast<double>(read_le_bytes<uint64_t>(code, ip));
                                    stack.push_back(makert<NGFloatingPoint<double>>(val));
                                    break;
                                }
                                case OpCode::ADD: { auto b = pop(); auto a = pop(); stack.push_back(value_add(a, b)); break; }
                                case OpCode::SUB: { auto b = pop(); auto a = pop(); stack.push_back(value_subtract(a, b)); break; }
                                case OpCode::MUL: { auto b = pop(); auto a = pop(); stack.push_back(value_multiply(a, b)); break; }
                                case OpCode::DIV: { auto b = pop(); auto a = pop(); stack.push_back(value_divide(a, b)); break; }
                                case OpCode::ADD_I32: { auto b = pop(); auto a = pop(); stack.push_back(value_add(a, b)); break; }
                                case OpCode::SUB_I32: { auto b = pop(); auto a = pop(); stack.push_back(value_subtract(a, b)); break; }
                                case OpCode::MUL_I32: { auto b = pop(); auto a = pop(); stack.push_back(value_multiply(a, b)); break; }
                                case OpCode::DIV_I32: { auto b = pop(); auto a = pop(); stack.push_back(value_divide(a, b)); break; }                            case OpCode::MOD_I32: { 
                                auto b = pop(); auto a = pop(); 
                                try { stack.push_back(value_modulus(a, b)); }
                                catch (const std::exception& ex) {
                                    throw RuntimeException(Str(ex.what()) + " (MOD_I32: " + runtime_value_type(a)->name + " % " +
                                                           runtime_value_type(b)->name + ")");
                                }
                                break; 
                            }                case OpCode::LOAD_STR: { stack.push_back(makert<NGString>(current_module->strings[read_u16()])); break; }
                case OpCode::LOAD_CONST: { stack.push_back(makert<NGIntegral<int64_t>>(current_module->constants[read_u16()])); break; }
                case OpCode::EQ_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(value_equals(a, b))); break; }
                case OpCode::LT_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(value_less_than(a, b))); break; }
                case OpCode::GT_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(value_greater_than(a, b))); break; }
                case OpCode::PUSH_BOOL: stack.push_back(NGObject::boolean(code[ip++] != 0)); break;
                case OpCode::NOT: { auto val = pop(); stack.push_back(NGObject::boolean(!runtime_value_bool(val))); break; }
                case OpCode::INSTANCE_OF:
                {
                    uint16_t typeNameIdx = read_u16();
                    Str typeName = current_module->strings[typeNameIdx];
                    auto val = access_target(pop());
                    bool result = false;
                    if (auto valueType = runtime_value_type(val); valueType && valueType->name == typeName) result = true;
                    stack.push_back(NGObject::boolean(result));
                    break;
                }
                case OpCode::NEG_I32: { auto val = pop(); auto numeric = std::dynamic_pointer_cast<NumeralBase>(val); if (numeric) stack.push_back(numeric->opNegate()); else throw RuntimeException("Not a number"); break; }
                case OpCode::RETURN: {
                    auto res = stack.empty() ? makert<NGUnit>() : pop();
                    guard.released = true;
                    call_stack.pop_back();
                    return res;
                }
                case OpCode::LOAD_LOCAL: { stack.push_back(copy_out(call_stack[frame_idx].locals[read_u16()])); break; }
                case OpCode::LOAD_PARAM: { stack.push_back(copy_out(call_stack[frame_idx].locals[read_u16()])); break; }
                case OpCode::STORE_LOCAL: { uint16_t idx = read_u16(); auto &locals = call_stack[frame_idx].locals; if (idx >= locals.size()) locals.resize(idx + 1, makert<NGUnit>()); locals[idx] = stack.back(); break; }
                case OpCode::LOAD_GLOBAL: { stack.push_back(copy_out(globals[read_u16()])); break; }
                case OpCode::STORE_GLOBAL: { uint16_t idx = read_u16(); if (idx >= globals.size()) globals.resize(idx + 1, makert<NGUnit>()); globals[idx] = stack.back(); break; }
                case OpCode::MAKE_LOCAL_REF:
                {
                    uint16_t idx = read_u16();
                    stack.push_back(makert<NGReference>(
                        [this, frame_idx, idx]() -> RuntimeRef<NGObject> { return call_stack.at(frame_idx).locals.at(idx); },
                        [this, frame_idx, idx](RuntimeRef<NGObject> value) { call_stack.at(frame_idx).locals.at(idx) = std::move(value); },
                        "local:" + std::to_string(idx)));
                    break;
                }
                case OpCode::MAKE_GLOBAL_REF:
                {
                    uint16_t idx = read_u16();
                    stack.push_back(makert<NGReference>(
                        [this, idx]() -> RuntimeRef<NGObject> { return globals.at(idx); },
                        [this, idx](RuntimeRef<NGObject> value) { globals.at(idx) = std::move(value); },
                        "global:" + std::to_string(idx)));
                    break;
                }
                case OpCode::MAKE_PROPERTY_REF:
                {
                    uint16_t fieldIdx = read_u16();
                    auto target = pop();
                    if (auto baseRef = std::dynamic_pointer_cast<NGReference>(target))
                    {
                        stack.push_back(makert<NGReference>(
                            [baseRef, fieldIdx]() -> RuntimeRef<NGObject> {
                                auto structural = std::dynamic_pointer_cast<NGStructuralObject>(auto_deref_value(baseRef->read()));
                                if (!structural) throw RuntimeException("Cannot reference property on non-object");
                                auto fields = structural->payload_fields();
                                if (fieldIdx >= fields.size()) fields.resize(fieldIdx + 1, makert<NGUnit>());
                                structural->replace_payload_fields(fields);
                                return structural->payload_fields().at(fieldIdx);
                            },
                            [baseRef, fieldIdx](RuntimeRef<NGObject> value) {
                                auto structural = std::dynamic_pointer_cast<NGStructuralObject>(auto_deref_value(baseRef->read()));
                                if (!structural) throw RuntimeException("Cannot reference property on non-object");
                                auto fields = structural->payload_fields();
                                if (fieldIdx >= fields.size()) fields.resize(fieldIdx + 1, makert<NGUnit>());
                                fields.at(fieldIdx) = value;
                                structural->replace_payload_fields(fields);
                                baseRef->write(structural);
                            },
                            "field:" + std::to_string(fieldIdx)));
                    }
                    else
                    {
                        auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target);
                        if (!structural) throw RuntimeException("Cannot reference property on non-object");
                        auto fields = structural->payload_fields();
                        if (fieldIdx >= fields.size()) fields.resize(fieldIdx + 1, makert<NGUnit>());
                        structural->replace_payload_fields(fields);
                        stack.push_back(makert<NGReference>(
                            [structural, fieldIdx]() -> RuntimeRef<NGObject> { return structural->payload_fields().at(fieldIdx); },
                            [structural, fieldIdx](RuntimeRef<NGObject> value) {
                                auto fields = structural->payload_fields();
                                fields.at(fieldIdx) = value;
                                structural->replace_payload_fields(fields);
                            },
                            "field:" + std::to_string(fieldIdx)));
                    }
                    break;
                }
                case OpCode::MAKE_PROPERTY_STR_REF:
                {
                    uint16_t nameIdx = read_u16();
                    Str propName = current_module->strings[nameIdx];
                    auto target = pop();
                    auto makePropertyRef = [&propName](const std::shared_ptr<NGStructuralObject> &structural,
                                                       const std::function<void(RuntimeRef<NGObject>)> &commit) {
                        return makert<NGReference>(
                            [structural, propName]() -> RuntimeRef<NGObject> {
                                return structural_read_member(structural, propName);
                            },
                            [structural, propName, commit](RuntimeRef<NGObject> value) {
                                structural_write_member(structural, propName, value);
                                commit(structural);
                            },
                            "property:" + propName);
                    };
                    if (auto baseRef = std::dynamic_pointer_cast<NGReference>(target))
                    {
                        auto structural = std::dynamic_pointer_cast<NGStructuralObject>(auto_deref_value(baseRef->read()));
                        if (!structural) throw RuntimeException("Cannot reference property on non-object");
                        stack.push_back(makePropertyRef(structural, [baseRef](RuntimeRef<NGObject> value) { baseRef->write(value); }));
                    }
                    else
                    {
                        auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target);
                        if (!structural) throw RuntimeException("Cannot reference property on non-object");
                        stack.push_back(makePropertyRef(structural, [](RuntimeRef<NGObject>) {}));
                    }
                    break;
                }
                case OpCode::MAKE_INDEX_REF:
                {
                    auto index = pop();
                    auto target = pop();
                    auto makeIndexRef = [&index](const RuntimeRef<NGObject> &container,
                                                 const std::function<void(RuntimeRef<NGObject>)> &commit) {
                        return makert<NGReference>(
                            [container, index]() -> RuntimeRef<NGObject> {
                                auto idx = NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(index).get());
                                if (auto tuple = std::dynamic_pointer_cast<NGTuple>(container))
                                    return tuple_read_element(*tuple, static_cast<size_t>(idx));
                                if (auto array = std::dynamic_pointer_cast<NGArray>(container))
                                    return array_read_element(*array, static_cast<size_t>(idx));
                                throw RuntimeException("Cannot reference index on non-indexable value");
                            },
                            [container, index, commit](RuntimeRef<NGObject> value) {
                                auto idx = NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(index).get());
                                if (auto tuple = std::dynamic_pointer_cast<NGTuple>(container))
                                {
                                    tuple_write_element(*tuple, static_cast<size_t>(idx), value);
                                    commit(tuple);
                                    return;
                                }
                                if (auto array = std::dynamic_pointer_cast<NGArray>(container))
                                {
                                    array_write_element(*array, static_cast<size_t>(idx), value);
                                    commit(array);
                                    return;
                                }
                                throw RuntimeException("Cannot reference index on non-indexable value");
                            },
                            "index");
                    };
                    if (auto baseRef = std::dynamic_pointer_cast<NGReference>(target))
                    {
                        auto container = auto_deref_value(baseRef->read());
                        stack.push_back(makeIndexRef(container, [baseRef](RuntimeRef<NGObject> value) { baseRef->write(value); }));
                    }
                    else
                    {
                        stack.push_back(makeIndexRef(target, [](RuntimeRef<NGObject>) {}));
                    }
                    break;
                }
                case OpCode::LOAD_REF:
                {
                    auto reference = std::dynamic_pointer_cast<NGReference>(pop());
                    if (!reference) throw RuntimeException("Cannot dereference non-reference value");
                    stack.push_back(copy_out(reference->read()));
                    break;
                }
                case OpCode::STORE_REF:
                {
                    auto value = pop();
                    auto reference = std::dynamic_pointer_cast<NGReference>(pop());
                    if (!reference) throw RuntimeException("Cannot assign through non-reference value");
                    reference->write(value);
                    stack.push_back(value);
                    break;
                }
                case OpCode::MOVE_LOCAL:
                {
                    uint16_t idx = read_u16();
                    auto &slot = call_stack[frame_idx].locals[idx];
                    ensure_usable_value(slot);
                    auto moved = slot;
                    slot = moved_object();
                    stack.push_back(moved);
                    break;
                }
                case OpCode::MOVE_GLOBAL:
                {
                    uint16_t idx = read_u16();
                    auto &slot = globals[idx];
                    ensure_usable_value(slot);
                    auto moved = slot;
                    slot = moved_object();
                    stack.push_back(moved);
                    break;
                }
                case OpCode::MOVE_REF:
                {
                    auto reference = std::dynamic_pointer_cast<NGReference>(pop());
                    if (!reference) throw RuntimeException("Cannot move from non-reference value");
                    auto moved = reference->read();
                    ensure_usable_value(moved);
                    reference->write(moved_object());
                    stack.push_back(moved);
                    break;
                }
                case OpCode::GET_TUPLE_ITEM:
                {
                    auto idxObj = pop();
                    auto tupleObj = access_target(pop());
                    auto tuple = std::dynamic_pointer_cast<NGTuple>(tupleObj);
                    auto idx = NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(idxObj).get());
                    stack.push_back(copy_out(tuple_read_element(*tuple, static_cast<size_t>(idx))));
                    break;
                }
                            case OpCode::POP: pop(); break;
                            case OpCode::DUP: stack.push_back(stack.back()); break;
                            case OpCode::PUSH_UNIT: stack.push_back(makert<NGUnit>()); break;                case OpCode::CALL:
                {
                    uint16_t funIndex = read_u16();
                    uint16_t numArgs = read_u16();
                    Vec<RuntimeRef<NGObject>> callArgs;
                    for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                    stack.push_back(execute(current_module->functions[funIndex], callArgs));
                    break;
                }
                case OpCode::CALL_IMPORT:
                {
                    uint16_t importIdx = read_u16();
                    uint16_t numArgs = read_u16();
                    auto &imp = current_module->imports[importIdx];
                    
                    auto moduleInfo = NG::module::get_module_registry().queryModuleById(imp.moduleName);
                    if (!moduleInfo) throw RuntimeException("Module not found: " + imp.moduleName);
                    
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
                        
                        Vec<RuntimeRef<NGObject>> callArgs;
                        for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                        
                        auto *saved_module = current_module;
                        struct ModuleGuard
                        {
                            const BytecodeModule *&current;
                            const BytecodeModule *saved;

                            ~ModuleGuard()
                            {
                                current = saved;
                            }
                        } module_guard{current_module, saved_module};
                        current_module = &otherModule;
                        
                        stack.push_back(execute(otherModule.functions[funIdx], callArgs));
                    } else {
                        // Try native function fallback
                        Vec<RuntimeRef<NGObject>> callArgs;
                        for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                        if (native_functions.contains(imp.symbolName)) {
                            stack.push_back(native_functions[imp.symbolName](callArgs));
                        } else {
                            throw RuntimeException("Module " + imp.moduleName + " is not a bytecode module and no native function found for " + imp.symbolName);
                        }
                    }
                    break;
                }
                case OpCode::GET_PROPERTY:
            {
                uint16_t fieldIdx = read_u16();
                        auto target = access_target(pop());
                        if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                            auto fields = structural->payload_fields();
                            if (fieldIdx < fields.size()) {
                                stack.push_back(copy_out(fields[fieldIdx]));
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
                auto val = pop();
                auto target = access_target(pop());
                if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    auto fields = structural->payload_fields();
                    if (fieldIdx >= fields.size()) fields.resize(fieldIdx + 1, makert<NGUnit>());
                    fields[fieldIdx] = val;
                    structural->replace_payload_fields(fields);
                } else {
                    throw RuntimeException("Cannot set property on non-object");
                }
                // Push the target (object) back so subsequent property sets work
                stack.push_back(target);
                break;
            }
                case OpCode::GET_PROPERTY_STR:
            {
                uint16_t nameIdx = read_u16();
                Str propName = current_module->strings[nameIdx];
                auto target = access_target(pop());
                if (auto tup = std::dynamic_pointer_cast<NGTuple>(target)) {
                    if (auto value = tuple_read_member(*tup, propName)) {
                        stack.push_back(value);
                    } else {
                        throw RuntimeException("Tuple has no property: " + propName);
                    }
                } else if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    if (auto value = structural_read_member(structural, propName)) {
                        stack.push_back(copy_out(value));
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
                auto val = pop();
                auto target = access_target(pop());
                if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    structural_write_member(structural, propName, val);
                } else {
                    throw RuntimeException("Cannot set property on non-object");
                }
                stack.push_back(val);
                break;
            }
            case OpCode::NEW_ARRAY:
                {
                    uint16_t num = read_u16(); Vec<RuntimeRef<NGObject>> elems;
                    for (int i = 0; i < num; ++i) elems.insert(elems.begin(), pop());
                    stack.push_back(makert<NGArray>(elems));
                    break;
                }
                case OpCode::NEW_TUPLE:
                {
                    uint16_t num = read_u16(); Vec<RuntimeRef<NGObject>> elems;
                    for (int i = 0; i < num; ++i) elems.insert(elems.begin(), pop());
                    stack.push_back(makert<NGTuple>(elems));
                    break;
                }
                case OpCode::GET_INDEX:
                {
                    auto idx = pop(); auto obj = access_target(pop());
                    stack.push_back(copy_out(obj->opIndex(idx)));
                    break;
                }
                case OpCode::SET_INDEX:
                {
                    auto val = pop(); auto idx = pop(); auto obj = access_target(pop());
                    stack.push_back(obj->opIndex(idx, val));
                    break;
                }
                case OpCode::NEW_OBJECT:
                                {
                                    uint16_t typeStrIdx = read_u16();
                                    Str typeName = current_module->strings[typeStrIdx];
                                    uint16_t numFields = read_u16();

                                    Vec<RuntimeRef<NGObject>> fields(static_cast<size_t>(numFields), makert<NGUnit>());
                                    for (int i = numFields - 1; i >= 0; --i)
                                    {
                                        fields[static_cast<size_t>(i)] = pop();
                                    }

                                    if (root_types.contains(typeName)) {
                                        auto obj = makert<NGStructuralObject>();
                                        obj->customizedType = root_types[typeName];
                                        obj->replace_payload_fields(fields);
                                        auto &typeProps = root_types[typeName]->properties;
                                        for (size_t i = 0; i < typeProps.size() && i < fields.size(); ++i) {
                                            structural_write_member(obj, typeProps[i], fields[i]);
                                        }
                                        stack.push_back(allocate_heap_object(obj, "heap:" + typeName));
                                        break;
                                    }

                                    bool foundVariant = false;
                                    for (const auto &type : current_module->types) {
                                        for (size_t variantIndex = 0; variantIndex < type.variants.size(); ++variantIndex) {
                                            const auto &variant = type.variants[variantIndex];
                                            if (variant.name != typeName) {
                                                continue;
                                            }

                                            stack.push_back(allocate_heap_object(
                                                makert<NGTaggedValue>(type.name, variant.name, static_cast<int32_t>(variantIndex),
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
                    uint16_t numArgs = read_u16();
                    Str memberName = current_module->strings[nameIdx];
                    Vec<RuntimeRef<NGObject>> callArgs;
                    for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                    auto target = access_target(pop());
                    
                    Str typeName = runtime_value_type(target) ? runtime_value_type(target)->name : "Object";
                    Str fullFunName = typeName + "." + memberName;
                    
                    int32_t funIdx = -1;
                    for (size_t i = 0; i < current_module->functions.size(); ++i) {
                        if (current_module->functions[i].name == fullFunName) { funIdx = static_cast<int32_t>(i); break; }
                    }
                    
                    if (funIdx != -1) {
                        callArgs.insert(callArgs.begin(), target);
                        stack.push_back(execute(current_module->functions[funIdx], callArgs));
                    } else {
                        stack.push_back(runtime_value_respond(target, memberName, root_context, callArgs));
                    }
                    break;
                }
                case OpCode::NEW_TUPLE_SPREAD:
                case OpCode::NEW_ARRAY_SPREAD:
                {
                    uint16_t num = read_u16();
                    Vec<uint8_t> flags(num);
                    for (int i = 0; i < num; ++i) flags[i] = code[ip++];
                    
                    Vec<RuntimeRef<NGObject>> segments;
                    for (int i = 0; i < num; ++i) segments.insert(segments.begin(), pop());
                    
                    Vec<RuntimeRef<NGObject>> elems;
                    for (int i = 0; i < num; ++i) {
                        if (flags[i] == 1) { // Spread
                            auto segment = segments[i];
                            if (auto tup = std::dynamic_pointer_cast<NGTuple>(segment)) {
                                auto values = tup->payload_items();
                                elems.insert(elems.end(), values.begin(), values.end());
                            } else if (auto arr = std::dynamic_pointer_cast<NGArray>(segment)) {
                                auto values = arr->payload_items();
                                elems.insert(elems.end(), values.begin(), values.end());
                            } else {
                                throw RuntimeException("Cannot spread non-iterable");
                            }
                        } else {
                            elems.push_back(segments[i]);
                        }
                    }
                    
                    if (op == OpCode::NEW_TUPLE_SPREAD) stack.push_back(makert<NGTuple>(elems));
                    else stack.push_back(makert<NGArray>(elems));
                    break;
                }
                case OpCode::GET_TUPLE_REST:
                {
                    auto idxObj = pop();
                    auto tupleObj = pop();
                    auto tuple = std::dynamic_pointer_cast<NGTuple>(tupleObj);
                    auto idx = NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(idxObj).get());
                    
                    Vec<RuntimeRef<NGObject>> rest;
                    auto values = tuple->payload_items();
                    if (idx < values.size()) {
                        for (auto it = values.begin() + idx; it != values.end(); ++it) rest.push_back(clone_value(*it));
                    }
                    stack.push_back(makert<NGTuple>(rest));
                    break;
                }
                case OpCode::LSHIFT: { auto b = pop(); auto a = pop(); stack.push_back(value_lshift(a, b)); break; }
                case OpCode::RSHIFT: { auto b = pop(); auto a = pop(); stack.push_back(value_rshift(a, b)); break; }
                case OpCode::WRAP_NEWTYPE:
                {
                    uint16_t typeIdx = read_u16();
                    Str typeName = current_module->strings[typeIdx];
                    auto value = pop();
                    RuntimeRef<NGType> newType;
                    if (root_types.contains(typeName)) {
                        newType = root_types[typeName];
                    } else {
                        newType = makert<NGType>();
                        newType->name = typeName;
                        root_types[typeName] = newType;
                    }
                    stack.push_back(makert<NGNewType>(newType, value));
                    break;
                }
                case OpCode::UNWRAP_NEWTYPE:
                {
                    auto value = pop();
                    if (auto nt = std::dynamic_pointer_cast<NGNewType>(value)) {
                        stack.push_back(nt->wrapped);
                    } else {
                        throw RuntimeException("Cannot unwrap non-newtype value");
                    }
                    break;
                }
                case OpCode::PRINT:
            {
                uint16_t numArgs = read_u16();
                Vec<RuntimeRef<NGObject>> args_to_print;
                for (int i = 0; i < numArgs; ++i) args_to_print.push_back(pop());
                for (int i = static_cast<int>(args_to_print.size()) - 1; i >= 0; --i) {
                    std::cout << runtime_value_show(args_to_print[i]) << (i == 0 ? "" : ", ");
                }
                std::cout << std::endl;
                stack.push_back(makert<NGUnit>());
                break;
            }
                case OpCode::NATIVE_CALL:
                {
                    uint16_t nameIdx = read_u16();
                    uint16_t numArgs = read_u16();
                    Str funcName = current_module->strings[nameIdx];
                    Vec<RuntimeRef<NGObject>> callArgs;
                    for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                    if (!native_functions.contains(funcName)) {
                        throw RuntimeException("Native function not registered: " + funcName);
                    }
                    stack.push_back(native_functions[funcName](callArgs));
                    break;
                }
                case OpCode::ASSERT: { 
                    auto val = pop(); 
                    if (!runtime_value_bool(val)) {
                        std::cerr << "Assertion Failed. Value: " << runtime_value_show(val) << std::endl;
                        throw AssertionException(); 
                    }
                    stack.push_back(makert<NGUnit>()); 
                    break; 
                }
                case OpCode::JUMP: { int32_t target = std::bit_cast<int32_t>(read_le_bytes<uint32_t>(code, ip)); ip = static_cast<size_t>(target); break; }
                case OpCode::JUMP_IF_FALSE: { int32_t target = std::bit_cast<int32_t>(read_le_bytes<uint32_t>(code, ip)); if (!runtime_value_bool(pop())) ip = static_cast<size_t>(target); break; }

                case OpCode::CONSTRUCT_TAGGED: {
                    uint16_t typeIdx = read_u16();
                    uint16_t variantIdx = read_u16();
                    uint16_t numPayload = read_u16();
                    Vec<RuntimeRef<NGObject>> payload;
                    payload.reserve(numPayload);
                    for (uint16_t i = 0; i < numPayload; ++i) {
                        payload.push_back(pop());
                    }
                    std::reverse(payload.begin(), payload.end());
                    auto &type = current_module->types[typeIdx];
                    auto tagged = makert<NGTaggedValue>(
                        type.name,
                        type.variants[variantIdx].name,
                        static_cast<int32_t>(variantIdx),
                        std::move(payload));
                    stack.push_back(std::move(tagged));
                    break;
                }

                case OpCode::GET_TAG: {
                    auto val = pop();
                    auto *tagged = dynamic_cast<NGTaggedValue*>(val.get());
                    if (!tagged) throw IllegalTypeException("GET_TAG: not a tagged value");
                    stack.push_back(makert<NGIntegral<int32_t>>(tagged->variantIndex));
                    break;
                }

                case OpCode::GET_PAYLOAD: {
                    uint16_t fieldIdx = read_u16();
                    auto val = pop();
                    auto *tagged = dynamic_cast<NGTaggedValue*>(val.get());
                    if (!tagged) throw IllegalTypeException("GET_PAYLOAD: not a tagged value");
                    auto payload = tagged->payload_items();
                    if (fieldIdx >= payload.size()) throw IllegalTypeException("GET_PAYLOAD: index out of bounds");
                    stack.push_back(payload[fieldIdx]);
                    break;
                }

                case OpCode::SWITCH_TAG: {
                    uint16_t numCases = read_u16();
                    // Peek at the tagged value on the stack (don't pop — case bodies need it)
                    auto &taggedRef = stack.back();
                    auto *tagged = dynamic_cast<NGTaggedValue*>(taggedRef.get());
                    if (!tagged) throw IllegalTypeException("SWITCH_TAG: not a tagged value");
                    int32_t tagVal = tagged->variantIndex;
                    // Read jump table and find matching case
                    bool found = false;
                    int32_t defaultAddr = -1;
                    for (uint16_t i = 0; i < numCases; ++i) {
                        uint16_t tag = read_u16();
                        int32_t addr = std::bit_cast<int32_t>(read_le_bytes<uint32_t>(code, ip));
                        if (tag == SWITCH_DEFAULT_TAG) {
                            defaultAddr = addr;
                            continue;
                        }
                        if (static_cast<int32_t>(tag) == tagVal) {
                            ip = static_cast<size_t>(addr);
                            found = true;
                            break;
                        }
                    }
                    if (!found && defaultAddr >= 0) {
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
                std::cerr << "Error at ip=" << ip-1 << " op=" << static_cast<int>(op) << " in " << fun.name << ": " << ex.what() << std::endl;
                throw;
            }
        }
        guard.released = true;
        call_stack.pop_back();
        return makert<NGUnit>();
    }
} // namespace NG::orgasm
