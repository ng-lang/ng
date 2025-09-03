
#include <algorithm>
#include <intp/runtime.hpp>
#include <intp/intp.hpp>
#include <intp/runtime_numerals.hpp>
#include <ast.hpp>
#include <utility>
#include <visitor.hpp>
#include <token.hpp>
#include <module.hpp>

#include <debug.hpp>

#include <unordered_map>
#include <functional>

#include <unordered_set>

using namespace NG;
using namespace NG::ast;

namespace NG::runtime
{
    static auto get_native_registry() -> Map<Str, Map<Str, NGInvocable>> &
    {
        static Map<Str, Map<Str, NGInvocable>> natives;
        return natives;
    }

    void register_native_library(Str moduleId, Map<Str, NGInvocable> handlers)
    {
        get_native_registry().insert_or_assign(moduleId, handlers);
    }
}

namespace NG::intp
{

    using namespace NG::runtime;

    struct NextIteration : public std::exception
    {
        Vec<RuntimeRef<NGObject>> slotValues;
        NextIteration(Vec<RuntimeRef<NGObject>> slotValues) : slotValues(slotValues) {};
    };

    struct StopIteration : public std::exception
    {
    };

    template <class T>
    using Set = std::unordered_set<T>;

    void ISummarizable::summary()
    {
    }

    ISummarizable::~ISummarizable() = default;

    static auto evaluateExpr(Operators optr, const RuntimeRef<NGObject> &leftParam, const RuntimeRef<NGObject> &rightParam) -> RuntimeRef<NGObject>
    {
        switch (optr)
        {
        case Operators::PLUS:
            return leftParam->opPlus(rightParam);
        case Operators::MINUS:
            return leftParam->opMinus(rightParam);
        case Operators::TIMES:
            return leftParam->opTimes(rightParam);
        case Operators::DIVIDE:
            return leftParam->opDividedBy(rightParam);
        case Operators::MODULUS:
            return leftParam->opModulus(rightParam);
        case Operators::EQUAL:
            return NGObject::boolean(leftParam->opEquals(rightParam));
        case Operators::NOT_EQUAL:
            return NGObject::boolean(!leftParam->opEquals(rightParam));
        case Operators::LE:
            return NGObject::boolean(leftParam->opLessEqual(rightParam));
        case Operators::LT:
            return NGObject::boolean(leftParam->opLessThan(rightParam));
        case Operators::GE:
            return NGObject::boolean(leftParam->opGreaterEqual(rightParam));
        case Operators::GT:
            return NGObject::boolean(leftParam->opGreaterThan(rightParam));
        case Operators::RSHIFT:
            return leftParam->opRShift(rightParam);
        case Operators::LSHIFT:
            return leftParam->opLShift(rightParam);
        //            case Operators::ASSIGN:
        case Operators::ASSIGN:
        default:
            break;
        }
        return nullptr;
    }

    auto predefs() -> Map<Str, NGInvocable>
    {
        return {};
    }

    struct FunctionPathVisitor : public DummyVisitor
    {

        Str path;

        void visit(IdExpression *idExpr) override
        {
            this->path = idExpr->id;
        }
    };

    struct ExpressionVisitor : public DummyVisitor
    {

        RuntimeRef<NGObject> object = nullptr;

        RuntimeRef<NGContext> context = nullptr;

        explicit ExpressionVisitor(RuntimeRef<NGContext> context) : context(context) {}

#pragma region Visit numeral literals

        void visit(IntegralValue<int8_t> *intVal) override
        {
            object = makert<NGIntegral<int8_t>>(intVal->value);
        }
        void visit(IntegralValue<uint8_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<uint8_t>>(intVal->value);
        }
        void visit(IntegralValue<int16_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<int16_t>>(intVal->value);
        }
        void visit(IntegralValue<uint16_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<uint16_t>>(intVal->value);
        }
        void visit(IntegralValue<int32_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<int32_t>>(intVal->value);
        }
        void visit(IntegralValue<uint32_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<uint32_t>>(intVal->value);
        }
        void visit(IntegralValue<int64_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<int64_t>>(intVal->value);
        }
        void visit(IntegralValue<uint64_t> *intVal) override
        {
            object = std::make_shared<NGIntegral<uint64_t>>(intVal->value);
        }

        // void visit(FloatingPointValue<float16_t> *floatVal) override
        // {
        //     object = std::make_shared<FloatingPointValue<float16_t>>(floatVal->value);
        // }

