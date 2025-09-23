
#include <ast.hpp>
#include <visitor.hpp>
#include <token.hpp>
#include <algorithm>
#include <iterator>

namespace NG::ast
{

    template <class T>
    static auto strOfNodeList(const Vec<ASTRef<T>> &nodes, const Str &separator = ", ") -> Str
    {
        // return nodes | views::transform([](ASTRef<T> node)
        //                                 { return node->repr(); }) |
        //        views::join_with(", ");

        Str str{};
        for (const auto &node : nodes)
        {
            if (!str.empty())
            {
                str += separator;
            }
            str += node->repr();
        }
        return str;
    }

    ASTNode::~ASTNode() = default;

    void ASTNode::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    void Module::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    Module::~Module()
    {
        for (auto &&def : definitions)
        {
            destroyast(def);
        }

        for (auto &&imp : imports)
        {
            destroyast(imp);
        }

        for (auto &&stmt : statements)
        {
            destroyast(stmt);
        }
    }

    auto Module::repr() const -> Str
    {
        return Str{"module:"} + this->name + "\n" + strOfNodeList(definitions, "\n") + strOfNodeList(statements, "\n");
    }

    auto Definition::names() const -> Vec<Str>
    {
        return Vec<Str>{};
    }

    void TypeAnnotation::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeAnnotation::repr() const -> Str
    {
        return this->name;
    }

    TypeAnnotation::~TypeAnnotation() = default;

    void Param::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto Param::repr() const -> Str
    {
        return paramName + (annotatedType ? (": " + annotatedType->repr()) : "");
    }

    void CompoundStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    CompoundStatement::~CompoundStatement()
    {
        for (const auto &stmt : statements)
        {
            destroyast(stmt);
        }
    }

    auto CompoundStatement::repr() const -> Str
    {
        return "{\n" + strOfNodeList(this->statements, "\n") + "}";
    }

    void ReturnStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    ReturnStatement::~ReturnStatement()
    {
        if (expression != nullptr)
        {
            destroyast(expression);
        }
    }

    auto ReturnStatement::repr() const -> Str
    {
        return "return " + this->expression->repr() + ";";
    }

    void IfStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    IfStatement::~IfStatement()
    {
        if (testing != nullptr)
        {
            destroyast(testing);
        }
        if (consequence != nullptr)
        {
            destroyast(consequence);
        }
        if (alternative != nullptr)
        {
            destroyast(alternative);
        }
    }

    auto IfStatement::repr() const -> Str
    {
        return "if (" + this->testing->repr() + ") {\n" + this->consequence->repr() + "}" +
               (this->alternative == nullptr ? "" : (" else {\n" + this->alternative->repr() + "}"));
    }

    LoopStatement::~LoopStatement()
    {
        if (loopBody != nullptr)
        {
            destroyast(loopBody);
        }

        for (auto binding : this->bindings)
        {
            destroyast(binding.target);
            if (binding.annotation != nullptr)
            {
                destroyast(binding.annotation);
            }
        }
    }

    void LoopStatement::accept(AstVisitor *visitor)
    {
        return visitor->visit(this);
    }

    auto LoopStatement::repr() const -> Str
    {
        return "loop (...) " + loopBody->repr();
    }

    NextStatement::~NextStatement()
    {
        for (auto expr : this->expressions)
        {
            destroyast(expr);
        }
    }

    void NextStatement::accept(AstVisitor *visitor)
    {
        return visitor->visit(this);
    }

    auto NextStatement::repr() const -> Str
    {
        return "next " + strOfNodeList(this->expressions);
    }

    void EmptyStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto EmptyStatement::repr() const -> Str
    {
        return ";";
    }

    SimpleStatement::~SimpleStatement()
    {
        if (expression != nullptr)
        {
            destroyast(expression);
        }
    }

    void SimpleStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto SimpleStatement::repr() const -> Str
    {
        return this->expression->repr() + ";";
    }

    void FunCallExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    FunCallExpression::~FunCallExpression()
    {
        destroyast(primaryExpression);
        for (const auto &arg : arguments)
        {
            destroyast(arg);
        }
    }

