
#include <ast.hpp>
#include <visitor.hpp>

namespace NG::ast
{
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

    void AstVisitor::visit(IntegralValue<int8_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<uint8_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<int16_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<uint16_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<int32_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<uint32_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<int64_t> *intVal) {}
    void AstVisitor::visit(IntegralValue<uint64_t> *intVal) {}
    // void AstVisitor::visit(FloatingPointValue<float16_t> *floatVal) {}
    void AstVisitor::visit(FloatingPointValue<float /* float32_t */> *floatVal) {}
    void AstVisitor::visit(FloatingPointValue<double /* float64_t */> *floatVal) {}
    // void AstVisitor::visit(FloatingPointValue<float128_t> *floatVal) {}

    void AstVisitor::visit(StringValue *strVal) {}

    void AstVisitor::visit(BooleanValue *boolVal) {}

    void AstVisitor::visit(ArrayLiteral *array) {}

    void AstVisitor::visit(IndexAccessorExpression *index) {}

    void AstVisitor::visit(IndexAssignmentExpression *index) {}

    void AstVisitor::visit(TypeDef *typeDef) {}

    void AstVisitor::visit(PropertyDef *propertyDef) {}

    void AstVisitor::visit(NewObjectExpression *newObj) {}

    void AstVisitor::visit(ImportDecl *importDecl) {}

    void AstVisitor::visit(CompileUnit *compileUnit) {}

    void AstVisitor::visit(NextStatement *nextStatement) {}
    void AstVisitor::visit(LoopStatement *loopStatement) {}

    AstVisitor::~AstVisitor() = default;
}