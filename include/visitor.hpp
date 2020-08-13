
#ifndef NG_AST_VISITOR_HPP
#define NG_AST_VISITOR_HPP

#include <ast.hpp>

namespace NG::AST {


    class IASTVisitor : NonCopyable {
    public:
        virtual void visit(ASTNode *astNode) = 0;

        virtual void visit(Module *mod) = 0;

        virtual void visit(Statement *stmt) = 0;

        virtual void visit(SimpleStatement *simpleStmt) = 0;

        virtual void visit(ReturnStatement *returnStmt) = 0;

        virtual void visit(CompoundStatement *compoundStmt) = 0;

        virtual void visit(IfStatement *ifStmt) = 0;

        virtual void visit(ValDefStatement *valDef) = 0;

        virtual void visit(Definition *def) = 0;

        virtual void visit(Param *param) = 0;

        virtual void visit(FunctionDef *funDef) = 0;

        virtual void visit(ValDef *valDef) = 0;

        virtual void visit(Expression *expr) = 0;

        virtual void visit(IdExpression *idExpr) = 0;

        virtual void visit(FunCallExpression *funCallExpr) = 0;

        virtual void visit(IdAccessorExpression *idAccExpr) = 0;

        virtual void visit(IndexAccessorExpression *index) = 0;

        virtual void visit(IndexAssignmentExpression *index) = 0;

        virtual void visit(BinaryExpression *binExpr) = 0;

        virtual void visit(AssignmentExpression *assignmentExpr) = 0;

        virtual void visit(IntegerValue *intVal) = 0;

        virtual void visit(StringValue *strVal) = 0;

        virtual void visit(BooleanValue *boolVal) = 0;

        virtual void visit(ArrayLiteral *array) = 0;

        virtual void visit(TypeDef* typeDef) = 0;

        virtual void visit(PropertyDef* propertyDef) = 0;

        virtual void visit(NewObjectExpression * newObj) = 0;

        ~IASTVisitor() override = 0;
    };

    class DefaultDummyAstVisitor : public virtual IASTVisitor {
    public:
        void visit(ASTNode *astNode) override;

        void visit(Module *mod) override;

        void visit(Statement *stmt) override;

        void visit(SimpleStatement *simpleStmt) override;

        void visit(ReturnStatement *returnStmt) override;

        void visit(CompoundStatement *compoundStmt) override;

        void visit(IfStatement *ifStmt) override;

        void visit(ValDefStatement *valDef) override;

        void visit(Definition *def) override;

        void visit(Param *param) override;

        void visit(FunctionDef *funDef) override;

        void visit(ValDef *valDef) override;

        void visit(Expression *expr) override;

        void visit(IdExpression *idExpr) override;

        void visit(FunCallExpression *funCallExpr) override;

        void visit(IdAccessorExpression *idAccExpr) override;

        void visit(IndexAccessorExpression *index) override;

        void visit(IndexAssignmentExpression *index) override;

        void visit(BinaryExpression *binExpr) override;

        void visit(AssignmentExpression *assignmentExpr) override;

        void visit(IntegerValue *intVal) override;

        void visit(StringValue *strVal) override;

        void visit(BooleanValue *boolVal) override;

        void visit(ArrayLiteral *array) override;

        void visit(TypeDef *typeDef) override;

        void visit(PropertyDef *propertyDef) override;

        void visit(NewObjectExpression *newObj) override;

        ~DefaultDummyAstVisitor() override;
    };
} // namespace NG::AST

#endif // NG_AST_VISITOR_HPP