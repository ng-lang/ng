
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

    NGObject *NGObject::number(long long number) {

        return new NGInteger { number };
    }

    NGObject *NGObject::boolean(bool boolean) {
        return new NGBoolean { boolean };
    }

    NGObject *NGObject::str(const Str& str) {
        return new NGString { str };

    }

    NGObject *NGObject::array(const Vec<NGObject*>& array) {
        return new NGArray { array };
    }

    Str NGObject::show() {
        return "[NGObject]";
    }

    bool NGObject::opEquals(NGObject *other) const {
        return false;
    }

    NGObject *NGObject::opIndex(NGObject *index) const {
        throw IllegalTypeException("Not index-accessible");
    }

    NGObject *NGObject::opIndex(NGObject *accessor, NGObject *newValue) {
        throw IllegalTypeException("Not index-accessible");
    }

    NGObject *IOverloadedOperators::opIndex(NGObject *index) const {
        return nullptr;
    }

    NGObject *IOverloadedOperators::opIndex(NGObject *index, NGObject *newValue) {
        return nullptr;
    }

    bool IOverloadedOperators::opGreaterThan(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opGreaterEqual(NGObject *other) const {
        return false;
    }

    bool IOverloadedOperators::opLessThan(NGObject *other) const { return false; }

    bool IOverloadedOperators::opLessEqual(NGObject *other) const { return false; }

    bool IOverloadedOperators::opEquals(NGObject *other) const { return false; }

    bool IOverloadedOperators::opNotEqual(NGObject *other) const { return false; }

    NGObject *IOverloadedOperators::opPlus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opMinus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opTimes(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opModulus(NGObject *other) const { return nullptr; }

    NGObject *IOverloadedOperators::opDividedBy(NGObject *other) const { return nullptr; }


    NGObject *IOverloadedOperators::respond(const Str &member, NGInvocationContext *invocationContext) const {
        return nullptr;
    }

    NGObject* IOverloadedOperators::opLShift(NGObject* object) {
        return nullptr;
    }

    NGObject* IOverloadedOperators::opRShift(NGObject* object) {
        return nullptr;
    }

    IOverloadedOperators::~IOverloadedOperators() noexcept = default;


    NGObject *NGObject::opPlus(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opMinus(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opTimes(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opDividedBy(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::opModulus(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterThan(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opLessThan(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opGreaterEqual(NGObject *other) const {
        throw NotImplementedException();
    }

    bool NGObject::opLessEqual(NGObject *other) const {
        throw NotImplementedException();
    }

    NGObject *NGObject::respond(const Str &member, NGInvocationContext *invocationContext) const {
        throw NotImplementedException();
    }

    bool NGObject::opNotEqual(NGObject *other) const {
        return !opEquals(other);
    }

    NGObject *NGObject::opLShift(NGObject *other) {
        throw NotImplementedException();
    }

    NGObject *NGObject::opRShift(NGObject *other) {
        throw NotImplementedException();
    }

    NGObject::~NGObject() = default;

    NGContext::~NGContext() = default;

    NGObject *NGArray::opIndex(NGObject *index) const {

        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return this->items[ngInt->value];
    }

    NGObject *NGArray::opIndex(NGObject *index, NGObject *newValue) {
        auto ngInt = dynamic_cast<NGInteger *>(index);
        if (ngInt == nullptr) {
            throw IllegalTypeException("Not a valid index");
        }

        return items[ngInt->value] = newValue;
    }

    Str NGArray::show() {
        Str result{};

        for (const auto &item : this->items) {
            if (!result.empty()) {
                result += ", ";
            }

            result += item->show();
        }

        return "[" + result + "]";
    }

    bool NGArray::opEquals(NGObject *other) const {

        if (auto array = dynamic_cast<NGArray *>(other); array != nullptr) {
            if (items.size() != array->items.size()) {
                return false;
            }
            for (int i = 0; i < items.size(); ++i) {
                if (!items[i]->opEquals(array->items[i])) {
                    return false;
                }
            }
            return true;
        }

        return false;
    }

    bool NGArray::boolValue() {
        return !items.empty();
    }

    NGObject *NGArray::opLShift(NGObject *other) {
        items.push_back(other);

        return this;
    }

    Str IBasicObject::show() {
        return NG::Str();
    }

    bool IBasicObject::boolValue() {
        return false;
    }

    IBasicObject::~IBasicObject() = default;

    Str NGBoolean::show() {
        return value ? "true" : "false";
    }

    bool NGBoolean::opEquals(NGObject *other) const {
        if (auto otherBoolean = dynamic_cast<NGBoolean *>(other); otherBoolean != nullptr) {
            return otherBoolean->value == value;
        }
        return false;
    }

    bool NGBoolean::boolValue() {
        return value;
    }


    Str NGString::show() {
        return "\"" + value + "\"";
    }

    bool NGString::opEquals(NGObject *other) const {
        if (auto otherString = dynamic_cast<NGString *>(other); otherString != nullptr) {
            return otherString->value == value;
        }
        return false;
    }

    bool NGString::boolValue() {
        return !value.empty();
    }

    Orders NGInteger::comparator(const NGObject *left, const NGObject *right) {
        auto leftInt = dynamic_cast<const NGInteger *>(left);
        auto rightInt = dynamic_cast<const NGInteger *>(right);

        if (leftInt == nullptr || rightInt == nullptr) {
            return Orders::UNORDERED;
        }

        long long int result = leftInt->value - rightInt->value;
        if (result > 0) {
            return Orders::GT;
        } else if (result < 0) {
            return Orders::LT;
        }
        return Orders::EQ;
    }

    NGObject *NGInteger::opPlus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger { value + integer->value };
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opMinus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger { value - integer->value };
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opTimes(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger { value * integer->value };
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opDividedBy(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger { value / integer->value };
        }
        throw IllegalTypeException("Not a number");
    }

    NGObject *NGInteger::opModulus(NGObject *other) const {
        if (auto integer = dynamic_cast<NGInteger *>(other); integer != nullptr) {

            return new NGInteger { value % integer->value };
        }
        throw IllegalTypeException("Not a number");
    }

    bool NGInteger::boolValue() {
        return value != 0;
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
            object = NGObject::number(intVal->value);
        }

        void visit(StringValue *strVal) override {
            object = NGObject::str(strVal->value);
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

        void visit(ArrayLiteral *array) override {
            Vec<NGObject*> objects;

            ExpressionVisitor vis{context};

            for (const auto &element : array->elements) {
                element->accept(&vis);
                objects.push_back(vis.object);
            }

            object = NGObject::array(objects);
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

    IASTVisitor *interpreter() {
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
