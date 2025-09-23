#pragma once

#include <ast.hpp>

namespace NG::ast
{

    /**
     * @brief An interface for visiting AST nodes.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    struct AstVisitor : NonCopyable
    {
    public:
        /**
         * @brief Visits an AST node.
         *
         * @param astNode The AST node to visit.
         */
        virtual void visit(ASTNode *astNode) = 0;

        /**
         * @brief Visits a module.
         *
         * @param mod The module to visit.
         */
        virtual void visit(Module *mod) = 0;

        /**
         * @brief Visits a statement.
         *
         * @param stmt The statement to visit.
         */
        virtual void visit(Statement *stmt) = 0;

        /**
         * @brief Visits a simple statement.
         *
         * @param simpleStmt The simple statement to visit.
         */
        virtual void visit(SimpleStatement *simpleStmt) = 0;

        /**
         * @brief Visits a return statement.
         *
         * @param returnStmt The return statement to visit.
         */
        virtual void visit(ReturnStatement *returnStmt) = 0;

        /**
         * @brief Visits a compound statement.
         *
         * @param compoundStmt The compound statement to visit.
         */
        virtual void visit(CompoundStatement *compoundStmt) = 0;

        /**
         * @brief Visits an if statement.
         *
         * @param ifStmt The if statement to visit.
         */
        virtual void visit(IfStatement *ifStmt) = 0;

        /**
         * @brief Visits a value definition statement.
         *
         * @param valDef The value definition statement to visit.
         */
        virtual void visit(ValDefStatement *valDef) = 0;

        /**
         * @brief Visits a definition.
         *
         * @param def The definition to visit.
         */
        virtual void visit(Definition *def) = 0;

        /**
         * @brief Visits a parameter.
         *
         * @param param The parameter to visit.
         */
        virtual void visit(Param *param) = 0;

        /**
         * @brief Visits a type annotation.
         *
         * @param typeAnno The type annotation to visit.
         */
        virtual void visit(TypeAnnotation *typeAnno) = 0;

        /**
         * @brief Visits a function definition.
         *
         * @param funDef The function definition to visit.
         */
        virtual void visit(FunctionDef *funDef) = 0;

        /**
         * @brief Visits a value definition.
         *
         * @param valDef The value definition to visit.
         */
        virtual void visit(ValDef *valDef) = 0;

        /**
         * @brief Visits an expression.
         *
         * @param expr The expression to visit.
         */
        virtual void visit(Expression *expr) = 0;

        /**
         * @brief Visits an ID expression.
         *
         * @param idExpr The ID expression to visit.
         */
        virtual void visit(IdExpression *idExpr) = 0;

        /**
         * @brief Visits a function call expression.
         *
         * @param funCallExpr The function call expression to visit.
         */
        virtual void visit(FunCallExpression *funCallExpr) = 0;

        /**
         * @brief Visits an ID accessor expression.
         *
         * @param idAccExpr The ID accessor expression to visit.
         */
        virtual void visit(IdAccessorExpression *idAccExpr) = 0;

        /**
         * @brief Visits an index accessor expression.
         *
         * @param index The index accessor expression to visit.
         */
        virtual void visit(IndexAccessorExpression *index) = 0;

        /**
         * @brief Visits an index assignment expression.
         *
         * @param index The index assignment expression to visit.
         */
        virtual void visit(IndexAssignmentExpression *index) = 0;

        /**
         * @brief Visits a type checking expression.
         *
         * @param typeCheck The type checking expression to visit.
         */
        virtual void visit(TypeCheckingExpression *typeCheck) = 0;

        /**
         * @brief Visits a unary expression.
         *
         * @param unoExpr The unary expression to visit.
         */
        virtual void visit(UnaryExpression *unoExpr) = 0;

        /**
         * @brief Visits a binary expression.
         *
         * @param binExpr The binary expression to visit.
         */
        virtual void visit(BinaryExpression *binExpr) = 0;

        /**
         * @brief Visits an assignment expression.
         *
         * @param assignmentExpr The assignment expression to visit.
         */
        virtual void visit(AssignmentExpression *assignmentExpr) = 0;

        /**
         * @brief Visits an integral value.
         *
         * @param intVal The integral value to visit.
         */
        virtual void visit(IntegralValue<int8_t> *intVal) = 0;
        virtual void visit(IntegralValue<uint8_t> *intVal) = 0;
        virtual void visit(IntegralValue<int16_t> *intVal) = 0;
        virtual void visit(IntegralValue<uint16_t> *intVal) = 0;
        virtual void visit(IntegralValue<int32_t> *intVal) = 0;
        virtual void visit(IntegralValue<uint32_t> *intVal) = 0;
        virtual void visit(IntegralValue<int64_t> *intVal) = 0;
        virtual void visit(IntegralValue<uint64_t> *intVal) = 0;

        // virtual void visit(FloatingPointValue<float16_t> *floatVal) = 0;
        /**
         * @brief Visits a floating point value.
         *
         * @param floatVal The floating point value to visit.
         */
        virtual void visit(FloatingPointValue<float /* float32_t */> *floatVal) = 0;
        virtual void visit(FloatingPointValue<double /* float64_t */> *floatVal) = 0;
        // virtual void visit(FloatingPointValue<float128_t> *floatVal) = 0;

