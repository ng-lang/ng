
#include <ast.hpp>
#include <visitor.hpp>

namespace NG::ast {
    void IASTVisitor::visit(ASTNode *node) {}

    void IASTVisitor::visit(Expression *expr) {}

    void IASTVisitor::visit(IdExpression *idExpr) {}

    void IASTVisitor::visit(FunCallExpression *funCallExpr) {}

    void IASTVisitor::visit(IdAccessorExpression *idAccExpr) {}

    void IASTVisitor::visit(BinaryExpression *binExpr) {}

    void IASTVisitor::visit(AssignmentExpression *assignmentExpr) {}

    void IASTVisitor::visit(Definition *def) {}

    void IASTVisitor::visit(Param *param) {}

    void IASTVisitor::visit(FunctionDef *funDef) {}

    void IASTVisitor::visit(ValDef *valDef) {}

    void IASTVisitor::visit(Statement *stmt) {}

    void IASTVisitor::visit(SimpleStatement *simpleStmt) {}

    void IASTVisitor::visit(CompoundStatement *compoundStmt) {}

    void IASTVisitor::visit(ReturnStatement *returnStmt) {}

    void IASTVisitor::visit(IfStatement *ifStmt) {}

    void IASTVisitor::visit(ValDefStatement *valDef) {}

    void IASTVisitor::visit(Module *mod) {}

    void IASTVisitor::visit(IntegerValue *intVal) {}

    void IASTVisitor::visit(StringValue *strVal) {}

    void IASTVisitor::visit(BooleanValue *boolVal) {}

    void IASTVisitor::visit(ArrayLiteral *array) {}

    void IASTVisitor::visit(IndexAccessorExpression *index) {}

    void IASTVisitor::visit(IndexAssignmentExpression *index) {}

    void IASTVisitor::visit(TypeDef *typeDef) {}

    void IASTVisitor::visit(PropertyDef *propertyDef) {}

    void IASTVisitor::visit(NewObjectExpression *newObj) {}

    IASTVisitor::~IASTVisitor() = default;
}