        void visit(FloatingPointValue<float /*float32_t*/> *floatVal) override
        {
            object = std::make_shared<NGFloatingPoint<float /* float32_t */>>(floatVal->value);
        }

        void visit(FloatingPointValue<double /*float64_t*/> *floatVal) override
        {
            object = std::make_shared<NGFloatingPoint<float /* float64_t */>>(floatVal->value);
        }

        // void visit(FloatingPointValue<float128_t> *floatVal) override
        // {
        //     object = std::make_shared<FloatingPointValue<float128_t>>(floatVal->value);
        // }

#pragma endregion

        void visit(StringValue *strVal) override
        {
            object = makert<NGString>(strVal->value);
        }

        void visit(BooleanValue *boolVal) override
        {
            object = makert<NGBoolean>(boolVal->value);
        }

        void visit(FunCallExpression *funCallExpr) override
        {
            FunctionPathVisitor fpVis{};
            funCallExpr->primaryExpression->accept(&fpVis);
            NGInvCtx invocationContext = makert<NGInvocationContext>();

            for (auto &param : funCallExpr->arguments)
            {
                ExpressionVisitor vis{context};
                param->accept(&vis);
                invocationContext->params.push_back(vis.object);
            }

            RuntimeRef<NGObject> dummy = makert<NGObject>();
            invocationContext->target = dummy;

            if (!context->has_function(fpVis.path, true))
            {
                throw RuntimeException("No such function: " + fpVis.path);
            }

            context->get_function(fpVis.path)(dummy, context, invocationContext);

            this->object = context->retVal;
            context->retVal = nullptr;
        }

        void visit(BinaryExpression *binExpr) override
        {
            ExpressionVisitor leftVisitor{context};
            ExpressionVisitor rightVisitor{context};
            binExpr->left->accept(&leftVisitor);
            binExpr->right->accept(&rightVisitor);

            object = evaluateExpr(binExpr->optr->operatorType, leftVisitor.object, rightVisitor.object);
        }

        void visit(IdExpression *idExpr) override
        {
            object = context->get(idExpr->id);
        }

        void visit(ArrayLiteral *array) override
        {
            Vec<RuntimeRef<NGObject>> objects;

            ExpressionVisitor vis{context};

            for (const auto &element : array->elements)
            {
                element->accept(&vis);
                objects.push_back(vis.object);
            }

            object = makert<NGArray>(objects);
        }

        void visit(IndexAccessorExpression *index) override
        {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            RuntimeRef<NGObject> primaryObject = vis.object;

            index->accessor->accept(&vis);

            RuntimeRef<NGObject> accessorObject = vis.object;

            object = primaryObject->opIndex(accessorObject);
        }

        void visit(IndexAssignmentExpression *index) override
        {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            RuntimeRef<NGObject> primaryObject = vis.object;

            index->accessor->accept(&vis);

            RuntimeRef<NGObject> accessorObject = vis.object;

            index->value->accept(&vis);

            RuntimeRef<NGObject> valueObject = vis.object;

            object = primaryObject->opIndex(accessorObject, valueObject);
        }

        void visit(IdAccessorExpression *idAccExpr) override
        {
            const Str &repr = idAccExpr->accessor->repr();

            ExpressionVisitor vis{context};

            idAccExpr->primaryExpression->accept(&vis);

            RuntimeRef<NGObject> main = vis.object;

            auto invCtx = makert<NGInvocationContext>();
            invCtx->target = main;
            for (const auto &argument : idAccExpr->arguments)
            {
                argument->accept(&vis);
                invCtx->params.push_back(vis.object);
            }

            object = main->respond(repr, context, invCtx);
        }

        void visit(NewObjectExpression *newObj) override
        {
            Str &typeName = newObj->typeName;
            auto ngType = context->get_type(typeName);

            auto structural = makert<NGStructuralObject>();

            structural->customizedType = ngType;

            RuntimeRef<NGContext> newContext = context->fork();
            ExpressionVisitor visitor{newContext};

            for (auto &&[name, expr] : newObj->properties)
            {
                expr->accept(&visitor);
                RuntimeRef<NGObject> result = visitor.object;

                visitor.context->define(name, result);

                structural->properties[name] = result;
            }

            for (const auto &property : ngType->properties)
            {
                if (!structural->properties.contains(property))
                {
                    structural->properties[property] = makert<NGObject>();
                }
            }

            object = structural;
        }

