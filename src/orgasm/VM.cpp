#include <orgasm/vm.hpp>
#include <iostream>
#include <intp/runtime_numerals.hpp>
#include <cstring>
#include <module.hpp>

namespace NG::orgasm
{
    void VM::register_native_raw(const Str &name, NativeFunction func)
    {
        native_functions[name] = std::move(func);
    }

    auto VM::run(const BytecodeModule &module) -> RuntimeRef<NGObject>
    {
        current_module = &module;
        root_context = makert<NGContext>();
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
        root_context->define_function("not", [](NGSelf self, NGCtx ctx, NGInvCtx invCtx) {
            if (invCtx->params.empty()) throw RuntimeException("not expects 1 arg");
            ctx->retVal = NGObject::boolean(!invCtx->params[0]->boolValue());
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
            for (const auto &fun : module.functions)
            {
                if (fun.name == "main") { target = &fun; break; }
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
        frame.fun = &fun;
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

        const auto &code = fun.code;
        while (call_stack[frame_idx].ip < code.size())
        {
            size_t &ip = call_stack[frame_idx].ip;
            OpCode op = static_cast<OpCode>(code[ip++]);

            auto read_u16 = [&code, &ip]() -> uint16_t
            {
                uint16_t lo = code[ip++];
                uint16_t hi = code[ip++];
                return lo | (hi << 8);
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
                                    int16_t val; std::memcpy(&val, &code[ip], 2);
                                    ip += 2;
                                    stack.push_back(makert<NGIntegral<int16_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_I32:
                                {
                                    int32_t val; std::memcpy(&val, &code[ip], 4);
                                    ip += 4;
                                    stack.push_back(makert<NGIntegral<int32_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_I64:
                                {
                                    int64_t val; std::memcpy(&val, &code[ip], 8);
                                    ip += 8;
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
                                    uint16_t val; std::memcpy(&val, &code[ip], 2);
                                    ip += 2;
                                    stack.push_back(makert<NGIntegral<uint16_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U32:
                                {
                                    uint32_t val; std::memcpy(&val, &code[ip], 4);
                                    ip += 4;
                                    stack.push_back(makert<NGIntegral<uint32_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_U64:
                                {
                                    uint64_t val; std::memcpy(&val, &code[ip], 8);
                                    ip += 8;
                                    stack.push_back(makert<NGIntegral<uint64_t>>(val));
                                    break;
                                }
                                case OpCode::PUSH_F32:
                                {
                                    float val; std::memcpy(&val, &code[ip], 4);
                                    ip += 4;
                                    stack.push_back(makert<NGFloatingPoint<float>>(val));
                                    break;
                                }
                                case OpCode::PUSH_F64:
                                {
                                    double val; std::memcpy(&val, &code[ip], 8);
                                    ip += 8;
                                    stack.push_back(makert<NGFloatingPoint<double>>(val));
                                    break;
                                }
                                case OpCode::ADD: { auto b = pop(); auto a = pop(); stack.push_back(a->opPlus(b)); break; }
                                case OpCode::SUB: { auto b = pop(); auto a = pop(); stack.push_back(a->opMinus(b)); break; }
                                case OpCode::MUL: { auto b = pop(); auto a = pop(); stack.push_back(a->opTimes(b)); break; }
                                case OpCode::DIV: { auto b = pop(); auto a = pop(); stack.push_back(a->opDividedBy(b)); break; }
                                case OpCode::ADD_I32: { auto b = pop(); auto a = pop(); stack.push_back(a->opPlus(b)); break; }
                                case OpCode::SUB_I32: { auto b = pop(); auto a = pop(); stack.push_back(a->opMinus(b)); break; }
                                case OpCode::MUL_I32: { auto b = pop(); auto a = pop(); stack.push_back(a->opTimes(b)); break; }
                                case OpCode::DIV_I32: { auto b = pop(); auto a = pop(); stack.push_back(a->opDividedBy(b)); break; }                            case OpCode::MOD_I32: { 
                                auto b = pop(); auto a = pop(); 
                                try { stack.push_back(a->opModulus(b)); }
                                catch (const std::exception& ex) {
                                    throw RuntimeException(Str(ex.what()) + " (MOD_I32: " + a->type()->name + " % " + b->type()->name + ")");
                                }
                                break; 
                            }                case OpCode::LOAD_STR: { stack.push_back(makert<NGString>(current_module->strings[read_u16()])); break; }
                case OpCode::LOAD_CONST: { stack.push_back(makert<NGIntegral<int64_t>>(current_module->constants[read_u16()])); break; }
                case OpCode::EQ_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(a->opEquals(b))); break; }
                case OpCode::LT_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(a->opLessThan(b))); break; }
                case OpCode::GT_I32: { auto b = pop(); auto a = pop(); stack.push_back(NGObject::boolean(a->opGreaterThan(b))); break; }
                case OpCode::PUSH_BOOL: stack.push_back(NGObject::boolean(code[ip++] != 0)); break;
                case OpCode::NOT: { auto val = pop(); stack.push_back(NGObject::boolean(!val->boolValue())); break; }
                case OpCode::INSTANCE_OF:
                {
                    uint16_t typeNameIdx = read_u16();
                    Str typeName = current_module->strings[typeNameIdx];
                    auto val = pop();
                    bool result = false;
                    if (val->type() && val->type()->name == typeName) result = true;
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
                case OpCode::LOAD_LOCAL: { stack.push_back(call_stack[frame_idx].locals[read_u16()]); break; }
                case OpCode::LOAD_PARAM: { stack.push_back(call_stack[frame_idx].locals[read_u16()]); break; }
                case OpCode::STORE_LOCAL: { uint16_t idx = read_u16(); auto &locals = call_stack[frame_idx].locals; if (idx >= locals.size()) locals.resize(idx + 1, makert<NGUnit>()); locals[idx] = stack.back(); break; }
                case OpCode::LOAD_GLOBAL: { stack.push_back(globals[read_u16()]); break; }
                case OpCode::STORE_GLOBAL: { uint16_t idx = read_u16(); if (idx >= globals.size()) globals.resize(idx + 1, makert<NGUnit>()); globals[idx] = stack.back(); break; }
                            case OpCode::GET_TUPLE_ITEM:
                            {
                                auto idxObj = pop();
                                auto tupleObj = pop();
                                auto tuple = std::dynamic_pointer_cast<NGTuple>(tupleObj);
                                auto idx = NGIntegral<int32_t>::valueOf(std::dynamic_pointer_cast<NumeralBase>(idxObj).get());
                                stack.push_back((*tuple->items)[idx]);
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
                        current_module = &otherModule;
                        
                        stack.push_back(execute(otherModule.functions[funIdx], callArgs));
                        
                        current_module = saved_module;
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
                auto target = pop();
                if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    if (fieldIdx < structural->fields.size()) {
                        stack.push_back(structural->fields[fieldIdx]);
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
                auto target = pop();
                if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    if (fieldIdx >= structural->fields.size()) structural->fields.resize(fieldIdx + 1, makert<NGUnit>());
                    structural->fields[fieldIdx] = val;
                    // Also update properties map for compatibility
                    if (structural->customizedType && fieldIdx < structural->customizedType->properties.size()) {
                        structural->properties[structural->customizedType->properties[fieldIdx]] = val;
                    }
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
                auto target = pop();
                if (auto tup = std::dynamic_pointer_cast<NGTuple>(target)) {
                    if (propName == "size") {
                        stack.push_back(makert<NGIntegral<int32_t>>(static_cast<int32_t>(tup->items->size())));
                    } else {
                        throw RuntimeException("Tuple has no property: " + propName);
                    }
                } else if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    // Try fields first, then properties map
                    if (structural->customizedType) {
                        auto &props = structural->customizedType->properties;
                        for (size_t i = 0; i < props.size(); ++i) {
                            if (props[i] == propName && i < structural->fields.size()) {
                                stack.push_back(structural->fields[i]);
                                goto next_instruction;
                            }
                        }
                    }
                    if (structural->properties.contains(propName)) {
                        stack.push_back(structural->properties[propName]);
                    } else {
                        throw RuntimeException("Property not found: " + propName);
                    }
                } else {
                    throw RuntimeException("Cannot get property from non-object");
                }
                next_instruction:
                break;
            }
                case OpCode::SET_PROPERTY_STR:
            {
                uint16_t nameIdx = read_u16();
                Str propName = current_module->strings[nameIdx];
                auto val = pop();
                auto target = pop();
                if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(target)) {
                    structural->properties[propName] = val;
                    // Also update fields if we can find the index
                    if (structural->customizedType) {
                        auto &props = structural->customizedType->properties;
                        for (size_t i = 0; i < props.size(); ++i) {
                            if (props[i] == propName) {
                                if (i >= structural->fields.size()) structural->fields.resize(i + 1, makert<NGUnit>());
                                structural->fields[i] = val;
                                break;
                            }
                        }
                    }
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
                    auto idx = pop(); auto obj = pop();
                    stack.push_back(obj->opIndex(idx));
                    break;
                }
                case OpCode::SET_INDEX:
                {
                    auto val = pop(); auto idx = pop(); auto obj = pop();
                    stack.push_back(obj->opIndex(idx, val));
                    break;
                }
                                case OpCode::NEW_OBJECT:
                                {
                                    uint16_t typeStrIdx = read_u16();
                                    Str typeName = current_module->strings[typeStrIdx];
                                    uint16_t numFields = read_u16();

                                    auto obj = makert<NGStructuralObject>();
                                    obj->fields.resize(numFields, makert<NGUnit>());

                                    // Pop values from stack (in reverse order)
                                    for (int i = numFields - 1; i >= 0; --i)
                                    {
                                        obj->fields[i] = pop();
                                    }

                                    // Set type
                                    if (root_types.contains(typeName)) {
                                        obj->customizedType = root_types[typeName];
                                        // Populate properties map from fields for compatibility
                                        auto &typeProps = root_types[typeName]->properties;
                                        for (size_t i = 0; i < typeProps.size() && i < numFields; ++i) {
                                            obj->properties[typeProps[i]] = obj->fields[i];
                                        }
                                    }
                                    stack.push_back(obj);
                                    break;
                                }
                case OpCode::INVOKE_MEMBER:
                {
                    uint16_t nameIdx = read_u16();
                    uint16_t numArgs = read_u16();
                    Str memberName = current_module->strings[nameIdx];
                    Vec<RuntimeRef<NGObject>> callArgs;
                    for (int i = 0; i < numArgs; ++i) callArgs.insert(callArgs.begin(), pop());
                    auto target = pop();
                    
                    Str typeName = target->type() ? target->type()->name : "Object";
                    Str fullFunName = typeName + "." + memberName;
                    
                    int32_t funIdx = -1;
                    for (size_t i = 0; i < current_module->functions.size(); ++i) {
                        if (current_module->functions[i].name == fullFunName) { funIdx = static_cast<int32_t>(i); break; }
                    }
                    
                    if (funIdx != -1) {
                        callArgs.insert(callArgs.begin(), target);
                        stack.push_back(execute(current_module->functions[funIdx], callArgs));
                    } else {
                        auto invCtx = makert<NGInvocationContext>();
                        invCtx->target = target; invCtx->params = std::move(callArgs);
                        stack.push_back(target->respond(memberName, root_context, invCtx));
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
                                elems.insert(elems.end(), tup->items->begin(), tup->items->end());
                            } else if (auto arr = std::dynamic_pointer_cast<NGArray>(segment)) {
                                elems.insert(elems.end(), arr->items->begin(), arr->items->end());
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
                    if (idx < tuple->items->size()) {
                        rest.insert(rest.end(), tuple->items->begin() + idx, tuple->items->end());
                    }
                    stack.push_back(makert<NGTuple>(rest));
                    break;
                }
                case OpCode::LSHIFT: { auto b = pop(); auto a = pop(); stack.push_back(a->opLShift(b)); break; }
                case OpCode::RSHIFT: { auto b = pop(); auto a = pop(); stack.push_back(a->opRShift(b)); break; }
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
                    std::cout << args_to_print[i]->show() << (i == 0 ? "" : ", ");
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
                    if (!val->boolValue()) {
                        std::cerr << "Assertion Failed. Value: " << val->show() << std::endl;
                        throw AssertionException(); 
                    }
                    stack.push_back(makert<NGUnit>()); 
                    break; 
                }
                case OpCode::JUMP: { int32_t target; std::memcpy(&target, &code[ip], 4); ip = target; break; }
                case OpCode::JUMP_IF_FALSE: { int32_t target; std::memcpy(&target, &code[ip], 4); ip += 4; if (!pop()->boolValue()) ip = target; break; }

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
                    if (fieldIdx >= tagged->payload.size()) throw IllegalTypeException("GET_PAYLOAD: index out of bounds");
                    stack.push_back(tagged->payload[fieldIdx]);
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
                    for (uint16_t i = 0; i < numCases; ++i) {
                        uint16_t tag = read_u16();
                        int32_t addr;
                        std::memcpy(&addr, &code[ip], 4);
                        ip += 4;
                        if (static_cast<int32_t>(tag) == tagVal) {
                            ip = static_cast<size_t>(addr);
                            found = true;
                            break;
                        }
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
