
#include <visitor.hpp>

namespace NG::ast {

    void DummyVisitor::visit(ASTNode *node) {}

    void DummyVisitor::visit(Expression *expr) {}

    void DummyVisitor::visit(IdExpression *idExpr) {}

    void DummyVisitor::visit(FunCallExpression *funCallExpr) {}

    void DummyVisitor::visit(IdAccessorExpression *idAccExpr) {}

    void DummyVisitor::visit(BinaryExpression *binExpr) {}

    void DummyVisitor::visit(AssignmentExpression *assignmentExpr) {}

    void DummyVisitor::visit(Definition *def) {}

    void DummyVisitor::visit(Param *param) {}

    void DummyVisitor::visit(FunctionDef *funDef) {}

    void DummyVisitor::visit(ValDef *valDef) {}

    void DummyVisitor::visit(Statement *stmt) {}

    void DummyVisitor::visit(SimpleStatement *simpleStmt) {}

    void DummyVisitor::visit(CompoundStatement *compoundStmt) {}

    void DummyVisitor::visit(ReturnStatement *returnStmt) {}

    void DummyVisitor::visit(IfStatement *ifStmt) {}

    void DummyVisitor::visit(ValDefStatement *valDef) {}

    void DummyVisitor::visit(Module *mod) {}

    void DummyVisitor::visit(IntegerValue *intVal) {}

    void DummyVisitor::visit(StringValue *strVal) {}

    void DummyVisitor::visit(BooleanValue *boolVal) {}

    void DummyVisitor::visit(ArrayLiteral *array) {}

    void DummyVisitor::visit(IndexAccessorExpression *index) {}

    void DummyVisitor::visit(IndexAssignmentExpression *index) {}

    void DummyVisitor::visit(TypeDef *typeDef) {}

    void DummyVisitor::visit(PropertyDef *propertyDef) {}

    void DummyVisitor::visit(NewObjectExpression *newObj) {}

    void DummyVisitor::visit(ImportDecl *importDecl) {}

    void DummyVisitor::visit(CompileUnit *compileUnit) {}

    DummyVisitor::~DummyVisitor() = default;

}
