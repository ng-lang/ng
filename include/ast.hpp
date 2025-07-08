
#ifndef __NG_AST_HPP
#define __NG_AST_HPP

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "common.hpp"
#include <fwd.hpp>

#ifdef NG_CONING_USING_SHARED_PTR_FOR_AST

#include <ast/ref_adapter_shared_ptr.hpp>

#else // ifndef NG_CONING_USING_SHARED_PTR_FOR_AST
#include <ast/ref_adapter_raw.hpp>
#endif // NG_CONING_USING_SHARED_PTR_FOR_AST

namespace NG::ast {

    enum class [[nodiscard]] ASTNodeType : uint32_t {
        UNKNOWN = 0,
        COMPILE_UNIT = 0x01,
        NODE = 0xDEADBEEF,

        DEFINITION = 0x100,
        MODULE,
        VAL_DEFINITION,
        FUN_DEFINITION,

        PARAM,
        TYPE_DEFINITION,
        PROPERTY_DEFINITION,
        IMPORT_DECLARATION,

        EXPRESSION = 0x200,
        ID_EXPRESSION,
        ID_ACCESSOR_EXPRESSION,
        FUN_CALL_EXPRESSION,
        BINARY_EXPRESSION,
        ASSIGNMENT_EXPRESSION,
        INDEX_ACCESSOR_EXPRESSION,
        INDEX_ASSIGNMENT_EXPRESSION,
        NEW_OBJECT_EXPRESSION,

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

    struct ASTNode : NonCopyable {
        ASTNode() = default;

        virtual void accept(AstVisitor *visitor) = 0;

        virtual ASTNodeType astNodeType() const = 0;

        virtual bool operator==(const ASTNode &node) const = 0;

        virtual Str repr() = 0;

        ~ASTNode() override = 0;
    };

    struct ImportDecl : ASTNode {
        Str module;
        Str alias;
        Vec<Str> imports;

        ASTNodeType astNodeType() const override {
            return ASTNodeType::IMPORT_DECLARATION;
        }

        bool operator==(const ASTNode &node) const override;

        void accept(AstVisitor *visitor) override;

        Str repr() override;

        ~ImportDecl() override;

    };

    struct Module;
    struct Statement : ASTNode {
    };

    struct Expression : ASTNode {
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

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::PARAM; }

        bool operator==(const ASTNode &node) const override;

        ~Param() override = default;

        Str repr() override;
    };

    struct FunctionDef : Definition {
        Str funName{};
        Vec<ASTRef<Param>> params{};
        ASTRef<Statement> body = nullptr;

        [[nodiscard]] Str name() const override;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::FUN_DEFINITION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~FunctionDef() override;
    };

    struct CompoundStatement : Statement {
        Vec<ASTRef<Statement>> statements{};

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::COMPOUND_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~CompoundStatement() override;
    };

    struct ReturnStatement : Statement {
        ASTRef<Expression> expression = nullptr;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::RETURN_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~ReturnStatement() override;
    };

    struct IfStatement : Statement {
        ASTRef<Expression> testing = nullptr;
        ASTRef<Statement> consequence = nullptr;
        ASTRef<Statement> alternative = nullptr;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::IF_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IfStatement() override;
    };

    struct SimpleStatement : Statement {

        ASTRef<Expression> expression = nullptr;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::SIMPLE_STATEMENT; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~SimpleStatement() override;
    };

    struct Module : ASTNode {
        const ASTNodeType ast_node_type = ASTNodeType::MODULE;
        Str name;

        Vec<ASTRef<Definition>> definitions;

        Vec<ASTRef<Statement>> statements;

        Vec<Str> exports;

        Vec<ASTRef<ImportDecl>> imports;

        explicit Module(Str _name = "default") : name(std::move(_name)) {
        }

        ASTNodeType astNodeType() const override { return ast_node_type; }

        bool operator==(const ASTNode &node) const override;

        void accept(AstVisitor *visitor) override;

        Str repr() override;

        ~Module() override;
    };

    struct CompileUnit : ASTNode {
        Vec<ASTRef<Module>> modules;
        Str fileName;
        Str path;

        ASTNodeType astNodeType() const override { return ASTNodeType::COMPILE_UNIT; }

        bool operator==(const ASTNode &node) const override;

        void accept(AstVisitor *visitor) override;

        Str repr() override;

