
#ifndef __NG_AST_HPP
#define __NG_AST_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include <fwd.hpp>

namespace NG::AST {

    template<class T>
    using ASTRef = T *;

    enum class [[nodiscard]] ASTNodeType : uint32_t {
        UNKNOWN = 0,
        NODE = 0xDEADBEEF,

        DEFINITION = 0x100,
        MODULE,
        VAL_DEFINITION,
        FUN_DEFINITION,

        PARAM,

        EXPRESSION = 0x200,
        ID_EXPRESSION,
        ID_ACCESSOR_EXPRESSION,
        FUN_CALL_EXPRESSION,
        BINARY_EXPRESSION,
        ASSIGNMENT_EXPRESSION,
        INDEX_ACCESSOR_EXPRESSION,

        LITERAL = 0x300,
        INTEGER_VALUE,
        STRING_VALUE,
        BOOLEAN_VALUE,
        ARRAY_LITERAL,

        STATEMENT = 0x400,
        SIMPLE_STATEMENT,
        COMPOUND_STATEMENT,
        VAL_DEF_STATEMENT,
        RETURN_STATEMENT,
        IF_STATEMENT,

        BOTTOM
    };

    template<class T, class... Args>
    inline ASTRef<T> makeast(Args &&... args) {
        return new T{std::move(args)...};
    }

    template<class T>
    inline void destroyast(ASTRef<T> t) {
        if (t != nullptr) {
            delete t;
        }
    }

    struct ASTNode : NonCopyable {
        ASTNode() = default;

        virtual void accept(IASTVisitor *visitor) = 0;

        [[nodiscard]] virtual ASTNodeType astNodeType() const = 0;

        virtual bool operator==(const ASTNode &node) const = 0;

        virtual Str repr() = 0;

        ~ASTNode() override = 0;
    };

    struct Module : ASTNode {
        const ASTNodeType ast_node_type = ASTNodeType::MODULE;
        Str name;
        Vec<ASTRef<Definition>> definitions;

        Vec<ASTRef<Statement>> statements;

        explicit Module(Str _name = "default") : name(std::move(_name)) {
        }

        [[nodiscard]] ASTNodeType astNodeType() const override { return ast_node_type; }

        bool operator==(const ASTNode &node) const override;

        void accept(IASTVisitor *visitor) override;

        Str repr() override;

        ~Module() override;
    };

    struct Statement : ASTNode {
    };

    struct Definition : ASTNode {
        [[nodiscard]] virtual Str name() const = 0;
    };

    enum class ParamType : int32_t {
        Simple = 0x01,
        Annotated = 0x02
    };

    struct Param : ASTNode {
        const ParamType type;
        const Str paramName;
        const Str annotatedType;

        explicit Param(const Str &name) : Param(name, "", ParamType::Simple) {}

        Param(const Str &name, const Str &type) : Param(name, type, ParamType::Annotated) {}

        Param(Str name, Str _annotatedType, ParamType type)
                : paramName(std::move(name)), annotatedType(std::move(_annotatedType)), type(type) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::PARAM; }

        bool operator==(const ASTNode &node) const override;

        ~Param() override = default;

