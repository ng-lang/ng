
#include <algorithm>
#include <ast.hpp>
#include <iterator>
#include <token.hpp>
#include <visitor.hpp>

namespace NG::ast
{
  namespace
  {
    auto genericParamsRepr(const Vec<ASTRef<GenericParam>> &params) -> Str
    {
      if (params.empty())
      {
        return "";
      }

      Str result = "<";
      for (size_t i = 0; i < params.size(); ++i)
      {
        if (i > 0)
        {
          result += ", ";
        }
        result += params[i]->repr();
      }
      result += ">";
      return result;
    }
  } // namespace


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
    if (constLiteral)
    {
      return this->name;
    }
    if (genericArgs.empty())
    {
      return this->name;
    }
    Str result = this->name + "<";
    for (size_t i = 0; i < genericArgs.size(); ++i)
    {
      if (i > 0) result += ", ";
      result += genericArgs[i]->repr();
    }
    result += ">";
    return result;
  }

  TypeAnnotation::~TypeAnnotation()
  {
    for (const auto &arg : genericArgs)
    {
      destroyast(arg);
    }
  }

  void GenericParam::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto GenericParam::repr() const -> Str
  {
    if (isConst)
    {
      return "const " + name + ": " + (constType ? constType->repr() : "?");
    }
    Str result = name;
    if (kindArity > 0 || kindVariadicTail)
    {
      result += "<";
      for (size_t i = 0; i < kindArity; ++i)
      {
        if (i > 0) result += ", ";
        result += "_";
      }
      if (kindVariadicTail)
      {
        if (kindArity > 0) result += ", ";
        result += "...";
      }
      result += ">";
    }
    if (isPack) result += "...";
    if (bound) result += ": " + bound->repr();
    return result;
  }

  void TraitBound::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto TraitBound::repr() const -> Str
  {
    if (predicate)
    {
      return predicate->repr();
    }
    return (subject ? subject->repr() : "?") + ": " + (trait ? trait->repr() : "?");
  }

  TraitBound::~TraitBound()
  {
    destroyast(subject);
    destroyast(trait);
    destroyast(predicate);
  }

  void ConstDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto ConstDef::repr() const -> Str
  {
    auto target = specializationPattern ? specializationPattern->repr() : constName + genericParamsRepr(genericParams);
    auto whereRepr = whereBounds.empty() ? "" : " where " + strOfNodeList(whereBounds, " && ");
    auto body = deleted ? Str{"delete"} : (native ? Str{"native"} : (value ? value->repr() : "?"));
    return "const " + (specializationPattern ? genericParamsRepr(genericParams) + " " : "") + target + whereRepr + ": " +
           (returnType ? returnType->repr() : "?") + " = " + body + ";";
  }

  ConstDef::~ConstDef()
  {
    for (const auto &genericParam : genericParams)
    {
      destroyast(genericParam);
    }
    destroyast(specializationPattern);
    for (auto &bound : whereBounds)
    {
      destroyast(bound);
    }
    destroyast(returnType);
    destroyast(value);
  }


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
    return (this->isConst ? "const " : "") +
           std::string{"if ("} + this->testing->repr() + ") {\n" + this->consequence->repr() + "}" +
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
    for (const auto &arg : genericArgs)
    {
      destroyast(arg);
    }
    for (const auto &arg : arguments)
    {
      destroyast(arg);
    }
  }

  auto FunCallExpression::repr() const -> Str
  {
    Str genericRepr{};
    if (!genericArgs.empty())
    {
      genericRepr = "<";
      for (size_t i = 0; i < genericArgs.size(); ++i)
      {
        if (i > 0) genericRepr += ", ";
        genericRepr += genericArgs[i]->repr();
      }
      genericRepr += ">";
    }
    if (!genericArgs.empty() && arguments.empty())
    {
      return this->primaryExpression->repr() + genericRepr;
    }
    return this->primaryExpression->repr() + genericRepr + "(" + strOfNodeList(this->arguments) + ")";
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
        if (!binding->name.empty())
        {
          names.push_back(binding->name);
        }
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
    return this->primaryExpression->repr() + (this->accessor == nullptr ? "" : ("." + this->accessor->repr()));
  }

  void QualifiedTraitCallExpression::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  QualifiedTraitCallExpression::~QualifiedTraitCallExpression()
  {
    destroyast(receiver);
    for (const auto &arg : arguments)
    {
      destroyast(arg);
    }
  }

  auto QualifiedTraitCallExpression::repr() const -> Str
  {
    auto prefix = receiver ? receiver->repr() + "." : "";
    return prefix + traitName + "::" + methodName + "(" + strOfNodeList(arguments) + ")";
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

  void UnitLiteral::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  void FunctionDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  FunctionDef::~FunctionDef()
  {
    for (const auto &genericParam : genericParams)
    {
      destroyast(genericParam);
    }
    for (const auto &param : params)
    {
      destroyast(param);
    }
    for (const auto &bound : whereBounds)
    {
      destroyast(bound);
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
    auto whereRepr = whereBounds.empty() ? "" : " where " + strOfNodeList(whereBounds, " && ");
    return (constEval ? "const " : "") + Str{"fun "} + funName + genericParamsRepr(genericParams) + "(" + strOfNodeList(params) + ")" +
           (returnType ? " -> " + returnType->repr() : "") + whereRepr +
           (deleted ? " = delete;" : (body ? body->repr() : ";"));
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
    if (optr != nullptr &&
        (optr->type == TokenType::KEYWORD_REF || optr->type == TokenType::KEYWORD_MOVE))
    {
      return this->optr->repr + " " + operand->repr();
    }
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

    return "type " + typeName + genericParamsRepr(genericParams) + "{" + propertiesRepr + membersRepr + "}";
  }

  TypeDef::~TypeDef()
  {
    for (const auto &genericParam : genericParams)
    {
      destroyast(genericParam);
    }
    for (const auto &item : memberFunctions)
    {
      destroyast(item);
    }

    for (const auto &item : properties)
    {
      destroyast(item);
    }
  }

  auto TraitDef::names() const -> Vec<Str>
  {
    return Vec<Str>{traitName};
  }

  void TraitDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto TraitDef::repr() const -> Str
  {
    Str supers;
    if (!superTraits.empty())
    {
      supers = ": " + strOfNodeList(superTraits, " + ");
    }
    return "trait " + traitName + genericParamsRepr(genericParams) + supers + "{" + strOfNodeList(methods, "\n") + "}";
  }

  TraitDef::~TraitDef()
  {
    for (const auto &genericParam : genericParams)
    {
      destroyast(genericParam);
    }
    for (const auto &trait : superTraits)
    {
      destroyast(trait);
    }
    for (const auto &method : methods)
    {
      destroyast(method);
    }
  }

  auto ImplDef::names() const -> Vec<Str>
  {
    if (!trait || !targetType)
    {
      return {};
    }
    return Vec<Str>{"impl " + trait->repr() + " for " + targetType->repr()};
  }

  void ImplDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto ImplDef::repr() const -> Str
  {
    auto whereRepr = whereBounds.empty() ? "" : " where " + strOfNodeList(whereBounds, " && ");
    return "impl " + genericParamsRepr(genericParams) + (trait ? trait->repr() : "?") + " for " +
           (targetType ? targetType->repr() : "?") + whereRepr + "{" + strOfNodeList(methods, "\n") + "}";
  }

  ImplDef::~ImplDef()
  {
    for (const auto &genericParam : genericParams)
    {
      destroyast(genericParam);
    }
    destroyast(trait);
    destroyast(targetType);
    for (const auto &bound : whereBounds)
    {
      destroyast(bound);
    }
    for (const auto &method : methods)
    {
      destroyast(method);
    }
  }

  auto UseImplDecl::names() const -> Vec<Str>
  {
    return {};
  }

  void UseImplDecl::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto UseImplDecl::repr() const -> Str
  {
    auto traitName = (moduleQualifier.empty() ? Str{} : moduleQualifier + "::") +
                     (trait ? trait->repr() : "?");
    return "use impl " + traitName + " for " +
           (targetType ? targetType->repr() : "?") + ";";
  }

  UseImplDecl::~UseImplDecl()
  {
    destroyast(trait);
    destroyast(targetType);
  }

  void TypeAliasDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto TypeAliasDef::repr() const -> Str
  {
    auto target = specializationPattern ? specializationPattern->repr() : aliasName + genericParamsRepr(genericParams);
    auto whereRepr = whereBounds.empty() ? "" : "where " + strOfNodeList(whereBounds, " && ");
    auto constraintsRepr = Str{};
    if (!whereRepr.empty())
    {
      constraintsRepr = ": " + whereRepr;
    }
    if (abstract)
    {
      return "type " + target + constraintsRepr + ";";
    }
    auto body = deleted ? Str{"delete"} : (nativeOpaque ? Str{"native"} : (underlyingType ? underlyingType->repr() : "?"));
    return "type " + (specializationPattern ? genericParamsRepr(genericParams) + " " : "") + target + constraintsRepr +
           " = " + body + ";";
  }

  TypeAliasDef::~TypeAliasDef()
  {
    destroyast(specializationPattern);
    for (auto &bound : whereBounds)
    {
      destroyast(bound);
    }
    destroyast(underlyingType);
  }

  void NewTypeDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto NewTypeDef::repr() const -> Str
  {
    return "type " + typeName + genericParamsRepr(genericParams) + " wraps " + (wrappedType ? wrappedType->repr() : "?") + ";";
  }

  NewTypeDef::~NewTypeDef()
  {
    destroyast(wrappedType);
  }

  void CastExpression::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto CastExpression::repr() const -> Str
  {
    return "cast<" + (targetType ? targetType->repr() : "?") + ">(" + (expression ? expression->repr() : "?") + ")";
  }

  CastExpression::~CastExpression()
  {
    destroyast(expression);
    destroyast(targetType);
  }

  void TaggedUnionDef::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto TaggedUnionDef::repr() const -> Str
  {
    Str out = "type " + typeName + genericParamsRepr(genericParams) + " = ";
    for (size_t i = 0; i < variants.size(); ++i) {
      if (i > 0) out += " | ";
      out += variants[i].variantName;
      out += "(";
      for (size_t j = 0; j < variants[i].payloadTypes.size(); ++j) {
        if (j > 0) out += ", ";
        out += variants[i].payloadTypes[j]->repr();
      }
      out += ")";
    }
    return out;
  }

  TaggedUnionDef::~TaggedUnionDef()
  {
    for (auto &gp : genericParams)
    {
      destroyast(gp);
    }
    for (auto &variant : variants)
    {
      for (auto &payloadType : variant.payloadTypes)
      {
        destroyast(payloadType);
      }
    }
  }

  void TaggedValueExpression::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto TaggedValueExpression::repr() const -> Str
  {
    Str out = variantName + "(";
    for (size_t i = 0; i < payload.size(); ++i) {
      if (i > 0) out += ", ";
      out += payload[i]->repr();
    }
    out += ")";
    return out;
  }

  TaggedValueExpression::~TaggedValueExpression()
  {
    for (auto &expr : payload) destroyast(expr);
  }

  void SwitchStatement::accept(AstVisitor *visitor)
  {
    visitor->visit(this);
  }

  auto SwitchStatement::repr() const -> Str
  {
    Str out = "switch (" + scrutinee->repr() + ") { ";
    for (const auto &c : cases) {
      out += "case " + c.variantName + "(";
      for (size_t i = 0; i < c.bindings.size(); ++i) {
        if (i > 0) out += ", ";
        out += c.bindings[i];
      }
      out += ") " + c.body->repr() + " ";
    }
    out += "}";
    return out;
  }

  SwitchStatement::~SwitchStatement()
  {
    destroyast(scrutinee);
    for (auto &c : cases) destroyast(c.body);
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

    const Str target = targetType ? targetType->repr() : typeName;
    return "new " + target + " { " + props + " }";
  }

  NewObjectExpression::~NewObjectExpression()
  {
    destroyast(targetType);
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
  }

  auto Binding::repr() const -> Str
  {
    return (spreadReceiver ? "..." + name : name) + (annotation ? (": " + annotation->repr()) : "");
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
    const char open = (this->type == BindingType::TUPLE_UNPACK   ? '('
                       : this->type == BindingType::ARRAY_UNPACK ? '['
                                                                 : '\0');
    const char close = (this->type == BindingType::TUPLE_UNPACK   ? ')'
                        : this->type == BindingType::ARRAY_UNPACK ? ']'
                                                                  : '\0');
    Str out = "val ";
    if (open != '\0')
    {
      out += open;
    }
    out += strOfNodeList(this->bindings, ", ");
    if (close != '\0')
    {
      out += close;
    }
    if (this->value != nullptr)
    {
      out += " = " + this->value->repr();
    }
    out += ";";
    return out;
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

} // namespace NG::ast