        ~CompileUnit() override;
    };



    struct FunCallExpression : Expression {
        ASTRef<Expression> primaryExpression;
        Vec<ASTRef<Expression>> arguments;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::FUN_CALL_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~FunCallExpression() override;
    };

    struct IdExpression : Expression {
        const Str id;

        explicit IdExpression(Str _id) : id(std::move(_id)) {}

        ASTNodeType astNodeType() const override { return ASTNodeType::ID_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;
        bool operator==(const IdExpression &node) const;

        Str repr() override;

        void accept(AstVisitor *visitor) override;
    };

    struct IdAccessorExpression : Expression {
        ASTRef<Expression> primaryExpression;
        ASTRef<IdExpression> accessor;
        Vec<ASTRef<Expression>> arguments;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::ID_ACCESSOR_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IdAccessorExpression() override;
    };

    struct IndexAccessorExpression : Expression {
        ASTRef<Expression> primary;
        ASTRef<Expression> accessor;

        IndexAccessorExpression(ASTRef<Expression> primary, ASTRef<Expression> accessor) : primary{primary},
                                                                                           accessor{accessor} {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override;

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IndexAccessorExpression() override;
    };

    struct IndexAssignmentExpression : Expression {
        ASTRef<Expression> primary;
        ASTRef<Expression> accessor;
        ASTRef<Expression> value;

        IndexAssignmentExpression(
                ASTRef<Expression> primary,
                ASTRef<Expression> accessor,
                ASTRef<Expression> value
        ) : primary{primary}, accessor{accessor}, value{value} {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override;

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~IndexAssignmentExpression() override;
    };

    struct ValDefStatement : Statement {
        const Str name;

        ASTRef<Expression> value{};

        explicit ValDefStatement(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::VAL_DEF_STATEMENT; }

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

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::VAL_DEFINITION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~ValDef() override;
    };

    struct AssignmentExpression : Expression {
        const Str name;
        ASTRef<Expression> value{};

        explicit AssignmentExpression(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::ASSIGNMENT_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~AssignmentExpression() override;
    };

    struct BinaryExpression : Expression {
        Token *optr;
        ASTRef<Expression> left;
        ASTRef<Expression> right;

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::BINARY_EXPRESSION; }

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~BinaryExpression() override;
    };

    struct IntegerValue : Expression {
        const int value;

        explicit IntegerValue(int v) : value(v) {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::INTEGER_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct StringValue : Expression {
        const Str value;

        explicit StringValue(Str v) : value(std::move(v)) {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::STRING_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct BooleanValue : Expression {
        const bool value;

        explicit BooleanValue(bool _value) : value(_value) {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override { return ASTNodeType::BOOLEAN_VALUE; }

        Str repr() override;

        bool operator==(const ASTNode &node) const override;
    };

    struct ArrayLiteral : Expression {
        Vec<ASTRef<Expression>> elements;

        explicit ArrayLiteral() : elements{} {
        }

        explicit ArrayLiteral(const Vec<ASTRef<Expression>> &exprs) : elements{exprs} {}

        void accept(AstVisitor *visitor) override;

        ASTNodeType astNodeType() const override;

        Str repr() override;

        bool operator==(const ASTNode &node) const override;

        ~ArrayLiteral() override;
    };

    struct PropertyDef : Definition {
        Str propertyName;

        explicit PropertyDef(Str name) : propertyName{std::move(name)} {}

        ASTNodeType astNodeType() const override;

        [[nodiscard]] Str name() const override;

        bool operator==(const ASTNode &node) const override;

        void accept(AstVisitor *visitor) override;

        Str repr() override;
    };

    struct TypeDef : Definition {
        Str typeName;
        Vec<ASTRef<FunctionDef>> memberFunctions;
        Vec<ASTRef<PropertyDef>> properties;


        ASTNodeType astNodeType() const override;

        [[nodiscard]] Str name() const override;

        void accept(AstVisitor *visitor) override;

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~TypeDef() override;
    };

    struct NewObjectExpression : Expression {
        Str typeName;
        Map<Str, ASTRef<Expression>> properties;

        ASTNodeType astNodeType() const override;

        void accept(AstVisitor *visitor) override;

        bool operator==(const ASTNode &node) const override;

        Str repr() override;

        ~NewObjectExpression() override;
    };
} // namespace NG

#endif // __NG_AST_HPP
