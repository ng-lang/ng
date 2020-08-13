
#include <intp/interpreter.hpp>
#include <intp/runtime.hpp>
#include <ast.hpp>
#include <token.hpp>

#include <debug.hpp>

#include <unordered_map>
#include <functional>
#include <test.hpp>

using namespace NG;
using namespace NG::AST;

namespace NG::interpreter {

    using namespace NG::runtime;

    void ISummarizable::summary() {
    }

    ISummarizable::~ISummarizable() = default;

    static NGObject *evaluateExpr(Operators optr, NGObject *leftParam, NGObject *rightParam) {
        switch (optr) {
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

    struct FunctionPathVisitor : public DefaultDummyAstVisitor {

        Str path;

        void visit(IdExpression *idExpr) override {
            this->path = idExpr->id;
        }
    };

    struct ExpressionVisitor : public DefaultDummyAstVisitor {

        NGObject *object = nullptr;

        NGContext *context = nullptr;

        explicit ExpressionVisitor(NGContext *context) : context(context) {}

        void visit(IntegerValue *intVal) override {
            object = new NGInteger(intVal->value);
        }

        void visit(StringValue *strVal) override {
            object = new NGString(strVal->value);
        }

        void visit(BooleanValue *boolVal) override {
            object = new NGBoolean(boolVal->value);
        }

        void visit(FunCallExpression *funCallExpr) override {
            FunctionPathVisitor fpVis{};
            funCallExpr->primaryExpression->accept(&fpVis);
            NGInvocationContext invocationContext{};

            for (auto param: funCallExpr->arguments) {
                param->accept(this);
                invocationContext.params.push_back(this->object);
            }

            context->functions[fpVis.path](*context, invocationContext);

            this->object = context->retVal;
            context->retVal = nullptr;
        }

        void visit(BinaryExpression *binExpr) override {
            ExpressionVisitor leftVisitor{context};
            ExpressionVisitor rightVisitor{context};
            binExpr->left->accept(&leftVisitor);
            binExpr->right->accept(&rightVisitor);


            object = evaluateExpr(binExpr->optr->operatorType, leftVisitor.object, rightVisitor.object);
        }

        void visit(IdExpression *idExpr) override {
            object = context->objects[idExpr->id];
        }

        void visit(ArrayLiteral *array) override {
            Vec<NGObject *> objects;

            ExpressionVisitor vis{context};

            for (const auto &element : array->elements) {
                element->accept(&vis);
                objects.push_back(vis.object);
            }

            object = new NGArray(objects);
        }

        void visit(IndexAccessorExpression *index) override {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            NGObject *primaryObject = vis.object;

            index->accessor->accept(&vis);

            NGObject *accessorObject = vis.object;

            object = primaryObject->opIndex(accessorObject);
        }

        void visit(IndexAssignmentExpression *index) override {
            ExpressionVisitor vis{context};

            index->primary->accept(&vis);

            NGObject *primaryObject = vis.object;

            index->accessor->accept(&vis);

            NGObject *accessorObject = vis.object;

            index->value->accept(&vis);

            NGObject *valueObject = vis.object;

            object = primaryObject->opIndex(accessorObject, valueObject);
        }
    };

    struct StatementVisitor : public DefaultDummyAstVisitor {
        NGContext *context;

        explicit StatementVisitor(NGContext *context) : context(context) {}

        void visit(ReturnStatement *returnStatement) override {
            ExpressionVisitor vis{context};
            returnStatement->expression->accept(&vis);

            context->retVal = vis.object;
        }

        void visit(IfStatement *ifStmt) override {
            ExpressionVisitor vis{context};
            ifStmt->testing->accept(&vis);

            StatementVisitor stmtVis{context};
            if (vis.object->boolValue()) {
                ifStmt->consequence->accept(&stmtVis);
            } else if (ifStmt->alternative != nullptr) {
                ifStmt->alternative->accept(&stmtVis);
            }
        }

        void visit(CompoundStatement *stmt) override {
            StatementVisitor vis{context};
            for (auto innerStmt: stmt->statements) {
                innerStmt->accept(&vis);
                if (vis.context->retVal != nullptr) {
                    break;
                }
            }
        }

        void visit(ValDefStatement *valDef) override {
            ExpressionVisitor vis{context};
            valDef->value->accept(&vis);

            context->objects[valDef->name] = vis.object;
        }

        void visit(SimpleStatement *simpleStmt) override {
            ExpressionVisitor vis{context};
            simpleStmt->expression->accept(&vis);
        }
    };

    struct Interpreter : public IInterperter, public DefaultDummyAstVisitor {
        NGContext *context;

        void withNewContext(NGContext &newContext, const std::function<void()> &execution) {
            NGContext *previousContext = context;
            context = &newContext;
            execution();
            context = previousContext;
        }

        explicit Interpreter(NGContext *_context) : context(_context) {
        }

        void visit(Module *mod) override {
            for (auto defs: mod->definitions) {
                defs->accept(this);
            }

            StatementVisitor vis{context};

            for (const auto &stmt : mod->statements) {
                stmt->accept(&vis);
            }
        }

        // virtual void visit(Definition *def);
        // virtual void visit(Param *param);
        void visit(FunctionDef *funDef) override {
            context->functions[funDef->funName] = [this, funDef](NGContext &ngContext,
                                                                 NGInvocationContext &invocationContext) {
                NGContext newContext{ngContext};
                for (int i = 0; i < funDef->params.size(); ++i) {
                    newContext.objects[funDef->params[i]->paramName] = invocationContext.params[i];
                }

                withNewContext(newContext, [this, funDef] {
                    StatementVisitor vis{context};

                    funDef->body->accept(&vis);
                });
                ngContext.retVal = newContext.retVal;
            };
        }

        void visit(Statement *stmt) override {
            StatementVisitor vis{context};
            stmt->accept(&vis);
        }

        void visit(ValDef *valDef) override {
            ExpressionVisitor vis{context};
            valDef->body->value->accept(&vis);
            context->objects[valDef->name()] = vis.object;
        }

        void summary() override {
            debug_log("Context objects size", context->objects.size());

            for (auto p : context->objects) {
                debug_log("Context object", "key:", p.first, "value:", p.second->show());
            }

            debug_log("Context modules size", context->modules.size());

            for (const auto &p : context->modules) {
                debug_log("Context module", "name:", p.first, "value:", code(p.second->defs.size()));
            }
        }

        ~Interpreter() override {
            delete context;
        }

    };

    IInterperter *interpreter() {
        auto context = new NGContext{
                .functions = {
                        {"print",  [](NGContext &context, NGInvocationContext &invocationContext) {
                            Vec<NGObject *> &params = invocationContext.params;
                            for (int i = 0; i < params.size(); ++i) {
                                std::cout << params[i]->show();
                                if (i != params.size() - 1) {
                                    std::cout << ", ";
                                }
                            }
                            std::cout << std::endl;
                        }},
                        {"assert", [](NGContext &context, NGInvocationContext &invocationContext) {
                            for (const auto &param : invocationContext.params) {
                                auto ngBool = dynamic_cast<NGBoolean *>(param);
                                if (ngBool == nullptr || !ngBool->value) {
                                    std::cerr << param->show();
                                    throw AssertionException();
                                }
                            }
                        }}
                },
        };

        return new Interpreter(context);
    }

} // namespace NG::interpreter
