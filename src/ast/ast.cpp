
#include <ast.hpp>
#include <visitor.hpp>
#include <token.hpp>
#include <algorithm>
#include <iterator>

namespace NG::AST {

    template<class T>
    static Str strOfNodeList(Vec<T> nodes, const Str &separator = ", ") {
        Str str{};
        for (const auto &node : nodes) {
            if (!str.empty()) {
                str += separator;
            }
            str += node->repr();
        }
        return str;
    }

    ASTNode::~ASTNode() = default;

    const auto ASTComparator = [](const ASTRef<ASTNode>& left, const ASTRef<ASTNode>& right) -> bool {
        return *left == *right;
    };

    void ASTNode::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool ASTNode::operator==(const ASTNode &node) const {
        return false;
    }

    bool Module::operator==(const ASTNode &node) const {
        auto &mod = dynamic_cast<const Module &>(node);
        return astNodeType() == node.astNodeType() &&
               name == mod.name &&
               definitions.size() == mod.definitions.size() &&
               std::equal(std::begin(definitions),
                          std::end(definitions),
                          begin(mod.definitions),
                          ASTComparator);
    }

    void Module::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    Module::~Module() {
        for (auto def : definitions) {
            destroyast(def);
        }
    }

    Str Module::repr() {
        return Str{"module:"} + this->name + "\n" + strOfNodeList(definitions, "\n")  + strOfNodeList(statements, "\n");
    }

    Str Definition::name() const {
        return "unknown";
    }

    void Param::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool Param::operator==(const ASTNode &node) const {
        auto &param = dynamic_cast<const Param &>(node);
        return astNodeType() == node.astNodeType() &&
               paramName == param.paramName &&
               annotatedType == param.annotatedType &&
               type == param.type;
    }

    Str Param::repr() {
        return paramName + ": " + annotatedType;
    }

    void CompoundStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    CompoundStatement::~CompoundStatement() {
        for (auto stmt : statements) {
            destroyast(stmt);
        }
    }

    bool CompoundStatement::operator==(const ASTNode &node) const {
        auto &stmt = dynamic_cast<const CompoundStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               std::equal(begin(statements),
                          end(statements),
                          begin(stmt.statements),
                          ASTComparator);
    }

    Str CompoundStatement::repr() {
        return "{\n" + strOfNodeList(this->statements, "\n") + "}";
    }

    void ReturnStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    ReturnStatement::~ReturnStatement() {
        if (expression != nullptr) {
            destroyast(expression);
        }
    }

    bool ReturnStatement::operator==(const ASTNode &node) const {
        auto &ret = dynamic_cast<const ReturnStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *ret.expression;
    }

    Str ReturnStatement::repr() {
        return "return " + this->expression->repr() + ";";
    }

    void IfStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    IfStatement::~IfStatement() {
        if (testing != nullptr) {
            destroyast(testing);
        }
        if (consequence != nullptr) {
            destroyast(consequence);
        }
        if (alternative != nullptr) {
            destroyast(alternative);
        }
    }

    bool IfStatement::operator==(const ASTNode &node) const {
        auto &ifstmt = dynamic_cast<const IfStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *testing == *ifstmt.testing &&
               *consequence == *ifstmt.consequence &&
               ((alternative == nullptr &&
                 ifstmt.alternative == nullptr) ||
                *alternative == *ifstmt.alternative);
    }

    Str IfStatement::repr() {
        return "if (" + this->testing->repr() + ") {\n" + this->consequence->repr() + "}" +
               (this->alternative == nullptr ? "" : (" else {\n" + this->alternative->repr() + "}"));
    }

    SimpleStatement::~SimpleStatement() {
        if (expression != nullptr) {
            destroyast(expression);
        }
    }

    void SimpleStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool SimpleStatement::operator==(const ASTNode &node) const {
        auto &simple = dynamic_cast<const SimpleStatement &>(node);
        return astNodeType() == node.astNodeType() &&
               *expression == *simple.expression;
    }

    Str SimpleStatement::repr() {
        return this->expression->repr() + ";";
    }

    void FunCallExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool FunCallExpression::operator==(const ASTNode &node) const {
        auto &funCall = dynamic_cast<const FunCallExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               *funCall.primaryExpression == *primaryExpression &&
               std::equal(begin(arguments),
                          end(arguments),
                          begin(funCall.arguments),
                          ASTComparator);
    }

    FunCallExpression::~FunCallExpression() {
        destroyast(primaryExpression);
        for (auto arg : arguments) {
            destroyast(arg);
        }
    }

