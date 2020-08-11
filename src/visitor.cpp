
#include "ast.hpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
namespace NG::AST {
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

    void IASTVisitor::visit(IndexAccessorExpression* index) {}

    void IASTVisitor::visit(IndexAssignmentExpression* index) {}

    IASTVisitor::~IASTVisitor() = default;

    void DefaultDummyAstVisitor::visit(ASTNode *node) {}

    void DefaultDummyAstVisitor::visit(Expression *expr) {}

    void DefaultDummyAstVisitor::visit(IdExpression *idExpr) {}

    void DefaultDummyAstVisitor::visit(FunCallExpression *funCallExpr) {}

    void DefaultDummyAstVisitor::visit(IdAccessorExpression *idAccExpr) {}

    void DefaultDummyAstVisitor::visit(BinaryExpression *binExpr) {}

    void DefaultDummyAstVisitor::visit(AssignmentExpression *assignmentExpr) {}

    void DefaultDummyAstVisitor::visit(Definition *def) {}

    void DefaultDummyAstVisitor::visit(Param *param) {}

    void DefaultDummyAstVisitor::visit(FunctionDef *funDef) {}

    void DefaultDummyAstVisitor::visit(ValDef *valDef) {}

    void DefaultDummyAstVisitor::visit(Statement *stmt) {}

    void DefaultDummyAstVisitor::visit(SimpleStatement *simpleStmt) {}

    void DefaultDummyAstVisitor::visit(CompoundStatement *compoundStmt) {}

    void DefaultDummyAstVisitor::visit(ReturnStatement *returnStmt) {}

    void DefaultDummyAstVisitor::visit(IfStatement *ifStmt) {}

    void DefaultDummyAstVisitor::visit(ValDefStatement *valDef) {}

    void DefaultDummyAstVisitor::visit(Module *mod) {}

    void DefaultDummyAstVisitor::visit(IntegerValue *intVal) {}

    void DefaultDummyAstVisitor::visit(StringValue *strVal) {}

    void DefaultDummyAstVisitor::visit(BooleanValue *boolVal) {}

    void DefaultDummyAstVisitor::visit(ArrayLiteral *array) {}

    void DefaultDummyAstVisitor::visit(IndexAccessorExpression *index) {}

    void DefaultDummyAstVisitor::visit(IndexAssignmentExpression *index) {}

    DefaultDummyAstVisitor::~DefaultDummyAstVisitor() = default;

} // namespace NG

#pragma clang diagnostic pop