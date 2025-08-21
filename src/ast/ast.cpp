
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

    const auto ASTComparator = [](const ASTRef<ASTNode> &left, const ASTRef<ASTNode> &right) -> bool
    {
        return *left == *right;
    };

    void ASTNode::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto ASTNode::operator==(const ASTNode &node) const -> bool
    {
        return false;
    }

    auto Module::operator==(const ASTNode &node) const -> bool
    {
        const auto &mod = dynamic_cast<const Module &>(node);
        return astNodeType() == node.astNodeType() &&
               name == mod.name &&
               definitions.size() == mod.definitions.size() &&
               std::equal(std::begin(definitions),
                          std::end(definitions),
                          begin(mod.definitions),
                          ASTComparator);
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

    auto Definition::name() const -> Str
    {
        return "unknown";
    }

    void TypeAnnotation::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeAnnotation::operator==(const ASTNode &node) const -> bool
    {
        const auto &anno = dynamic_cast<const TypeAnnotation &>(node);
        return astNodeType() == node.astNodeType() &&
               anno.name == name &&
               anno.type == type;
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

    auto Param::operator==(const ASTNode &node) const -> bool
    {
        const auto &param = dynamic_cast<const Param &>(node);
        return astNodeType() == node.astNodeType() &&
               paramName == param.paramName &&
               annotatedType == param.annotatedType &&
               type == param.type;
    }

    auto Param::repr() const -> Str
    {
        return paramName + (annotatedType.has_value() ? (": " + (*annotatedType)->repr()) : "");
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

    auto CompoundStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &stmt = dynamic_cast<const CompoundStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               std::equal(begin(statements),
                          end(statements),
                          begin(stmt.statements),
                          ASTComparator);
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

    auto ReturnStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &ret = dynamic_cast<const ReturnStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *ret.expression;
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

    auto IfStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &ifstmt = dynamic_cast<const IfStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *testing == *ifstmt.testing &&
               *consequence == *ifstmt.consequence &&
               ((alternative == nullptr &&
                 ifstmt.alternative == nullptr) ||
                *alternative == *ifstmt.alternative);
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
        }
    }

    void LoopStatement::accept(AstVisitor *visitor)
    {
        return visitor->visit(this);
    }

    auto LoopStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &loopStmt = dynamic_cast<const LoopStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *loopBody == *loopStmt.loopBody &&
               bindings.size() == loopStmt.bindings.size() &&
               std::equal(std::begin(bindings),
                          std::end(bindings),
                          begin(loopStmt.bindings),
                          [](auto &&left, auto &&right)
                          {
                              return left.name == right.name &&
                                     left.type == right.type &&
                                     ASTComparator(left.target, right.target);
                          });
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

    auto NextStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &nextStmt = dynamic_cast<const NextStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               expressions.size() == nextStmt.expressions.size() &&
               std::equal(std::begin(expressions),
                          std::end(expressions),
                          begin(nextStmt.expressions),
                          ASTComparator);
    }

    auto NextStatement::repr() const -> Str
    {
        return "next " + strOfNodeList(this->expressions);
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

    auto SimpleStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &simple = dynamic_cast<const SimpleStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *simple.expression;
    }

    auto SimpleStatement::repr() const -> Str
    {
        return this->expression->repr() + ";";
    }

    void FunCallExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto FunCallExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &funCall = dynamic_cast<const FunCallExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               *funCall.primaryExpression == *primaryExpression &&
               std::equal(begin(arguments),
                          end(arguments),
                          begin(funCall.arguments),
                          ASTComparator);
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

    auto AssignmentExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &assign = dynamic_cast<const AssignmentExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               name == assign.name &&
               *value == *assign.value;
    }

    AssignmentExpression::~AssignmentExpression()
    {
        if (value != nullptr)
        {
            destroyast(value);
        }
    }

    auto AssignmentExpression::repr() const -> Str
    {
        return this->name + " = " + this->value->repr();
    }

    void ValDefStatement::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto ValDefStatement::operator==(const ASTNode &node) const -> bool
    {
        const auto &valDef = dynamic_cast<const ValDefStatement &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.name == name &&
               *valDef.value == *value;
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

    void ValDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto ValDef::operator==(const ASTNode &node) const -> bool
    {
        const auto &valDef = dynamic_cast<const ValDef &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.body->operator==(*(valDef.body));
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

    auto IdExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &idexpr = dynamic_cast<const IdExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               idexpr.id == id;
    }

    auto IdExpression::operator==(const IdExpression &node) const -> bool
    {
        return node.id == id;
    }

    auto IdExpression::repr() const -> Str
    {
        return id;
    }

    void IdAccessorExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto IdAccessorExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &idacc = dynamic_cast<const IdAccessorExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *idacc.primaryExpression == *primaryExpression &&
               *idacc.accessor == *accessor;
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

    auto StringValue::operator==(const ASTNode &node) const -> bool
    {
        const auto &strVal = dynamic_cast<const StringValue &>(node);
        return astNodeType() == node.astNodeType() &&
               strVal.value == value;
    }

    auto StringValue::repr() const -> Str
    {
        return "\"" + this->value + "\"";
    }

    void BooleanValue::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto BooleanValue::operator==(const ASTNode &node) const -> bool
    {
        const auto &val = dynamic_cast<const BooleanValue &>(node);
        return astNodeType() == node.astNodeType() &&
               value == val.value;
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

    auto ArrayLiteral::operator==(const ASTNode &node) const -> bool
    {
        const auto &arrayLit = dynamic_cast<const ArrayLiteral &>(node);
        return astNodeType() == arrayLit.astNodeType() &&
               elements.size() == arrayLit.elements.size() &&
               std::equal(begin(elements), end(elements), begin(arrayLit.elements), ASTComparator);
    }

    ArrayLiteral::~ArrayLiteral()
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

    auto FunctionDef::operator==(const ASTNode &node) const -> bool
    {
        const auto &funDef = dynamic_cast<const FunctionDef &>(node);
        return astNodeType() == node.astNodeType() &&
               funDef.funName == funName &&
               std::equal(begin(params),
                          end(params),
                          begin(funDef.params),
                          ASTComparator) &&
               *funDef.body == *body;
    }

    FunctionDef::~FunctionDef()
    {
        for (const auto &param : params)
        {
            destroyast(param);
        }
        destroyast(body);
    }

    auto FunctionDef::name() const -> Str
    {
        return funName;
    }

    auto FunctionDef::repr() const -> Str
    {
        return "fun " + funName + "(" + strOfNodeList(params) + ")" + body->repr();
    }

    void BinaryExpression::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto BinaryExpression::operator==(const ASTNode &node) const -> bool
    {
        auto &&binexpr = dynamic_cast<const BinaryExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *binexpr.optr == *optr &&
               *left == *binexpr.left &&
               *right == *binexpr.right;
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

    auto IndexAccessorExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &indexAccExpr = dynamic_cast<const IndexAccessorExpression &>(node);

        return *primary == *(indexAccExpr.primary) &&
               *accessor == *(indexAccExpr.accessor);
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

    auto IndexAssignmentExpression::operator==(const ASTNode &node) const -> bool
    {
        const auto &indexAssExpr = dynamic_cast<const IndexAssignmentExpression &>(node);

        return *primary == *(indexAssExpr.primary) &&
               *accessor == *(indexAssExpr.accessor) &&
               *value == *(indexAssExpr.value);
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

    auto TypeDef::name() const -> Str
    {
        return this->typeName;
    }

    auto TypeDef::astNodeType() const -> ASTNodeType
    {
        return ASTNodeType::TYPE_DEFINITION;
    }

    void TypeDef::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto TypeDef::operator==(const ASTNode &node) const -> bool
    {
        auto &&typeDef = dynamic_cast<const TypeDef &>(node);

        return typeName == typeDef.name() &&
               std::equal(begin(properties),
                          end(properties),
                          begin(typeDef.properties),
                          ASTComparator) &&
               std::equal(begin(memberFunctions),
                          end(memberFunctions),
                          begin(typeDef.memberFunctions),
                          ASTComparator);
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

    auto PropertyDef::name() const -> Str
    {
        return propertyName;
    }

    auto PropertyDef::operator==(const ASTNode &node) const -> bool
    {
        const auto &property = dynamic_cast<const PropertyDef &>(node);

        return propertyName == property.propertyName;
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

    auto NewObjectExpression::operator==(const ASTNode &node) const -> bool
    {
        auto &&newObj = dynamic_cast<const NewObjectExpression &>(node);

        return newObj.typeName == typeName &&
               std::equal(begin(newObj.properties), end(newObj.properties), begin(properties),
                          [](auto &&left, auto &&right)
                          {
                              return left.first == right.first && ASTComparator(left.second, right.second);
                          });
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

    auto ImportDecl::operator==(const ASTNode &node) const -> bool
    {
        auto &&imports = dynamic_cast<const ImportDecl &>(node);

        return this->module == imports.module &&
               this->alias == imports.alias &&
               this->imports == imports.imports;
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

    auto CompileUnit::operator==(const ASTNode &node) const -> bool
    {
        auto &&compileUnit = dynamic_cast<const CompileUnit &>(node);

        return this->fileName == compileUnit.fileName &&
               std::equal(begin(this->modules), end(this->modules), begin(compileUnit.modules), ASTComparator);
    }

    void CompileUnit::accept(AstVisitor *visitor)
    {
        visitor->visit(this);
    }

    auto CompileUnit::repr() const -> Str
    {
        return Str{"Compile Unit: "} + fileName;
    }

    CompileUnit::~CompileUnit()
    {
        for (auto &&module : modules)
        {
            destroyast(module);
        }
    }
} // namespace NG
