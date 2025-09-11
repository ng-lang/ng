#pragma once

#include <memory>
#include <utility>
#include <optional>

#include <common.hpp>

#ifdef NG_CONING_USING_SHARED_PTR_FOR_AST

#include <ast/ref_adapter_shared_ptr.hpp>

#else // ifndef NG_CONING_USING_SHARED_PTR_FOR_AST
#include <ast/ref_adapter_raw.hpp>
#endif // NG_CONING_USING_SHARED_PTR_FOR_AST

namespace NG::ast
{

    enum class [[nodiscard]] ASTNodeType : uint32_t
    {
        UNKNOWN = 0,
        COMPILE_UNIT = 0x01,
        TYPE_ANNOTATION = 0x02,
        NODE = 0xDEADBEEF,

        DEFINITION = 0x100,
        MODULE = 0x101,
        VAL_DEFINITION = 0x102,
        FUN_DEFINITION = 0x103,

        PARAM = 0x110,
        TYPE_DEFINITION = 0x111,
        PROPERTY_DEFINITION = 0x112,
        IMPORT_DECLARATION = 0x113,

        EXPRESSION = 0x200,
        ID_EXPRESSION = 0x201,
        ID_ACCESSOR_EXPRESSION = 0x202,
        FUN_CALL_EXPRESSION = 0x203,
        BINARY_EXPRESSION = 0x204,
        ASSIGNMENT_EXPRESSION = 0x205,
        INDEX_ACCESSOR_EXPRESSION = 0x206,
        INDEX_ASSIGNMENT_EXPRESSION = 0x207,
        NEW_OBJECT_EXPRESSION = 0x208,
        TYPE_CHECKING_EXPRESSION = 0x209,

        LITERAL = 0x300,
        INTEGER_VALUE = 0x301,
        STRING_VALUE = 0x302,
        BOOLEAN_VALUE = 0x303,
        ARRAY_LITERAL = 0x304,
        INTEGRAL_VALUE = 0x305,
        FLOATING_POINT_VALUE = 0x306,

        STATEMENT = 0x400,
        SIMPLE_STATEMENT = 0x401,
        COMPOUND_STATEMENT = 0x402,
        VAL_DEF_STATEMENT = 0x403,
        RETURN_STATEMENT = 0x404,
        IF_STATEMENT = 0x405,
        LOOP_STATEMENT = 0x406,

        LOOP_DECLARATION = 0x501,
        NEXT_STATEMENT = 0x503,
        YIELD_STATEMENT = 0x504,

        BOTTOM = 1030
    };

    // NOLINTBEGIN(cppcoreguidelines-special-member-functions)
    struct ASTNode : NonCopyable
    {
        ASTNode() = default;

        virtual void accept(AstVisitor *visitor) = 0;

        [[nodiscard]]
        virtual auto astNodeType() const -> ASTNodeType = 0;

        virtual auto operator==(const ASTNode &node) const -> bool = 0;

        [[nodiscard]]
        virtual auto repr() const -> Str = 0;

        virtual ~ASTNode() = 0;
    };

    struct ImportDecl : ASTNode
    {
        Str module;
        Vec<Str> modulePath;
        Str alias;
        Vec<Str> imports;

        auto astNodeType() const -> ASTNodeType override
        {
            return ASTNodeType::IMPORT_DECLARATION;
        }

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ImportDecl() override;
    };

    struct Module;
    struct Statement : ASTNode
    {
    };

    struct Expression : ASTNode
    {
    };
    struct Definition : ASTNode
    {
        [[nodiscard]] virtual auto name() const -> Str = 0;
    };

    enum class ParamType : uint8_t
    {
        Simple = 0x01,
        Annotated = 0x02
    };

    enum TypeAnnotationType : uint8_t
    {
        UNKNOWN,

        BUILTIN = 0x01,
        BUILTIN_UNIT = 0x11,

        BUILTIN_INT = 0x31,
        BUILTIN_BOOL,
        BUILTIN_STRING,
        BUILTIN_FLOAT,

        // integer variants
        BUILTIN_BYTE,
        BUILTIN_UBYTE,
        BUILTIN_SHORT,
        BUILTIN_USHORT,
        BUILTIN_UINT,
        BUILTIN_LONG,
        BUILTIN_ULONG,
        BUILTIN_U8,
        BUILTIN_I8,
        BUILTIN_U16,
        BUILTIN_I16,
        BUILTIN_U32,
        BUILTIN_I32,
        BUILTIN_U64,
        BUILTIN_I64,
        BUILTIN_UPTR,
        BUILTIN_IPTR,