    auto FunCallExpression::repr() const -> Str
    {
        return this->primaryExpression->repr() + "(" + strOfNodeList(this->arguments) + ")";
    }

    void AssignmentExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    AssignmentExpression::~AssignmentExpression()
    {
        if (target != nullptr)
        {
            destroyast(target);
        }
        if (value != nullptr)
        {
            destroyast(value);
        }
    }

    auto AssignmentExpression::repr() const -> Str
    {
        return this->target->repr() + " = " + this->value->repr();
    }

    void ValDefStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    ValDefStatement::~ValDefStatement()
    {
        if (value != nullptr)
        {
            destroyast(value);
        }
    }

    auto ValDefStatement::repr() const -> Str
    {
        return "val " + this->name + " = " + this->value->repr() + ";";
    }

    auto ValDef::names() const -> Vec<Str>
    {
        if (auto valDefStmt = dynamic_cast<ValDefStatement *>(body.get()))
        {
            return Vec<Str>{valDefStmt->name};
        }
        else if (auto valBindStmt = dynamic_cast<ValueBindingStatement *>(body.get()))
        {
            Vec<Str> names;
            for (const auto &binding : valBindStmt->bindings)
            {
                names.push_back(binding->name);
            }
            return names;
        }
        return Vec<Str>{};
    }

    void ValDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    ValDef::~ValDef()
    {
        if (body != nullptr)
        {
            destroyast(body);
        }
    }

    auto ValDef::repr() const -> Str
    {
        return this->body->repr();
    }

    void IdExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto IdExpression::repr() const -> Str
    {
        return id;
    }

    void IdAccessorExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    IdAccessorExpression::~IdAccessorExpression()
    {
        destroyast(primaryExpression);
        destroyast(accessor);
    }

    auto IdAccessorExpression::repr() const -> Str
    {
        return this->primaryExpression->repr() +
               (this->accessor == nullptr ? "" : ("." + this->accessor->repr()));
    }

    void StringValue::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto StringValue::repr() const -> Str
    {
        return "\"" + this->value + "\"";
    }

    void BooleanValue::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto BooleanValue::repr() const -> Str
    {
        return this->value ? "true" : "false";
    }

