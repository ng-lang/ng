
#include "ast.hpp"
#include <token.hpp>
#include <algorithm>
#include <iterator>

namespace NG::AST {

    ASTNode::~ASTNode() = default;

    const auto ASTComparator = [](ASTRef<ASTNode> left, ASTRef<ASTNode> right) -> bool {
        return *left == *right;
    };

    void ASTNode::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool ASTNode::operator==(const ASTNode &node) const {
        return false;
    }

    bool Module::operator==(const ASTNode &node) const {
        auto &mod = dynamic_cast<const Module &>(node);
        return astNodeType() == node.astNodeType() &&
               name == mod.name &&
               definitions.size() == mod.definitions.size() &&
               std::equal(std::begin(definitions),
                          std::end(definitions),
                          begin(mod.definitions),
                          ASTComparator);
    }

    void Module::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    Module::~Module() {
        for (auto def : definitions) {
            destroyast(def);
        }
    }

    Str Definition::name() const {
        return "unknown";
    }

    void Param::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool Param::operator==(const ASTNode &node) const {
        auto &param = dynamic_cast<const Param &>(node);
        return astNodeType() == node.astNodeType() &&
               paramName == param.paramName &&
               annotatedType == param.annotatedType &&
               type == param.type;
    }

    void CompoundStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    CompoundStatement::~CompoundStatement() {
        for (auto stmt : statements) {
            destroyast(stmt);
        }
    }

    bool CompoundStatement::operator==(const ASTNode &node) const {
        auto &stmt = dynamic_cast<const CompoundStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               std::equal(begin(statements),
                          end(statements),
                          begin(stmt.statements),
                          ASTComparator);
    }

    void ReturnStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    ReturnStatement::~ReturnStatement() {
        if (expression != nullptr) {
            destroyast(expression);
        }
    }

    bool ReturnStatement::operator==(const ASTNode &node) const {
        auto &ret = dynamic_cast<const ReturnStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *ret.expression;
    }

    void IfStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    IfStatement::~IfStatement() {
        if (testing != nullptr) {
            destroyast(testing);
        }
        if (consequence != nullptr) {
            destroyast(consequence);
        }
        if (alternative != nullptr) {
            destroyast(alternative);
        }
    }

    bool IfStatement::operator==(const ASTNode &node) const {
        auto &ifstmt = dynamic_cast<const IfStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *testing == *ifstmt.testing &&
               *consequence == *ifstmt.consequence &&
               ((alternative == nullptr &&
                 ifstmt.alternative == nullptr) ||
                *alternative == *ifstmt.alternative);
    }

    SimpleStatement::~SimpleStatement() {
        if (expression != nullptr) {
            destroyast(expression);
        }
    }

    void SimpleStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool SimpleStatement::operator==(const ASTNode &node) const {
        auto &simple = dynamic_cast<const SimpleStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *simple.expression;
    }

    void FunCallExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool FunCallExpression::operator==(const ASTNode &node) const {
        auto &funCall = dynamic_cast<const FunCallExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               *funCall.primaryExpression == *primaryExpression &&
               std::equal(begin(arguments),
                          end(arguments),
                          begin(funCall.arguments),
                          ASTComparator);
    }

    FunCallExpression::~FunCallExpression() {
        destroyast(primaryExpression);
        for (auto arg : arguments) {
            destroyast(arg);
        }
    }

    void AssignmentExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool AssignmentExpression::operator==(const ASTNode &node) const {
        auto &assign = dynamic_cast<const AssignmentExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               name == assign.name &&
               *value == *assign.value;
    }

    AssignmentExpression::~AssignmentExpression() {
        if (value != nullptr)
            destroyast(value);
    }

    void ValDefStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool ValDefStatement::operator==(const ASTNode &node) const {
        auto &valDef = dynamic_cast<const ValDefStatement &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.name == name &&
               *valDef.value == *value;
    }

    ValDefStatement::~ValDefStatement() {
        if (value != nullptr)
            destroyast(value);
    }

    void ValDef::accept(IASTVisitor *visitor) {
        return visitor->visit(this);
    }

    bool ValDef::operator==(const ASTNode &node) const {
        auto &valDef = dynamic_cast<const ValDef &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.body->operator==(*(valDef.body));
    }

    ValDef::~ValDef() {
        if (body != nullptr) {
            destroyast(body);
        }
    }

    void IdExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IdExpression::operator==(const ASTNode &node) const {
        auto &idexpr = dynamic_cast<const IdExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               idexpr.id == id;
    }

    void IdAccessorExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IdAccessorExpression::operator==(const ASTNode &node) const {
        auto &idacc = dynamic_cast<const IdAccessorExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *idacc.primaryExpression == *primaryExpression &&
               *idacc.accessor == *accessor;
    }

    IdAccessorExpression::~IdAccessorExpression() {
        destroyast(primaryExpression);
        destroyast(accessor);
    }

    void IntegerValue::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IntegerValue::operator==(const ASTNode &node) const {
        auto &intVal = dynamic_cast<const IntegerValue &>(node);
        return astNodeType() == node.astNodeType() &&
               intVal.value == value;
    }

    void StringValue::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool StringValue::operator==(const ASTNode &node) const {
        auto &strVal = dynamic_cast<const StringValue &>(node);
        return astNodeType() == node.astNodeType() &&
               strVal.value == value;
    }

    void BooleanValue::accept(IASTVisitor *visitor) {
        return visitor->visit(this);
    }

    bool BooleanValue::operator==(const ASTNode &node) const {
        auto &val = dynamic_cast<const BooleanValue &>(node);
        return astNodeType() == node.astNodeType() &&
               value == val.value;
    }

    void FunctionDef::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool FunctionDef::operator==(const ASTNode &node) const {
        auto &funDef = dynamic_cast<const FunctionDef &>(node);
        return astNodeType() == node.astNodeType() &&
               funDef.funName == funName &&
               std::equal(begin(params),
                          end(params),
                          begin(funDef.params),
                          ASTComparator) &&
               *funDef.body == *body;
    }

    FunctionDef::~FunctionDef() {
        for (auto param : params) {
            destroyast(param);
        }
        destroyast(body);
    }

    Str FunctionDef::name() const {
        return funName;
    }

    void BinaryExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool BinaryExpression::operator==(const ASTNode &node) const {
        auto &&binexpr = dynamic_cast<const BinaryExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *binexpr.optr == *optr &&
               *left == *binexpr.left &&
               *right == *binexpr.right;
    }

    BinaryExpression::~BinaryExpression() {
        destroyast(left);
        destroyast(right);
        delete optr;
    }

} // namespace NG
