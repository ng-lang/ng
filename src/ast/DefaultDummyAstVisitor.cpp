
#include <visitor.hpp>

namespace NG::ast {

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

    void DefaultDummyAstVisitor::visit(TypeDef *typeDef) {}

    void DefaultDummyAstVisitor::visit(PropertyDef *propertyDef) {}

    void DefaultDummyAstVisitor::visit(NewObjectExpression *newObj) {}

    void DefaultDummyAstVisitor::visit(ImportDecl *importDecl) {}

    void DefaultDummyAstVisitor::visit(CompileUnit *compileUnit) {}

    DefaultDummyAstVisitor::~DefaultDummyAstVisitor() = default;

}