    Str FunCallExpression::repr() {
        return this->primaryExpression->repr() + "(" + strOfNodeList(this->arguments) + ")";
    }

    void AssignmentExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool AssignmentExpression::operator==(const ASTNode &node) const {
        auto &assign = dynamic_cast<const AssignmentExpression &>(node);

        return astNodeType() == node.astNodeType() &&
               name == assign.name &&
               *value == *assign.value;
    }

    AssignmentExpression::~AssignmentExpression() {
        if (value != nullptr)
            destroyast(value);
    }

    Str AssignmentExpression::repr() {
        return this->name + " = " + this->value->repr();
    }

    void ValDefStatement::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool ValDefStatement::operator==(const ASTNode &node) const {
        auto &valDef = dynamic_cast<const ValDefStatement &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.name == name &&
               *valDef.value == *value;
    }

    ValDefStatement::~ValDefStatement() {
        if (value != nullptr)
            destroyast(value);
    }

    Str ValDefStatement::repr() {
        return "val " + this->name + " = " + this->value->repr() + ";";
    }

    void ValDef::accept(IASTVisitor *visitor) {
        return visitor->visit(this);
    }

    bool ValDef::operator==(const ASTNode &node) const {
        auto &valDef = dynamic_cast<const ValDef &>(node);

        return astNodeType() == node.astNodeType() &&
               valDef.body->operator==(*(valDef.body));
    }

    ValDef::~ValDef() {
        if (body != nullptr) {
            destroyast(body);
        }
    }

    Str ValDef::repr() {
        return this->body->repr();
    }

    void IdExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IdExpression::operator==(const ASTNode &node) const {
        auto &idexpr = dynamic_cast<const IdExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               idexpr.id == id;
    }

    Str IdExpression::repr() {
        return id;
    }

    void IdAccessorExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IdAccessorExpression::operator==(const ASTNode &node) const {
        auto &idacc = dynamic_cast<const IdAccessorExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *idacc.primaryExpression == *primaryExpression &&
               *idacc.accessor == *accessor;
    }

    IdAccessorExpression::~IdAccessorExpression() {
        destroyast(primaryExpression);
        destroyast(accessor);
    }

    Str IdAccessorExpression::repr() {
        return this->primaryExpression->repr() +
               (this->accessor == nullptr ? "" : ("." + this->accessor->repr()));
    }

    void IntegerValue::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool IntegerValue::operator==(const ASTNode &node) const {
        auto &intVal = dynamic_cast<const IntegerValue &>(node);
        return astNodeType() == node.astNodeType() &&
               intVal.value == value;
    }

    Str IntegerValue::repr() {
        return std::to_string(this->value);
    }

    void StringValue::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool StringValue::operator==(const ASTNode &node) const {
        auto &strVal = dynamic_cast<const StringValue &>(node);
        return astNodeType() == node.astNodeType() &&
               strVal.value == value;
    }

    Str StringValue::repr() {
        return "\"" + this->value + "\"";
    }

    void BooleanValue::accept(IASTVisitor *visitor) {
        return visitor->visit(this);
    }

    bool BooleanValue::operator==(const ASTNode &node) const {
        auto &val = dynamic_cast<const BooleanValue &>(node);
        return astNodeType() == node.astNodeType() &&
               value == val.value;
    }

    Str BooleanValue::repr() {
        return this->value ? "true" : "false";
    }

    void ArrayLiteral::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    ASTNodeType ArrayLiteral::astNodeType() const {
        return ASTNodeType::ARRAY_LITERAL;
    }

    Str ArrayLiteral::repr() {
        return "[" + strOfNodeList(elements) + "]";
    }

    bool ArrayLiteral::operator==(const ASTNode &node) const {
        auto &arrayLit = dynamic_cast<const ArrayLiteral&>(node);
        return astNodeType() == arrayLit.astNodeType() &&
                elements.size() == arrayLit.elements.size() &&
                std::equal(begin(elements), end(elements), begin(arrayLit.elements), ASTComparator);
    }

    ArrayLiteral::~ArrayLiteral() {
        for (const auto &element : elements) {
            destroyast(element);
        }
    }

    void FunctionDef::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool FunctionDef::operator==(const ASTNode &node) const {
        auto &funDef = dynamic_cast<const FunctionDef &>(node);
        return astNodeType() == node.astNodeType() &&
               funDef.funName == funName &&
               std::equal(begin(params),
                          end(params),
                          begin(funDef.params),
                          ASTComparator) &&
               *funDef.body == *body;
    }

