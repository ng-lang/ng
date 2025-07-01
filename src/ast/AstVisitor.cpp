
#include <ast.hpp>
#include <visitor.hpp>

namespace NG::ast {
    void AstVisitor::visit(ASTNode *node) {}

    void AstVisitor::visit(Expression *expr) {}

    void AstVisitor::visit(IdExpression *idExpr) {}

    void AstVisitor::visit(FunCallExpression *funCallExpr) {}

    void AstVisitor::visit(IdAccessorExpression *idAccExpr) {}

    void AstVisitor::visit(BinaryExpression *binExpr) {}

    void AstVisitor::visit(AssignmentExpression *assignmentExpr) {}

    void AstVisitor::visit(Definition *def) {}

    void AstVisitor::visit(Param *param) {}

    void AstVisitor::visit(FunctionDef *funDef) {}

    void AstVisitor::visit(ValDef *valDef) {}

    void AstVisitor::visit(Statement *stmt) {}

    void AstVisitor::visit(SimpleStatement *simpleStmt) {}

    void AstVisitor::visit(CompoundStatement *compoundStmt) {}

    void AstVisitor::visit(ReturnStatement *returnStmt) {}

    void AstVisitor::visit(IfStatement *ifStmt) {}

    void AstVisitor::visit(ValDefStatement *valDef) {}

    void AstVisitor::visit(Module *mod) {}

    void AstVisitor::visit(IntegerValue *intVal) {}

    void AstVisitor::visit(StringValue *strVal) {}

    void AstVisitor::visit(BooleanValue *boolVal) {}

    void AstVisitor::visit(ArrayLiteral *array) {}

    void AstVisitor::visit(IndexAccessorExpression *index) {}

    void AstVisitor::visit(IndexAssignmentExpression *index) {}

    void AstVisitor::visit(TypeDef *typeDef) {}

    void AstVisitor::visit(PropertyDef *propertyDef) {}

    void AstVisitor::visit(NewObjectExpression *newObj) {}

    void AstVisitor::visit(ImportDecl *importDecl) {}

    void AstVisitor::visit(CompileUnit *importDecl) {}

    AstVisitor::~AstVisitor() = default;
}