        // floating point variants
        BUILTIN_HALF,
        BUILTIN_DOUBLE,
        BUILTIN_QUADRUPLE,
        BUILTIN_F16,
        BUILTIN_F32,
        BUILTIN_F64,
        BUILTIN_F128,
        CUSTOMIZED = 0x81,
    };

    struct TypeAnnotation : ASTNode
    {
        const Str name;
        TypeAnnotationType type{};

        explicit TypeAnnotation(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;
        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::TYPE_ANNOTATION; }

        auto operator==(const ASTNode &node) const -> bool override;
        [[nodiscard]]
        auto repr() const -> Str override;
        ~TypeAnnotation() override;
    };
    struct Param : ASTNode
    {
        const ParamType type;
        const Str paramName;
        std::optional<ASTRef<TypeAnnotation>> annotatedType;
        ASTRef<Expression> value = nullptr;

        explicit Param(const Str &name) : Param(name, {}, ParamType::Simple) {}

        Param(const Str &name, ASTRef<TypeAnnotation> type) : Param(name, type, ParamType::Annotated) {}

        Param(Str name, std::optional<ASTRef<TypeAnnotation>> _annotatedType, ParamType type)
            : paramName(std::move(name)), annotatedType(std::move(std::move(_annotatedType))), type(type) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::PARAM; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~Param() override = default;
    };

    struct FunctionDef : Definition
    {
        Str funName;
        Vec<ASTRef<Param>> params;
        ASTRef<TypeAnnotation> returnType;
        ASTRef<Statement> body = nullptr;
        bool native = false;

        [[nodiscard]] auto name() const -> Str override;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::FUN_DEFINITION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~FunctionDef() override;
    };

    struct CompoundStatement : Statement
    {
        Vec<ASTRef<Statement>> statements;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::COMPOUND_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~CompoundStatement() override;
    };

    // todo: multiple returns
    struct ReturnStatement : Statement
    {
        ASTRef<Expression> expression = nullptr;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::RETURN_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ReturnStatement() override;
    };

    struct IfStatement : Statement
    {
        ASTRef<Expression> testing = nullptr;
        ASTRef<Statement> consequence = nullptr;
        ASTRef<Statement> alternative = nullptr;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::IF_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IfStatement() override;
    };

    enum class LoopBindingType
    {
        LOOP_ASSIGN = 0,
        LOOP_IN = 1,
        // not supported now
        LOOP_DESTRUCT = 2,
    };

    struct LoopBinding
    {
        Str name;
        LoopBindingType type;
        ASTRef<Expression> target;
    };

    struct LoopStatement : Statement
    {
        ASTRef<Statement> loopBody = nullptr;
        Vec<LoopBinding> bindings{};

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::LOOP_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~LoopStatement() override;
    };

    struct NextStatement : Statement
    {
        Vec<ASTRef<Expression>> expressions{};

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::NEXT_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~NextStatement() override;
    };

    struct SimpleStatement : Statement
    {

        ASTRef<Expression> expression = nullptr;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::SIMPLE_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~SimpleStatement() override;
    };

    struct Module : ASTNode
    {
        const ASTNodeType ast_node_type = ASTNodeType::MODULE;
        Str name;

        Vec<ASTRef<Definition>> definitions;

        Vec<ASTRef<Statement>> statements;

        Vec<Str> exports;

        Vec<ASTRef<ImportDecl>> imports;

        explicit Module(Str _name = "default") : name(std::move(_name))
        {
        }

        auto astNodeType() const -> ASTNodeType override { return ast_node_type; }

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~Module() override;
    };

