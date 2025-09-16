#pragma once

#include <memory>
#include <utility>
#include <optional>

#include <common.hpp>

#ifdef NG_CONFIG_USING_SHARED_PTR_FOR_AST

#include <ast/ref_adapter_shared_ptr.hpp>

#else // ifndef NG_CONFIG_USING_SHARED_PTR_FOR_AST
#include <ast/ref_adapter_raw.hpp>
#endif // NG_CONFIG_USING_SHARED_PTR_FOR_AST

namespace NG::ast
{

    /**
     * @brief The type of an AST node.
     */
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
        UNARY_EXPRESSION = 0x210,

        LITERAL = 0x300,
        INTEGER_VALUE = 0x301,
        STRING_VALUE = 0x302,
        BOOLEAN_VALUE = 0x303,
        ARRAY_LITERAL = 0x304,
        INTEGRAL_VALUE = 0x305,
        FLOATING_POINT_VALUE = 0x306,

        STATEMENT = 0x400,
        EMPTY_STATEMENT = 0x400,
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

    /**
     * @brief The base class for all AST nodes.
     */
    // NOLINTBEGIN(cppcoreguidelines-special-member-functions)
    struct ASTNode : NonCopyable
    {
        ASTNode() = default;

        /**
         * @brief Accepts a visitor.
         *
         * @param visitor The visitor to accept.
         */
        virtual void accept(AstVisitor *visitor) = 0;

        /**
         * @brief Returns the type of the AST node.
         *
         * @return The type of the AST node.
         */
        [[nodiscard]]
        virtual auto astNodeType() const -> ASTNodeType = 0;

        virtual auto operator==(const ASTNode &node) const -> bool = 0;

        /**
         * @brief Returns a string representation of the AST node.
         *
         * @return A string representation of the AST node.
         */
        [[nodiscard]]
        virtual auto repr() const -> Str = 0;

        virtual ~ASTNode() = 0;
    };

    /**
     * @brief An import declaration.
     */
    struct ImportDecl : ASTNode
    {
        Str module;          ///< The module to import from.
        Vec<Str> modulePath; ///< The path to the module.
        Str alias;           ///< The alias for the module.
        Vec<Str> imports;    ///< The symbols to import.

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
    /**
     * @brief A statement.
     */
    struct Statement : ASTNode
    {
    };

    struct EmptyStatement : Statement
    {
        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::EMPTY_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~EmptyStatement() override = default;
    };

    /**
     * @brief An expression.
     */
    struct Expression : ASTNode
    {
    };
    /**
     * @brief A definition.
     */
    struct Definition : ASTNode
    {
        /**
         * @brief Returns the name of the definition.
         *
         * @return The name of the definition.
         */
        [[nodiscard]] virtual auto name() const -> Str = 0;
    };

    /**
     * @brief The type of a parameter.
     */
    enum class ParamType : uint8_t
    {
        Simple = 0x01,   ///< A simple parameter.
        Annotated = 0x02 ///< A parameter with a type annotation.
    };

    /**
     * @brief The type of a type annotation.
     */
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
        END_OF_BUILTIN = 0x7F,
        COMPOSITE = 0x80,
        ARRAY = 0x81,
        VECTOR = 0x82,
        // TUPLE,
        // LIST,
        // DICT,
        CUSTOMIZED = 0xD1,
    };

    struct TypeAnnotation;
    /**
     * @brief A type annotation.
     */
    struct TypeAnnotation : ASTNode
    {
        const Str name;            ///< The name of the type.
        TypeAnnotationType type{}; ///< The type of the annotation.
        Vec<ASTRef<ASTNode>> arguments;

        explicit TypeAnnotation(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;
        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::TYPE_ANNOTATION; }

        auto operator==(const ASTNode &node) const -> bool override;
        [[nodiscard]]
        auto repr() const -> Str override;
        ~TypeAnnotation() override;
    };
    /**
     * @brief A parameter.
     */
    struct Param : ASTNode
    {
        const ParamType type;                           ///< The type of the parameter.
        const Str paramName;                            ///< The name of the parameter.
        ASTRef<TypeAnnotation> annotatedType = nullptr; ///< The annotated type of the parameter.
        ASTRef<Expression> value = nullptr;             ///< The default value of the parameter.