        void visit(AssignmentExpression *assignmentExpr) override
        {
            ExpressionVisitor vis{context};

            assignmentExpr->value->accept(&vis);
            auto result = vis.object;
            if (auto idexpr = dynamic_ast_cast<IdExpression>(assignmentExpr->target); idexpr)
            {
                context->set(idexpr->id, result);
                object = result;
            }
            else if (auto idAcc = dynamic_ast_cast<IdAccessorExpression>(assignmentExpr->target); idAcc)
            {
                idAcc->primaryExpression->accept(&vis);
                auto obj = std::dynamic_pointer_cast<NGStructuralObject>(vis.object);
                obj->properties[idAcc->accessor->id] = result;
            }
            else
            {
                throw RuntimeException("Invalid assignment: " + assignmentExpr->repr());
            }
        }

        void visit(TypeCheckingExpression *typeCheckExpr) override
        {
            ExpressionVisitor vis{context};
            typeCheckExpr->value->accept(&vis);
            RuntimeRef<NGObject> value = vis.object;

            if (auto idExpr = dynamic_ast_cast<IdExpression>(typeCheckExpr->type); idExpr)
            {
                auto name = idExpr->id;
                auto targetType = context->get_type(name);

                this->object = makert<NGBoolean>(*(value->type()) == *(targetType));
            }
            else if (auto idAcccessor = dynamic_ast_cast<IdAccessorExpression>(typeCheckExpr->type); idAcccessor)
            {
                idAcccessor->primaryExpression->accept(&vis);
                auto name = idAcccessor->accessor->id;
                auto result = vis.object;
                auto mod = std::dynamic_pointer_cast<NGModule>(result);
                if (!mod)
                {
                    throw RuntimeException("Invalid module to locate type: " + name);
                }
                auto targetType = mod->types[name];
                if (!targetType)
                {
                    throw RuntimeException("Invalid type name, cannot found." + name);
                }

                this->object = makert<NGBoolean>(*(value->type()) == *(targetType));
            }
            else
            {
                throw RuntimeException("Invalid target expresssion for type checking: " + typeCheckExpr->type->repr());
            }
        }
    };

    struct StatementVisitor : public DummyVisitor
    {
        RuntimeRef<NGContext> context;

        explicit StatementVisitor(RuntimeRef<NGContext> context) : context(context) {}

        void visit(ReturnStatement *returnStatement) override
        {
            ExpressionVisitor vis{context};
            returnStatement->expression->accept(&vis);

            context->retVal = vis.object;
        }

        void visit(IfStatement *ifStmt) override
        {
            ExpressionVisitor vis{context};
            ifStmt->testing->accept(&vis);

            StatementVisitor stmtVis{context};
            if (vis.object->boolValue())
            {
                ifStmt->consequence->accept(&stmtVis);
            }
            else if (ifStmt->alternative != nullptr)
            {
                ifStmt->alternative->accept(&stmtVis);
            }
        }

        void visit(CompoundStatement *stmt) override
        {
            auto newContext = context->fork();
            StatementVisitor vis{newContext};
            for (const auto &innerStmt : stmt->statements)
            {
                innerStmt->accept(&vis);
                if (vis.context->retVal != nullptr)
                {
                    context->retVal = vis.context->retVal;
                    break;
                }
            }
        }

        void visit(ValDefStatement *valDef) override
        {
            ExpressionVisitor vis{context};
            valDef->value->accept(&vis);

            context->define(valDef->name, vis.object);
        }

        void visit(SimpleStatement *simpleStmt) override
        {
            ExpressionVisitor vis{context};
            simpleStmt->expression->accept(&vis);
        }

        void visit(LoopStatement *loopStatement) override
        {
            auto context = this->context->fork();
            ExpressionVisitor vis{context};
            for (auto &&binding : loopStatement->bindings)
            {
                binding.target->accept(&vis);
                switch (binding.type)
                {
                case LoopBindingType::LOOP_ASSIGN:
                    context->define(binding.name, vis.object);
                    break;
                default:
                    throw RuntimeException("Unsupported loop binding");
                }
            }

            StatementVisitor stmtVis{context};
            bool stopLoop = false;
            while (!stopLoop)
            {
                try
                {
                    loopStatement->loopBody->accept(&stmtVis);
                    if (vis.context->retVal != nullptr)
                    {
                        this->context->retVal = vis.context->retVal;
                    }
                    stopLoop = true;
                }
                catch (NextIteration iter)
                {
                    int i = 0;
                    for (auto &&object : iter.slotValues)
                    {
                        context->set(loopStatement->bindings[i].name, object);
                        i++;
                    }
                }
            }
        }

