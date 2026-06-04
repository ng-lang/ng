#include <orgasm/compiler.hpp>
#include <algorithm>
#include <bit>
#include <array>
#include <limits>
#include <module.hpp>
#include <cstring>
#include <token.hpp>
#include <typecheck/typecheck.hpp>

namespace NG::orgasm
{
    using namespace NG::ast;
    using NG::module::get_module_registry;
    using NG::module::FileBasedExternalModuleLoader;

    namespace
    {
        constexpr uint16_t SWITCH_DEFAULT_TAG = std::numeric_limits<uint16_t>::max();
        constexpr const char *COPY_TRAIT_NAME = "Copy";
        constexpr const char *CLONE_TRAIT_NAME = "Clone";
        constexpr const char *DROP_TRAIT_NAME = "Drop";

        auto bare_type_name(Str typeName) -> Str
        {
            if (auto genericStart = typeName.find('<'); genericStart != Str::npos)
            {
                typeName = typeName.substr(0, genericStart);
            }
            return typeName;
        }

        void install_builtin_lifecycle_traits(Map<Str, Compiler::RuntimeTraitInfo> &runtimeTraits)
        {
            runtimeTraits.try_emplace(COPY_TRAIT_NAME);
            runtimeTraits.try_emplace(CLONE_TRAIT_NAME);
            runtimeTraits.try_emplace(DROP_TRAIT_NAME);
        }

        auto is_self_type_annotation(const TypeAnnotation *annotation) -> bool
        {
            return annotation && annotation->name == "Self" && annotation->genericArgs.empty();
        }

        auto is_ref_self_type_annotation(const TypeAnnotation *annotation) -> bool
        {
            return annotation && annotation->name == "ref" && annotation->genericArgs.size() == 1 &&
                   is_self_type_annotation(annotation->genericArgs[0].get());
        }

        auto is_explicit_receiver_param(const Param *param) -> bool
        {
            return param && (is_self_type_annotation(param->annotatedType.get()) ||
                             is_ref_self_type_annotation(param->annotatedType.get()));
        }

        auto is_variadic_param(const Param *param) -> bool
        {
            return param && param->annotatedType && param->annotatedType->name.size() > 3 &&
                   param->annotatedType->name.ends_with("...");
        }

        auto append_function_if_missing(BytecodeModule &module, Map<Str, FunctionDef*> &functionDefs,
                                        const Str &functionName, FunctionDef *functionDef) -> void
        {
            for (auto &&existing : module.functions)
            {
                if (existing.name == functionName)
                {
                    return;
                }
            }
            Function fun;
            fun.name = functionName;
            const bool explicitReceiver =
                !functionDef->params.empty() && is_explicit_receiver_param(functionDef->params.front().get());
            fun.num_params = static_cast<int32_t>(functionDef->params.size() + (explicitReceiver ? 0 : 1));
            fun.explicit_receiver = explicitReceiver;
            module.functions.push_back(std::move(fun));
            functionDefs[functionName] = functionDef;
        }

        auto resolve_trait_closure(const Str &traitName, const Map<Str, TraitDef *> &traitDefs,
                                   Map<Str, Compiler::RuntimeTraitInfo> &traits, Set<Str> &visiting,
                                   Set<Str> &visited) -> Compiler::RuntimeTraitInfo &
        {
            if (visited.contains(traitName))
            {
                return traits[traitName];
            }
            if (!visiting.insert(traitName).second)
            {
                throw RuntimeException("Cyclic trait inheritance involving " + traitName);
            }
            auto traitDefIt = traitDefs.find(traitName);
            if (traitDefIt == traitDefs.end())
            {
                throw RuntimeException("Unknown trait: " + traitName);
            }
            auto &info = traits[traitName];
            info = Compiler::RuntimeTraitInfo{};
            auto *traitDef = traitDefIt->second;
            for (const auto &superTraitAnnotation : traitDef->superTraits)
            {
                auto superName = superTraitAnnotation->repr();
                auto &superInfo = resolve_trait_closure(superName, traitDefs, traits, visiting, visited);
                for (auto &[methodName, method] : superInfo.methods)
                {
                    info.methods[methodName] = method;
                    info.allMethodOrigins[methodName] =
                        superInfo.allMethodOrigins.contains(methodName) ? superInfo.allMethodOrigins[methodName] : superName;
                }
                for (auto &[methodName, method] : superInfo.allDefaultMethods)
                {
                    if (info.allDefaultMethods.contains(methodName))
                    {
                        throw RuntimeException("Conflicting default trait method " + methodName + " inherited by " + traitName);
                    }
                    info.allDefaultMethods[methodName] = method;
                    info.allDefaultOrigins[methodName] =
                        superInfo.allDefaultOrigins.contains(methodName) ? superInfo.allDefaultOrigins[methodName] : superName;
                }
            }
            for (const auto &method : traitDef->methods)
            {
                info.methods[method->funName] = method.get();
                info.allMethodOrigins[method->funName] = traitName;
                if (method->body)
                {
                    info.allDefaultMethods[method->funName] = method.get();
                    info.allDefaultOrigins[method->funName] = traitName;
                }
                else
                {
                    info.allDefaultMethods.erase(method->funName);
                    info.allDefaultOrigins.erase(method->funName);
                }
            }
            visiting.erase(traitName);
            visited.insert(traitName);
            return info;
        }

        template <typename UInt>
        void append_le_bytes(Vec<uint8_t> &code, UInt value)
        {
            for (size_t i = 0; i < sizeof(UInt); ++i)
            {
                code.push_back(static_cast<uint8_t>((value >> (i * 8U)) & static_cast<UInt>(0xFFU)));
            }
        }
    }

    auto Compiler::compile(ASTRef<CompileUnit> compileUnit) -> BytecodeModule
    {
        auto preludeTypes = NG::typecheck::build_prelude_type_index();
        NG::typecheck::type_check(compileUnit, preludeTypes, modulePaths);
        current_function = nullptr;
        last_emit_was_return = false;
        locals.clear();
        localValueTypes.clear();
        globals.clear();
        globalValueTypes.clear();
        imported_symbols.clear();
        functionDefs.clear();
        genericFunctionDefs.clear();
        importedDefinitions.clear();
        genericFunctionInstances.clear();
        genericFunctionInstanceSet.clear();
        traitDefs.clear();
        runtimeTraits.clear();
        loop_stack.clear();
        current_type_name.clear();
        activeGenericInstanceName.clear();
        variant_map.clear();
        module = BytecodeModule{};
        module.name = compileUnit->module && !compileUnit->module->name.empty()
                          ? compileUnit->module->name
                          : compileUnit->fileName;
        
        module.constants.push_back(0);
        module.constants.push_back(1);
        install_builtin_lifecycle_traits(runtimeTraits);

        compileUnit->module->accept(this);
        if (auto artifact = NG::module::get_module_registry().queryArtifactById(module.name))
        {
            for (const auto &[name, type] : artifact->exports.types)
            {
                if (type)
                {
                    module.exportTypeReprs[name] = type->repr();
                }
            }
            for (const auto &[name, type] : artifact->traits)
            {
                auto trait = std::dynamic_pointer_cast<NG::typecheck::TraitType>(type);
                if (!trait)
                {
                    continue;
                }
                auto methodReprs = [](const Map<Str, NG::typecheck::CheckingRef<NG::typecheck::FunctionType>> &methods) {
                    Map<Str, Str> reprs;
                    for (const auto &[methodName, methodType] : methods)
                    {
                        if (methodType)
                        {
                            reprs.insert_or_assign(methodName, methodType->repr());
                        }
                    }
                    return reprs;
                };
                Vec<Str> superTraits;
                for (const auto &superTrait : trait->superTraits)
                {
                    if (superTrait)
                    {
                        superTraits.push_back(superTrait->repr());
                    }
                }
                module.traitMetadata.push_back(BytecodeTraitMetadata{
                    .name = name,
                    .moduleId = trait->moduleId,
                    .typeParamNames = trait->typeParamNames,
                    .superTraits = std::move(superTraits),
                    .methods = methodReprs(trait->methods),
                    .allMethods = methodReprs(trait->allMethods),
                });
            }
            for (const auto &impl : artifact->impls)
            {
                module.implMetadata.push_back(BytecodeImplMetadata{
                    .traitName = impl.traitName,
                    .targetPattern = impl.targetPattern,
                    .moduleId = impl.moduleId,
                    .genericParamNames = Vec<Str>{impl.genericParamNames.begin(), impl.genericParamNames.end()},
                    .whereBounds = impl.whereBounds,
                    .methods = impl.methods,
                });
            }
        }
        return std::move(module);
    }