        explicit Param(const Str &name) : Param(name, {}, ParamType::Simple) {}

        Param(const Str &name, ASTRef<TypeAnnotation> type) : Param(name, type, ParamType::Annotated) {}

        Param(Str name, ASTRef<TypeAnnotation> _annotatedType, ParamType type)
            : paramName(std::move(name)), annotatedType(std::move(_annotatedType)), type(type) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::PARAM; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~Param() override = default;
    };

    /**
     * @brief A function definition.
     */
    struct FunctionDef : Definition
    {
        Str funName;                                 ///< The name of the function.
        Vec<ASTRef<Param>> params;                   ///< The parameters of the function.
        ASTRef<TypeAnnotation> returnType = nullptr; ///< The return type of the function.
        ASTRef<Statement> body = nullptr;            ///< The body of the function.
        bool native = false;                         ///< Whether the function is a native function.

        [[nodiscard]] auto name() const -> Str override;

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::FUN_DEFINITION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~FunctionDef() override;
    };

    /**
     * @brief A compound statement.
     */
    struct CompoundStatement : Statement
    {
        Vec<ASTRef<Statement>> statements; ///< The statements in the compound statement.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::COMPOUND_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~CompoundStatement() override;
    };

    /**
     * @brief A return statement.
     */
    // todo: multiple returns
    struct ReturnStatement : Statement
    {
        ASTRef<Expression> expression = nullptr; ///< The expression to return.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::RETURN_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ReturnStatement() override;
    };

    /**
     * @brief An if statement.
     */
    struct IfStatement : Statement
    {
        ASTRef<Expression> testing = nullptr;    ///< The condition of the if statement.
        ASTRef<Statement> consequence = nullptr; ///< The consequence of the if statement.
        ASTRef<Statement> alternative = nullptr; ///< The alternative of the if statement.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::IF_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IfStatement() override;
    };

    /**
     * @brief The type of a loop binding.
     */
    enum class LoopBindingType
    {
        LOOP_ASSIGN = 0, ///< An assignment binding.
        LOOP_IN = 1,     ///< An in binding.
        // not supported now
        LOOP_DESTRUCT = 2, ///< A destructuring binding.
    };

    /**
     * @brief A loop binding.
     */
    struct LoopBinding
    {
        Str name;                          ///< The name of the binding.
        LoopBindingType type;              ///< The type of the binding.
        ASTRef<Expression> target;         ///< The target of the binding.
        ASTRef<TypeAnnotation> annotation; /// < The type annotation of the binding.
    };

    /**
     * @brief A loop statement.
     */
    struct LoopStatement : Statement
    {
        ASTRef<Statement> loopBody = nullptr; ///< The body of the loop.
        Vec<LoopBinding> bindings{};          ///< The bindings of the loop.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::LOOP_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~LoopStatement() override;
    };

    /**
     * @brief A next statement.
     */
    struct NextStatement : Statement
    {
        Vec<ASTRef<Expression>> expressions{}; ///< The expressions to evaluate.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::NEXT_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~NextStatement() override;
    };

    /**
     * @brief A simple statement.
     */
    struct SimpleStatement : Statement
    {

        ASTRef<Expression> expression = nullptr; ///< The expression of the simple statement.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::SIMPLE_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~SimpleStatement() override;
    };

    /**
     * @brief A module.
     */
    struct Module : ASTNode
    {
        Str name; ///< The name of the module.

        Vec<ASTRef<Definition>> definitions; ///< The definitions in the module.

        Vec<ASTRef<Statement>> statements; ///< The statements in the module.

        Vec<Str> exports; ///< The exported symbols.

        Vec<ASTRef<ImportDecl>> imports; ///< The imported modules.