    struct CompileUnit : ASTNode
    {
        ASTRef<Module> module = nullptr;
        Str fileName;
        Str path;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::COMPILE_UNIT; }

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~CompileUnit() override;
    };

    struct FunCallExpression : Expression
    {
        ASTRef<Expression> primaryExpression;
        Vec<ASTRef<Expression>> arguments;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::FUN_CALL_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~FunCallExpression() override;
    };

    struct IdExpression : Expression
    {
        const Str id;

        explicit IdExpression(Str _id) : id(std::move(_id)) {}

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ID_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;
        auto operator==(const IdExpression &node) const -> bool;

        [[nodiscard]]
        auto repr() const -> Str override;

        void accept(AstVisitor *visitor) override;
    };

    struct IdAccessorExpression : Expression
    {
        ASTRef<Expression> primaryExpression;
        ASTRef<IdExpression> accessor;
        Vec<ASTRef<Expression>> arguments;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ID_ACCESSOR_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IdAccessorExpression() override;
    };

    struct IndexAccessorExpression : Expression
    {
        ASTRef<Expression> primary;
        ASTRef<Expression> accessor;

        IndexAccessorExpression(ASTRef<Expression> primary, ASTRef<Expression> accessor) : primary{std::move(std::move(primary))},
                                                                                           accessor{std::move(std::move(accessor))} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IndexAccessorExpression() override;
    };

    struct IndexAssignmentExpression : Expression
    {
        ASTRef<Expression> primary;
        ASTRef<Expression> accessor;
        ASTRef<Expression> value;

        IndexAssignmentExpression(
            ASTRef<Expression> primary,
            ASTRef<Expression> accessor,
            ASTRef<Expression> value) : primary{std::move(std::move(primary))}, accessor{std::move(std::move(accessor))}, value{std::move(std::move(value))} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IndexAssignmentExpression() override;
    };

    struct TypeCheckingExpression : Expression
    {
        ASTRef<Expression> value;
        ASTRef<Expression> type;

        TypeCheckingExpression(
            ASTRef<Expression> value,
            ASTRef<Expression> type) : value{value}, type{type} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~TypeCheckingExpression() override;
    };

    struct ValDefStatement : Statement
    {
        const Str name;

        ASTRef<Expression> value;

        std::optional<ASTRef<TypeAnnotation>> typeAnnotation;

        explicit ValDefStatement(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::VAL_DEF_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ValDefStatement() override;
    };

    struct ValDef : Definition
    {
        ASTRef<ValDefStatement> body = nullptr;

        [[nodiscard]] auto name() const -> Str override
        {
            return body->name;
        }

        explicit ValDef(ASTRef<ValDefStatement> defStmt) : body(std::move(std::move(defStmt))) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::VAL_DEFINITION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ValDef() override;
    };

    struct AssignmentExpression : Expression
    {
        ASTRef<Expression> target;
        ASTRef<Expression> value;

        explicit AssignmentExpression(ASTRef<Expression> target) : target(target) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ASSIGNMENT_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~AssignmentExpression() override;
    };

    struct BinaryExpression : Expression
    {
        std::shared_ptr<Token> optr;
        ASTRef<Expression> left;
        ASTRef<Expression> right;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::BINARY_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~BinaryExpression() override;
    };

    // NOLINTBEGIN(portability-template-virtual-member-function)
    template <std::integral T>
    struct IntegralValue : Expression
    {
        const T value;
        constexpr static size_t size = sizeof(T);

        explicit IntegralValue(T integralValue) : value(integralValue) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override
        {
            return ASTNodeType::INTEGRAL_VALUE;
        }

        [[nodiscard]]
        auto repr() const -> Str override
        {
            return std::to_string(this->value);
        }

        auto operator==(const ASTNode &node) const -> bool override
        {
            return this->astNodeType() == node.astNodeType() &&
                   this->repr() == node.repr();
        }
    };

    template <std::floating_point T>
    struct FloatingPointValue : Expression
    {
        const T value;
        constexpr static size_t size = sizeof(T);

        explicit FloatingPointValue(T floatingPointValue) : value(floatingPointValue) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override
        {
            return ASTNodeType::FLOATING_POINT_VALUE;
        }

        [[nodiscard]]
        auto repr() const -> Str override
        {
            return std::to_string(this->value);
        }

        auto operator==(const ASTNode &node) const -> bool override
        {
            return this->astNodeType() == node.astNodeType() &&
                   this->repr() == node.repr();
        }
    };

    // NOLINTEND(portability-template-virtual-member-function)

    struct StringValue : Expression
    {
        const Str value;

        explicit StringValue(Str stringValue) : value(std::move(stringValue)) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::STRING_VALUE; }

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;
    };

    struct BooleanValue : Expression
    {
        const bool value;

        explicit BooleanValue(bool _value) : value(_value) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::BOOLEAN_VALUE; }

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;
    };

    struct ArrayLiteral : Expression
    {
        Vec<ASTRef<Expression>> elements;

        explicit ArrayLiteral() = default;

        explicit ArrayLiteral(const Vec<ASTRef<Expression>> &exprs) : elements{exprs} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;

        ~ArrayLiteral() override;
    };

    struct PropertyDef : Definition
    {
        Str propertyName;

        explicit PropertyDef(Str name) : propertyName{std::move(name)} {}

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]] auto name() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;
    };

    struct TypeDef : Definition
    {
        Str typeName;
        Vec<ASTRef<FunctionDef>> memberFunctions;
        Vec<ASTRef<PropertyDef>> properties;

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]] auto name() const -> Str override;

        void accept(AstVisitor *visitor) override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~TypeDef() override;
    };

    struct NewObjectExpression : Expression
    {
        Str typeName;
        Map<Str, ASTRef<Expression>> properties;

        auto astNodeType() const -> ASTNodeType override;

        void accept(AstVisitor *visitor) override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~NewObjectExpression() override;
    };
    // NOLINTEND(cppcoreguidelines-special-member-functions)
} // namespace NG