        /**
         * @brief Visits a string value.
         *
         * @param strVal The string value to visit.
         */
        virtual void visit(StringValue *strVal) = 0;

        /**
         * @brief Visits a boolean value.
         *
         * @param boolVal The boolean value to visit.
         */
        virtual void visit(BooleanValue *boolVal) = 0;

        /**
         * @brief Visits an array literal.
         *
         * @param array The array literal to visit.
         */
        virtual void visit(ArrayLiteral *array) = 0;

        /**
         * @brief Visits a tuple literal.
         *
         * @param tuple The tuple literal to visit.
         */
        virtual void visit(TupleLiteral *tuple) = 0;

        /**
         * @brief Visits a typeof expression.
         *
         * @param typeofExpr The typeof expression to visit.
         */
        virtual void visit(TypeOfExpression *typeofExpr) = 0;

        /**
         * @brief Visits a spread expression.
         *
         * @param spreadExpr The spread expression to visit.
         */
        virtual void visit(SpreadExpression *spreadExpr) = 0;

        /**
         * @brief Visits a value binding statement.
         */
        virtual void visit(ValueBindingStatement *valBind) = 0;

        /**
         * @brief Visits a binding.
         */
        virtual void visit(Binding *binding) = 0;

        /**
         * @brief Visits a type definition.
         *
         * @param typeDef The type definition to visit.
         */
        virtual void visit(TypeDef *typeDef) = 0;

        /**
         * @brief Visits a property definition.
         *
         * @param propertyDef The property definition to visit.
         */
        virtual void visit(PropertyDef *propertyDef) = 0;

        /**
         * @brief Visits a new object expression.
         *
         * @param newObj The new object expression to visit.
         */
        virtual void visit(NewObjectExpression *newObj) = 0;

        /**
         * @brief Visits an import declaration.
         *
         * @param importDecl The import declaration to visit.
         */
        virtual void visit(ImportDecl *importDecl) = 0;

        /**
         * @brief Visits a compile unit.
         *
         * @param compileUnit The compile unit to visit.
         */
        virtual void visit(CompileUnit *compileUnit) = 0;

        /**
         * @brief Visits a next statement.
         *
         * @param nextStatement The next statement to visit.
         */
        virtual void visit(NextStatement *nextStatement) = 0;
        /**
         * @brief Visits a loop statement.
         *
         * @param loopStatement The loop statement to visit.
         */
        virtual void visit(LoopStatement *loopStatement) = 0;

        virtual ~AstVisitor() = 0;
    };

    /**
     * @brief A dummy implementation of `AstVisitor` that does nothing.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
    class DummyVisitor : public virtual AstVisitor
    {
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

        void visit(TypeAnnotation *typeAnno) override;

        void visit(FunctionDef *funDef) override;

        void visit(ValDef *valDef) override;

        void visit(Expression *expr) override;

        void visit(IdExpression *idExpr) override;

        void visit(FunCallExpression *funCallExpr) override;

        void visit(IdAccessorExpression *idAccExpr) override;

        void visit(IndexAccessorExpression *index) override;

        void visit(IndexAssignmentExpression *index) override;

        void visit(TypeCheckingExpression *typeCheck) override;

        void visit(UnaryExpression *unoExpr) override;

        void visit(BinaryExpression *binExpr) override;

        void visit(AssignmentExpression *assignmentExpr) override;

        void visit(IntegralValue<int8_t> *intVal) override;
        void visit(IntegralValue<uint8_t> *intVal) override;
        void visit(IntegralValue<int16_t> *intVal) override;
        void visit(IntegralValue<uint16_t> *intVal) override;
        void visit(IntegralValue<int32_t> *intVal) override;
        void visit(IntegralValue<uint32_t> *intVal) override;
        void visit(IntegralValue<int64_t> *intVal) override;
        void visit(IntegralValue<uint64_t> *intVal) override;

        // void visit(FloatingPointValue<float16_t> *floatVal) override;
        void visit(FloatingPointValue<float /* float32_t */> *floatVal) override;
        void visit(FloatingPointValue<double /* float64_t */> *floatVal) override;
        // void visit(FloatingPointValue<float128_t> *floatVal) override;

        void visit(StringValue *strVal) override;

        void visit(BooleanValue *boolVal) override;

        void visit(ArrayLiteral *array) override;

        void visit(TupleLiteral *tuple) override;

        void visit(TypeOfExpression *typeofExpr) override;
        void visit(SpreadExpression *spreadExpr) override;
        void visit(ValueBindingStatement *valBind) override;
        void visit(Binding *binding) override;

        void visit(TypeDef *typeDef) override;

        void visit(PropertyDef *propertyDef) override;

        void visit(NewObjectExpression *newObj) override;

        void visit(ImportDecl *importDecl) override;

        void visit(CompileUnit *compileUnit) override;

        void visit(NextStatement *nextStatement) override;
        void visit(LoopStatement *loopStatement) override;

        ~DummyVisitor() override;
    };

    template <std::integral T>
    void IntegralValue<T>::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    template <std::floating_point T>
    void FloatingPointValue<T>::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

} // namespace NG::ast