        void visit(NextStatement *nextStatement) override
        {
            ExpressionVisitor vis{context};
            try
            {
                Vec<RuntimeRef<NGObject>> slotValues{};
                for (auto &&expr : nextStatement->expressions)
                {
                    expr->accept(&vis);
                    slotValues.push_back(vis.object);
                }
                throw NextIteration{slotValues};
            }
            catch (StopIteration)
            {
                // do nothing
            }
        }
    };

    struct Stupid : public Interpreter, DummyVisitor // NOLINT(cppcoreguidelines-special-member-functions)
    {
        RuntimeRef<NGContext> context;

        bool loading_prelude = false;

        void withNewContext(RuntimeRef<NGContext> newContext, const std::function<void()> &execution)
        {
            RuntimeRef<NGContext> previousContext = context;
            context = std::move(newContext);
            execution();
            context = previousContext;
        }

        explicit Stupid(RuntimeRef<NGContext> _context) : context(_context)
        {
            loadPrelude();
        }

        void loadPrelude()
        {
            // load std.prelude by default.
            auto importPrelude = makeast<ImportDecl>();
            importPrelude->module = "prelude";
            importPrelude->modulePath.push_back("std");
            importPrelude->modulePath.push_back("prelude");
            importPrelude->imports.push_back("*");
            importPrelude->accept(this);
        }

        void visit(CompileUnit *compileUnit) override
        {
            debug_log("Visiting module", compileUnit->module->name);
            if (!compileUnit->path.empty())
            {
                context->appendModulePath(compileUnit->path);
            }
            if (compileUnit->path.contains("std") && compileUnit->fileName.contains("prelude"))
            {
                loading_prelude = true;
            }
            compileUnit->module->accept(this);
            loading_prelude = false;
        }

        void visit(Module *mod) override
        {

            context->try_save_module();

            context->new_current(mod);

            if (!loading_prelude)
            {
                // this->loadPrelude();
            }
            for (auto &&import : mod->imports)
            {
                import->accept(this);
            }

            Set<Str> definedSymbols = {};
            for (auto &&defs : mod->definitions)
            {
                definedSymbols.insert(defs->name());
                defs->accept(this);
            }

            for (auto &&exp : mod->exports)
            {
                if (!definedSymbols.contains(exp) && exp != "*")
                {
                    throw RuntimeException("Export undefined symbol: " + exp);
                }
            }

            StatementVisitor vis{context};

            for (const auto &stmt : mod->statements)
            {
                stmt->accept(&vis);
            }
        }

        void visit(ImportDecl *importDecl) override
        {
            if (!context->has_module(importDecl->module))
            {
                // load module
                NG::module::FileBasedExternalModuleLoader loader{context->modulePaths};
                auto &&moduleInfo = loader.load(importDecl->modulePath);
                auto &&ast = moduleInfo->moduleAst;

                RuntimeRef<NGContext> ctx = makert<NGContext>(Vec<Str>{
                    NG::module::standard_library_base_path(),
                });
                withNewContext(ctx,
                               [&ast, &moduleInfo, intp = this]()
                               {
                                    ast->accept(intp);
                                    intp->context->try_save_module();
                                    if (get_native_registry().contains(moduleInfo->moduleId)) {
                                        intp->context->current_module()->native_functions 
                                            = get_native_registry()[moduleInfo->moduleId];
                                    } });
                context->define_module(importDecl->module, ctx->current_module());
            }
            RuntimeRef<NGModule> targetModule = context->get_module(importDecl->module);
            Set<Str> imports = resolveImports(importDecl, targetModule);

            for (auto &&imp : imports)
            {
                if (targetModule->functions.contains(imp))
                {
                    context->define_function(imp, targetModule->functions[imp]);
                }
                if (targetModule->types.contains(imp))
                {
                    context->define_type(imp, targetModule->types[imp]);
                }
                if (targetModule->objects.contains(imp))
                {
                    context->define(imp, targetModule->objects[imp]);
                }
                if (targetModule->native_functions.contains(imp))
                {
                    context->define_function(imp, targetModule->native_functions[imp]);
                }
            }

            if (!importDecl->alias.empty())
            {
                context->define(importDecl->alias, targetModule);
            }
        }

