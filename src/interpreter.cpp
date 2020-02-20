
#include <interpreter.hpp>
#include <runtime.hpp>
#include <ast.hpp>
#include <token.hpp>

#include <debug.hpp>

#include <unordered_map>
#include <functional>
#include <test.hpp>

using namespace NG;
using namespace NG::AST;

namespace NG::runtime {

    NGObject *NGObject::number(double number) {
        auto *object = new NGObject;
        object->tag = tag_t::NG_NUM;
        object->value.number = number;
        return object;
    }

    NGObject *NGObject::boolean(bool boolean) {
        auto *object = new NGObject;
        object->tag = tag_t::NG_BOOL;
        object->value.boolean = boolean;
        return object;
    }

    NGObject *NGObject::str(const Str *str) {
        auto *object = new NGObject;
        object->tag = tag_t::NG_STR;
        object->value.str = str;
        return object;
    }

    NGObject *NGObject::array(NGArray *array) {
        auto *object = new NGObject;
        object->tag = tag_t::NG_ARRAY;
        object->value.array = array;
        return object;
    }

    Str NGObject::show() {
        switch (tag) {
            case tag_t::NG_NIL:
                return {"nil"};
            case tag_t::NG_NUM:
                return std::to_string(this->value.number);
            case tag_t::NG_BOOL:
                return this->value.boolean ? "true" : "false";
            case tag_t::NG_STR:
                return *(this->value.str);
            case tag_t::NG_COMPOSITE:
            case tag_t::NG_CUSTOMIZED:
                return {"[CUSTOMIZED TYPE]"};
            default:
                return {"[UNKNOWN]"};
        }
    }

    bool NGObject::equals(NGObject *ngObject) {
        if (tag != ngObject->tag) {
            switch (tag) {
                case tag_t::NG_NIL:
                    return true;
                case tag_t::NG_NUM:
                    return value.number == ngObject->value.number;
                case tag_t::NG_BOOL:
                    return value.boolean == ngObject->value.boolean;
                case tag_t::NG_ARRAY:
                    return value.array->equals(ngObject->value.array);
                case tag_t::NG_STR:
                    return *value.str == *(ngObject->value.str);
                case tag_t::NG_COMPOSITE:
                case tag_t::NG_CUSTOMIZED:
                    break;
            }
        }
        return false;
    }

    NGContext::~NGContext() = default;

    bool NGArray::equals(NGArray *array) {
        if (itemTag != array->itemTag || items.size() != array->items.size()) {
            return false;
        }
        for (int i = 0; i < items.size(); ++i) {
            if (!items[i]->equals(array->items[i])) {
                return false;
            }
        }
        return true;
    }
} // namespace NG::runtime

namespace NG::interpreter {

    using namespace NG::runtime;

    void ISummarizable::summary() {
    }

    ISummarizable::~ISummarizable() = default;

    static NGObject *evaluateExpr(Operators optr, NGObject *leftParam, NGObject *rightParam) {
        switch (optr) {
            case Operators::PLUS:
                return NGObject::number(leftParam->numValue() + rightParam->numValue());
            case Operators::MINUS:
                return NGObject::number(leftParam->numValue() - rightParam->numValue());
            case Operators::TIMES:
                return NGObject::number(leftParam->numValue() * rightParam->numValue());
            case Operators::DIVIDE:
                return NGObject::number(leftParam->numValue() / rightParam->numValue());
            case Operators::MODULUS:
                return NGObject::number(leftParam->numValue() - rightParam->numValue());
            case Operators::EQUAL:
                return NGObject::boolean(leftParam->equals(rightParam));
            case Operators::NOT_EQUAL:
                return NGObject::boolean(!leftParam->equals(rightParam));
            case Operators::LE:
                return NGObject::boolean(leftParam->numValue() <= rightParam->numValue());
            case Operators::LT:
                return NGObject::boolean(leftParam->numValue() < rightParam->numValue());
            case Operators::GE:
                return NGObject::boolean(leftParam->numValue() >= rightParam->numValue());
            case Operators::GT:
                return NGObject::boolean(leftParam->numValue() > rightParam->numValue());
            case Operators::RSHIFT:
                return NGObject::number(static_cast<unsigned>(leftParam->numValue()) <<
                                                                                     static_cast<unsigned>(rightParam->numValue()));
            case Operators::LSHIFT:
                return NGObject::number(static_cast<unsigned>(leftParam->numValue()) >>
                                                                                     static_cast<unsigned>(rightParam->numValue()));
            case Operators::ASSIGN:
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
            object = NGObject::number(intVal->value);
        }

        void visit(StringValue *strVal) override {
            object = NGObject::str(&(strVal->value));
        }

        void visit(BooleanValue *boolVal) override {
            object = NGObject::boolean(boolVal->value);
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
    };

    struct StatementVisitor : public DefaultDummyAstVisitor {
        NGContext *context;

        explicit StatementVisitor(NGContext *context) : context(context) {}

        void visit(ReturnStatement *returnStatement) override {
            ExpressionVisitor vis{context};
            returnStatement->expression->accept(&vis);

            context->retVal = vis.object;
        }
    };

    struct Interpreter : public DefaultDummyAstVisitor, public ISummarizable {
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
                    funDef->body->accept(this);
                });
                ngContext.retVal = newContext.retVal;
            };
        }

        void visit(Statement *stmt) override {
            StatementVisitor vis{context};
            stmt->accept(&vis);
        }

        void visit(CompoundStatement *stmt) override {
            StatementVisitor vis{context};
            for (auto innerStmt: stmt->statements) {
                innerStmt->accept(&vis);
            }
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

    IASTVisitor *interpreter() {
        return new Interpreter(new NGContext{});
    }

} // namespace NG::interpreter