    void Compiler::visit(Module *mod)
    {
        // First pass: collect all function signatures and create placeholders
        Function startFun;
        startFun.name = "__start__";
        startFun.num_params = 0;
        module.functions.push_back(std::move(startFun));

        for (auto &&import : mod->imports) {
            import->accept(this);
        }

        for (auto *def : importedDefinitions)
        {
            if (auto traitDef = dynamic_cast<TraitDef *>(def))
            {
                traitDefs[traitDef->traitName] = traitDef;
            }
        }
        for (auto &&def : mod->definitions)
        {
            if (auto traitDef = dynamic_ast_cast<TraitDef>(def))
            {
                traitDefs[traitDef->traitName] = traitDef.get();
            }
        }
        Set<Str> visitingTraits;
        Set<Str> visitedTraits;
        for (auto &&[traitName, _traitDef] : traitDefs)
        {
            if (visitedTraits.contains(traitName))
            {
                continue;
            }
            resolve_trait_closure(traitName, traitDefs, runtimeTraits, visitingTraits, visitedTraits);
        }

        auto registerDefinitionShape = [&](Definition *def) {
            if (auto funDef = dynamic_cast<FunctionDef *>(def))
            {
                if (funDef->deleted || funDef->native)
                {
                    return;
                }
                if (!funDef->genericParams.empty())
                {
                    genericFunctionDefs[funDef->funName] = funDef;
                }
                return;
            }
            if (auto typeDef = dynamic_cast<TypeDef *>(def))
            {
                if (std::ranges::none_of(module.types, [&](const Type &type) { return type.name == typeDef->typeName; }))
                {
                    Type type;
                    type.name = typeDef->typeName;
                    for (auto &&prop : typeDef->properties) type.properties.push_back(prop->propertyName);
                    for (auto &&trait : typeDef->derivedTraits)
                    {
                        if (trait) type.derivedTraits.push_back(trait->repr());
                    }
                    module.types.push_back(std::move(type));
                }
                for (auto &&memFn : typeDef->memberFunctions)
                {
                    append_function_if_missing(module, functionDefs, typeDef->typeName + "." + memFn->funName,
                                               memFn.get());
                }
                return;
            }
            if (auto taggedUnion = dynamic_cast<TaggedUnionDef *>(def))
            {
                if (std::ranges::none_of(module.types, [&](const Type &type) { return type.name == taggedUnion->typeName; }))
                {
                    Type type;
                    type.name = taggedUnion->typeName;
                    for (int32_t i = 0; i < static_cast<int32_t>(taggedUnion->variants.size()); ++i)
                    {
                        Variant v;
                        v.name = taggedUnion->variants[i].variantName;
                        v.payloadFields = taggedUnion->variants[i].payloadNames;
                        type.variants.push_back(std::move(v));

                        variant_map[taggedUnion->variants[i].variantName] = VariantInfo{
                            .unionName = taggedUnion->typeName,
                            .variantIndex = i,
                            .payloadFields = taggedUnion->variants[i].payloadNames,
                            .payloadTypes = [&]() {
                                Vec<Str> types;
                                for (auto &&payloadType : taggedUnion->variants[i].payloadTypes)
                                {
                                    types.push_back(payloadType->repr());
                                }
                                return types;
                            }(),
                        };
                    }
                    module.types.push_back(std::move(type));
                }
                return;
            }
            if (auto implDef = dynamic_cast<ImplDef *>(def))
            {
                auto targetName = bare_type_name(implDef->targetType->repr());
                auto traitName = bare_type_name(implDef->trait->repr());
                for (auto &&method : implDef->methods)
                {
                    append_function_if_missing(module, functionDefs, targetName + "." + traitName + "::" + method->funName,
                                               method.get());
                    append_function_if_missing(module, functionDefs, targetName + "." + method->funName, method.get());
                }
            }
        };

        for (auto *def : importedDefinitions)
        {
            registerDefinitionShape(def);
        }
        for (auto &&def : mod->definitions)
        {
            if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
            {
                if (funDef->deleted)
                {
                    continue;
                }
                if (!funDef->genericParams.empty())
                {
                    genericFunctionDefs[funDef->funName] = funDef.get();
                    continue;
                }
                Function fun;
                fun.name = funDef->funName;
                fun.num_params = static_cast<int32_t>(funDef->params.size());
                module.functions.push_back(std::move(fun));
                functionDefs[funDef->funName] = funDef.get();
            }
            else if (auto valDef = dynamic_ast_cast<ValDef>(def))
            {
                if (auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body))
                {
                    globals[valStmt->name] = static_cast<int32_t>(globals.size());
                    if (valStmt->typeAnnotation)
                    {
                        globalValueTypes[valStmt->name] = valStmt->typeAnnotation->repr();
                    }
                    else if (auto inferred = infer_expression_type_name(valStmt->value); !inferred.empty())
                    {
                        if (auto traitName = trait_ref_name_from_type_repr(inferred); !traitName.empty())
                        {
                            globalTraitObjectTypes[valStmt->name] = traitName;
                        }
                        globalValueTypes[valStmt->name] = std::move(inferred);
                    }
                    if (auto traitName = trait_ref_name(valStmt->typeAnnotation.get()); !traitName.empty())
                    {
                        globalTraitObjectTypes[valStmt->name] = traitName;
                    }
                }
                else if (auto valBind = dynamic_ast_cast<ValueBindingStatement>(valDef->body))
                {
                    for (auto &&binding : valBind->bindings)
                    {
                        if (!binding->name.empty())
                            globals[binding->name] = static_cast<int32_t>(globals.size());
                    }
                }
            }
            else if (auto typeDef = dynamic_ast_cast<TypeDef>(def))
            {
                Type type;
                type.name = typeDef->typeName;
                for (auto &&prop : typeDef->properties) type.properties.push_back(prop->propertyName);
                for (auto &&trait : typeDef->derivedTraits)
                {
                    if (trait) type.derivedTraits.push_back(trait->repr());
                }
                module.types.push_back(std::move(type));

                for (auto &&memFn : typeDef->memberFunctions)
                {
                    Function fun;
                    fun.name = typeDef->typeName + "." + memFn->funName;
                    const bool explicitReceiver = !memFn->params.empty() && is_explicit_receiver_param(memFn->params.front().get());
                    fun.num_params = static_cast<int32_t>(memFn->params.size() + (explicitReceiver ? 0 : 1));
                    fun.explicit_receiver = explicitReceiver;
                    module.functions.push_back(std::move(fun));
                    functionDefs[fun.name] = memFn.get();
                }
            }
            else if (auto implDef = dynamic_ast_cast<ImplDef>(def))
            {
                auto targetName = bare_type_name(implDef->targetType->repr());
                auto traitName = bare_type_name(implDef->trait->repr());
                Map<Str, FunctionDef*> providedMethods;
                for (auto &&method : implDef->methods)
                {
                    providedMethods[method->funName] = method.get();
                    auto originTraitName = traitName;
                    if (runtimeTraits.contains(traitName) &&
                        runtimeTraits[traitName].allMethodOrigins.contains(method->funName))
                    {
                        originTraitName = runtimeTraits[traitName].allMethodOrigins[method->funName];
                    }
                    append_function_if_missing(module, functionDefs,
                                               targetName + "." + originTraitName + "::" + method->funName, method.get());
                    if (originTraitName != traitName)
                    {
                        append_function_if_missing(module, functionDefs,
                                                   targetName + "." + traitName + "::" + method->funName, method.get());
                    }
                    append_function_if_missing(module, functionDefs, targetName + "." + method->funName, method.get());
                }
                if (runtimeTraits.contains(traitName))
                {
                    for (auto &&[methodName, defaultMethod] : runtimeTraits[traitName].allDefaultMethods)
                    {
                        if (providedMethods.contains(methodName))
                        {
                            continue;
                        }
                        auto originTraitName = runtimeTraits[traitName].allDefaultOrigins.contains(methodName)
                                                   ? runtimeTraits[traitName].allDefaultOrigins[methodName]
                                                   : traitName;
                        append_function_if_missing(module, functionDefs, targetName + "." + originTraitName + "::" + methodName,
                                                   defaultMethod);
                        if (originTraitName != traitName)
                        {
                            append_function_if_missing(module, functionDefs, targetName + "." + traitName + "::" + methodName,
                                                       defaultMethod);
                        }
                        append_function_if_missing(module, functionDefs, targetName + "." + methodName, defaultMethod);
                    }
                }
            }
            else if (auto aliasDef = dynamic_ast_cast<TypeAliasDef>(def))
            {
                if (aliasDef->specializationPattern || aliasDef->deleted || aliasDef->abstract)
                {
                    continue;
                }
                // Native opaque aliases are nominal runtime types; transparent aliases are registered without fields.
                Type type;
                type.name = aliasDef->aliasName;
                module.types.push_back(std::move(type));
            }
            else if (auto newTypeDef = dynamic_ast_cast<NewTypeDef>(def))
            {
                // Newtype — register as a type with no properties
                Type type;
                type.name = newTypeDef->typeName;
                module.types.push_back(std::move(type));
            }
            else if (auto taggedUnion = dynamic_ast_cast<TaggedUnionDef>(def))
            {
                Type type;
                type.name = taggedUnion->typeName;
                for (int32_t i = 0; i < static_cast<int32_t>(taggedUnion->variants.size()); ++i)
                {
                    Variant v;
                    v.name = taggedUnion->variants[i].variantName;
                    v.payloadFields = taggedUnion->variants[i].payloadNames;
                    type.variants.push_back(std::move(v));

                    variant_map[taggedUnion->variants[i].variantName] = VariantInfo{
                        .unionName = taggedUnion->typeName,
                        .variantIndex = i,
                        .payloadFields = taggedUnion->variants[i].payloadNames,
                        .payloadTypes = [&]() {
                            Vec<Str> types;
                            for (auto &&payloadType : taggedUnion->variants[i].payloadTypes)
                            {
                                types.push_back(payloadType->repr());
                            }
                            return types;
                        }(),
                    };
                }
                module.types.push_back(std::move(type));
            }
        }

        for (auto &&def : mod->definitions)
        {
            collect_generic_function_instances(def);
        }
        for (auto &&stmt : mod->statements)
        {
            collect_generic_function_instances(stmt);
        }
        for (size_t i = 0; i < genericFunctionInstances.size(); ++i)
        {
            const auto &instanceName = genericFunctionInstances[i];
            auto *genericDef = functionDefs.contains(instanceName) ? functionDefs[instanceName] : nullptr;
            if (genericDef)
            {
                collect_generic_function_instances(genericDef->body, instanceName);
            }
        }

        if (std::ranges::find(mod->exports, "*") != mod->exports.end())
        {
            for (size_t i = 0; i < module.functions.size(); ++i)
            {
                const auto &name = module.functions[i].name;
                if (name != "__start__" && !genericFunctionInstanceSet.contains(name))
                {
                    module.exports[name] = static_cast<int32_t>(i);
                }
            }
        }
        for (auto &&exportedName : mod->exports)
        {
            if (exportedName == "*")
            {
                continue;
            }
            for (size_t i = 0; i < module.functions.size(); ++i)
            {
                if (module.functions[i].name == exportedName)
                {
                    module.exports[exportedName] = static_cast<int32_t>(i);
                    break;
                }
            }
        }

        // Second pass: compile top-level code into __start__ (at index 0)
        current_function = &module.functions[0];
        last_emit_was_return = false;
        locals.clear();
        localValueTypes.clear();
        loop_stack.clear();

