#include <orgasm/compiler.hpp>
#include <module.hpp>
#include <cstring>
#include <token.hpp>

namespace NG::orgasm
{
    using namespace NG::ast;
    using NG::module::get_module_registry;
    using NG::module::FileBasedExternalModuleLoader;

    auto Compiler::compile(ASTRef<CompileUnit> compileUnit) -> BytecodeModule
    {
        module = BytecodeModule{};
        module.name = compileUnit->fileName;
        
        module.constants.push_back(0);
        module.constants.push_back(1);

        compileUnit->module->accept(this);
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

        for (auto &&def : mod->definitions)
        {
            if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
            {
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
                module.types.push_back(std::move(type));

                for (auto &&memFn : typeDef->memberFunctions)
                {
                    Function fun;
                    fun.name = typeDef->typeName + "." + memFn->funName;
                    fun.num_params = static_cast<int32_t>(memFn->params.size() + 1); // +1 for self
                    module.functions.push_back(std::move(fun));
                    functionDefs[fun.name] = memFn.get();
                }
            }
            else if (auto aliasDef = dynamic_ast_cast<TypeAliasDef>(def))
            {
                // Type alias is transparent — register as a type with no properties
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
                    type.variants.push_back(std::move(v));

                    variant_map[taggedUnion->variants[i].variantName] = VariantInfo{
                        .unionName = taggedUnion->typeName,
                        .variantIndex = i,
                    };
                }
                module.types.push_back(std::move(type));
            }
        }