    void ArrayLiteral::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto ArrayLiteral::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::ARRAY_LITERAL;
    }

    auto ArrayLiteral::repr() const -> Str
    {
        return "[" + strOfNodeList(elements) + "]";
    }

    ArrayLiteral::~ArrayLiteral()
    {
        for (const auto &element : elements)
        {
            destroyast(element);
        }
    }

    void TupleLiteral::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TupleLiteral::repr() const -> Str
    {
        return "(" + strOfNodeList(elements) + ")";
    }

    TupleLiteral::~TupleLiteral()
    {
        for (const auto &element : elements)
        {
            destroyast(element);
        }
    }

    void FunctionDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    FunctionDef::~FunctionDef()
    {
        for (const auto &param : params)
        {
            destroyast(param);
        }
        destroyast(returnType);
        destroyast(body);
    }

    auto FunctionDef::names() const -> Vec<Str>
    {
        return Vec<Str>{funName};
    }

    auto FunctionDef::repr() const -> Str
    {
        return "fun " + funName + "(" + strOfNodeList(params) + ")" +
               (returnType ? " -> " + returnType->repr() : "") +
               body->repr();
    }

    void UnaryExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    UnaryExpression::~UnaryExpression()
    {
        destroyast(operand);
    }

    auto UnaryExpression::repr() const -> Str
    {
        return this->optr->repr + operand->repr();
    }

    void BinaryExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    BinaryExpression::~BinaryExpression()
    {
        destroyast(left);
        destroyast(right);
    }

    auto BinaryExpression::repr() const -> Str
    {
        return left->repr() + this->optr->repr + right->repr();
    }

    void IndexAccessorExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto IndexAccessorExpression::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::INDEX_ACCESSOR_EXPRESSION;
    }

    auto IndexAccessorExpression::repr() const -> Str
    {
        return primary->repr() + "[" + accessor->repr() + "]";
    }

    IndexAccessorExpression::~IndexAccessorExpression()
    {
        destroyast(primary);
        destroyast(accessor);
    }

    void IndexAssignmentExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto IndexAssignmentExpression::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::INDEX_ASSIGNMENT_EXPRESSION;
    }

    auto IndexAssignmentExpression::repr() const -> Str
    {
        return primary->repr() + "[" + accessor->repr() + "] = " + value->repr();
    }

    IndexAssignmentExpression::~IndexAssignmentExpression()
    {
        destroyast(primary);
        destroyast(accessor);
        destroyast(value);
    }

    void TypeCheckingExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeCheckingExpression::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::TYPE_CHECKING_EXPRESSION;
    }

    auto TypeCheckingExpression::repr() const -> Str
    {
        return value->repr() + " is " + this->type->repr();
    }

    TypeCheckingExpression::~TypeCheckingExpression()
    {
        destroyast(value);
        destroyast(type);
    }

    auto TypeDef::names() const -> Vec<Str>
    {
        return Vec<Str>{typeName};
    }

    auto TypeDef::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::TYPE_DEFINITION;
    }

    void TypeDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeDef::repr() const -> Str
    {
        const Str &propertiesRepr = strOfNodeList(properties, "\n");
        const Str &membersRepr = strOfNodeList(memberFunctions, "\n");

        return "type " + typeName + "{" + propertiesRepr + membersRepr + "}";
    }

    TypeDef::~TypeDef()
    {
        for (const auto &item : memberFunctions)
        {
            destroyast(item);
        }

        for (const auto &item : properties)
        {
            destroyast(item);
        }
    }

    auto PropertyDef::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::PROPERTY_DEFINITION;
    }

    auto PropertyDef::names() const -> Vec<Str>
    {
        return Vec<Str>{propertyName};
    }

    void PropertyDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto PropertyDef::repr() const -> Str
    {
        return "property " + propertyName + ";";
    }

    auto NewObjectExpression::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::NEW_OBJECT_EXPRESSION;
    }

    void NewObjectExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto NewObjectExpression::repr() const -> Str
    {
        Str props{};

        for (const auto &property : properties)
        {
            if (!props.empty())
            {
                props += ",";
            }

            props += (property.first + ": " + property.second->repr());
        }

        return "new " + typeName + " { " + props + " }";
    }

    NewObjectExpression::~NewObjectExpression()
    {
        for (auto &[_, value] : properties) // NOLINT(readability-identifier-length)
        {
            destroyast(value);
        }
    }

    void ImportDecl::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto ImportDecl::repr() const -> Str
    {
        return module;
    }

    ImportDecl::~ImportDecl() = default;

    void CompileUnit::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto CompileUnit::repr() const -> Str
    {
        return Str{"Compile Unit: "} + fileName + "\n" + module->repr();
    }

    CompileUnit::~CompileUnit()
    {
        destroyast(module);
    }

    void TypeOfExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeOfExpression::repr() const -> Str
    {
        return "typeof(" + expression->repr() + ")";
    }

    TypeOfExpression::~TypeOfExpression()
    {
        destroyast(expression);
    }

    void SpreadExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto SpreadExpression::repr() const -> Str
    {
        return "..." + expression->repr();
    }

    SpreadExpression::~SpreadExpression()
    {
        destroyast(expression);
    }

    Binding::~Binding()
    {
        if (annotation != nullptr)
        {
            destroyast(annotation);
        }
        if (value != nullptr)
        {
            destroyast(value);
        }
    }

    auto Binding::repr() const -> Str
    {
        return name + (annotation ? (": " + annotation->repr()) : "") +
               (value ? (" = " + value->repr()) : "");
    }

    auto Binding::accept(AstVisitor *visitor) -> void
    {
        visitor->visit(this);
    }

    auto ValueBindingStatement::accept(AstVisitor *visitor) -> void
    {
        visitor->visit(this);
    }

    auto ValueBindingStatement::repr() const -> Str
    {
        return "val " + strOfNodeList(this->bindings, ", ") + ";";
    }

    ValueBindingStatement::~ValueBindingStatement()
    {
        for (const auto &binding : bindings)
        {
            destroyast(binding);
        }
        if (value != nullptr)
        {
            destroyast(value);
        }
    }

} // namespace NG
