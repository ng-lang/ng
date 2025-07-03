
#include <intp/intp.hpp>
#include <intp/runtime.hpp>
#include <ast.hpp>
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

    static NGObject *evaluateExpr(Operators optr, NGObject *leftParam, NGObject *rightParam)
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

    static Map<Str, NGInvocationHandler> predefs()
    {
        return {
            {"print", [](NGObject &dummy, NGContext &context, NGInvocationContext &invocationContext)
             {
                 Vec<NGObject *> &params = invocationContext.params;
                 for (size_t i = 0; i < params.size(); ++i)
                 {
                     std::cout << params[i]->show();
                     if (i != params.size() - 1)
                     {
                         std::cout << ", ";
                     }
                 }
                 std::cout << std::endl;
             }},
            {"assert", [](NGObject &dummy, NGContext &context, NGInvocationContext &invocationContext)
             {
                 for (const auto &param : invocationContext.params)
                 {
                     auto ngBool = dynamic_cast<NGBoolean *>(param);
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

        NGObject *object = nullptr;

        NGContext *context = nullptr;

        explicit ExpressionVisitor(NGContext *context) : context(context) {}

        void visit(IntegerValue *intVal) override
        {
            object = new NGInteger(intVal->value);
        }

        void visit(StringValue *strVal) override
        {
            object = new NGString(strVal->value);
        }

        void visit(BooleanValue *boolVal) override
        {
            object = new NGBoolean(boolVal->value);
        }

        void visit(FunCallExpression *funCallExpr) override
        {
            FunctionPathVisitor fpVis{};
            funCallExpr->primaryExpression->accept(&fpVis);
            NGInvocationContext invocationContext{};

            for (auto &param : funCallExpr->arguments)
            {
                param->accept(this);
                invocationContext.params.push_back(this->object);
            }

            NGObject dummy{};

            if (!context->functions.contains(fpVis.path))
            {
                throw RuntimeException("No such function: " + fpVis.path);
            }

            context->functions[fpVis.path](dummy, *context, invocationContext);

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
            Vec<NGObject *> objects;

            ExpressionVisitor vis{context};

            for (const auto &element : array->elements)
            {
                element->accept(&vis);
                objects.push_back(vis.object);
            }

            object = new NGArray(objects);
        }

        void visit(IndexAccessorExpression *index) override
        {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            NGObject *primaryObject = vis.object;

            index->accessor->accept(&vis);

            NGObject *accessorObject = vis.object;

            object = primaryObject->opIndex(accessorObject);
        }

        void visit(IndexAssignmentExpression *index) override
        {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            NGObject *primaryObject = vis.object;

            index->accessor->accept(&vis);

            NGObject *accessorObject = vis.object;

            index->value->accept(&vis);

            NGObject *valueObject = vis.object;

            object = primaryObject->opIndex(accessorObject, valueObject);
        }

        void visit(IdAccessorExpression *idAccExpr) override
        {
            const Str &repr = idAccExpr->accessor->repr();

            ExpressionVisitor vis{context};

            idAccExpr->primaryExpression->accept(&vis);

            NGObject *main = vis.object;

            NGInvocationContext invCtx{};
            for (const auto &argument : idAccExpr->arguments)
            {
                argument->accept(&vis);
                invCtx.params.push_back(vis.object);
            }

            object = main->respond(repr, context, &invCtx);
        }

        void visit(NewObjectExpression *newObj) override
        {
            Str &typeName = newObj->typeName;
            auto ngType = context->types[typeName];

            auto structural = new NGStructuralObject{};

            structural->customizedType = ngType;

            NGContext newContext{*context};
            ExpressionVisitor visitor{&newContext};

            for (auto &&[name, expr] : newObj->properties)
            {
                expr->accept(&visitor);
                NGObject *result = visitor.object;

                visitor.context->objects[name] = result;

                structural->properties[name] = result;
            }

            for (const auto &property : ngType->properties)
            {
                if (structural->properties.find(property) == structural->properties.end())
                {
                    structural->properties[property] = new NGObject{};
                }
            }

            object = structural;
        }
    };

    struct StatementVisitor : public DummyVisitor
    {
        NGContext *context;

        explicit StatementVisitor(NGContext *context) : context(context) {}

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
            for (auto innerStmt : stmt->statements)
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
        NGContext *context;

        Map<Str, ASTRef<ASTNode>> externalModuleAstCache;

        void withNewContext(NGContext &newContext, const std::function<void()> &execution)
        {
            NGContext *previousContext = context;
            context = &newContext;
            execution();
            context = previousContext;
        }

        explicit Stupid(NGContext *_context) : context(_context)
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
            context->currentModule = new NGModule{};

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

        void saveModule()
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
                NGContext ctx{};
                withNewContext(ctx, [&ast, intp = this]()
                               {
                                    ast->accept(intp);
                                    intp->saveModule(); });
                context->modules.insert_or_assign(importDecl->module, ctx.currentModule);
            }
            NGModule *targetModule = context->modules[importDecl->module];
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

        Set<Str> resolveImports(ImportDecl *importDecl, NGModule *targetModule)
        {
            bool importAll = (std::find(begin(importDecl->imports), end(importDecl->imports),
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
            else
            {
                for (auto &&imp : importDecl->imports)
                {
                    if (!exported.contains(imp))
                    {
                        throw RuntimeException("Cannot found symbol " + imp + " in module " + importDecl->module);
                    }
                }
                return Set<Str>{begin(importDecl->imports), end(importDecl->imports)};
            }
        }

        // virtual void visit(Definition *def);
        // virtual void visit(Param *param);
        void visit(FunctionDef *funDef) override
        {
            context->functions[funDef->funName] = [this, funDef](NGObject &dummy,
                                                                 NGContext &ngContext,
                                                                 NGInvocationContext &invocationContext)
            {
                NGContext newContext{ngContext};
                for (size_t i = 0; i < funDef->params.size(); ++i)
                {
                    newContext.objects[funDef->params[i]->paramName] = invocationContext.params[i];
                }

                withNewContext(newContext, [this, funDef]
                               {
                    StatementVisitor vis{context};

                    funDef->body->accept(&vis); });
                ngContext.retVal = newContext.retVal;
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
            auto type = new NGType{};

            type->name = typeDef->typeName;

            for (const auto &property : typeDef->properties)
            {
                type->properties.push_back(property->propertyName);
            }

            for (const auto &memFn : typeDef->memberFunctions)
            {
                type->memberFunctions[memFn->funName] = [this, memFn](NGObject &dummy,
                                                                      NGContext &ngContext,
                                                                      NGInvocationContext &invocationContext)
                {
                    NGContext newContext{ngContext};
                    for (size_t i = 0; i < memFn->params.size(); ++i)
                    {
                        newContext.objects[memFn->params[i]->paramName] = invocationContext.params[i];
                    }

                    newContext.objects["self"] = &dummy;

                    if (auto structural = dynamic_cast<NGStructuralObject *>(&dummy); structural != nullptr)
                    {
                        for (const auto &property : structural->properties)
                        {
                            newContext.objects[property.first] = property.second;
                        }
                    }

                    withNewContext(newContext, [this, memFn]
                                   {
                        StatementVisitor vis{context};

                        memFn->body->accept(&vis); });
                    ngContext.retVal = newContext.retVal;
                };
            }

            context->types[type->name] = type;
        }

        void summary() override
        {
            debug_log("Context objects size", context->objects.size());

            for (auto p : context->objects)
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

        NGContext *intpContext() override
        {
            return context;
        }

        ~Stupid() override
        {
            delete context;
        }
    };

    Interpreter *stupid()
    {
        auto context = new NGContext{
            .functions = predefs(),
            .modulePaths = {""},
        };

        return new Stupid(context);
    }

} // namespace NG::interpreter