        for (auto &&exportedName : mod->exports)
        {
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
        locals.clear();
        loop_stack.clear();

        for (auto &&def : mod->definitions)
        {
            if (auto valDef = dynamic_ast_cast<ValDef>(def))
            {
                if (auto valStmt = dynamic_ast_cast<ValDefStatement>(valDef->body))
                {
                    valStmt->value->accept(this);
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
        for (auto &&def : mod->definitions)
        {
            if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
            {
                current_function = &module.functions[funIndex++];
                locals.clear();
                loop_stack.clear();
                
                LoopInfo info;
                info.startIp = 0; // Function start
                for (int32_t i = 0; i < funDef->params.size(); ++i)
                {
                    locals[funDef->params[i]->paramName] = i;
                    info.bindingSlots.push_back(i);
                }
                loop_stack.push_back(std::move(info));

                if (funDef->body) funDef->body->accept(this);

                loop_stack.pop_back();
                current_function->num_locals = static_cast<int32_t>(locals.size());

                if (current_function->code.empty() || static_cast<OpCode>(current_function->code.back()) != OpCode::RETURN)
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
                    locals.clear();
                    loop_stack.clear();
                    current_type_name = typeDef->typeName;
                    
                    LoopInfo info;
                    info.startIp = 0;
                    locals["self"] = 0;
                    info.bindingSlots.push_back(0); // self can be updated by next? maybe not, but keep slot.
                    
                    for (int32_t i = 0; i < memFn->params.size(); ++i)
                    {
                        locals[memFn->params[i]->paramName] = i + 1;
                        info.bindingSlots.push_back(i + 1);
                    }
                    loop_stack.push_back(std::move(info));

                    if (memFn->body) memFn->body->accept(this);

                    loop_stack.pop_back();
                    current_function->num_locals = static_cast<int32_t>(locals.size());

                    if (current_function->code.empty() || static_cast<OpCode>(current_function->code.back()) != OpCode::RETURN)
                    {
                        emit(OpCode::PUSH_UNIT);
                        emit(OpCode::RETURN);
                    }
                }
            }
        }
        current_function = nullptr;
    }

    void Compiler::visit(ast::FunctionDef *funDef) {}
    void Compiler::visit(ast::TypeDef *typeDef) {}
    void Compiler::visit(ast::TypeAliasDef *typeAliasDef) {}
    void Compiler::visit(ast::NewTypeDef *newTypeDef) {}

    void Compiler::visit(ast::CastExpression *castExpr)
    {
        castExpr->expression->accept(this);

        Str targetTypeName;
        if (auto anno = dynamic_ast_cast<ast::TypeAnnotation>(castExpr->targetType))
        {
            targetTypeName = anno->name;
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
        auto moduleInfo = registry.queryModuleById(importDecl->module);
        
        if (!moduleInfo) {
            NG::module::FileBasedExternalModuleLoader loader{modulePaths};
            moduleInfo = loader.load(importDecl->modulePath);
            if (!moduleInfo->bytecodeModule) {
                Compiler inner(modulePaths);
                auto cu = dynamic_ast_cast<CompileUnit>(moduleInfo->moduleAst);
                if (!cu) throw RuntimeException("Failed to cast AST to CompileUnit for module " + importDecl->module);
                auto bc = inner.compile(cu);
                moduleInfo->bytecodeModule = std::make_shared<BytecodeModule>(std::move(bc));
            }
            registry.addModuleInfo(moduleInfo);
        }

        auto bc = moduleInfo->bytecodeModule;
        for (auto &&imp : importDecl->imports) {
            if (imp == "*") {
                for (auto &&[name, index] : bc->exports) {
                    int32_t importIdx = static_cast<int32_t>(module.imports.size());
                    module.imports.push_back({importDecl->module, name});
                    imported_symbols[name] = {importDecl->module, importIdx};
                }
            } else {
                int32_t importIdx = static_cast<int32_t>(module.imports.size());
                module.imports.push_back({importDecl->module, imp});
                imported_symbols[imp] = {importDecl->module, importIdx};
            }
        }
    }

    void Compiler::visit(ast::FunCallExpression *funCallExpr)
    {
        if (auto idExpr = dynamic_ast_cast<IdExpression>(funCallExpr->primaryExpression))
        {
            if (idExpr->id == "print")
            {
                for (auto &&arg : funCallExpr->arguments) arg->accept(this);
                emit(OpCode::PRINT);
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
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
            for (size_t i = 0; i < module.functions.size(); ++i)
            {
                if (module.functions[i].name == idExpr->id) { funIndex = static_cast<int32_t>(i); break; }
            }

            if (funIndex != -1)
            {
                auto &funName = module.functions[funIndex].name;
                FunctionDef *def = functionDefs.contains(funName) ? functionDefs[funName] : nullptr;
                bool hasPack = def && !def->genericParams.empty() && def->genericParams.back()->isPack;

                if (hasPack) {
                    // Emit non-pack args normally
                    int32_t nonPackCount = static_cast<int32_t>(def->params.size()) - 1;
                    for (int32_t i = 0; i < nonPackCount && i < static_cast<int32_t>(funCallExpr->arguments.size()); ++i) {
                        funCallExpr->arguments[i]->accept(this);
                    }
                    // Pack remaining args into a NEW_TUPLE
                    int32_t packStart = nonPackCount;
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
                    size_t provided = funCallExpr->arguments.size();
                    size_t expected = def->params.size();
                    for (auto &&arg : funCallExpr->arguments) arg->accept(this);
                    if (provided < expected) {
                        for (size_t i = provided; i < expected; ++i) {
                            if (def->params[i]->value) {
                                def->params[i]->value->accept(this);
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
                emit(OpCode::CALL_IMPORT);
                emit_u16(static_cast<uint16_t>(imp.importIndex));
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
                return;
            }

            if (nativeFnNames.contains(idExpr->id)) {
                for (auto &&arg : funCallExpr->arguments) arg->accept(this);
                uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(idExpr->id);
                emit(OpCode::NATIVE_CALL);
                emit_u16(nameIdx);
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
                return;
            }

            if (locals.contains("self")) {
                emit(OpCode::LOAD_LOCAL);
                emit_u16(static_cast<uint16_t>(locals["self"]));
                uint16_t nameIdx = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(idExpr->id);
                emit(OpCode::INVOKE_MEMBER);
                emit_u16(nameIdx);
                emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
                return;
            }

            throw NotImplementedException("Unknown function: " + idExpr->id);
        }
        else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(funCallExpr->primaryExpression))
        {
            for (auto &&arg : funCallExpr->arguments) arg->accept(this);
            idAcc->primaryExpression->accept(this);
            uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
            module.strings.push_back(idAcc->accessor->repr());
            emit(OpCode::INVOKE_MEMBER);
            emit_u16(nameIndex);
            emit_u16(static_cast<uint16_t>(funCallExpr->arguments.size()));
        }
        else throw NotImplementedException("Complex calls not implemented");
    }

    void Compiler::visit(ast::IdAccessorExpression *idAccExpr)
    {
        idAccExpr->primaryExpression->accept(this);
        for (auto &&arg : idAccExpr->arguments)
        {
            arg->accept(this);
        }
        uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(idAccExpr->accessor->repr());
        emit(OpCode::INVOKE_MEMBER);
        emit_u16(nameIndex);
        emit_u16(static_cast<uint16_t>(idAccExpr->arguments.size()));
    }

    void Compiler::visit(ast::IndexAccessorExpression *idxAccExpr)
    {
        idxAccExpr->primary->accept(this);
        idxAccExpr->accessor->accept(this);
        emit(OpCode::GET_INDEX);
    }

    void Compiler::visit(ast::IndexAssignmentExpression *idxAssignExpr)
    {
        idxAssignExpr->primary->accept(this);
        idxAssignExpr->accessor->accept(this);
        idxAssignExpr->value->accept(this);
        emit(OpCode::SET_INDEX);
    }

    void Compiler::visit(ast::CompoundStatement *compoundStmt)
    {
        auto oldLocals = locals;
        for (auto &&stmt : compoundStmt->statements) stmt->accept(this);
        locals = std::move(oldLocals);
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

    void Compiler::visit(ast::TypeOfExpression *typeofExpr) {}

    void Compiler::visit(ast::TypeCheckingExpression *typeCheck)
    {
        typeCheck->value->accept(this);
        uint16_t index = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(typeCheck->type->name);
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
        if (current_function)
        {
            int32_t index = static_cast<int32_t>(locals.size());
            locals[valDefStmt->name] = index;
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
            bool condValue = evaluate_const_bool(ifStmt->testing);
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
            emit_u16(static_cast<uint16_t>(i));
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

            // DUP the tagged value for payload extraction
            emit(OpCode::DUP);

            // Extract payload fields for each binding
            for (size_t j = 0; j < c.bindings.size(); ++j) {
                emit(OpCode::DUP);
                emit(OpCode::GET_PAYLOAD);
                emit_u16(static_cast<uint16_t>(j));
                if (!c.bindings[j].empty()) {
                    locals[c.bindings[j]] = static_cast<int32_t>(locals.size());
                    emit(OpCode::STORE_LOCAL);
                    emit_u16(static_cast<uint16_t>(locals[c.bindings[j]]));
                    emit(OpCode::POP);
                } else {
                    emit(OpCode::POP);
                }
            }

            // Pop the scrutinee copy
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

        // Pop the original scrutinee
        emit(OpCode::POP);
    }


    void Compiler::visit(ast::UnaryExpression *unaryExpr)
    {
        unaryExpr->operand->accept(this);
        switch (unaryExpr->optr->type)
        {
        case TokenType::MINUS: emit(OpCode::NEG_I32); break;
        case TokenType::NOT:   emit(OpCode::NOT);     break;
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
                emit(OpCode::STORE_LOCAL);
                emit_u16(static_cast<uint16_t>(locals[idExpr->id]));
            }
            else if (globals.contains(idExpr->id))
            {
                emit(OpCode::STORE_GLOBAL);
                emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
            }
            else if (locals.contains("self"))
            {
                // Assign to property of self: push self then val
                emit(OpCode::LOAD_LOCAL);
                emit_u16(static_cast<uint16_t>(locals["self"]));
                assignExpr->value->accept(this);
                int32_t fieldIdx = find_field_index(idExpr->id);
                if (fieldIdx >= 0) {
                    emit(OpCode::SET_PROPERTY);
                    emit_u16(static_cast<uint16_t>(fieldIdx));
                } else {
                    throw NotImplementedException("Unknown property: " + idExpr->id + " in type " + current_type_name);
                }
            }
            else throw NotImplementedException("Unknown assignment target: " + idExpr->id);
        }
        else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignExpr->target))
        {
            idAcc->primaryExpression->accept(this);  // push object
            // Check if accessor is a numeric literal (tuple index like x.1)
            Str accessorRepr = idAcc->accessor->repr();
            bool isNumeric = !accessorRepr.empty() && std::all_of(accessorRepr.begin(), accessorRepr.end(), ::isdigit);
            if (isNumeric) {
                // SET_INDEX pops: val, idx, obj. Push order: obj, idx, val.
                int32_t idx = std::stoi(accessorRepr);
                emit(OpCode::PUSH_I32);
                emit_i32(idx);
                assignExpr->value->accept(this);  // push value on top
                emit(OpCode::SET_INDEX);
            } else {
                assignExpr->value->accept(this);
                uint16_t nameIndex = static_cast<uint16_t>(module.strings.size());
                module.strings.push_back(accessorRepr);
                emit(OpCode::SET_PROPERTY_STR);
                emit_u16(nameIndex);
            }
        }
        else throw NotImplementedException("Assignment to non-id: " + assignExpr->target->repr());
    }

    void Compiler::visit(ast::NewObjectExpression *newObj)
    {
        // Find the type definition to get property order
        int32_t typeIdx = -1;
        for (size_t i = 0; i < module.types.size(); ++i) {
            if (module.types[i].name == newObj->typeName) { typeIdx = static_cast<int32_t>(i); break; }
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
            // Fallback: push values in iteration order
            numFields = static_cast<uint16_t>(newObj->properties.size());
            for (auto &&[name, expr] : newObj->properties) expr->accept(this);
        }

        uint16_t typeStrIdx = static_cast<uint16_t>(module.strings.size());
        module.strings.push_back(newObj->typeName);
        emit(OpCode::NEW_OBJECT);
        emit_u16(typeStrIdx);
        emit_u16(numFields);
    }

    void Compiler::visit(ast::IntegralValue<int8_t> *intVal) { emit(OpCode::PUSH_I8); emit_i32(static_cast<int32_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<uint8_t> *intVal) { emit(OpCode::PUSH_U8); emit_i32(static_cast<int32_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<int16_t> *intVal) { emit(OpCode::PUSH_I16); emit_i32(static_cast<int32_t>(intVal->value)); }
    void Compiler::visit(ast::IntegralValue<uint16_t> *intVal) { emit(OpCode::PUSH_U16); emit_i32(static_cast<int32_t>(intVal->value)); }
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

    void Compiler::visit(ast::ArrayLiteral *arrayLit)
    {
        bool hasSpread = false;
        for (auto &&elem : arrayLit->elements) {
            if (dynamic_ast_cast<SpreadExpression>(elem)) hasSpread = true;
            elem->accept(this);
        }

        if (hasSpread) {
            emit(OpCode::NEW_ARRAY_SPREAD);
            emit_u16(static_cast<uint16_t>(arrayLit->elements.size()));
            for (auto &&elem : arrayLit->elements) {
                if (dynamic_ast_cast<SpreadExpression>(elem)) emit_u8(1);
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
        }
        else if (globals.contains(idExpr->id))
        {
            emit(OpCode::LOAD_GLOBAL);
            emit_u16(static_cast<uint16_t>(globals[idExpr->id]));
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

    void Compiler::emit(OpCode op) { if (current_function) current_function->code.push_back(static_cast<uint8_t>(op)); }
    void Compiler::emit_u8(uint8_t val) { if (current_function) current_function->code.push_back(val); }
    void Compiler::emit_u16(uint16_t val) {
        if (current_function) {
            current_function->code.push_back(static_cast<uint8_t>(val & 0xFF));
            current_function->code.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        }
    }
    void Compiler::emit_i32(int32_t val) {
        if (current_function) {
            uint8_t bytes[4]; std::memcpy(bytes, &val, 4);
            for (int i = 0; i < 4; ++i) current_function->code.push_back(bytes[i]);
        }
    }
    void Compiler::emit_i64(int64_t val) {
        if (current_function) {
            uint8_t bytes[8]; std::memcpy(bytes, &val, 8);
            for (int i = 0; i < 8; ++i) current_function->code.push_back(bytes[i]);
        }
    }
    void Compiler::emit_f32(float val) {
        if (current_function) {
            uint8_t bytes[4]; std::memcpy(bytes, &val, 4);
            for (int i = 0; i < 4; ++i) current_function->code.push_back(bytes[i]);
        }
    }
    void Compiler::emit_f64(double val) {
        if (current_function) {
            uint8_t bytes[8]; std::memcpy(bytes, &val, 8);
            for (int i = 0; i < 8; ++i) current_function->code.push_back(bytes[i]);
        }
    }
    void Compiler::patch_i32(size_t offset, int32_t val) {
        if (current_function) {
            uint8_t bytes[4]; std::memcpy(bytes, &val, 4);
            for (int i = 0; i < 4; ++i) current_function->code[offset + i] = bytes[i];
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