        Str repr() override;
    };

    struct FunctionDef : Definition {
        Str funName{};
        Vec<ASTRef<Param>> params{};
        ASTRef<Statement> body = nullptr;

        [[nodiscard]] Str name() const override;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::FUN_DEFINITION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~FunctionDef() override;
    };

    struct CompoundStatement : Statement {
        Vec<ASTRef<Statement>> statements{};

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::COMPOUND_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~CompoundStatement() override;
    };

    struct ReturnStatement : Statement {
        ASTRef<Expression> expression = nullptr;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::RETURN_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~ReturnStatement() override;
    };

    struct IfStatement : Statement {
        ASTRef<Expression> testing = nullptr;
        ASTRef<Statement> consequence = nullptr;
        ASTRef<Statement> alternative = nullptr;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::IF_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IfStatement() override;
    };

    struct SimpleStatement : Statement {

        ASTRef<Expression> expression = nullptr;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::SIMPLE_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~SimpleStatement() override;
    };

    struct Expression : ASTNode {
    };

    struct FunCallExpression : Expression {
        ASTRef<Expression> primaryExpression;
        Vec<ASTRef<Expression>> arguments;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::FUN_CALL_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~FunCallExpression() override;
    };

    struct IdAccessorExpression : Expression {
        ASTRef<Expression> primaryExpression;
        ASTRef<Expression> accessor;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::ID_ACCESSOR_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IdAccessorExpression() override;
    };

    struct IndexAccessorExpression : Expression {
        ASTRef<Expression> primary;
        ASTRef<Expression> accessor;

        IndexAccessorExpression(ASTRef<Expression> primary, ASTRef<Expression> accessor): primary {primary}, accessor {accessor} {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override;

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IndexAccessorExpression() override;
    };

    struct ValDefStatement : Statement {
        const Str name;

        ASTRef<Expression> value{};

        explicit ValDefStatement(Str _name) : name(std::move(_name)) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::VAL_DEF_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~ValDefStatement() override;
    };

    struct ValDef : Definition {
        ASTRef<ValDefStatement> body = nullptr;

        [[nodiscard]] Str name() const override {
            return body->name;
        }

        explicit ValDef(ASTRef<ValDefStatement> defStmt) : body(defStmt) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::VAL_DEFINITION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~ValDef() override;
    };

    struct AssignmentExpression : Expression {
        const Str name;
        ASTRef<Expression> value{};

        explicit AssignmentExpression(Str _name) : name(std::move(_name)) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::ASSIGNMENT_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~AssignmentExpression() override;
    };

    struct BinaryExpression : Expression {
        Token *optr;
        ASTRef<Expression> left;
        ASTRef<Expression> right;

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::BINARY_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~BinaryExpression() override;
    };

    struct IdExpression : Expression {
        const Str id;

        explicit IdExpression(Str _id) : id(std::move(_id)) {}

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::ID_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        void accept(IASTVisitor *visitor) override;
    };

    struct IntegerValue : Expression {
        const int value;

        explicit IntegerValue(int v) : value(v) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::INTEGER_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct StringValue : Expression {
        const Str value;

        explicit StringValue(Str v) : value(std::move(v)) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::INTEGER_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct BooleanValue : Expression {
        const bool value;

        explicit BooleanValue(bool _value) : value(_value) {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override { return ASTNodeType::BOOLEAN_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct ArrayLiteral : Expression {
        Vec<ASTRef<Expression>> elements;

        explicit ArrayLiteral(): elements {} {
        }

        explicit ArrayLiteral(const Vec<ASTRef<Expression>>& exprs): elements {exprs} {}

        void accept(IASTVisitor *visitor) override;

        [[nodiscard]] ASTNodeType astNodeType() const override;

        Str repr() override;

        bool operator==(const ASTNode &node) const override;

        ~ArrayLiteral() override;
    };

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

        virtual void visit(BinaryExpression *binExpr) = 0;

        virtual void visit(AssignmentExpression *assignmentExpr) = 0;

        virtual void visit(IntegerValue *intVal) = 0;

        virtual void visit(StringValue *strVal) = 0;

        virtual void visit(BooleanValue *boolVal) = 0;

        virtual void visit(ArrayLiteral *array) = 0;

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

        void visit(BinaryExpression *binExpr) override;

        void visit(AssignmentExpression *assignmentExpr) override;

        void visit(IntegerValue *intVal) override;

        void visit(StringValue *strVal) override;

        void visit(BooleanValue *boolVal) override;

        void visit(ArrayLiteral *array) override;

        ~DefaultDummyAstVisitor() override;
    };

    std::vector<uint8_t> serialize_ast(ASTNode *node);

    ASTNode *deserialize_ast(std::vector<uint8_t> &bytes);

} // namespace NG

#endif // __NG_AST_HPP