        static auto resolveImports(ImportDecl *importDecl, const RuntimeRef<NGModule> &targetModule) -> Set<Str>
        {
            bool importAll = (std::ranges::find(importDecl->imports,
                                                "*") != end(importDecl->imports));

            bool exportsAll = (std::find(begin(targetModule->exports), end(targetModule->exports),
                                         "*") != end(targetModule->exports));

            Set<Str> exported{};
            if (exportsAll)
            {
                for (auto &&[fnName, _ignored] : targetModule->functions)
                {
                    exported.insert(fnName);
                }
                for (auto &&[typeName, _ignored] : targetModule->types)
                {
                    exported.insert(typeName);
                }
                for (auto &&[objName, _ignored] : targetModule->objects)
                {
                    exported.insert(objName);
                }
                for (auto &&[fnName, _ignored] : targetModule->native_functions)
                {
                    exported.insert(fnName);
                }
            }
            else
            {
                exported.insert(begin(targetModule->exports), end(targetModule->exports));
            }

            if (importAll)
            {
                return exported;
            }

            for (auto &&imp : importDecl->imports)
            {
                if (!exported.contains(imp))
                {
                    throw RuntimeException("Cannot found symbol " + imp + " in module " + importDecl->module);
                }
            }
            return Set<Str>{begin(importDecl->imports), end(importDecl->imports)};
        }

        // virtual void visit(Definition *def);
        // virtual void visit(Param *param);
        void visit(FunctionDef *funDef) override
        {
            if (funDef->native)
            {
                return;
            }
            context->define_function(funDef->funName, [this, funDef](const NGSelf &dummy,
                                                                     const NGCtx &ngContext,
                                                                     const NGInvCtx &invocationContext)
                                     {
                RuntimeRef<NGContext> newContext = ngContext->fork();
                for (size_t i = 0; i < funDef->params.size(); ++i)
                {
                    if (invocationContext->params.size() > i) 
                    {
                        newContext->define(funDef->params[i]->paramName, invocationContext->params[i]);
                    } 
                    else if (funDef->params[i]->value != nullptr) 
                    {
                        ExpressionVisitor vis {ngContext};
                        funDef->params[i]->value->accept(&vis);
                        auto value = vis.object;
                        newContext->define(funDef->params[i]->paramName, value);
                    } else {
                        throw RuntimeException("No matched parameter");
                    }
                }

                withNewContext(newContext, [this, funDef]
                                {
                                bool tailRecur = true;
                                while (tailRecur) {
                                    try {
                                        StatementVisitor vis{context};
                                        funDef->body->accept(&vis);   
                                        tailRecur = false;              
                                    } catch (NextIteration nextIter) {
                                        tailRecur = true;
                                        for (size_t i = 0; i < nextIter.slotValues.size(); i++)
                                        {
                                            context->set(funDef->params[i]->paramName, nextIter.slotValues[i]);
                                        }   
                                    }
                                }
                });
                ngContext->retVal = newContext->retVal; });
        }

        void visit(Statement *stmt) override
        {
            StatementVisitor vis{context};
            stmt->accept(&vis);
        }

        void visit(ValDef *valDef) override
        {
            ExpressionVisitor vis{context};
            valDef->body->value->accept(&vis);
            context->define(valDef->name(), vis.object);
        }

        void visit(TypeDef *typeDef) override
        {
            auto type = makert<NGType>();

            type->name = typeDef->typeName;

            for (const auto &property : typeDef->properties)
            {
                type->properties.push_back(property->propertyName);
            }

            for (const auto &memFn : typeDef->memberFunctions)
            {
                type->memberFunctions[memFn->funName] = [this, memFn](const NGSelf &dummy,
                                                                      const NGCtx &ngContext,
                                                                      const NGInvCtx &invocationContext)
                {
                    RuntimeRef<NGContext> newContext = ngContext->fork();
                    for (size_t i = 0; i < memFn->params.size(); ++i)
                    {
                        newContext->define(memFn->params[i]->paramName, invocationContext->params[i]);
                    }

                    newContext->define("self", dummy);

                    if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(dummy); structural != nullptr)
                    {
                        for (const auto &property : structural->properties)
                        {
                            newContext->define(property.first, property.second);
                        }
                    }

                    withNewContext(newContext, [this, memFn]
                                   {
                        StatementVisitor vis{context};

                        memFn->body->accept(&vis); });
                    ngContext->retVal = newContext->retVal;
                };
            }

            context->define_type(type->name, type);
        }

        void summary() override
        {
            context->summary();
        }

        auto intpContext() -> NGContext * override
        {
            return context.get();
        }

        ~Stupid() override = default;
    };

    auto stupid() -> Interpreter *
    {
        NG::library::prelude::do_register();
        auto context = makert<NGContext>(Vec<Str>{
            "",
            NG::module::standard_library_base_path(),
        });

        return new Stupid(context); // NOLINT(cppcoreguidelines-owning-memory)
    }

} // namespace NG::interpreter