        explicit Module(Str _name = "default") : name(std::move(_name))
        {
        }

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::MODULE; }

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~Module() override;
    };

    /**
     * @brief A compile unit.
     */
    struct CompileUnit : ASTNode
    {
        ASTRef<Module> module = nullptr; ///< The module of the compile unit.
        Str fileName;                    ///< The file name of the compile unit.
        Str path;                        ///< The path of the compile unit.

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::COMPILE_UNIT; }

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~CompileUnit() override;
    };

    /**
     * @brief A function call expression.
     */
    struct FunCallExpression : Expression
    {
        ASTRef<Expression> primaryExpression; ///< The primary expression of the function call.
        Vec<ASTRef<Expression>> arguments;    ///< The arguments of the function call.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::FUN_CALL_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~FunCallExpression() override;
    };

    /**
     * @brief An ID expression.
     */
    struct IdExpression : Expression
    {
        const Str id; ///< The ID of the expression.

        explicit IdExpression(Str _id) : id(std::move(_id)) {}

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ID_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;
        auto operator==(const IdExpression &node) const -> bool;

        [[nodiscard]]
        auto repr() const -> Str override;

        void accept(AstVisitor *visitor) override;
    };

    /**
     * @brief An ID accessor expression.
     */
    struct IdAccessorExpression : Expression
    {
        ASTRef<Expression> primaryExpression = nullptr; ///< The primary expression of the accessor.
        ASTRef<IdExpression> accessor = nullptr;        ///< The accessor of the expression.
        Vec<ASTRef<Expression>> arguments;              ///< The arguments of the accessor.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ID_ACCESSOR_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IdAccessorExpression() override;
    };

    /**
     * @brief An index accessor expression.
     */
    struct IndexAccessorExpression : Expression
    {
        ASTRef<Expression> primary = nullptr;  ///< The primary expression of the accessor.
        ASTRef<Expression> accessor = nullptr; ///< The accessor of the expression.

        IndexAccessorExpression(ASTRef<Expression> primary, ASTRef<Expression> accessor) : primary{std::move(primary)},
                                                                                           accessor{std::move(accessor)} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IndexAccessorExpression() override;
    };

    /**
     * @brief An index assignment expression.
     */
    struct IndexAssignmentExpression : Expression
    {
        ASTRef<Expression> primary = nullptr;  ///< The primary expression of the assignment.
        ASTRef<Expression> accessor = nullptr; ///< The accessor of the assignment.
        ASTRef<Expression> value = nullptr;    ///< The value of the assignment.

        IndexAssignmentExpression(
            ASTRef<Expression> primary,
            ASTRef<Expression> accessor,
            ASTRef<Expression> value) : primary{std::move(primary)}, accessor{std::move(accessor)}, value{std::move(value)} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~IndexAssignmentExpression() override;
    };

    /**
     * @brief A type checking expression.
     */
    struct TypeCheckingExpression : Expression
    {
        ASTRef<Expression> value = nullptr; ///< The value to check.
        ASTRef<Expression> type = nullptr;  ///< The type to check against.

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

    /**
     * @brief A value definition statement.
     */
    struct ValDefStatement : Statement
    {
        const Str name; ///< The name of the value.

        ASTRef<Expression> value = nullptr; ///< The value of the value.

        std::optional<ASTRef<TypeAnnotation>> typeAnnotation; ///< The type annotation of the value.

        explicit ValDefStatement(Str _name) : name(std::move(_name)) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::VAL_DEF_STATEMENT; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ValDefStatement() override;
    };

    /**
     * @brief A value definition.
     */
    struct ValDef : Definition
    {
        ASTRef<ValDefStatement> body = nullptr; ///< The body of the value definition.

        [[nodiscard]] auto name() const -> Str override
        {
            return body->name;
        }

        explicit ValDef(ASTRef<ValDefStatement> defStmt) : body(std::move(defStmt)) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::VAL_DEFINITION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~ValDef() override;
    };

    /**
     * @brief An assignment expression.
     */
    struct AssignmentExpression : Expression
    {
        ASTRef<Expression> target = nullptr; ///< The target of the assignment.
        ASTRef<Expression> value = nullptr;  ///< The value of the assignment.

        explicit AssignmentExpression(ASTRef<Expression> target) : target(target) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::ASSIGNMENT_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~AssignmentExpression() override;
    };

    /**
     * @brief A unary expression.
     */
    struct UnaryExpression : Expression
    {
        std::shared_ptr<Token> optr = nullptr; ///< The operator of the expression.
        ASTRef<Expression> operand = nullptr;  ///< The operand of the expression.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::UNARY_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~UnaryExpression() override;
    };

    /**
     * @brief A binary expression.
     */
    struct BinaryExpression : Expression
    {
        std::shared_ptr<Token> optr = nullptr; ///< The operator of the expression.
        ASTRef<Expression> left = nullptr;     ///< The left operand of the expression.
        ASTRef<Expression> right = nullptr;    ///< The right operand of the expression.

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::BINARY_EXPRESSION; }

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~BinaryExpression() override;
    };

    /**
     * @brief An integral value.
     *
     * @tparam T The type of the integral value.
     */
    // NOLINTBEGIN(portability-template-virtual-member-function)
    template <std::integral T>
    struct IntegralValue : Expression
    {
        const T value;                            ///< The value of the integral.
        constexpr static size_t size = sizeof(T); ///< The size of the integral.

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

    /**
     * @brief A floating point value.
     *
     * @tparam T The type of the floating point value.
     */
    template <std::floating_point T>
    struct FloatingPointValue : Expression
    {
        const T value;                            ///< The value of the floating point.
        constexpr static size_t size = sizeof(T); ///< The size of the floating point.

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

    /**
     * @brief A string value.
     */
    struct StringValue : Expression
    {
        const Str value; ///< The value of the string.

        explicit StringValue(Str stringValue) : value(std::move(stringValue)) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::STRING_VALUE; }

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;
    };

    /**
     * @brief A boolean value.
     */
    struct BooleanValue : Expression
    {
        const bool value; ///< The value of the boolean.

        explicit BooleanValue(bool _value) : value(_value) {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override { return ASTNodeType::BOOLEAN_VALUE; }

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;
    };

    /**
     * @brief An array literal.
     */
    struct ArrayLiteral : Expression
    {
        Vec<ASTRef<Expression>> elements; ///< The elements of the array.

        explicit ArrayLiteral() = default;

        explicit ArrayLiteral(const Vec<ASTRef<Expression>> &exprs) : elements{exprs} {}

        void accept(AstVisitor *visitor) override;

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]]
        auto repr() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;

        ~ArrayLiteral() override;
    };

    /**
     * @brief A property definition.
     */
    struct PropertyDef : Definition
    {
        Str propertyName; ///< The name of the property.

        explicit PropertyDef(Str name) : propertyName{std::move(name)} {}

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]] auto name() const -> Str override;

        auto operator==(const ASTNode &node) const -> bool override;

        void accept(AstVisitor *visitor) override;

        [[nodiscard]]
        auto repr() const -> Str override;
    };

    /**
     * @brief A type definition.
     */
    struct TypeDef : Definition
    {
        Str typeName;                             ///< The name of the type.
        Vec<ASTRef<FunctionDef>> memberFunctions; ///< The member functions of the type.
        Vec<ASTRef<PropertyDef>> properties;      ///< The properties of the type.

        auto astNodeType() const -> ASTNodeType override;

        [[nodiscard]] auto name() const -> Str override;

        void accept(AstVisitor *visitor) override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~TypeDef() override;
    };

    /**
     * @brief A new object expression.
     */
    struct NewObjectExpression : Expression
    {
        Str typeName;                            ///< The name of the type.
        Map<Str, ASTRef<Expression>> properties; ///< The properties of the new object.

        auto astNodeType() const -> ASTNodeType override;

        void accept(AstVisitor *visitor) override;

        auto operator==(const ASTNode &node) const -> bool override;

        [[nodiscard]]
        auto repr() const -> Str override;

        ~NewObjectExpression() override;
    };
    // NOLINTEND(cppcoreguidelines-special-member-functions)
} // namespace NG::ast