        for (auto &&def : mod->definitions)
        {
            if (auto valDef = dynamic_ast_cast<ValDef>(def))
            {
                if (auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body))
                {
                    valStmt->value->accept(this);
                    emit_trait_ref_if_needed(valStmt->typeAnnotation.get());
                    emit(OpCode::STORE_GLOBAL);
                    emit_u16(static_cast<uint16_t>(globals[valStmt->name]));
                    emit(OpCode::POP);
                }
                else if (auto valBind = dynamic_ast_cast<ValueBindingStatement>(valDef->body))
                {
                    valBind->value->accept(this);
                    for (size_t i = 0; i < valBind->bindings.size(); ++i)
                    {
                        auto binding = valBind->bindings[i];
                        if (binding->name.empty()) continue;
                        emit(OpCode::DUP);
                        emit(OpCode::PUSH_I32);
                        emit_i32(static_cast<int32_t>(binding->index));
                        if (valBind->type == BindingType::TUPLE_UNPACK) {
                            if (binding->spreadReceiver) emit(OpCode::GET_TUPLE_REST);
                            else emit(OpCode::GET_TUPLE_ITEM);
                        }
                        else emit(OpCode::GET_INDEX);
                        emit(OpCode::STORE_GLOBAL);
                        emit_u16(static_cast<uint16_t>(globals[binding->name]));
                        emit(OpCode::POP);
                    }
                    emit(OpCode::POP);
                }
            }
        }

        for (auto &&stmt : mod->statements)
        {
            stmt->accept(this);
        }

        module.functions[0].num_locals = static_cast<int32_t>(locals.size());
        emit(OpCode::PUSH_UNIT);
        emit(OpCode::RETURN);

        // Third pass: compile function bodies
        int funIndex = 1;
        for (auto *importedDef : importedDefinitions)
        {
            auto *implDef = dynamic_cast<ImplDef *>(importedDef);
            if (!implDef)
            {
                continue;
            }
            auto targetName = bare_type_name(implDef->targetType->repr());
            auto traitName = bare_type_name(implDef->trait->repr());
            for (auto &&method : implDef->methods)
            {
                auto previousTraitMethodOrigin = activeTraitMethodOrigin;
                activeTraitMethodOrigin = traitName;
                if (auto *targetFunction = find_function(targetName + "." + traitName + "::" + method->funName))
                {
                    compile_function_body(method.get(), *targetFunction, false);
                }
                if (auto *targetFunction = find_function(targetName + "." + method->funName))
                {
                    compile_function_body(method.get(), *targetFunction, false);
                }
                activeTraitMethodOrigin = previousTraitMethodOrigin;
            }
        }
        for (auto &&def : mod->definitions)
        {
            if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
            {
                if (funDef->deleted)
                {
                    continue;
                }
                if (!funDef->genericParams.empty())
                {
                    continue;
                }
                current_function = &module.functions[funIndex++];
                last_emit_was_return = false;
                locals.clear();
                localTraitObjectTypes.clear();
                localValueTypes.clear();
                loop_stack.clear();
                
                LoopInfo info;
                info.startIp = 0; // Function start
                for (int32_t i = 0; i < funDef->params.size(); ++i)
                {
                    locals[funDef->params[i]->paramName] = i;
                    if (funDef->params[i]->annotatedType)
                    {
                        localValueTypes[funDef->params[i]->paramName] = funDef->params[i]->annotatedType->repr();
                    }
                    if (auto traitName = trait_ref_name(funDef->params[i]->annotatedType.get()); !traitName.empty())
                    {
                        localTraitObjectTypes[funDef->params[i]->paramName] = traitName;
                    }
                    info.bindingSlots.push_back(i);
                }
                loop_stack.push_back(std::move(info));

                if (funDef->body) funDef->body->accept(this);

                loop_stack.pop_back();
                current_function->num_locals = static_cast<int32_t>(locals.size());

                if (!last_emit_was_return)
                {
                    emit(OpCode::PUSH_UNIT);
                    emit(OpCode::RETURN);
                }
            }
            else if (auto typeDef = dynamic_ast_cast<TypeDef>(def))
            {
                for (auto &&memFn : typeDef->memberFunctions)
                {
                    current_function = &module.functions[funIndex++];
                    last_emit_was_return = false;
                    locals.clear();
                    localTraitObjectTypes.clear();
                    localValueTypes.clear();
                    loop_stack.clear();
                    current_type_name = typeDef->typeName;
                    const bool explicitReceiver = !memFn->params.empty() && is_explicit_receiver_param(memFn->params.front().get());
                    
                    LoopInfo info;
                    info.startIp = 0;
                    int32_t paramBase = 0;
                    if (!explicitReceiver)
                    {
                        locals["self"] = 0;
                        info.bindingSlots.push_back(0); // self can be updated by next? maybe not, but keep slot.
                        paramBase = 1;
                    }
                    
                    for (int32_t i = 0; i < memFn->params.size(); ++i)
                    {
                        locals[memFn->params[i]->paramName] = i + paramBase;
                        if (memFn->params[i]->annotatedType)
                        {
                            localValueTypes[memFn->params[i]->paramName] = memFn->params[i]->annotatedType->repr();
                        }
                        if (auto traitName = trait_ref_name(memFn->params[i]->annotatedType.get()); !traitName.empty())
                        {
                            localTraitObjectTypes[memFn->params[i]->paramName] = traitName;
                        }
                        info.bindingSlots.push_back(i + paramBase);
                    }
                    loop_stack.push_back(std::move(info));

                    if (memFn->body) memFn->body->accept(this);

                    loop_stack.pop_back();
                    current_function->num_locals = static_cast<int32_t>(locals.size());

                    if (!last_emit_was_return)
                    {
                        emit(OpCode::PUSH_UNIT);
                        emit(OpCode::RETURN);
                    }
                }
            }
            else if (auto implDef = dynamic_ast_cast<ImplDef>(def))
            {
                current_type_name = bare_type_name(implDef->targetType->repr());
                auto traitName = bare_type_name(implDef->trait->repr());
                Map<Str, FunctionDef*> providedMethods;
                for (auto &&method : implDef->methods)
                {
                    providedMethods[method->funName] = method.get();
                }
                auto compileMethodFunction = [&](FunctionDef *method, const Str &functionName, const Str &traitOrigin = Str{}) {
                    if (funIndex >= static_cast<int>(module.functions.size()) ||
                        module.functions[funIndex].name != functionName)
                    {
                        return;
                    }
                    auto previousTraitMethodOrigin = activeTraitMethodOrigin;
                    activeTraitMethodOrigin = traitOrigin;
                    current_function = &module.functions[funIndex++];
                    last_emit_was_return = false;
                    locals.clear();
                    localTraitObjectTypes.clear();
                    localValueTypes.clear();
                    loop_stack.clear();

                    const bool explicitReceiver =
                        !method->params.empty() && is_explicit_receiver_param(method->params.front().get());
                    LoopInfo info;
                    info.startIp = 0;
                    int32_t paramBase = 0;
                    if (!explicitReceiver)
                    {
                        locals["self"] = 0;
                        info.bindingSlots.push_back(0);
                        paramBase = 1;
                    }
                    for (int32_t i = 0; i < method->params.size(); ++i)
                    {
                        locals[method->params[i]->paramName] = i + paramBase;
                        if (method->params[i]->annotatedType)
                        {
                            localValueTypes[method->params[i]->paramName] = method->params[i]->annotatedType->repr();
                        }
                        if (auto traitName = trait_ref_name(method->params[i]->annotatedType.get()); !traitName.empty())
                        {
                            localTraitObjectTypes[method->params[i]->paramName] = traitName;
                        }
                        info.bindingSlots.push_back(i + paramBase);
                    }
                    loop_stack.push_back(std::move(info));

                    if (method->body) method->body->accept(this);
                    activeTraitMethodOrigin = previousTraitMethodOrigin;

                    loop_stack.pop_back();
                    current_function->num_locals = static_cast<int32_t>(locals.size());
                    if (!last_emit_was_return)
                    {
                        emit(OpCode::PUSH_UNIT);
                        emit(OpCode::RETURN);
                    }
                };
                for (auto &&method : implDef->methods)
                {
                    auto originTraitName = traitName;
                    if (runtimeTraits.contains(traitName) &&
                        runtimeTraits[traitName].allMethodOrigins.contains(method->funName))
                    {
                        originTraitName = runtimeTraits[traitName].allMethodOrigins[method->funName];
                    }
                    compileMethodFunction(method.get(), current_type_name + "." + originTraitName + "::" + method->funName,
                                          originTraitName);
                    if (originTraitName != traitName)
                    {
                        compileMethodFunction(method.get(),
                                              current_type_name + "." + traitName + "::" + method->funName, traitName);
                    }
                    compileMethodFunction(method.get(), current_type_name + "." + method->funName);
                }
                if (runtimeTraits.contains(traitName))
                {
                    for (auto &&[methodName, defaultMethod] : runtimeTraits[traitName].allDefaultMethods)
                    {
                        if (providedMethods.contains(methodName))
                        {
                            continue;
                        }
                        auto originTraitName = runtimeTraits[traitName].allDefaultOrigins.contains(methodName)
                                                   ? runtimeTraits[traitName].allDefaultOrigins[methodName]
                                                   : traitName;
                        compileMethodFunction(defaultMethod, current_type_name + "." + originTraitName + "::" + methodName,
                                              originTraitName);
                        if (originTraitName != traitName)
                        {
                            compileMethodFunction(defaultMethod, current_type_name + "." + traitName + "::" + methodName,
                                                  originTraitName);
                        }
                        compileMethodFunction(defaultMethod, current_type_name + "." + methodName);
                    }
                }
            }
        }
        for (const auto &instanceName : genericFunctionInstances)
        {
            auto *funDef = functionDefs.contains(instanceName) ? functionDefs[instanceName] : nullptr;
            auto *targetFunction = find_function(instanceName);
            if (!funDef || !targetFunction)
            {
                continue;
            }
            activeGenericInstanceName = instanceName;
            compile_function_body(funDef, *targetFunction, false);
            activeGenericInstanceName.clear();
        }
        current_function = nullptr;
    }

    void Compiler::visit(ast::FunctionDef *funDef) {}
    void Compiler::visit(ast::TypeDef *typeDef) {}
    void Compiler::visit(ast::ImplDef *implDef) {}
    void Compiler::visit(ast::TypeAliasDef *typeAliasDef) {}
    void Compiler::visit(ast::NewTypeDef *newTypeDef) {}

    auto Compiler::find_function(const Str &name) -> Function *
    {
        for (auto &function : module.functions)
        {
            if (function.name == name)
            {
                return &function;
            }
        }
        return nullptr;
    }

    auto Compiler::find_function_index(const Str &name) const -> int32_t
    {
        for (size_t i = 0; i < module.functions.size(); ++i)
        {
            if (module.functions[i].name == name)
            {
                return static_cast<int32_t>(i);
            }
        }
        return -1;
    }

    void Compiler::register_generic_function_instance(const Str &symbolName, FunctionDef *funDef)
    {
        if (symbolName.empty() || !funDef)
        {
            return;
        }
        if (!genericFunctionInstanceSet.insert(symbolName).second)
        {
            return;
        }
        Function fun;
        fun.name = symbolName;
        fun.num_params = static_cast<int32_t>(funDef->params.size());
        fun.explicit_receiver = false;
        module.functions.push_back(std::move(fun));
        functionDefs[symbolName] = funDef;
        genericFunctionInstances.push_back(symbolName);
    }

    void Compiler::compile_function_body(FunctionDef *funDef, Function &targetFunction, bool allowImplicitSelf)
    {
        current_function = &targetFunction;
        last_emit_was_return = false;
        locals.clear();
        localTraitObjectTypes.clear();
        localValueTypes.clear();
        loop_stack.clear();

        const bool explicitReceiver =
            !funDef->params.empty() && is_explicit_receiver_param(funDef->params.front().get());
        LoopInfo info;
        info.startIp = 0;
        int32_t paramBase = 0;
        if (allowImplicitSelf && !explicitReceiver)
        {
            locals["self"] = 0;
            info.bindingSlots.push_back(0);
            paramBase = 1;
        }
        for (int32_t i = 0; i < static_cast<int32_t>(funDef->params.size()); ++i)
        {
            locals[funDef->params[i]->paramName] = i + paramBase;
            if (funDef->params[i]->annotatedType)
            {
                localValueTypes[funDef->params[i]->paramName] = funDef->params[i]->annotatedType->repr();
            }
            if (auto traitName = trait_ref_name(funDef->params[i]->annotatedType.get()); !traitName.empty())
            {
                localTraitObjectTypes[funDef->params[i]->paramName] = traitName;
            }
            info.bindingSlots.push_back(i + paramBase);
        }
        loop_stack.push_back(std::move(info));

        if (funDef->body)
        {
            funDef->body->accept(this);
        }

        loop_stack.pop_back();
        current_function->num_locals = static_cast<int32_t>(locals.size());
        if (!last_emit_was_return)
        {
            emit(OpCode::PUSH_UNIT);
            emit(OpCode::RETURN);
        }
    }

    void Compiler::collect_generic_function_instances(ASTRef<Definition> def, const Str &instanceContext)
    {
        if (!def)
        {
            return;
        }
        if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
        {
            collect_generic_function_instances(funDef->body, instanceContext);
        }
        else if (auto valDef = dynamic_ast_cast<ValDef>(def))
        {
            collect_generic_function_instances(valDef->body, instanceContext);
        }
        else if (auto typeDef = dynamic_ast_cast<TypeDef>(def))
        {
            for (auto &method : typeDef->memberFunctions)
            {
                collect_generic_function_instances(method->body, instanceContext);
            }
        }
        else if (auto implDef = dynamic_ast_cast<ImplDef>(def))
        {
            for (auto &method : implDef->methods)
            {
                collect_generic_function_instances(method->body, instanceContext);
            }
        }
    }

    void Compiler::collect_generic_function_instances(ASTRef<Statement> stmt, const Str &instanceContext)
    {
        if (!stmt)
        {
            return;
        }
        if (auto compound = dynamic_ast_cast<CompoundStatement>(stmt))
        {
            for (auto &child : compound->statements)
            {
                collect_generic_function_instances(child, instanceContext);
            }
        }
        else if (auto simple = dynamic_ast_cast<SimpleStatement>(stmt))
        {
            collect_generic_function_instances(simple->expression, instanceContext);
        }
        else if (auto ret = dynamic_ast_cast<ReturnStatement>(stmt))
        {
            collect_generic_function_instances(ret->expression, instanceContext);
        }
        else if (auto ifStmt = dynamic_ast_cast<IfStatement>(stmt))
        {
            collect_generic_function_instances(ifStmt->testing, instanceContext);
            collect_generic_function_instances(ifStmt->consequence, instanceContext);
            collect_generic_function_instances(ifStmt->alternative, instanceContext);
        }
        else if (auto val = dynamic_ast_cast<ValDefStatement>(stmt))
        {
            collect_generic_function_instances(val->value, instanceContext);
        }
        else if (auto bind = dynamic_ast_cast<ValueBindingStatement>(stmt))
        {
            collect_generic_function_instances(bind->value, instanceContext);
        }
        else if (auto loop = dynamic_ast_cast<LoopStatement>(stmt))
        {
            for (auto &binding : loop->bindings)
            {
                collect_generic_function_instances(binding.target, instanceContext);
            }
            collect_generic_function_instances(loop->loopBody, instanceContext);
        }
        else if (auto next = dynamic_ast_cast<NextStatement>(stmt))
        {
            for (auto &expr : next->expressions)
            {
                collect_generic_function_instances(expr, instanceContext);
            }
        }
        else if (auto switchStmt = dynamic_ast_cast<SwitchStatement>(stmt))
        {
            collect_generic_function_instances(switchStmt->scrutinee, instanceContext);
            for (auto &caseClause : switchStmt->cases)
            {
                collect_generic_function_instances(caseClause.body, instanceContext);
            }
        }
    }

    void Compiler::collect_generic_function_instances(ASTRef<Expression> expr, const Str &instanceContext)
    {
        if (!expr)
        {
            return;
        }
        if (auto call = dynamic_ast_cast<FunCallExpression>(expr))
        {
            Str symbol = call->mangledCalleeName;
            if (!instanceContext.empty())
            {
                if (auto it = call->mangledCalleeNameByInstance.find(instanceContext);
                    it != call->mangledCalleeNameByInstance.end())
                {
                    symbol = it->second;
                }
            }
            if (!symbol.empty())
            {
                if (auto id = dynamic_ast_cast<IdExpression>(call->primaryExpression))
                {
                    if (auto defIt = genericFunctionDefs.find(id->id); defIt != genericFunctionDefs.end())
                    {
                        register_generic_function_instance(symbol, defIt->second);
                    }
                }
            }
            collect_generic_function_instances(call->primaryExpression, instanceContext);
            for (auto &arg : call->arguments)
            {
                collect_generic_function_instances(arg, instanceContext);
            }
        }
        else if (auto accessor = dynamic_ast_cast<IdAccessorExpression>(expr))
        {
            collect_generic_function_instances(accessor->primaryExpression, instanceContext);
            collect_generic_function_instances(accessor->accessor, instanceContext);
            for (auto &arg : accessor->arguments)
            {
                collect_generic_function_instances(arg, instanceContext);
            }
        }
        else if (auto qualified = dynamic_ast_cast<QualifiedTraitCallExpression>(expr))
        {
            collect_generic_function_instances(qualified->receiver, instanceContext);
            for (auto &arg : qualified->arguments)
            {
                collect_generic_function_instances(arg, instanceContext);
            }
        }
        else if (auto index = dynamic_ast_cast<IndexAccessorExpression>(expr))
        {
            collect_generic_function_instances(index->primary, instanceContext);
            collect_generic_function_instances(index->accessor, instanceContext);
        }
        else if (auto indexAssign = dynamic_ast_cast<IndexAssignmentExpression>(expr))
        {
            collect_generic_function_instances(indexAssign->primary, instanceContext);
            collect_generic_function_instances(indexAssign->accessor, instanceContext);
            collect_generic_function_instances(indexAssign->value, instanceContext);
        }
        else if (auto typeCheck = dynamic_ast_cast<TypeCheckingExpression>(expr))
        {
            collect_generic_function_instances(typeCheck->value, instanceContext);
        }
        else if (auto assign = dynamic_ast_cast<AssignmentExpression>(expr))
        {
            collect_generic_function_instances(assign->target, instanceContext);
            collect_generic_function_instances(assign->value, instanceContext);
        }
        else if (auto unary = dynamic_ast_cast<UnaryExpression>(expr))
        {
            collect_generic_function_instances(unary->operand, instanceContext);
        }
        else if (auto binary = dynamic_ast_cast<BinaryExpression>(expr))
        {
            collect_generic_function_instances(binary->left, instanceContext);
            collect_generic_function_instances(binary->right, instanceContext);
        }
        else if (auto array = dynamic_ast_cast<ArrayLiteral>(expr))
        {
            for (auto &element : array->elements)
            {
                collect_generic_function_instances(element, instanceContext);
            }
        }
        else if (auto tuple = dynamic_ast_cast<TupleLiteral>(expr))
        {
            for (auto &element : tuple->elements)
            {
                collect_generic_function_instances(element, instanceContext);
            }
        }
        else if (auto typeofExpr = dynamic_ast_cast<TypeOfExpression>(expr))
        {
            collect_generic_function_instances(typeofExpr->expression, instanceContext);
        }
        else if (auto spread = dynamic_ast_cast<SpreadExpression>(expr))
        {
            collect_generic_function_instances(spread->expression, instanceContext);
        }
        else if (auto cast = dynamic_ast_cast<CastExpression>(expr))
        {
            collect_generic_function_instances(cast->expression, instanceContext);
        }
        else if (auto newObj = dynamic_ast_cast<NewObjectExpression>(expr))
        {
            for (auto &[_, value] : newObj->properties)
            {
                collect_generic_function_instances(value, instanceContext);
            }
        }
        else if (auto tagged = dynamic_ast_cast<TaggedValueExpression>(expr))
        {
            for (auto &payload : tagged->payload)
            {
                collect_generic_function_instances(payload, instanceContext);
            }
        }
    }

    void Compiler::visit(ast::CastExpression *castExpr)
    {
        castExpr->expression->accept(this);

        Str targetTypeName;
        if (auto anno = dynamic_ast_cast<ast::TypeAnnotation>(castExpr->targetType))
        {
            targetTypeName = anno->repr();
        }
        else
        {
            throw NotImplementedException("Cast target must be a simple type name");
        }

        uint16_t typeIndex = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(targetTypeName);
        emit(OpCode::WRAP_NEWTYPE);
        emit_u16(typeIndex);
    }

    void Compiler::visit(ast::ImportDecl *importDecl)
    {
        auto &registry = NG::module::get_module_registry();
        auto moduleId = NG::module::canonical_module_id(importDecl->modulePath);
        if (moduleId.empty()) moduleId = importDecl->module;
        auto addShortImport = [&](const Str &name) {
            if (auto existing = imported_symbols.find(name); existing != imported_symbols.end())
            {
                if (existing->second.moduleName == moduleId)
                {
                    return;
                }
                throw RuntimeException("Import conflict for symbol: " + name);
            }
            int32_t importIdx = static_cast<int32_t>(module.imports.size());
            module.imports.push_back({moduleId, name});
            imported_symbols[name] = {moduleId, importIdx};
        };

        if (auto artifact = registry.queryArtifactById(moduleId);
            artifact && artifact->format == NG::module::ModuleFormat::Native)
        {
            const bool importAll = std::ranges::find(importDecl->imports, "*") != importDecl->imports.end();
            Vec<Str> names;
            if (importAll)
            {
                for (const auto &[name, _type] : artifact->exports.types)
                {
                    names.push_back(name);
                }
            }
            else
            {
                names = importDecl->imports;
            }
            if (importDecl->imports.empty())
            {
                auto alias = importDecl->alias.empty() ? importDecl->module : importDecl->alias;
                auto &qualified = qualified_import_symbols[alias];
                for (const auto &[name, _type] : artifact->exports.types)
                {
                    int32_t importIdx = static_cast<int32_t>(module.imports.size());
                    module.imports.push_back({moduleId, name});
                    qualified[name] = {moduleId, importIdx};
                }
                return;
            }
            for (auto &&name : names)
            {
                addShortImport(name);
            }
            return;
        }

        auto moduleInfo = registry.queryModuleById(moduleId);
        
        if (!moduleInfo) {
            NG::module::FileBasedExternalModuleLoader loader{modulePaths};
            moduleInfo = loader.load(importDecl->modulePath);
            if (!moduleInfo) {
                throw RuntimeException("Failed to load module " + moduleId);
            }
            registry.addModuleInfo(moduleInfo);
        }
        if (!moduleInfo->bytecodeModule) {
            static Set<Str> compilingModules;
            if (!compilingModules.insert(moduleId).second)
            {
                throw RuntimeException("Cyclic source module compilation detected: " + moduleId);
            }
            Compiler inner(modulePaths);
            auto cu = dynamic_ast_cast<CompileUnit>(moduleInfo->moduleAst);
            try
            {
                if (!cu) throw RuntimeException("Failed to cast AST to CompileUnit for module " + moduleId);
                auto bc = inner.compile(cu);
                moduleInfo->bytecodeModule = std::make_shared<BytecodeModule>(std::move(bc));
                registry.addModuleInfo(moduleInfo);
                compilingModules.erase(moduleId);
            }
            catch (...)
            {
                compilingModules.erase(moduleId);
                throw;
            }
        }
        if (auto cu = dynamic_ast_cast<CompileUnit>(moduleInfo->moduleAst); cu && cu->module)
        {
            for (auto &&def : cu->module->definitions)
            {
                importedDefinitions.push_back(def.get());
            }
        }

        auto bc = moduleInfo->bytecodeModule;
        if (importDecl->imports.empty())
        {
            auto alias = importDecl->alias.empty() ? importDecl->module : importDecl->alias;
            auto &qualified = qualified_import_symbols[alias];
            for (auto &&[name, index] : bc->exports)
            {
                int32_t importIdx = static_cast<int32_t>(module.imports.size());
                module.imports.push_back({moduleId, name});
                qualified[name] = {moduleId, importIdx};
            }
            return;
        }
        for (auto &&imp : importDecl->imports) {
            if (imp == "*") {
                for (auto &&[name, index] : bc->exports) {
                    addShortImport(name);
                }
            } else {
                addShortImport(imp);
            }
        }
    }

    auto Compiler::emit_call_arguments(const Vec<ASTRef<Expression>> &arguments) -> uint16_t
    {
        uint16_t emitted = 0;
        for (auto &&arg : arguments)
        {
            if (auto spread = dynamic_ast_cast<SpreadExpression>(arg))
            {
                auto spreadTypeName = infer_expression_type_name(spread->expression);
                if (spreadTypeName.size() < 2 || spreadTypeName.front() != '(' || spreadTypeName.back() != ')')
                {
                    throw NotImplementedException("ORGASM spread call arguments require tuple-valued expressions");
                }
                auto tupleArityFromTypeName = [](const Str &typeName) -> size_t {
                    Str inner = typeName.substr(1, typeName.size() - 2);
                    if (inner.empty())
                    {
                        return 0;
                    }
                    size_t arity = 1;
                    int depth = 0;
                    for (char ch : inner)
                    {
                        if (ch == '<' || ch == '(' || ch == '[')
                        {
                            ++depth;
                        }
                        else if (ch == '>' || ch == ')' || ch == ']')
                        {
                            --depth;
                        }
                        else if (ch == ',' && depth == 0)
                        {
                            ++arity;
                        }
                    }
                    return arity;
                };
                auto arity = tupleArityFromTypeName(spreadTypeName);
                int32_t tempIndex = static_cast<int32_t>(locals.size());
                auto tempName = "$$spread_tmp" + std::to_string(tempIndex);
                locals[tempName] = tempIndex;
                spread->expression->accept(this);
                emit(OpCode::STORE_LOCAL);
                emit_u16(static_cast<uint16_t>(tempIndex));
                emit(OpCode::POP);
                for (size_t i = 0; i < arity; ++i)
                {
                    emit(OpCode::LOAD_LOCAL);
                    emit_u16(static_cast<uint16_t>(tempIndex));
                    emit(OpCode::PUSH_I32);
                    emit_i32(static_cast<int32_t>(i));
                    emit(OpCode::GET_TUPLE_ITEM);
                    ++emitted;
                }
                continue;
            }
            arg->accept(this);
            ++emitted;
        }
        return emitted;
    }

    void Compiler::visit(ast::FunCallExpression *funCallExpr)
    {
        auto foldIt = std::find_if(funCallExpr->arguments.begin(), funCallExpr->arguments.end(), [](const auto &arg) {
            return dynamic_ast_cast<PostfixFoldExpression>(arg) != nullptr;
        });
        if (foldIt != funCallExpr->arguments.end())
        {
            auto target = dynamic_ast_cast<IdExpression>(funCallExpr->primaryExpression);
            if (!target)
            {
                throw NotImplementedException("ORGASM fold calls require a direct function name");
            }
            int32_t funIndex = -1;
            for (size_t i = 0; i < module.functions.size(); ++i)
            {
                if (module.functions[i].name == target->id)
                {
                    funIndex = static_cast<int32_t>(i);
                    break;
                }
            }
            if (funIndex < 0)
            {
                throw NotImplementedException("ORGASM fold calls only support local functions: " + target->id);
            }
            auto foldIndex = static_cast<size_t>(std::distance(funCallExpr->arguments.begin(), foldIt));
            if (funCallExpr->arguments.size() != 2 || (foldIndex != 0 && foldIndex != 1))
            {
                throw NotImplementedException("Fold call expects `op(xs..., init)` or `op(init, xs...)`");
            }
            auto fold = dynamic_ast_cast<PostfixFoldExpression>(*foldIt);
            if (fold->filter)
            {
                throw NotImplementedException("Filter marker `?...` is only supported in array literals");
            }
            if (foldIndex == 0)
            {
                fold->expression->accept(this);
                funCallExpr->arguments[1]->accept(this);
                emit(OpCode::FOLD_RIGHT_CALL);
            }
            else
            {
                funCallExpr->arguments[0]->accept(this);
                fold->expression->accept(this);
                emit(OpCode::FOLD_LEFT_CALL);
            }
            emit_u16(static_cast<uint16_t>(funIndex));
            return;
        }

        if (auto idExpr = dynamic_ast_cast<IdExpression>(funCallExpr->primaryExpression))
        {
            if (idExpr->id == "print")
            {
                auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                emit(OpCode::PRINT);
                emit_u16(emittedArgs);
                return;
            }
            if (idExpr->id == "assert")
            {
                if (funCallExpr->arguments.size() != 1) throw NotImplementedException("assert supports 1 arg");
                funCallExpr->arguments[0]->accept(this);
                emit(OpCode::ASSERT);
                return;
            }
            if (idExpr->id == "not")
            {
                if (funCallExpr->arguments.size() != 1) throw NotImplementedException("not supports 1 arg");
                funCallExpr->arguments[0]->accept(this);
                emit(OpCode::NOT);
                return;
            }

            // Check if this is a tagged value construction (e.g. Ok(42), Err("msg"))
            if (variant_map.contains(idExpr->id))
            {
                auto &info = variant_map[idExpr->id];
                // Find the type index
                int32_t typeIdx = -1;
                for (size_t i = 0; i < module.types.size(); ++i) {
                    if (module.types[i].name == info.unionName) {
                        typeIdx = static_cast<int32_t>(i);
                        break;
                    }
                }
                if (typeIdx == -1) throw NotImplementedException("Unknown tagged union type: " + info.unionName);

                // Push payload values onto the stack
                for (auto &&arg : funCallExpr->arguments) arg->accept(this);

                emit(OpCode::CONSTRUCT_TAGGED);
                emit_u16(static_cast<uint16_t>(typeIdx));
                emit_u16(static_cast<uint16_t>(info.variantIndex));
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
                return;
            }

            // Check if function has a pack parameter — if so, pack extra args into a tuple
            int32_t funIndex = -1;
            Str targetFunctionName = idExpr->id;
            if (!activeGenericInstanceName.empty())
            {
                if (auto it = funCallExpr->mangledCalleeNameByInstance.find(activeGenericInstanceName);
                    it != funCallExpr->mangledCalleeNameByInstance.end())
                {
                    targetFunctionName = it->second;
                }
            }
            else if (!funCallExpr->mangledCalleeName.empty())
            {
                targetFunctionName = funCallExpr->mangledCalleeName;
            }
            for (size_t i = 0; i < module.functions.size(); ++i)
            {
                if (module.functions[i].name == targetFunctionName) { funIndex = static_cast<int32_t>(i); break; }
            }

            if (funIndex != -1)
            {
                auto &funName = module.functions[funIndex].name;
                FunctionDef *def = functionDefs.contains(funName) ? functionDefs[funName] : nullptr;
                int32_t packIndex = -1;
                if (def)
                {
                    for (int32_t i = 0; i < static_cast<int32_t>(def->params.size()); ++i)
                    {
                        if (is_variadic_param(def->params[i].get()))
                        {
                            packIndex = i;
                            break;
                        }
                    }
                }

                if (packIndex >= 0) {
                    // Emit non-pack args normally
                    int32_t nonPackCount = packIndex;
                    for (int32_t i = 0; i < nonPackCount && i < static_cast<int32_t>(funCallExpr->arguments.size()); ++i) {
                        funCallExpr->arguments[i]->accept(this);
                    }
                    // Pack remaining args into a NEW_TUPLE
                    int32_t packStart = packIndex;
                    int32_t packCount = static_cast<int32_t>(funCallExpr->arguments.size()) - packStart;
                    for (int32_t i = packStart; i < static_cast<int32_t>(funCallExpr->arguments.size()); ++i) {
                        funCallExpr->arguments[i]->accept(this);
                    }
                    emit(OpCode::NEW_TUPLE);
                    emit_u16(static_cast<uint16_t>(std::max(packCount, 0)));

                    emit(OpCode::CALL);
                    emit_u16(static_cast<uint16_t>(funIndex));
                    emit_u16(static_cast<uint16_t>(def->params.size()));
                    return;
                }

                if (def) {
                    const bool hasSpreadArg = std::ranges::any_of(funCallExpr->arguments, [](const auto &arg) {
                        return dynamic_ast_cast<SpreadExpression>(arg) != nullptr;
                    });
                    if (hasSpreadArg)
                    {
                        auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                        if (emittedArgs != def->params.size())
                        {
                            throw NotImplementedException("Spread call argument count does not match function arity");
                        }
                        emit(OpCode::CALL);
                        emit_u16(static_cast<uint16_t>(funIndex));
                        emit_u16(emittedArgs);
                        return;
                    }
                    size_t provided = funCallExpr->arguments.size();
                    size_t expected = def->params.size();
                    Map<Str, Str> typeBindings;
                    for (size_t i = 0; i < funCallExpr->arguments.size() && i < def->params.size(); ++i)
                    {
                        if (def->params[i]->annotatedType)
                        {
                            infer_type_bindings_from_reprs(def->params[i]->annotatedType->repr(),
                                                           infer_expression_type_name(funCallExpr->arguments[i]),
                                                           typeBindings);
                        }
                    }
                    for (size_t i = 0; i < funCallExpr->arguments.size(); ++i)
                    {
                        funCallExpr->arguments[i]->accept(this);
                        if (i < def->params.size())
                        {
                            auto emittedTraitRef = emit_trait_ref_if_needed(def->params[i]->annotatedType.get());
                            if (!emittedTraitRef && def->params[i]->annotatedType)
                            {
                                auto specialized = specialize_type_repr(def->params[i]->annotatedType->repr(), typeBindings);
                                if (auto traitName = trait_ref_name_from_type_repr(specialized); !traitName.empty())
                                {
                                    uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
                                    module.strings.push_back(traitName);
                                    emit(OpCode::MAKE_TRAIT_REF);
                                    emit_u16(traitIndex);
                                }
                            }
                        }
                    }
                    if (provided < expected) {
                        for (size_t i = provided; i < expected; ++i) {
                            if (def->params[i]->value) {
                                def->params[i]->value->accept(this);
                                emit_trait_ref_if_needed(def->params[i]->annotatedType.get());
                            } else {
                                throw NotImplementedException("Missing argument and no default value for param " + def->params[i]->paramName);
                            }
                        }
                        emit(OpCode::CALL);
                        emit_u16(static_cast<uint16_t>(funIndex));
                        emit_u16(static_cast<uint16_t>(expected));
                        return;
                    }
                } else {
                    for (auto &&arg : funCallExpr->arguments) arg->accept(this);
                }
                
                emit(OpCode::CALL);
                emit_u16(static_cast<uint16_t>(funIndex));
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
                return;
            }
            
            if (imported_symbols.contains(idExpr->id)) {
                auto &imp = imported_symbols[idExpr->id];
                if (nativeFnNames.contains(idExpr->id) && imp.moduleName.starts_with("std."))
                {
                    auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                    uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
                    module.strings.push_back(idExpr->id);
                    emit(OpCode::NATIVE_CALL);
                    emit_u16(nameIdx);
                    emit_u16(emittedArgs);
                    return;
                }
                auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                emit(OpCode::CALL_IMPORT);
                emit_u16(static_cast<uint16_t>(imp.importIndex));
                emit_u16(emittedArgs);
                return;
            }

            if (nativeFnNames.contains(idExpr->id)) {
                auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(idExpr->id);
                emit(OpCode::NATIVE_CALL);
                emit_u16(nameIdx);
                emit_u16(emittedArgs);
                return;
            }

            if (locals.contains("self")) {
                auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                emit(OpCode::LOAD_LOCAL);
                emit_u16(static_cast<uint16_t>(locals["self"]));
                uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
                auto memberName = idExpr->id;
                if (!activeTraitMethodOrigin.empty() && runtimeTraits.contains(activeTraitMethodOrigin) &&
                    runtimeTraits[activeTraitMethodOrigin].methods.contains(memberName))
                {
                    memberName = activeTraitMethodOrigin + "::" + memberName;
                }
                module.strings.push_back(memberName);
                emit(OpCode::INVOKE_MEMBER);
                emit_u16(nameIdx);
                emit_u16(emittedArgs);
                return;
            }

            throw NotImplementedException("Unknown function: " + idExpr->id);
        }
        else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(funCallExpr->primaryExpression))
        {
            if (auto moduleExpr = dynamic_ast_cast<IdExpression>(idAcc->primaryExpression);
                moduleExpr && qualified_import_symbols.contains(moduleExpr->id))
            {
                auto memberName = idAcc->accessor->repr();
                auto &qualified = qualified_import_symbols.at(moduleExpr->id);
                if (qualified.contains(memberName))
                {
                    auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
                    emit(OpCode::CALL_IMPORT);
                    emit_u16(static_cast<uint16_t>(qualified.at(memberName).importIndex));
                    emit_u16(emittedArgs);
                    return;
                }
            }
            auto emittedArgs = emit_call_arguments(funCallExpr->arguments);
            idAcc->primaryExpression->accept(this);
            uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
            auto memberName = idAcc->accessor->repr();
            if (!activeTraitMethodOrigin.empty() && runtimeTraits.contains(activeTraitMethodOrigin) &&
                runtimeTraits[activeTraitMethodOrigin].methods.contains(memberName))
            {
                memberName = activeTraitMethodOrigin + "::" + memberName;
            }
            module.strings.push_back(memberName);
            emit(OpCode::INVOKE_MEMBER);
            emit_u16(nameIndex);
            emit_u16(emittedArgs);
        }
        else if (auto qualifiedCall = dynamic_ast_cast<QualifiedTraitCallExpression>(funCallExpr->primaryExpression))
        {
            qualifiedCall->accept(this);
        }
        else throw NotImplementedException("Complex calls not implemented");
    }

    void Compiler::visit(ast::IdAccessorExpression *idAccExpr)
    {
        if (auto moduleExpr = dynamic_ast_cast<IdExpression>(idAccExpr->primaryExpression);
            moduleExpr && qualified_import_symbols.contains(moduleExpr->id))
        {
            auto memberName = idAccExpr->accessor->repr();
            auto &qualified = qualified_import_symbols.at(moduleExpr->id);
            if (qualified.contains(memberName))
            {
                auto emittedArgs = emit_call_arguments(idAccExpr->arguments);
                emit(OpCode::CALL_IMPORT);
                emit_u16(static_cast<uint16_t>(qualified.at(memberName).importIndex));
                emit_u16(emittedArgs);
                return;
            }
        }
        idAccExpr->primaryExpression->accept(this);
        auto emittedArgs = emit_call_arguments(idAccExpr->arguments);
        uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(idAccExpr->accessor->repr());
        emit(OpCode::INVOKE_MEMBER);
        emit_u16(nameIndex);
        emit_u16(emittedArgs);
    }

    void Compiler::visit(ast::QualifiedTraitCallExpression *qualifiedCall)
    {
        uint16_t emittedArgs = 0;
        if (qualifiedCall->receiver)
        {
            qualifiedCall->receiver->accept(this);
            emittedArgs = emit_call_arguments(qualifiedCall->arguments);
        }
        else
        {
            if (qualifiedCall->arguments.empty())
            {
                throw NotImplementedException("Trait-qualified call requires a receiver argument");
            }
            qualifiedCall->arguments.front()->accept(this);
            Vec<ASTRef<Expression>> regularArgs;
            regularArgs.reserve(qualifiedCall->arguments.size() - 1);
            regularArgs.insert(regularArgs.end(), qualifiedCall->arguments.begin() + 1, qualifiedCall->arguments.end());
            emittedArgs = emit_call_arguments(regularArgs);
        }
        uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(qualifiedCall->traitName + "::" + qualifiedCall->methodName);
        emit(OpCode::INVOKE_MEMBER);
        emit_u16(nameIndex);
        emit_u16(emittedArgs);
    }

    void Compiler::visit(ast::IndexAccessorExpression *idxAccExpr)
    {
        if (auto range = dynamic_ast_cast<RangeExpression>(idxAccExpr->accessor))
        {
            idxAccExpr->primary->accept(this);
            if (range->start)
            {
                range->start->accept(this);
            }
            else
            {
                emit(OpCode::PUSH_I32);
                emit_i32(0);
            }
            if (range->end)
            {
                range->end->accept(this);
                if (range->inclusive)
                {
                    emit(OpCode::PUSH_I32);
                    emit_i32(1);
                    emit(OpCode::ADD);
                }
            }
            else
            {
                emit(OpCode::PUSH_I32);
                emit_i32(std::numeric_limits<int32_t>::max());
            }
            emit(OpCode::SLICE_RANGE);
            return;
        }
        idxAccExpr->primary->accept(this);
        idxAccExpr->accessor->accept(this);
        emit(OpCode::GET_INDEX);
    }

    void Compiler::visit(ast::RangeExpression *rangeExpr)
    {
        if (!rangeExpr->start || !rangeExpr->end)
        {
            throw NotImplementedException("Open range expressions are only supported inside index access");
        }
        rangeExpr->start->accept(this);
        rangeExpr->end->accept(this);
        emit(OpCode::MAKE_RANGE);
        emit_u8(rangeExpr->inclusive ? 1 : 0);
    }

    void Compiler::visit(ast::FromEndIndexExpression *fromEndExpr)
    {
        fromEndExpr->index->accept(this);
        uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back("__ng_from_end");
        emit(OpCode::NATIVE_CALL);
        emit_u16(nameIdx);
        emit_u16(1);
    }

    void Compiler::visit(ast::IndexAssignmentExpression *idxAssignExpr)
    {
        emit_reference(idxAssignExpr->primary);
        idxAssignExpr->accessor->accept(this);
        emit(OpCode::MAKE_INDEX_REF);
        idxAssignExpr->value->accept(this);
        emit(OpCode::STORE_REF);
    }

    void Compiler::visit(ast::CompoundStatement *compoundStmt)
    {
        auto oldLocals = locals;
        auto oldLocalTraitObjectTypes = localTraitObjectTypes;
        auto oldLocalValueTypes = localValueTypes;
        for (auto &&stmt : compoundStmt->statements) stmt->accept(this);
        locals = std::move(oldLocals);
        localTraitObjectTypes = std::move(oldLocalTraitObjectTypes);
        localValueTypes = std::move(oldLocalValueTypes);
    }

    void Compiler::visit(ast::LoopStatement *loopStmt)
    {
        LoopInfo info;
        for (auto &&binding : loopStmt->bindings)
        {
            binding.target->accept(this);
            int32_t index = static_cast<int32_t>(locals.size());
            locals[binding.name] = index;
            info.bindingSlots.push_back(index);
            emit(OpCode::STORE_LOCAL);
            emit_u16(static_cast<uint16_t>(index));
            emit(OpCode::POP);
        }
        info.startIp = current_function->code.size();
        loop_stack.push_back(std::move(info));
        loopStmt->loopBody->accept(this);
        loop_stack.pop_back();
    }

    void Compiler::visit(ast::NextStatement *nextStmt)
    {
        if (loop_stack.empty()) throw NotImplementedException("next outside of loop");
        auto &info = loop_stack.back();
        if (nextStmt->expressions.size() != info.bindingSlots.size())
            throw NotImplementedException("next expression count mismatch");
        for (size_t i = 0; i < nextStmt->expressions.size(); ++i)
        {
            nextStmt->expressions[i]->accept(this);
        }
        for (int i = static_cast<int>(nextStmt->expressions.size()) - 1; i >= 0; --i)
        {
            emit(OpCode::STORE_LOCAL);
            emit_u16(static_cast<uint16_t>(info.bindingSlots[i]));
            emit(OpCode::POP);
        }
        emit(OpCode::JUMP);
        emit_i32(static_cast<int32_t>(info.startIp));
    }

    void Compiler::visit(ast::TypeOfExpression * /*typeofExpr*/)
    {
        throw NotImplementedException("typeof(expr) is only supported in compile-time type queries");
    }

    void Compiler::visit(ast::TypeCheckingExpression *typeCheck)
    {
        typeCheck->value->accept(this);
        uint16_t index = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(typeCheck->type->repr());
        emit(OpCode::INSTANCE_OF);
        emit_u16(index);
    }

    void Compiler::visit(ast::ValueBindingStatement *valBind)
    {
        valBind->value->accept(this);
        for (size_t i = 0; i < valBind->bindings.size(); ++i)
        {
            auto binding = valBind->bindings[i];
            if (binding->name.empty()) continue;
            emit(OpCode::DUP);
            emit(OpCode::PUSH_I32);
            emit_i32(static_cast<int32_t>(binding->index));
            if (valBind->type == BindingType::TUPLE_UNPACK) {
                if (binding->spreadReceiver) emit(OpCode::GET_TUPLE_REST);
                else emit(OpCode::GET_TUPLE_ITEM);
            }
            else emit(OpCode::GET_INDEX);
            int32_t index = static_cast<int32_t>(locals.size());
            locals[binding->name] = index;
            emit(OpCode::STORE_LOCAL);
            emit_u16(static_cast<uint16_t>(index));
            emit(OpCode::POP);
        }
        emit(OpCode::POP);
    }

    void Compiler::visit(ast::Binding *binding) {}

    void Compiler::visit(ast::SimpleStatement *simpleStmt)
    {
        simpleStmt->expression->accept(this);
        emit(OpCode::POP);
    }

    void Compiler::visit(ast::ValDefStatement *valDefStmt)
    {
                valDefStmt->value->accept(this);
                emit_trait_ref_if_needed(valDefStmt->typeAnnotation.get());
                if (current_function)
                {
                    int32_t index = static_cast<int32_t>(locals.size());
                    locals[valDefStmt->name] = index;
                    if (valDefStmt->typeAnnotation)
                    {
                        localValueTypes[valDefStmt->name] = valDefStmt->typeAnnotation->repr();
                    }
                    else if (auto inferred = infer_expression_type_name(valDefStmt->value); !inferred.empty())
                    {
                        if (auto traitName = trait_ref_name_from_type_repr(inferred); !traitName.empty())
                        {
                            localTraitObjectTypes[valDefStmt->name] = traitName;
                        }
                        localValueTypes[valDefStmt->name] = std::move(inferred);
                    }
                    if (auto traitName = trait_ref_name(valDefStmt->typeAnnotation.get()); !traitName.empty())
                    {
                        localTraitObjectTypes[valDefStmt->name] = traitName;
                    }
                    emit(OpCode::STORE_LOCAL);
            emit_u16(static_cast<uint16_t>(index));
            emit(OpCode::POP);
        }
    }

    void Compiler::visit(ast::ReturnStatement *returnStmt)
    {
        if (returnStmt->expression) returnStmt->expression->accept(this);
        else emit(OpCode::PUSH_UNIT);
        emit(OpCode::RETURN);
    }

    void Compiler::visit(ast::IfStatement *ifStmt)
    {
        if (ifStmt->isConst)
        {
            // Compile-time branch elimination: only compile the active branch
            std::optional<bool> instanceCondition;
            if (!activeGenericInstanceName.empty())
            {
                if (auto it = ifStmt->evaluatedConditionByInstance.find(activeGenericInstanceName);
                    it != ifStmt->evaluatedConditionByInstance.end())
                {
                    instanceCondition = it->second;
                }
            }
            bool condValue = instanceCondition.has_value()
                                 ? instanceCondition.value()
                                 : (ifStmt->evaluatedCondition.has_value()
                                        ? ifStmt->evaluatedCondition.value()
                                        : evaluate_const_bool(ifStmt->testing));
            if (condValue)
            {
                ifStmt->consequence->accept(this);
            }
            else if (ifStmt->alternative)
            {
                ifStmt->alternative->accept(this);
            }
        }
        else
        {
            ifStmt->testing->accept(this);
            size_t jumpIfFalseOffset = current_function->code.size() + 1;
            emit(OpCode::JUMP_IF_FALSE);
            emit_i32(0);

            ifStmt->consequence->accept(this);
            
            size_t jumpEndOffset = current_function->code.size() + 1;
            emit(OpCode::JUMP);
            emit_i32(0);

            patch_i32(jumpIfFalseOffset, static_cast<int32_t>(current_function->code.size()));

            if (ifStmt->alternative)
            {
                ifStmt->alternative->accept(this);
            }

            patch_i32(jumpEndOffset, static_cast<int32_t>(current_function->code.size()));
        }
    }

    void Compiler::visit(ast::TaggedUnionDef * /*taggedUnionDef*/)
    {
        // Already handled in Module first pass — nothing to emit here
    }

    void Compiler::visit(ast::SwitchStatement *switchStmt)
    {
        auto scrutineeType = infer_expression_type_name(switchStmt->scrutinee);
        Str scrutineeBase = scrutineeType;
        Map<Str, Str> typeBindings;
        if (auto genericStart = scrutineeType.find('<'); genericStart != Str::npos && scrutineeType.ends_with(">"))
        {
            scrutineeBase = scrutineeType.substr(0, genericStart);
            auto argsText = scrutineeType.substr(genericStart + 1, scrutineeType.size() - genericStart - 2);
            Vec<Str> args;
            Str current;
            int depth = 0;
            for (char ch : argsText)
            {
                if (ch == '<') ++depth;
                if (ch == '>') --depth;
                if (ch == ',' && depth == 0)
                {
                    args.push_back(current);
                    current.clear();
                    continue;
                }
                if (!std::isspace(static_cast<unsigned char>(ch)) || !current.empty())
                {
                    current.push_back(ch);
                }
            }
            if (!current.empty())
            {
                while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back())))
                {
                    current.pop_back();
                }
                args.push_back(current);
            }
            for (const auto &type : module.types)
            {
                if (type.name == scrutineeBase)
                {
                    // Current generic tagged unions use T, U, ... names in declaration order.
                    static const Vec<Str> conventionalParams{"T", "U", "V", "W"};
                    for (size_t i = 0; i < args.size() && i < conventionalParams.size(); ++i)
                    {
                        typeBindings[conventionalParams[i]] = args[i];
                    }
                    break;
                }
            }
        }

        // Compile the scrutinee
        switchStmt->scrutinee->accept(this);

        // Bytecode layout:
        //   SWITCH_TAG numCases
        //   [jump table: tag0:addr0, tag1:addr1, ...]   (6 bytes per entry)
        //   [case0 body] [JUMP end]
        //   [case1 body] [JUMP end]
        //   ...
        //   [end]

        emit(OpCode::SWITCH_TAG);
        emit_u16(static_cast<uint16_t>(switchStmt->cases.size()));

        // Emit placeholder jump table entries (tag u16 + addr i32 = 6 bytes each)
        size_t jumpTableStart = current_function->code.size();
        for (size_t i = 0; i < switchStmt->cases.size(); ++i) {
            auto &c = switchStmt->cases[i];
            uint16_t tag = SWITCH_DEFAULT_TAG;
            if (!c.isOtherwise)
            {
                if (!variant_map.contains(c.variantName))
                {
                    throw NotImplementedException("Unknown variant in switch: " + c.variantName);
                }
                tag = static_cast<uint16_t>(variant_map.at(c.variantName).variantIndex);
            }
            emit_u16(tag);
            emit_i32(0);
        }

        // Compile each case body, patching jump table entries as we go
        Vec<size_t> caseEndJumps;
        for (size_t i = 0; i < switchStmt->cases.size(); ++i) {
            auto &c = switchStmt->cases[i];

            // Record where this case body starts and patch the jump table
            size_t caseStart = current_function->code.size();
            size_t entryOffset = jumpTableStart + i * 6;
            patch_i32(entryOffset + 2, static_cast<int32_t>(caseStart));

            // Extract payload fields for each binding
            for (size_t j = 0; j < c.bindings.size(); ++j) {
                emit(OpCode::DUP);
                emit(OpCode::GET_PAYLOAD);
                emit_u16(static_cast<uint16_t>(j));
                if (!c.bindings[j].empty()) {
                    locals[c.bindings[j]] = static_cast<int32_t>(locals.size());
                    if (variant_map.contains(c.variantName) && j < variant_map[c.variantName].payloadTypes.size())
                    {
                        auto payloadType = specialize_type_repr(variant_map[c.variantName].payloadTypes[j], typeBindings);
                        localValueTypes[c.bindings[j]] = payloadType;
                        if (auto traitName = trait_ref_name_from_type_repr(payloadType); !traitName.empty())
                        {
                            localTraitObjectTypes[c.bindings[j]] = traitName;
                        }
                    }
                    emit(OpCode::STORE_LOCAL);
                    emit_u16(static_cast<uint16_t>(locals[c.bindings[j]]));
                    emit(OpCode::POP);
                } else {
                    emit(OpCode::POP);
                }
            }

            // Pop the matched scrutinee before executing the body. SWITCH_TAG
            // only peeks, so leaving it here would pollute return expressions.
            emit(OpCode::POP);

            // Compile case body
            c.body->accept(this);

            // Jump to end (unless last case)
            if (i < switchStmt->cases.size() - 1) {
                caseEndJumps.push_back(current_function->code.size() + 1);
                emit(OpCode::JUMP);
                emit_i32(0);
            }
        }

        // Patch all end jumps to point here
        for (auto jumpOffset : caseEndJumps) {
            patch_i32(jumpOffset, static_cast<int32_t>(current_function->code.size()));
        }
    }


    void Compiler::emit_reference(ast::ASTRef<ast::Expression> expr)
    {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(expr))
        {
            if (locals.contains(idExpr->id))
            {
                emit(OpCode::MAKE_LOCAL_REF);
                emit_u16(static_cast<uint16_t>(locals[idExpr->id]));
                return;
            }
            if (globals.contains(idExpr->id))
            {
                emit(OpCode::MAKE_GLOBAL_REF);
                emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
                return;
            }
            if (locals.contains("self"))
            {
                emit(OpCode::MAKE_LOCAL_REF);
                emit_u16(static_cast<uint16_t>(locals["self"]));
                int32_t fieldIdx = find_field_index(idExpr->id);
                if (fieldIdx >= 0)
                {
                    emit(OpCode::MAKE_PROPERTY_REF);
                    emit_u16(static_cast<uint16_t>(fieldIdx));
                    return;
                }
            }
        }
        else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(expr))
        {
            emit_reference(idAcc->primaryExpression);
            Str accessorRepr = idAcc->accessor->repr();
            bool isNumeric = !accessorRepr.empty() && std::all_of(accessorRepr.begin(), accessorRepr.end(), ::isdigit);
            if (isNumeric)
            {
                emit(OpCode::PUSH_I32);
                emit_i32(std::stoi(accessorRepr));
                emit(OpCode::MAKE_INDEX_REF);
            }
            else
            {
                uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(accessorRepr);
                emit(OpCode::MAKE_PROPERTY_STR_REF);
                emit_u16(nameIndex);
            }
            return;
        }
        else if (auto idxAcc = dynamic_ast_cast<IndexAccessorExpression>(expr))
        {
            emit_reference(idxAcc->primary);
            idxAcc->accessor->accept(this);
            emit(OpCode::MAKE_INDEX_REF);
            return;
        }
        else if (auto unaryExpr = dynamic_ast_cast<UnaryExpression>(expr);
                 unaryExpr && unaryExpr->optr && unaryExpr->optr->type == TokenType::TIMES)
        {
            unaryExpr->operand->accept(this);
            return;
        }

        throw NotImplementedException("Reference target not supported: " + expr->repr());
    }

    auto Compiler::trait_ref_name(const ast::TypeAnnotation *annotation) const -> Str
    {
        if (!annotation || annotation->name != "ref" || annotation->genericArgs.size() != 1)
        {
            return {};
        }
        auto traitName = annotation->genericArgs[0]->repr();
        return runtimeTraits.contains(traitName) ? traitName : Str{};
    }

    auto Compiler::trait_ref_name_from_type_repr(const Str &typeName) const -> Str
    {
        static constexpr Str::size_type refPrefixSize = 4;
        if (!typeName.starts_with("ref<") || !typeName.ends_with(">") || typeName.size() <= refPrefixSize + 1)
        {
            return {};
        }
        auto traitName = typeName.substr(refPrefixSize, typeName.size() - refPrefixSize - 1);
        return runtimeTraits.contains(traitName) ? traitName : Str{};
    }

    auto Compiler::specialize_type_repr(const Str &typeName, const Map<Str, Str> &typeBindings) const -> Str
    {
        auto it = typeBindings.find(typeName);
        if (it != typeBindings.end())
        {
            return it->second;
        }
        for (auto &&[name, replacement] : typeBindings)
        {
            if (typeName == "ref<" + name + ">")
            {
                return "ref<" + replacement + ">";
            }
        }
        return typeName;
    }

    void Compiler::infer_type_bindings_from_reprs(const Str &pattern, const Str &actual, Map<Str, Str> &typeBindings) const
    {
        if (pattern.size() == 1 && std::isupper(static_cast<unsigned char>(pattern.front())))
        {
            typeBindings.try_emplace(pattern, actual);
            return;
        }
        auto patternStart = pattern.find('<');
        auto actualStart = actual.find('<');
        if (patternStart == Str::npos || actualStart == Str::npos || !pattern.ends_with(">") || !actual.ends_with(">"))
        {
            return;
        }
        if (pattern.substr(0, patternStart) != actual.substr(0, actualStart))
        {
            return;
        }
        auto splitArgs = [](const Str &text) {
            Vec<Str> args;
            Str current;
            int depth = 0;
            for (char ch : text)
            {
                if (ch == '<') ++depth;
                if (ch == '>') --depth;
                if (ch == ',' && depth == 0)
                {
                    while (!current.empty() && std::isspace(static_cast<unsigned char>(current.front())))
                    {
                        current.erase(current.begin());
                    }
                    while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back())))
                    {
                        current.pop_back();
                    }
                    args.push_back(current);
                    current.clear();
                    continue;
                }
                current.push_back(ch);
            }
            while (!current.empty() && std::isspace(static_cast<unsigned char>(current.front())))
            {
                current.erase(current.begin());
            }
            while (!current.empty() && std::isspace(static_cast<unsigned char>(current.back())))
            {
                current.pop_back();
            }
            if (!current.empty())
            {
                args.push_back(current);
            }
            return args;
        };
        auto patternArgs = splitArgs(pattern.substr(patternStart + 1, pattern.size() - patternStart - 2));
        auto actualArgs = splitArgs(actual.substr(actualStart + 1, actual.size() - actualStart - 2));
        for (size_t i = 0; i < patternArgs.size() && i < actualArgs.size(); ++i)
        {
            infer_type_bindings_from_reprs(patternArgs[i], actualArgs[i], typeBindings);
        }
    }

    auto Compiler::infer_expression_type_name(ast::ASTRef<ast::Expression> expr) const -> Str
    {
        if (dynamic_ast_cast<UnitLiteral>(expr))
        {
            return "unit";
        }
        if (dynamic_ast_cast<StringValue>(expr))
        {
            return "string";
        }
        if (dynamic_ast_cast<BooleanValue>(expr))
        {
            return "bool";
        }
        if (dynamic_ast_cast<IntegralValue<int8_t>>(expr))
        {
            return "i8";
        }
        if (dynamic_ast_cast<IntegralValue<uint8_t>>(expr))
        {
            return "u8";
        }
        if (dynamic_ast_cast<IntegralValue<int16_t>>(expr))
        {
            return "i16";
        }
        if (dynamic_ast_cast<IntegralValue<uint16_t>>(expr))
        {
            return "u16";
        }
        if (dynamic_ast_cast<IntegralValue<int32_t>>(expr))
        {
            return "i32";
        }
        if (dynamic_ast_cast<IntegralValue<uint32_t>>(expr))
        {
            return "u32";
        }
        if (dynamic_ast_cast<IntegralValue<int64_t>>(expr))
        {
            return "i64";
        }
        if (dynamic_ast_cast<IntegralValue<uint64_t>>(expr))
        {
            return "u64";
        }
        if (dynamic_ast_cast<FloatingPointValue<float>>(expr))
        {
            return "f32";
        }
        if (dynamic_ast_cast<FloatingPointValue<double>>(expr))
        {
            return "f64";
        }
        if (auto idExpr = dynamic_ast_cast<IdExpression>(expr))
        {
            if (auto it = localValueTypes.find(idExpr->id); it != localValueTypes.end())
            {
                return it->second;
            }
            if (auto it = globalValueTypes.find(idExpr->id); it != globalValueTypes.end())
            {
                return it->second;
            }
        }
        if (auto tuple = dynamic_ast_cast<TupleLiteral>(expr))
        {
            Vec<Str> elementTypes;
            elementTypes.reserve(tuple->elements.size());
            for (auto &element : tuple->elements)
            {
                if (auto spread = dynamic_ast_cast<SpreadExpression>(element))
                {
                    auto spreadType = infer_expression_type_name(spread->expression);
                    if (spreadType.size() < 2 || spreadType.front() != '(' || spreadType.back() != ')')
                    {
                        return {};
                    }
                    Str inner = spreadType.substr(1, spreadType.size() - 2);
                    if (inner.empty())
                    {
                        continue;
                    }
                    int depth = 0;
                    Str current;
                    for (char ch : inner)
                    {
                        if (ch == '<' || ch == '(' || ch == '[') ++depth;
                        if (ch == '>' || ch == ')' || ch == ']') --depth;
                        if (ch == ',' && depth == 0)
                        {
                            elementTypes.push_back(current);
                            current.clear();
                            continue;
                        }
                        current.push_back(ch);
                    }
                    if (!current.empty()) elementTypes.push_back(current);
                    continue;
                }
                auto elementType = infer_expression_type_name(element);
                if (elementType.empty())
                {
                    return {};
                }
                elementTypes.push_back(elementType);
            }
            Str repr = "(";
            for (size_t i = 0; i < elementTypes.size(); ++i)
            {
                if (i > 0) repr += ", ";
                repr += elementTypes[i];
            }
            repr += ")";
            return repr;
        }
        if (auto unaryExpr = dynamic_ast_cast<UnaryExpression>(expr);
            unaryExpr && unaryExpr->optr && unaryExpr->optr->type == TokenType::TIMES)
        {
            auto refType = infer_expression_type_name(unaryExpr->operand);
            if (refType.starts_with("ref<") && refType.ends_with(">"))
            {
                return refType.substr(4, refType.size() - 5);
            }
        }
        return {};
    }

    auto Compiler::emit_trait_ref_if_needed(const ast::TypeAnnotation *annotation) -> bool
    {
        auto traitName = trait_ref_name(annotation);
        if (traitName.empty())
        {
            return false;
        }
        uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(traitName);
        emit(OpCode::MAKE_TRAIT_REF);
        emit_u16(traitIndex);
        return true;
    }

    void Compiler::emit_move_place(ast::ASTRef<ast::Expression> expr)
    {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(expr))
        {
            if (locals.contains(idExpr->id))
            {
                emit(OpCode::MOVE_LOCAL);
                emit_u16(static_cast<uint16_t>(locals[idExpr->id]));
                return;
            }
            if (globals.contains(idExpr->id))
            {
                emit(OpCode::MOVE_GLOBAL);
                emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
                return;
            }
        }
        else if (auto unaryExpr = dynamic_ast_cast<UnaryExpression>(expr);
                 unaryExpr && unaryExpr->optr && unaryExpr->optr->type == TokenType::TIMES)
        {
            unaryExpr->operand->accept(this);
            emit(OpCode::MOVE_REF);
            return;
        }
        else if (dynamic_ast_cast<IdAccessorExpression>(expr) || dynamic_ast_cast<IndexAccessorExpression>(expr))
        {
            emit_reference(expr);
            emit(OpCode::MOVE_REF);
            return;
        }

        throw NotImplementedException("Move target not supported: " + expr->repr());
    }

    void Compiler::visit(ast::UnaryExpression *unaryExpr)
    {
        switch (unaryExpr->optr->type)
        {
        case TokenType::KEYWORD_REF:
        case TokenType::AMPERSAND:
            emit_reference(unaryExpr->operand);
            break;
        case TokenType::TIMES:
            unaryExpr->operand->accept(this);
            emit(OpCode::LOAD_REF);
            break;
        case TokenType::KEYWORD_MOVE:
            emit_move_place(unaryExpr->operand);
            break;
        case TokenType::MINUS:
            unaryExpr->operand->accept(this);
            emit(OpCode::NEG_I32);
            break;
        case TokenType::NOT:
            unaryExpr->operand->accept(this);
            emit(OpCode::NOT);
            break;
        default: throw NotImplementedException("Unary op not implemented");
        }
    }

    void Compiler::visit(ast::BinaryExpression *binExpr)
    {
        binExpr->left->accept(this);
        binExpr->right->accept(this);
        switch (binExpr->optr->type)
        {
        case TokenType::PLUS:  emit(OpCode::ADD); break;
        case TokenType::MINUS: emit(OpCode::SUB); break;
        case TokenType::TIMES: emit(OpCode::MUL); break;
        case TokenType::DIVIDE:emit(OpCode::DIV); break;
        case TokenType::MODULUS:emit(OpCode::MOD_I32); break;
        case TokenType::EQUAL: emit(OpCode::EQ_I32);  break;
        case TokenType::LT:    emit(OpCode::LT_I32);  break;
        case TokenType::GT:    emit(OpCode::GT_I32);  break;
        case TokenType::LSHIFT:emit(OpCode::LSHIFT);  break;
        case TokenType::RSHIFT:emit(OpCode::RSHIFT);  break;
        default: throw NotImplementedException("Binary op not implemented");
        }
    }

    void Compiler::visit(ast::AssignmentExpression *assignExpr)
    {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(assignExpr->target))
        {
            assignExpr->value->accept(this);
            if (locals.contains(idExpr->id))
            {
                if (auto it = localTraitObjectTypes.find(idExpr->id); it != localTraitObjectTypes.end())
                {
                    uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
                    module.strings.push_back(it->second);
                    emit(OpCode::MAKE_TRAIT_REF);
                    emit_u16(traitIndex);
                }
                emit(OpCode::STORE_LOCAL);
                emit_u16(static_cast<uint16_t>(locals[idExpr->id]));
            }
            else if (globals.contains(idExpr->id))
            {
                if (auto it = globalTraitObjectTypes.find(idExpr->id); it != globalTraitObjectTypes.end())
                {
                    uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
                    module.strings.push_back(it->second);
                    emit(OpCode::MAKE_TRAIT_REF);
                    emit_u16(traitIndex);
                }
                emit(OpCode::STORE_GLOBAL);
                emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
            }
            else if (locals.contains("self"))
            {
                emit_reference(assignExpr->target);
                assignExpr->value->accept(this);
                emit(OpCode::STORE_REF);
            }
            else throw NotImplementedException("Unknown assignment target: " + idExpr->id);
        }
        else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignExpr->target))
        {
            emit_reference(idAcc);
            assignExpr->value->accept(this);
            emit(OpCode::STORE_REF);
        }
        else if (auto idxAssign = dynamic_ast_cast<IndexAssignmentExpression>(assignExpr->target))
        {
            emit_reference(idxAssign->primary);
            idxAssign->accessor->accept(this);
            emit(OpCode::MAKE_INDEX_REF);
            assignExpr->value->accept(this);
            emit(OpCode::STORE_REF);
        }
        else if (auto derefExpr = dynamic_ast_cast<UnaryExpression>(assignExpr->target);
                 derefExpr && derefExpr->optr && derefExpr->optr->type == TokenType::TIMES)
        {
            derefExpr->operand->accept(this);
            assignExpr->value->accept(this);
            emit(OpCode::STORE_REF);
        }
        else throw NotImplementedException("Assignment to non-id: " + assignExpr->target->repr());
    }

    void Compiler::visit(ast::NewObjectExpression *newObj)
    {
        Str typeName = newObj->targetType ? newObj->targetType->repr() : newObj->typeName;
        // Find the type definition to get property order
        int32_t typeIdx = -1;
        for (size_t i = 0; i < module.types.size(); ++i) {
            if (module.types[i].name == typeName) { typeIdx = static_cast<int32_t>(i); break; }
        }
        if (typeIdx < 0)
        {
            auto genericStart = typeName.find('<');
            if (genericStart != Str::npos)
            {
                auto baseName = typeName.substr(0, genericStart);
                for (size_t i = 0; i < module.types.size(); ++i) {
                    if (module.types[i].name == baseName) { typeIdx = static_cast<int32_t>(i); break; }
                }
            }
        }

        uint16_t numFields = 0;
        if (typeIdx >= 0) {
            // Push values in the type's property order
            auto &typeProps = module.types[typeIdx].properties;
            numFields = static_cast<uint16_t>(typeProps.size());
            for (auto &&propName : typeProps) {
                if (newObj->properties.contains(propName)) {
                    newObj->properties[propName]->accept(this);
                } else {
                    emit(OpCode::PUSH_UNIT);
                }
            }
        } else {
            bool foundVariant = false;
            if (auto variantIt = variant_map.find(typeName); variantIt != variant_map.end()) {
                foundVariant = true;
                const auto &payloadFields = variantIt->second.payloadFields;
                numFields = static_cast<uint16_t>(payloadFields.size());
                for (const auto &fieldName : payloadFields) {
                    auto it = newObj->properties.find(fieldName);
                    if (it == newObj->properties.end()) {
                        throw NotImplementedException("Missing payload property '" + fieldName + "' for variant " + typeName);
                    }
                    it->second->accept(this);
                }
            }

            if (!foundVariant) {
                // Fallback: push values in iteration order
                numFields = static_cast<uint16_t>(newObj->properties.size());
                for (auto &&[name, expr] : newObj->properties) expr->accept(this);
            }
        }

        uint16_t typeStrIdx = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(typeName);
        emit(OpCode::NEW_OBJECT);
        emit_u16(typeStrIdx);
        emit_u16(numFields);
    }

    void Compiler::visit(ast::IntegralValue<int8_t> *intVal) { emit(OpCode::PUSH_I8); emit_u8(static_cast<uint8_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<uint8_t> *intVal) { emit(OpCode::PUSH_U8); emit_u8(intVal->value); }
    void Compiler::visit(ast::IntegralValue<int16_t> *intVal) { emit(OpCode::PUSH_I16); emit_u16(static_cast<uint16_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<uint16_t> *intVal) { emit(OpCode::PUSH_U16); emit_u16(intVal->value); }
    void Compiler::visit(ast::IntegralValue<int32_t> *intVal) { emit(OpCode::PUSH_I32); emit_i32(intVal->value); }
    void Compiler::visit(ast::IntegralValue<uint32_t> *intVal) { emit(OpCode::PUSH_U32); emit_i32(static_cast<int32_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<int64_t> *intVal) { emit(OpCode::PUSH_I64); emit_i64(intVal->value); }
    void Compiler::visit(ast::IntegralValue<uint64_t> *intVal) { emit(OpCode::PUSH_U64); emit_i64(static_cast<int64_t>(intVal->value)); }

    void Compiler::visit(ast::FloatingPointValue<float> *floatVal) { emit(OpCode::PUSH_F32); emit_f32(floatVal->value); }
    void Compiler::visit(ast::FloatingPointValue<double> *floatVal) { emit(OpCode::PUSH_F64); emit_f64(floatVal->value); }

    void Compiler::visit(ast::StringValue *strVal)
    {
        uint16_t index = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(strVal->value);
        emit(OpCode::LOAD_STR);
        emit_u16(index);
    }

    void Compiler::visit(ast::BooleanValue *boolVal)
    {
        emit(OpCode::PUSH_BOOL);
        emit_u8(boolVal->value ? 1 : 0);
    }

    void Compiler::visit(ast::SpreadExpression *spreadExpr)
    {
        spreadExpr->expression->accept(this);
    }

    void Compiler::visit(ast::PostfixFoldExpression *foldExpr)
    {
        throw NotImplementedException("Postfix fold expression is only supported in array literals or fold calls");
    }

    void Compiler::visit(ast::ArrayLiteral *arrayLit)
    {
        if (arrayLit->elements.size() == 1)
        {
            if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(arrayLit->elements[0]))
            {
                auto call = dynamic_ast_cast<FunCallExpression>(fold->expression);
                auto target = call ? dynamic_ast_cast<IdExpression>(call->primaryExpression) : nullptr;
                if (!call || !target || call->arguments.size() != 1 || !dynamic_ast_cast<IdExpression>(call->arguments[0]))
                {
                    throw NotImplementedException("ORGASM map/filter fold expects `fn(xs)...` or `fn(xs)?...`");
                }
                int32_t funIndex = -1;
                for (size_t i = 0; i < module.functions.size(); ++i)
                {
                    if (module.functions[i].name == target->id)
                    {
                        funIndex = static_cast<int32_t>(i);
                        break;
                    }
                }
                if (funIndex < 0)
                {
                    throw NotImplementedException("ORGASM map/filter fold only supports local functions: " + target->id);
                }
                call->arguments[0]->accept(this);
                emit(fold->filter ? OpCode::FOLD_FILTER_CALL : OpCode::FOLD_MAP_CALL);
                emit_u16(static_cast<uint16_t>(funIndex));
                return;
            }
        }
        bool hasSpread = false;
        for (auto &&elem : arrayLit->elements) {
            if (dynamic_ast_cast<SpreadExpression>(elem) || dynamic_ast_cast<PostfixFoldExpression>(elem)) hasSpread = true;
            if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(elem))
            {
                auto call = dynamic_ast_cast<FunCallExpression>(fold->expression);
                auto target = call ? dynamic_ast_cast<IdExpression>(call->primaryExpression) : nullptr;
                if (!call || !target || call->arguments.size() != 1 || !dynamic_ast_cast<IdExpression>(call->arguments[0]))
                {
                    throw NotImplementedException("ORGASM map/filter fold expects `fn(xs)...` or `fn(xs)?...`");
                }
                int32_t funIndex = -1;
                for (size_t i = 0; i < module.functions.size(); ++i)
                {
                    if (module.functions[i].name == target->id)
                    {
                        funIndex = static_cast<int32_t>(i);
                        break;
                    }
                }
                if (funIndex < 0)
                {
                    throw NotImplementedException("ORGASM map/filter fold only supports local functions: " + target->id);
                }
                call->arguments[0]->accept(this);
                emit(fold->filter ? OpCode::FOLD_FILTER_CALL : OpCode::FOLD_MAP_CALL);
                emit_u16(static_cast<uint16_t>(funIndex));
                continue;
            }
            elem->accept(this);
        }

        if (hasSpread) {
            emit(OpCode::NEW_ARRAY_SPREAD);
            emit_u16(static_cast<uint16_t>(arrayLit->elements.size()));
            for (auto &&elem : arrayLit->elements) {
                if (dynamic_ast_cast<SpreadExpression>(elem) || dynamic_ast_cast<PostfixFoldExpression>(elem)) emit_u8(1);
                else emit_u8(0);
            }
        } else {
            emit(OpCode::NEW_ARRAY);
            emit_u16(static_cast<uint16_t>(arrayLit->elements.size()));
        }
    }

    void Compiler::visit(ast::TupleLiteral *tupleLit)
    {
        bool hasSpread = false;
        for (auto &&elem : tupleLit->elements) {
            if (dynamic_ast_cast<SpreadExpression>(elem)) hasSpread = true;
            elem->accept(this);
        }

        if (hasSpread) {
            emit(OpCode::NEW_TUPLE_SPREAD);
            emit_u16(static_cast<uint16_t>(tupleLit->elements.size()));
            for (auto &&elem : tupleLit->elements) {
                if (dynamic_ast_cast<SpreadExpression>(elem)) emit_u8(1);
                else emit_u8(0);
            }
        } else {
            emit(OpCode::NEW_TUPLE);
            emit_u16(static_cast<uint16_t>(tupleLit->elements.size()));
        }
    }

    void Compiler::visit(ast::UnitLiteral *unitLit)
    {
        emit(OpCode::PUSH_UNIT);
    }

    void Compiler::visit(ast::IdExpression *idExpr)
    {
        if (locals.contains(idExpr->id))
        {
            emit(OpCode::LOAD_LOCAL);
            emit_u16(static_cast<uint16_t>(locals[idExpr->id]));
            if (auto it = localTraitObjectTypes.find(idExpr->id); it != localTraitObjectTypes.end())
            {
                uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(it->second);
                emit(OpCode::MAKE_TRAIT_REF);
                emit_u16(traitIndex);
            }
        }
        else if (globals.contains(idExpr->id))
        {
            emit(OpCode::LOAD_GLOBAL);
            emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
            if (auto it = globalTraitObjectTypes.find(idExpr->id); it != globalTraitObjectTypes.end())
            {
                uint16_t traitIndex = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(it->second);
                emit(OpCode::MAKE_TRAIT_REF);
                emit_u16(traitIndex);
            }
        }
        else if (locals.contains("self"))
        {
            emit(OpCode::LOAD_LOCAL);
            emit_u16(static_cast<uint16_t>(locals["self"]));
            int32_t fieldIdx = find_field_index(idExpr->id);
            if (fieldIdx >= 0) {
                emit(OpCode::GET_PROPERTY);
                emit_u16(static_cast<uint16_t>(fieldIdx));
            } else {
                // Fallback: treat as method call with 0 args
                uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(idExpr->id);
                emit(OpCode::INVOKE_MEMBER);
                emit_u16(nameIndex);
                emit_u16(0);
            }
        }
        else throw NotImplementedException("Unknown identifier: " + idExpr->id);
    }

    auto Compiler::find_field_index(const Str &propertyName) const -> int32_t
    {
        for (const auto &type : module.types) {
            if (type.name == current_type_name) {
                for (size_t i = 0; i < type.properties.size(); ++i) {
                    if (type.properties[i] == propertyName) return static_cast<int32_t>(i);
                }
                return -1;
            }
        }
        return -1;
    }

    void Compiler::emit(OpCode op) {
        if (current_function) {
            current_function->code.push_back(static_cast<uint8_t>(op));
            last_emit_was_return = (op == OpCode::RETURN);
        }
    }
    void Compiler::emit_u8(uint8_t val) {
        if (current_function) {
            current_function->code.push_back(val);
            last_emit_was_return = false;
        }
    }
    void Compiler::emit_u16(uint16_t val) {
        if (current_function) {
            current_function->code.push_back(static_cast<uint8_t>(val & 0xFF));
            current_function->code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
            last_emit_was_return = false;
        }
    }
    void Compiler::emit_i32(int32_t val) {
        if (current_function) {
            append_le_bytes(current_function->code, std::bit_cast<uint32_t>(val));
            last_emit_was_return = false;
        }
    }
    void Compiler::emit_i64(int64_t val) {
        if (current_function) {
            append_le_bytes(current_function->code, std::bit_cast<uint64_t>(val));
            last_emit_was_return = false;
        }
    }
    void Compiler::emit_f32(float val) {
        if (current_function) {
            append_le_bytes(current_function->code, std::bit_cast<uint32_t>(val));
            last_emit_was_return = false;
        }
    }
    void Compiler::emit_f64(double val) {
        if (current_function) {
            append_le_bytes(current_function->code, std::bit_cast<uint64_t>(val));
            last_emit_was_return = false;
        }
    }
    void Compiler::patch_i32(size_t offset, int32_t val) {
        if (current_function) {
            auto raw = std::bit_cast<uint32_t>(val);
            for (size_t i = 0; i < sizeof(raw); ++i)
            {
                current_function->code[offset + i] = static_cast<uint8_t>((raw >> (i * 8U)) & 0xFFU);
            }
            last_emit_was_return = false;
        }
    }

    bool Compiler::evaluate_const_bool(ASTRef<ast::Expression> expr)
    {
        using namespace ast;
        if (auto boolVal = dynamic_ast_cast<BooleanValue>(expr))
        {
            return boolVal->value;
        }
        else if (auto unaryExpr = dynamic_ast_cast<UnaryExpression>(expr))
        {
            if (unaryExpr->optr && unaryExpr->optr->type == TokenType::NOT)
            {
                return !evaluate_const_bool(unaryExpr->operand);
            }
            throw RuntimeException("Unsupported unary operator in const if condition");
        }
        else if (auto binExpr = dynamic_ast_cast<BinaryExpression>(expr))
        {
            if (binExpr->optr && binExpr->optr->type == TokenType::AND)
            {
                return evaluate_const_bool(binExpr->left) && evaluate_const_bool(binExpr->right);
            }
            else if (binExpr->optr && binExpr->optr->type == TokenType::OR)
            {
                return evaluate_const_bool(binExpr->left) || evaluate_const_bool(binExpr->right);
            }
            throw RuntimeException("Unsupported binary operator in const if condition");
        }
        throw RuntimeException("const if condition must be a constant boolean expression");
    }
} // namespace NG::orgasm
