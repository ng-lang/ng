
#include <visitor.hpp>

namespace NG::ast
{

    void DummyVisitor::visit(ASTNode *node) {}

    void DummyVisitor::visit(Expression *expr) {}

    void DummyVisitor::visit(IdExpression *idExpr) {}

    void DummyVisitor::visit(FunCallExpression *funCallExpr) {}

    void DummyVisitor::visit(IdAccessorExpression *idAccExpr) {}

    void DummyVisitor::visit(UnaryExpression *unoExpr) {}

    void DummyVisitor::visit(BinaryExpression *binExpr) {}

    void DummyVisitor::visit(AssignmentExpression *assignmentExpr) {}

    void DummyVisitor::visit(Definition *def) {}

    void DummyVisitor::visit(Param *param) {}
    void DummyVisitor::visit(TypeAnnotation *typeAnno) {}

    void DummyVisitor::visit(FunctionDef *funDef) {}

    void DummyVisitor::visit(ValDef *valDef) {}

    void DummyVisitor::visit(Statement *stmt) {}

    void DummyVisitor::visit(SimpleStatement *simpleStmt) {}

    void DummyVisitor::visit(CompoundStatement *compoundStmt) {}

    void DummyVisitor::visit(ReturnStatement *returnStmt) {}

    void DummyVisitor::visit(IfStatement *ifStmt) {}

    void DummyVisitor::visit(ValDefStatement *valDef) {}

    void DummyVisitor::visit(Module *mod) {}

    void DummyVisitor::visit(IntegralValue<int8_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<uint8_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<int16_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<uint16_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<int32_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<uint32_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<int64_t> *intVal) {}
    void DummyVisitor::visit(IntegralValue<uint64_t> *intVal) {}

    // void DummyVisitor::visit(FloatingPointValue<float16_t> *floatVal) {}
    void DummyVisitor::visit(FloatingPointValue<float /* float32_t */> *floatVal) {}
    void DummyVisitor::visit(FloatingPointValue<double /* float64_t */> *floatVal) {}
    // void DummyVisitor::visit(FloatingPointValue<float128_t> *floatVal) {}

    void DummyVisitor::visit(StringValue *strVal) {}

    void DummyVisitor::visit(BooleanValue *boolVal) {}

    void DummyVisitor::visit(ArrayLiteral *array) {}

    void DummyVisitor::visit(TupleLiteral *tuple) {}

    void DummyVisitor::visit(TypeOfExpression *typeofExpr) {}

    void DummyVisitor::visit(SpreadExpression *spreadExpr) {}

    void DummyVisitor::visit(ValueBindingStatement *valBind) {}

    void DummyVisitor::visit(Binding *binding) {}

    void DummyVisitor::visit(IndexAccessorExpression *index) {}

    void DummyVisitor::visit(IndexAssignmentExpression *index) {}

    void DummyVisitor::visit(TypeCheckingExpression *typeCheck) {}

    void DummyVisitor::visit(TypeDef *typeDef) {}

    void DummyVisitor::visit(PropertyDef *propertyDef) {}

    void DummyVisitor::visit(NewObjectExpression *newObj) {}

    void DummyVisitor::visit(ImportDecl *importDecl) {}

    void DummyVisitor::visit(CompileUnit *compileUnit) {}

    void DummyVisitor::visit(NextStatement *nextStatement) {}

    void DummyVisitor::visit(LoopStatement *loopStatement) {}

    DummyVisitor::~DummyVisitor() = default;

}