    FunctionDef::~FunctionDef() {
        for (auto param : params) {
            destroyast(param);
        }
        destroyast(body);
    }

    Str FunctionDef::name() const {
        return funName;
    }

    Str FunctionDef::repr() {
        return "fun " + funName + "(" + strOfNodeList(params) + ")" + body->repr();
    }

    void BinaryExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool BinaryExpression::operator==(const ASTNode &node) const {
        auto &&binexpr = dynamic_cast<const BinaryExpression &>(node);
        return astNodeType() == node.astNodeType() &&
               *binexpr.optr == *optr &&
               *left == *binexpr.left &&
               *right == *binexpr.right;
    }

    BinaryExpression::~BinaryExpression() {
        destroyast(left);
        destroyast(right);
        delete optr;
    }

    Str BinaryExpression::repr() {
        return left->repr() + this->optr->repr + right->repr();
    }

    void IndexAccessorExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    ASTNodeType IndexAccessorExpression::astNodeType() const {
        return ASTNodeType::INDEX_ACCESSOR_EXPRESSION;
    }

    bool IndexAccessorExpression::operator==(const ASTNode &node) const {
        auto& indexAccExpr = dynamic_cast<const IndexAccessorExpression&>(node);

        return *primary == *(indexAccExpr.primary) &&
            *accessor == *(indexAccExpr.accessor);
    }

    Str IndexAccessorExpression::repr() {
        return primary->repr() + "[" + accessor->repr() + "]";
    }

    IndexAccessorExpression::~IndexAccessorExpression() {
        destroyast(primary);
        destroyast(accessor);
    }

    void IndexAssignmentExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    ASTNodeType IndexAssignmentExpression::astNodeType() const {
        return ASTNodeType::INDEX_ASSIGNMENT_EXPRESSION;
    }

    bool IndexAssignmentExpression::operator==(const ASTNode &node) const {
        auto& indexAssExpr = dynamic_cast<const IndexAssignmentExpression&>(node);

        return *primary == *(indexAssExpr.primary) &&
               *accessor == *(indexAssExpr.accessor) &&
               *value == *(indexAssExpr.value);

    }

    Str IndexAssignmentExpression::repr() {
        return primary->repr() + "[" + accessor->repr() + "] = " + value->repr();
    }

    IndexAssignmentExpression::~IndexAssignmentExpression() {
        destroyast(primary);
        destroyast(accessor);
        destroyast(value);
    }

    Str TypeDef::name() const {
        return this->typeName;
    }

    ASTNodeType TypeDef::astNodeType() const {
        return ASTNodeType::TYPE_DEFINITION;
    }

    void TypeDef::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool TypeDef::operator==(const ASTNode &node) const {
        auto&& typeDef = dynamic_cast<const TypeDef&>(node);

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

    Str TypeDef::repr() {
        const Str& propertiesRepr = strOfNodeList(properties, "\n");
        const Str& membersRepr = strOfNodeList(memberFunctions, "\n");

        return "type " + typeName + "{" + propertiesRepr + membersRepr + "}";
    }

    TypeDef::~TypeDef() {
        for (const auto &item : memberFunctions) {
            destroyast(item);
        }

        for (const auto &item : properties) {
            destroyast(item);
        }
    }

    ASTNodeType PropertyDef::astNodeType() const {
        return ASTNodeType::PROPERTY_DEFINITION;
    }

    Str PropertyDef::name() const {
        return propertyName;
    }

    bool PropertyDef::operator==(const ASTNode &node) const {
        auto& property = dynamic_cast<const PropertyDef&>(node);

        return propertyName == property.propertyName;
    }

    void PropertyDef::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    Str PropertyDef::repr() {
        return "property " + propertyName + ";";
    }

    ASTNodeType NewObjectExpression::astNodeType() const {
        return ASTNodeType::NEW_OBJECT_EXPRESSION;
    }

    void NewObjectExpression::accept(IASTVisitor *visitor) {
        visitor->visit(this);
    }

    bool NewObjectExpression::operator==(const ASTNode &node) const {
        auto&& newObj = dynamic_cast<const NewObjectExpression &>(node);
        
        return newObj.typeName == typeName &&
            newObj.properties == properties;
    }

    Str NewObjectExpression::repr() {
        Str props {};

        for (const auto &property : properties) {
            if (!props.empty()) {
                props += ",";
            }

            props += (property.first + ": " + property.second->repr());
        }
        
        return "new " + typeName + " { " + props + " }";
    }

    NewObjectExpression::~NewObjectExpression() {
        for (auto& [_, value] : properties) {
            destroyast(value);
        }
    }
} // namespace NG
