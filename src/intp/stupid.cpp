
#include <algorithm>
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <ast.hpp>
#include <utility>
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

namespace NG::intp
{

    using namespace NG::runtime;

    template <class T>
    using Set = std::unordered_set<T>;

    void ISummarizable::summary()
    {
    }

    ISummarizable::~ISummarizable() = default;

    static auto evaluateExpr(Operators optr, const RuntimeRef<NGObject>& leftParam, const RuntimeRef<NGObject>& rightParam) -> RuntimeRef<NGObject>
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
        default:
            break;
        }
        return nullptr;
    }

    static auto predefs() -> Map<Str, NGInvocationHandler>
    {
        return {
            {"print", [](const RuntimeRef<NGObject>& self, const RuntimeRef<NGContext>& context, const RuntimeRef<NGInvocationContext>& invCtx)
             {
                 Vec<RuntimeRef<NGObject>> &params = invCtx->params;
                 for (size_t i = 0; i < params.size(); ++i)
                 {
                     std::cout << params[i]->show();
                     if (i != params.size() - 1)
                     {
                         std::cout << ", ";
                     }
                 }
                 std::cout << '\n';
             }},
            {"assert", [](const RuntimeRef<NGObject>& self, const RuntimeRef<NGContext>& context, const RuntimeRef<NGInvocationContext>& invCtx)
             {
                 for (const auto &param : invCtx->params)
                 {
                     auto ngBool = std::dynamic_pointer_cast<NGBoolean>(param);
                     if (ngBool == nullptr || !ngBool->value)
                     {
                         std::cerr << param->show();
                         throw AssertionException();
                     }
                 }
             }}};
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

        explicit ExpressionVisitor(RuntimeRef<NGContext> context) : context(std::move(std::move(context))) {}

#pragma region Visit numeral literals

        void visit(IntegerValue *intVal) override
        {
            object = makert<NGIntegral<int32_t>>(intVal->value);
        }

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
            RuntimeRef<NGInvocationContext> invocationContext = makert<NGInvocationContext>();

            for (auto &param : funCallExpr->arguments)
            {
                param->accept(this);
                invocationContext->params.push_back(this->object);
            }

            RuntimeRef<NGObject> dummy = makert<NGObject>();
            invocationContext->target = dummy;

            if (!context->functions.contains(fpVis.path))
            {
                throw RuntimeException("No such function: " + fpVis.path);
            }

            context->functions[fpVis.path](dummy, context, invocationContext);

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
            object = context->objects[idExpr->id];
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
            auto ngType = context->types[typeName];

            auto structural = makert<NGStructuralObject>();

            structural->customizedType = ngType;

            RuntimeRef<NGContext> newContext = makert<NGContext>(*context);
            ExpressionVisitor visitor{newContext};

            for (auto &&[name, expr] : newObj->properties)
            {
                expr->accept(&visitor);
                RuntimeRef<NGObject> result = visitor.object;

                visitor.context->objects[name] = result;

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
    };

    struct StatementVisitor : public DummyVisitor
    {
        RuntimeRef<NGContext> context;

        explicit StatementVisitor(RuntimeRef<NGContext> context) : context(std::move(std::move(context))) {}

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
            StatementVisitor vis{context};
            for (const auto& innerStmt : stmt->statements)
            {
                innerStmt->accept(&vis);
                if (vis.context->retVal != nullptr)
                {
                    break;
                }
            }
        }

        void visit(ValDefStatement *valDef) override
        {
            ExpressionVisitor vis{context};
            valDef->value->accept(&vis);

            context->objects[valDef->name] = vis.object;
        }

        void visit(SimpleStatement *simpleStmt) override
        {
            ExpressionVisitor vis{context};
            simpleStmt->expression->accept(&vis);
        }
    };

    struct Stupid : public Interpreter, DummyVisitor
    {
        RuntimeRef<NGContext> context;

        Map<Str, ASTRef<ASTNode>> externalModuleAstCache;

        void withNewContext(RuntimeRef<NGContext> newContext, const std::function<void()> &execution)
        {
            RuntimeRef<NGContext> previousContext = context;
            context = std::move(newContext);
            execution();
            context = previousContext;
        }

        explicit Stupid(RuntimeRef<NGContext> _context) : context(std::move(std::move(_context)))
        {
        }

        void visit(CompileUnit *compileUnit) override
        {
            if (!compileUnit->path.empty())
            {
                context->appendModulePath(compileUnit->path);
            }

            for (auto &&module : compileUnit->modules)
            {
                module->accept(this);
            }
        }

        void visit(Module *mod) override
        {

            if (context->currentModule != nullptr)
            {
                saveModule();
            }

            context->currentModuleName = mod->name;
            context->currentModule = makert<NGModule>();

            context->currentModule->exports = mod->exports;

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

        void saveModule() const
        {
            // copy
            context->currentModule->objects = context->objects;
            context->currentModule->types = context->types;
            context->currentModule->functions = context->functions;

            // save
            context->modules.insert_or_assign(context->currentModuleName, context->currentModule);

            // clear
            context->objects = {};
            context->types = {};
            context->functions = predefs();
        }

        void visit(ImportDecl *importDecl) override
        {
            if (!context->modules.contains(importDecl->module))
            {
                // load module
                NG::module::FileBasedExternalModuleLoader loader{context->modulePaths};
                auto &&ast = loader.load(importDecl->module);

                externalModuleAstCache.insert_or_assign(importDecl->module, ast);
                RuntimeRef<NGContext> ctx = makert<NGContext>();
                withNewContext(ctx, [&ast, intp = this]()
                               {
                                    ast->accept(intp);
                                    intp->saveModule(); });
                context->modules.insert_or_assign(importDecl->module, ctx->currentModule);
            }
            RuntimeRef<NGModule> targetModule = context->modules[importDecl->module];
            Set<Str> imports = resolveImports(importDecl, targetModule);

            for (auto &&imp : imports)
            {
                if (targetModule->functions.contains(imp))
                {
                    context->functions[imp] = targetModule->functions[imp];
                }
                if (targetModule->types.contains(imp))
                {
                    context->types[imp] = targetModule->types[imp];
                }
                if (targetModule->objects.contains(imp))
                {
                    context->objects[imp] = targetModule->objects[imp];
                }
            }

            if (!importDecl->alias.empty())
            {
                context->objects.insert_or_assign(importDecl->alias, targetModule);
            }
        }

        static auto resolveImports(ImportDecl *importDecl, const RuntimeRef<NGModule>& targetModule) -> Set<Str>
        {
            bool importAll = (std::ranges::find(importDecl->imports,
                                        "*") != end(importDecl->imports));

            bool exportsAll = (std::find(begin(targetModule->exports), end(targetModule->exports),
                                         "*") != end(targetModule->exports));

            Set<Str> exported{};
            if (exportsAll)
            {
                for (auto &&[fnName, _] : targetModule->functions)
                {
                    exported.insert(fnName);
                }
                for (auto &&[typeName, _] : targetModule->types)
                {
                    exported.insert(typeName);
                }
                for (auto &&[objName, _] : targetModule->objects)
                {
                    exported.insert(objName);
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
            context->functions[funDef->funName] = [this, funDef](const RuntimeRef<NGObject>& dummy,
                                                                 const RuntimeRef<NGContext>& ngContext,
                                                                 const RuntimeRef<NGInvocationContext>& invocationContext)
            {
                RuntimeRef<NGContext> newContext = makert<NGContext>(*ngContext);
                for (size_t i = 0; i < funDef->params.size(); ++i)
                {
                    newContext->objects[funDef->params[i]->paramName] = invocationContext->params[i];
                }

                withNewContext(newContext, [this, funDef]
                               {
                    StatementVisitor vis{context};

                    funDef->body->accept(&vis); });
                ngContext->retVal = newContext->retVal;
            };
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
            context->objects[valDef->name()] = vis.object;
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
                type->memberFunctions[memFn->funName] = [this, memFn](const RuntimeRef<NGObject>& dummy,
                                                                      const RuntimeRef<NGContext>& ngContext,
                                                                      const RuntimeRef<NGInvocationContext>& invocationContext)
                {
                    RuntimeRef<NGContext> newContext = makert<NGContext>(*ngContext);
                    for (size_t i = 0; i < memFn->params.size(); ++i)
                    {
                        newContext->objects[memFn->params[i]->paramName] = invocationContext->params[i];
                    }

                    newContext->objects["self"] = dummy;

                    if (auto structural = std::dynamic_pointer_cast<NGStructuralObject>(dummy); structural != nullptr)
                    {
                        for (const auto &property : structural->properties)
                        {
                            newContext->objects[property.first] = property.second;
                        }
                    }

                    withNewContext(newContext, [this, memFn]
                                   {
                        StatementVisitor vis{context};

                        memFn->body->accept(&vis); });
                    ngContext->retVal = newContext->retVal;
                };
            }

            context->types[type->name] = type;
        }

        void summary() override
        {
            debug_log("Context objects size", context->objects.size());

            for (const auto& p : context->objects)
            {
                debug_log("Context object", "key:", p.first, "value:", p.second->show());
            }

            debug_log("Context modules size", context->modules.size());

            for (const auto &p : context->modules)
            {
                debug_log("Context module", "name:", p.first, "value:", code(p.second->size()));
            }

            for (const auto &type : context->types)
            {
                debug_log("Context types", "name:", type.first, "members:",
                          type.second->properties.size() + type.second->memberFunctions.size());
            }
        }

        auto intpContext() -> NGContext * override
        {
            return context.get();
        }

        ~Stupid() override
        = default;
    };

    auto stupid() -> Interpreter *
    {
        auto context = makert<NGContext>(NGContext{
            .modulePaths = {""},
            .functions = predefs(),
        });

        return new Stupid(context);
    }

} // namespace NG::interpreter
