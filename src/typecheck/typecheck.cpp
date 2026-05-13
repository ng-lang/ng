
#include <debug.hpp>
#include <token.hpp>
#include <typecheck/typecheck.hpp>
#include <parser.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <variant>
namespace NG::typecheck
{
  using namespace NG::ast;

  constexpr inline bool isIntegralType(typeinfo_tag tag) noexcept
  {
    auto c = code(tag);
    return c >= code(typeinfo_tag::SIGNED) && c < code(typeinfo_tag::FLOATING_POINT);
  }
  constexpr inline bool isSigned(typeinfo_tag tag) noexcept
  {
    auto c = code(tag);
    return (c & 0xF0) == code(typeinfo_tag::SIGNED);
  }
  constexpr inline bool isPrimitive(typeinfo_tag tag) noexcept
  {
    auto c = code(tag);
    return c >= code(typeinfo_tag::PRIMITIVES) && c < code(typeinfo_tag::COLLECTION_TYPE);
  }
  constexpr inline bool isFloatingPoint(typeinfo_tag tag) noexcept
  {
    auto c = code(tag);
    return (c & 0xF0) == code(typeinfo_tag::FLOATING_POINT);
  }

  // Unwrap TypeAliasType to get the underlying concrete type
  inline const TypeInfo &unwrapAlias(const TypeInfo &t)
  {
    if (auto alias = dynamic_cast<const TypeAliasType *>(&t))
    {
      return unwrapAlias(*alias->underlyingType);
    }
    return t;
  }

  // Match types with alias transparency: unwrap aliases on both sides before matching
  inline bool typeMatch(const TypeInfo &a, const TypeInfo &b)
  {
    const auto &ua = unwrapAlias(a);
    const auto &ub = unwrapAlias(b);

    // Allow unit literal to match any custom (struct) type — acts like null
    auto isUnit = [](const TypeInfo &t) {
      if (auto p = dynamic_cast<const PrimitiveType *>(&t))
        return p->tag() == typeinfo_tag::UNIT;
      return false;
    };
    if (isUnit(ua) && dynamic_cast<const CustomizedType *>(&ub)) return true;
    if (isUnit(ub) && dynamic_cast<const CustomizedType *>(&ua)) return true;

    if (auto custom = dynamic_cast<const CustomizedType *>(&ua))
    {
      if (auto ref = dynamic_cast<const ReferenceType *>(&ub); ref && ref->referencedType)
      {
        return custom->match(*ref->referencedType);
      }
    }
    if (auto custom = dynamic_cast<const CustomizedType *>(&ub))
    {
      if (auto ref = dynamic_cast<const ReferenceType *>(&ua); ref && ref->referencedType)
      {
        return custom->match(*ref->referencedType);
      }
    }

    return ua.match(ub);
  }

  using ConstValue = std::variant<bool, int64_t, Str>;

  static auto unwrap(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>
  {
    if (type && type->tag() == typeinfo_tag::PARAM_WITH_DEFAULT_VALUE)
    {
      return static_cast<ParamWithDefaultValueType &>(*type).paramType;
    }
    return type;
  }

  static auto deref_reference_type(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>
  {
    if (auto refType = std::dynamic_pointer_cast<ReferenceType>(unwrap(type)))
    {
      return refType->referencedType;
    }
    return type;
  }

  static auto isReferenceableExpression(const Expression *expr) -> bool
  {
    return dynamic_cast<const IdExpression *>(expr) != nullptr ||
           dynamic_cast<const IdAccessorExpression *>(expr) != nullptr ||
           dynamic_cast<const IndexAccessorExpression *>(expr) != nullptr ||
           (dynamic_cast<const UnaryExpression *>(expr) != nullptr &&
            dynamic_cast<const UnaryExpression *>(expr)->optr != nullptr &&
            dynamic_cast<const UnaryExpression *>(expr)->optr->type == TokenType::TIMES);
  }

  static auto isMovableExpression(const Expression *expr) -> bool
  {
    if (dynamic_cast<const IdExpression *>(expr) != nullptr)
    {
      return true;
    }
    auto unaryExpr = dynamic_cast<const UnaryExpression *>(expr);
    return unaryExpr != nullptr && unaryExpr->optr != nullptr && unaryExpr->optr->type == TokenType::TIMES;
  }

  static auto scopeNames(const Map<Str, CheckingRef<TypeInfo>> &scope) -> Set<Str>
  {
    Set<Str> names;
    for (const auto &[name, _] : scope)
    {
      names.insert(name);
    }
    return names;
  }

  static auto filterMovedBindings(const Set<Str> &moved, const Set<Str> &allowed) -> Set<Str>
  {
    Set<Str> filtered;
    for (const auto &name : moved)
    {
      if (allowed.contains(name))
      {
        filtered.insert(name);
      }
    }
    return filtered;
  }

  struct TaggedVariantLookup
  {
    CheckingRef<TaggedUnionType> unionType;
    Vec<CheckingRef<TypeInfo>> payloadTypes;
    Vec<Str> payloadNames;
  };

  static auto findTaggedVariant(const Map<Str, CheckingRef<TypeInfo>> &locals, const Str &variantName)
      -> std::optional<TaggedVariantLookup>
  {
    for (const auto &[_, type] : locals)
    {
      auto unionType = std::dynamic_pointer_cast<TaggedUnionType>(type);
      if (!unionType || !unionType->variants.contains(variantName))
      {
        continue;
      }

      Vec<Str> payloadNames;
      if (unionType->variantPayloadNames.contains(variantName))
      {
        payloadNames = unionType->variantPayloadNames.at(variantName);
      }

      return TaggedVariantLookup{
          .unionType = unionType,
          .payloadTypes = unionType->variants.at(variantName),
          .payloadNames = payloadNames,
      };
    }

    return std::nullopt;
  }

  static auto widenVariantToUnionType(const Map<Str, CheckingRef<TypeInfo>> &locals, CheckingRef<TypeInfo> type)
      -> CheckingRef<TypeInfo>
  {
    auto unwrapped = unwrap(type);
    auto variantType = std::dynamic_pointer_cast<VariantType>(unwrapped);
    if (!variantType)
    {
      return type;
    }

    auto it = locals.find(variantType->unionName);
    if (it != locals.end() && it->second && it->second->tag() == typeinfo_tag::TAGGED_UNION)
    {
      return it->second;
    }

    return type;
  }

  static auto formatTypeInstanceName(const Str &baseName, const Vec<CheckingRef<TypeInfo>> &args) -> Str
  {
    auto safeTypeName = [](const CheckingRef<TypeInfo> &type, const auto &self) -> Str {
      if (!type)
      {
        return "?";
      }
      if (auto tagged = std::dynamic_pointer_cast<TaggedUnionType>(type))
      {
        return tagged->name;
      }
      if (auto variant = std::dynamic_pointer_cast<VariantType>(type))
      {
        return variant->unionName + "." + variant->variantName;
      }
      if (auto custom = std::dynamic_pointer_cast<CustomizedType>(type))
      {
        return custom->name;
      }
      if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(type))
      {
        return alias->name;
      }
      if (auto newType = std::dynamic_pointer_cast<NewTypeType>(type))
      {
        return newType->name;
      }
      if (auto ref = std::dynamic_pointer_cast<ReferenceType>(type))
      {
        return "ref<" + self(ref->referencedType, self) + ">";
      }
      if (auto array = std::dynamic_pointer_cast<ArrayType>(type))
      {
        return self(array->elementType, self) + " array";
      }
      if (auto tuple = std::dynamic_pointer_cast<TupleType>(type))
      {
        Str out = "(";
        for (size_t i = 0; i < tuple->elementTypes.size(); ++i)
        {
          if (i > 0)
          {
            out += ", ";
          }
          out += self(tuple->elementTypes[i], self);
        }
        return out + ")";
      }
      return type->repr();
    };

    Str result = baseName + "<";
    for (size_t i = 0; i < args.size(); ++i)
    {
      if (i > 0)
      {
        result += ", ";
      }
      result += safeTypeName(args[i], safeTypeName);
    }
    result += ">";
    return result;
  }

  static auto stripTypeInstanceSuffix(const Str &typeName) -> Str
  {
    auto genericStart = typeName.find('<');
    if (genericStart == Str::npos)
    {
      return typeName;
    }
    return typeName.substr(0, genericStart);
  }

  static auto typeKindName(const TypeInfo &type) -> Str
  {
    switch (type.tag())
    {
    case typeinfo_tag::BOOL:
      return "bool";
    case typeinfo_tag::STRING:
      return "string";
    case typeinfo_tag::ARRAY:
      return "array";
    case typeinfo_tag::TUPLE:
      return "tuple";
    case typeinfo_tag::FUNCTION:
      return "function";
    case typeinfo_tag::CUSTOMIZED:
      return "object";
    case typeinfo_tag::TYPE_ALIAS:
      return "alias";
    case typeinfo_tag::NEW_TYPE:
      return "newtype";
    case typeinfo_tag::TAGGED_UNION:
      return "tagged_union";
    case typeinfo_tag::VARIANT:
      return "variant";
    case typeinfo_tag::UNION:
      return "union";
    case typeinfo_tag::GENERIC_PARAM:
      return "generic_param";
    default:
      if (isPrimitive(type.tag()))
      {
        return "primitive";
      }
      return "type";
    }
  }

  struct TypeChecker : DummyVisitor
  {
    Map<Str, CheckingRef<TypeInfo>> type_index{};

    Map<Str, CheckingRef<TypeInfo>> locals{};

    CheckingRef<TypeInfo> result;

    Vec<CheckingRef<TypeInfo>> spreadResult{};

    Vec<CheckingRef<TypeInfo>> contextRequirement;

    CheckingRef<TypeInfo> expectedType; // For bidirectional type inference

    Set<Str> movedBindings{};

    bool allowMovedLvalueRead = false;

    // Sentinel key stored in locals to indicate wildcard imports are active.
    // This propagates automatically when locals are copied to child checkers.
    static constexpr const char *WILDCARD_IMPORT_KEY = "$$wildcard_import$$";

    explicit TypeChecker(Map<Str, CheckingRef<TypeInfo>> locals, Vec<CheckingRef<TypeInfo>> contextRequirement = {},
                         CheckingRef<TypeInfo> expectedType = nullptr, Set<Str> movedBindings = {},
                         bool allowMovedLvalueRead = false)
        : locals(std::move(locals)), contextRequirement(std::move(contextRequirement)), expectedType(std::move(expectedType)),
          movedBindings(std::move(movedBindings)), allowMovedLvalueRead(allowMovedLvalueRead)
    {
    }

    bool hasWildcardImportFlag() const { return locals.contains(WILDCARD_IMPORT_KEY); }

    auto instantiateGenericType(GenericTypeDef &genericDef,
                                const Vec<CheckingRef<TypeInfo>> &typeArgs) -> CheckingRef<TypeInfo>
    {
      if (std::any_of(genericDef.typeParamIsPack.begin(), genericDef.typeParamIsPack.end(), [](bool isPack) {
            return isPack;
          }))
      {
        throw TypeCheckingException("Generic type parameter packs are not supported for type declarations yet");
      }

      if (typeArgs.size() != genericDef.typeParamNames.size())
      {
        throw TypeCheckingException("Generic type '" + genericDef.name + "' expects " +
                                    std::to_string(genericDef.typeParamNames.size()) + " type arguments, got " +
                                    std::to_string(typeArgs.size()));
      }

      Str instanceName = formatTypeInstanceName(genericDef.name, typeArgs);
      if (genericDef.instances.contains(instanceName))
      {
        return genericDef.instances.at(instanceName);
      }

      Map<Str, CheckingRef<TypeInfo>> instLocals = genericDef.capturedLocals;
      for (size_t i = 0; i < genericDef.typeParamNames.size(); ++i)
      {
        instLocals[genericDef.typeParamNames[i]] = typeArgs[i];
      }

      switch (genericDef.kind)
      {
      case GenericTypeKind::TYPE_DEF:
      {
        auto customType = makecheck<CustomizedType>(instanceName);
        genericDef.instances[instanceName] = customType;

        TypeChecker checker{instLocals};
        for (auto &&prop : genericDef.typeDef->properties)
        {
          prop->accept(&checker);
          customType->properties[prop->propertyName] = checker.result;
        }

        for (auto &&memFn : genericDef.typeDef->memberFunctions)
        {
          Vec<CheckingRef<TypeInfo>> paramTypes;
          paramTypes.push_back(customType);
          for (auto &&param : memFn->params)
          {
            param->accept(&checker);
            paramTypes.push_back(checker.result);
          }

          CheckingRef<TypeInfo> returnType;
          if (memFn->returnType)
          {
            memFn->returnType->accept(&checker);
            returnType = checker.result;
          }
          else
          {
            returnType = makecheck<Untyped>();
          }

          auto funType = makecheck<FunctionType>(returnType, paramTypes);
          customType->memberFunctions[memFn->funName] = funType;

          TypeChecker bodyChecker{instLocals};
          bodyChecker.locals.insert_or_assign("self", customType);
          for (auto &&[name, type] : customType->properties)
          {
            bodyChecker.locals.insert_or_assign(name, type);
          }
          for (size_t i = 0; i < memFn->params.size(); ++i)
          {
            bodyChecker.locals.insert_or_assign(memFn->params[i]->paramName, unwrap(funType->parametersType[i + 1]));
          }
          bodyChecker.contextRequirement = funType->parametersType;

          if (memFn->body)
          {
            memFn->body->accept(&bodyChecker);
            auto bodyReturnType = bodyChecker.result ? bodyChecker.result : makecheck<PrimitiveType>(typeinfo_tag::UNIT);
            if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && returnType->tag() != typeinfo_tag::UNTYPED &&
                !typeMatch(*returnType, *bodyReturnType))
            {
              throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                              returnType->repr(),
                                          memFn->pos);
            }
          }
        }

        return customType;
      }
      case GenericTypeKind::TYPE_ALIAS:
      {
        auto aliasType = makecheck<TypeAliasType>(instanceName, makecheck<Untyped>());
        genericDef.instances[instanceName] = aliasType;
        TypeChecker checker{instLocals};
        genericDef.typeAliasDef->underlyingType->accept(&checker);
        aliasType->underlyingType = checker.result;
        return aliasType;
      }
      case GenericTypeKind::NEW_TYPE:
      {
        auto newType = makecheck<NewTypeType>(instanceName, makecheck<Untyped>());
        genericDef.instances[instanceName] = newType;
        TypeChecker checker{instLocals};
        genericDef.newTypeDef->wrappedType->accept(&checker);
        newType->wrappedType = checker.result;
        return newType;
      }
      case GenericTypeKind::TAGGED_UNION:
      {
        auto unionType = makecheck<TaggedUnionType>(instanceName);
        genericDef.instances[instanceName] = unionType;
        TypeChecker checker{instLocals};
        for (auto &variant : genericDef.taggedUnionDef->variants)
        {
          Vec<CheckingRef<TypeInfo>> payloadTypes;
          for (auto &payloadType : variant.payloadTypes)
          {
            payloadType->accept(&checker);
            payloadTypes.push_back(checker.result);
          }
          unionType->variants[variant.variantName] = payloadTypes;
          if (!variant.payloadNames.empty())
          {
            unionType->variantPayloadNames[variant.variantName] = variant.payloadNames;
          }
        }
        return unionType;
      }
      }

      throw TypeCheckingException("Unsupported generic type declaration: " + genericDef.name);
    }

    auto typeQueryPropertyType(CheckingRef<TypeInfo> inspectedType, const Str &memberName) -> CheckingRef<TypeInfo>
    {
      if (!inspectedType)
      {
        return makecheck<Untyped>();
      }

      if (memberName == "name" || memberName == "kind")
      {
        return makecheck<PrimitiveType>(typeinfo_tag::STRING);
      }
      if (memberName == "size")
      {
        switch (inspectedType->tag())
        {
        case typeinfo_tag::TUPLE:
        case typeinfo_tag::CUSTOMIZED:
        case typeinfo_tag::TAGGED_UNION:
        case typeinfo_tag::VARIANT:
        case typeinfo_tag::UNION:
          return makecheck<PrimitiveType>(typeinfo_tag::U32);
        default:
          break;
        }
      }
      if (memberName == "fieldCount")
      {
        switch (inspectedType->tag())
        {
        case typeinfo_tag::TUPLE:
        case typeinfo_tag::CUSTOMIZED:
        case typeinfo_tag::VARIANT:
          return makecheck<PrimitiveType>(typeinfo_tag::U32);
        default:
          break;
        }
      }
      if (memberName == "variantCount" && inspectedType->tag() == typeinfo_tag::TAGGED_UNION)
      {
        return makecheck<PrimitiveType>(typeinfo_tag::U32);
      }

      throw TypeCheckingException("Unknown type query property '" + memberName + "' for type " + inspectedType->repr());
    }

    auto typeQueryPropertyValue(CheckingRef<TypeInfo> inspectedType, const Str &memberName) -> std::optional<ConstValue>
    {
      if (!inspectedType)
      {
        return std::nullopt;
      }

      if (memberName == "name")
      {
        return inspectedType->repr();
      }
      if (memberName == "kind")
      {
        return typeKindName(*inspectedType);
      }
      if (memberName == "size")
      {
        switch (inspectedType->tag())
        {
        case typeinfo_tag::TUPLE:
          return static_cast<int64_t>(static_cast<TupleType &>(*inspectedType).elementTypes.size());
        case typeinfo_tag::CUSTOMIZED:
          return static_cast<int64_t>(static_cast<CustomizedType &>(*inspectedType).properties.size());
        case typeinfo_tag::TAGGED_UNION:
          return static_cast<int64_t>(static_cast<TaggedUnionType &>(*inspectedType).variants.size());
        case typeinfo_tag::VARIANT:
          return static_cast<int64_t>(static_cast<VariantType &>(*inspectedType).payloadTypes.size());
        case typeinfo_tag::UNION:
          return static_cast<int64_t>(static_cast<UnionType &>(*inspectedType).types.size());
        default:
          return std::nullopt;
        }
      }
      if (memberName == "fieldCount")
      {
        switch (inspectedType->tag())
        {
        case typeinfo_tag::TUPLE:
          return static_cast<int64_t>(static_cast<TupleType &>(*inspectedType).elementTypes.size());
        case typeinfo_tag::CUSTOMIZED:
          return static_cast<int64_t>(static_cast<CustomizedType &>(*inspectedType).properties.size());
        case typeinfo_tag::VARIANT:
          return static_cast<int64_t>(static_cast<VariantType &>(*inspectedType).payloadTypes.size());
        default:
          return std::nullopt;
        }
      }
      if (memberName == "variantCount" && inspectedType->tag() == typeinfo_tag::TAGGED_UNION)
      {
        return static_cast<int64_t>(static_cast<TaggedUnionType &>(*inspectedType).variants.size());
      }

      return std::nullopt;
    }

    auto tryEvalConstValue(Expression *expr) -> std::optional<ConstValue>
    {
      if (auto *boolVal = dynamic_cast<BooleanValue *>(expr))
      {
        return boolVal->value;
      }
      if (auto *intVal = dynamic_cast<IntegralValue<int32_t> *>(expr))
      {
        return static_cast<int64_t>(intVal->value);
      }
      if (auto *intVal = dynamic_cast<IntegralValue<int64_t> *>(expr))
      {
        return static_cast<int64_t>(intVal->value);
      }
      if (auto *intVal = dynamic_cast<IntegralValue<uint32_t> *>(expr))
      {
        return static_cast<int64_t>(intVal->value);
      }
      if (auto *intVal = dynamic_cast<IntegralValue<uint64_t> *>(expr))
      {
        return static_cast<int64_t>(intVal->value);
      }
      if (auto *strVal = dynamic_cast<StringValue *>(expr))
      {
        return strVal->value;
      }
      if (auto *typeCheck = dynamic_cast<TypeCheckingExpression *>(expr))
      {
        TypeChecker valueChecker{locals};
        typeCheck->value->accept(&valueChecker);
        auto valueType = valueChecker.result;
        TypeChecker annoChecker{locals};
        typeCheck->type->accept(&annoChecker);
        auto targetType = annoChecker.result;
        if (!valueType || !targetType || valueType->tag() == typeinfo_tag::UNTYPED ||
            valueType->tag() == typeinfo_tag::GENERIC_PARAM)
        {
          return std::nullopt;
        }
        return typeMatch(*valueType, *targetType);
      }
      if (auto *typeOfExpr = dynamic_cast<TypeOfExpression *>(expr))
      {
        TypeChecker checker{locals};
        typeOfExpr->expression->accept(&checker);
        if (!checker.result || checker.result->tag() == typeinfo_tag::UNTYPED)
        {
          return std::nullopt;
        }
        return checker.result->repr();
      }
      if (auto *idAccessor = dynamic_cast<IdAccessorExpression *>(expr))
      {
        if (auto *typeOfExpr = dynamic_cast<TypeOfExpression *>(idAccessor->primaryExpression.get()))
        {
          TypeChecker checker{locals};
          typeOfExpr->expression->accept(&checker);
          if (!checker.result || checker.result->tag() == typeinfo_tag::UNTYPED)
          {
            return std::nullopt;
          }
          return typeQueryPropertyValue(checker.result, idAccessor->accessor->repr());
        }
      }
      if (auto *unaryExpr = dynamic_cast<UnaryExpression *>(expr))
      {
        auto operand = tryEvalConstValue(unaryExpr->operand.get());
        if (operand.has_value() && unaryExpr->optr && unaryExpr->optr->type == TokenType::NOT &&
            std::holds_alternative<bool>(*operand))
        {
          return !std::get<bool>(*operand);
        }
        return std::nullopt;
      }
      if (auto *binaryExpr = dynamic_cast<BinaryExpression *>(expr))
      {
        auto left = tryEvalConstValue(binaryExpr->left.get());
        auto right = tryEvalConstValue(binaryExpr->right.get());
        if (!left.has_value() || !right.has_value())
        {
          return std::nullopt;
        }

        switch (binaryExpr->optr->type)
        {
        case TokenType::AND:
          if (std::holds_alternative<bool>(*left) && std::holds_alternative<bool>(*right))
          {
            return std::get<bool>(*left) && std::get<bool>(*right);
          }
          break;
        case TokenType::OR:
          if (std::holds_alternative<bool>(*left) && std::holds_alternative<bool>(*right))
          {
            return std::get<bool>(*left) || std::get<bool>(*right);
          }
          break;
        case TokenType::EQUAL:
          if (left->index() == right->index())
          {
            return *left == *right;
          }
          break;
        case TokenType::NOT_EQUAL:
          if (left->index() == right->index())
          {
            return *left != *right;
          }
          break;
        default:
          break;
        }
      }
      return std::nullopt;
    }

    auto tryEvalConstCondition(Expression *expr) -> std::optional<bool>
    {
      auto value = tryEvalConstValue(expr);
      if (value.has_value() && std::holds_alternative<bool>(*value))
      {
        return std::get<bool>(*value);
      }
      return std::nullopt;
    }

    void visit(CompileUnit *compileUnit) override { compileUnit->module->accept(this); }

    void visit(Module *module) override
    {
      // First pass: collect function signatures and type definitions
      for (auto def : module->definitions)
      {
        if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
        {
          // Check if this is a generic function (has type parameters)
          if (!funDef->genericParams.empty())
          {
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : funDef->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            auto genericDef = makecheck<GenericDefType>(
                funDef->funName, typeParamNames, typeParamIsPack, funDef, locals);
            locals[funDef->funName] = genericDef;

            // Register generic type params in a temporary scope so parameter
            // type annotations (e.g. `T array`) can resolve them during
            // monomorphization later.  We don't need to fully type-check the
            // body here, but the params must be visible for annotation parsing.
            {
              Map<Str, CheckingRef<TypeInfo>> genericLocals = locals;
              for (auto &gp : funDef->genericParams)
              {
                genericLocals[gp->name] = makecheck<GenericParamType>(gp->name, "", gp->isPack);
              }
              for (auto param : funDef->params)
              {
                if (param->annotatedType)
                {
                  TypeChecker annoChecker{genericLocals};
                  param->annotatedType->accept(&annoChecker);
                  // Result is discarded — we just need to validate the annotation
                  // is resolvable.  If it throws, the test catches it.
                }
              }
            }
            continue; // Skip normal function type registration
          }

          Vec<CheckingRef<TypeInfo>> paramTypes;
          TypeChecker checker{locals};
          for (auto param : funDef->params)
          {
            param->accept(&checker);
            paramTypes.push_back(checker.result);
          }

          CheckingRef<TypeInfo> returnType = makecheck<Untyped>();
          if (funDef->returnType)
          {
            TypeChecker checker{locals};
            funDef->returnType->accept(&checker);
            returnType = checker.result;
          }

          auto funcType = makecheck<FunctionType>(returnType, paramTypes);
          locals[funDef->funName] = funcType;
        }
        else if (auto typeAlias = dynamic_ast_cast<TypeAliasDef>(def))
        {
          if (!typeAlias->genericParams.empty())
          {
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : typeAlias->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            locals[typeAlias->aliasName] =
                makecheck<GenericTypeDef>(typeAlias->aliasName, typeParamNames, typeParamIsPack, typeAlias, locals);
          }
          else
          {
            TypeChecker checker{locals};
            typeAlias->underlyingType->accept(&checker);
            auto aliasType = makecheck<TypeAliasType>(typeAlias->aliasName, checker.result);
            locals.insert_or_assign(typeAlias->aliasName, aliasType);
          }
        }
        else if (auto newTypeDef = dynamic_ast_cast<NewTypeDef>(def))
        {
          if (!newTypeDef->genericParams.empty())
          {
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : newTypeDef->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            locals[newTypeDef->typeName] =
                makecheck<GenericTypeDef>(newTypeDef->typeName, typeParamNames, typeParamIsPack, newTypeDef, locals);
          }
          else
          {
            TypeChecker checker{locals};
            newTypeDef->wrappedType->accept(&checker);
            auto ntType = makecheck<NewTypeType>(newTypeDef->typeName, checker.result);
            locals.insert_or_assign(newTypeDef->typeName, ntType);
          }
        }
        else if (auto typeDef = dynamic_ast_cast<TypeDef>(def))
        {
          if (!typeDef->genericParams.empty())
          {
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : typeDef->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            locals[typeDef->typeName] =
                makecheck<GenericTypeDef>(typeDef->typeName, typeParamNames, typeParamIsPack, typeDef, locals);
          }
          else
          {
            auto customType = makecheck<CustomizedType>(typeDef->typeName);
            locals.insert_or_assign(typeDef->typeName, customType);
          }
        }
        else if (auto taggedUnion = dynamic_ast_cast<TaggedUnionDef>(def))
        {
          if (!taggedUnion->genericParams.empty())
          {
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : taggedUnion->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            auto genericDef = makecheck<GenericTypeDef>(taggedUnion->typeName, typeParamNames,
                                                        typeParamIsPack, taggedUnion, locals);
            locals[taggedUnion->typeName] = genericDef;
            genericDef->capturedLocals = locals;
          }
          else
          {
            auto tuType = makecheck<TaggedUnionType>(taggedUnion->typeName);
            locals.insert_or_assign(taggedUnion->typeName, tuType);
            TypeChecker checker{locals};
            for (int32_t i = 0; i < static_cast<int32_t>(taggedUnion->variants.size()); ++i)
            {
              auto &v = taggedUnion->variants[i];
              Vec<CheckingRef<TypeInfo>> payloadTypes;
              for (auto &pt : v.payloadTypes)
              {
                pt->accept(&checker);
                payloadTypes.push_back(checker.result);
              }
              tuType->variants[v.variantName] = payloadTypes;
              if (!v.payloadNames.empty())
              {
                tuType->variantPayloadNames[v.variantName] = v.payloadNames;
              }
            }
          }
        }
      }

      // Process import declarations first
      for (auto imp : module->imports)
      {
        imp->accept(this);
      }
      for (auto def : module->definitions)
      {
        def->accept(this);
      }
      for (auto stmt : module->statements)
      {
        stmt->accept(this);
      }
      type_index.merge(locals);
      result = makecheck<Untyped>();
    }

    void visit(TypeDef *typeDef) override
    {
      if (!typeDef->genericParams.empty())
      {
        return;
      }

      auto customType = std::dynamic_pointer_cast<CustomizedType>(locals[typeDef->typeName]);
      TypeChecker checker{locals};

      for (auto &&prop : typeDef->properties)
      {
        prop->accept(&checker);
        customType->properties[prop->propertyName] = checker.result;
      }

      for (auto &&memFn : typeDef->memberFunctions)
      {
        // First parameter is implicitly 'self'
        Vec<CheckingRef<TypeInfo>> paramTypes;
        paramTypes.push_back(customType);
        for (auto &&param : memFn->params)
        {
          param->accept(&checker);
          paramTypes.push_back(checker.result);
        }

        CheckingRef<TypeInfo> returnType;
        if (memFn->returnType)
        {
          memFn->returnType->accept(&checker);
          returnType = checker.result;
        }
        else
        {
          returnType = makecheck<Untyped>();
        }

        auto funType = makecheck<FunctionType>(returnType, paramTypes);
        customType->memberFunctions[memFn->funName] = funType;

        // Check member function body
        TypeChecker bodyChecker{locals};
        bodyChecker.locals.insert_or_assign("self", customType);
        // Flatten properties into body scope
        for (auto &&[name, type] : customType->properties)
        {
          bodyChecker.locals.insert_or_assign(name, type);
        }
        for (size_t i = 0; i < memFn->params.size(); ++i)
        {
          bodyChecker.locals.insert_or_assign(memFn->params[i]->paramName,
                                              unwrap(funType->parametersType[i + 1]));
        }
        bodyChecker.contextRequirement = funType->parametersType;

        if (memFn->body)
        {
          memFn->body->accept(&bodyChecker);
          auto bodyReturnType = bodyChecker.result;
          if (!bodyReturnType)
          {
            bodyReturnType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
          }
          if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && returnType->tag() != typeinfo_tag::UNTYPED &&
              !typeMatch(*returnType, *bodyReturnType))
          {
            throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                            returnType->repr(),
                                        memFn->pos);
          }
        }
      }
    }

    void visit(PropertyDef *prop) override
    {
      if (prop->typeAnnotation)
      {
        TypeChecker checker{locals};
        prop->typeAnnotation->accept(&checker);
        result = checker.result;
      }
      else
      {
        result = makecheck<Untyped>();
      }
    }

    void visit(ValDef *valDef) override { valDef->body->accept(this); }

    void visit(FunctionDef *funDef) override
    {
      // Skip generic functions — already registered as GenericDefType in Module first pass
      if (!funDef->genericParams.empty())
      {
        return;
      }

      CheckingRef<TypeInfo> funType;
      if (auto it = locals.find(funDef->funName); it != locals.end())
      {
        funType = it->second;
      }
      else
      {
        TypeChecker checker{locals};
        Vec<CheckingRef<TypeInfo>> paramTypes;
        for (auto param : funDef->params)
        {
          param->accept(&checker);
          paramTypes.push_back(checker.result);
        }
        CheckingRef<TypeInfo> returnType;
        if (funDef->returnType)
        {
          funDef->returnType->accept(&checker);
          returnType = checker.result;
        }
        else
        {
          returnType = makecheck<Untyped>();
        }
        funType = makecheck<FunctionType>(returnType, paramTypes);
        if (!funDef->funName.empty())
        {
          locals.insert_or_assign(funDef->funName, funType);
        }
      }

      auto &funcInfo = static_cast<FunctionType &>(*funType);
      // Pass return type as expectedType for bidirectional inference
      CheckingRef<TypeInfo> bodyExpectedType = nullptr;
      if (funcInfo.returnType->tag() != typeinfo_tag::UNTYPED)
      {
        bodyExpectedType = funcInfo.returnType;
      }
      TypeChecker bodyChecker{locals, {}, bodyExpectedType};
      for (size_t i = 0; i < funDef->params.size(); ++i)
      {
        bodyChecker.locals.insert_or_assign(funDef->params[i]->paramName, unwrap(funcInfo.parametersType[i]));
      }
      bodyChecker.contextRequirement = funcInfo.parametersType;

      if (funDef->body)
      {
        funDef->body->accept(&bodyChecker);
        auto bodyReturnType = bodyChecker.result;
        if (!bodyReturnType)
        {
          bodyReturnType = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
        }
        if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && funcInfo.returnType->tag() != typeinfo_tag::UNTYPED &&
            !typeMatch(*funcInfo.returnType, *bodyReturnType))
        {
          throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                          funcInfo.returnType->repr(),
                                      funDef->pos);
        }
      }
      result = funType;
    }

    void visit(SimpleStatement *simpleStatement) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      simpleStatement->expression->accept(&checker);
      movedBindings = checker.movedBindings;
      if (auto *assignmentExpr = dynamic_cast<AssignmentExpression *>(simpleStatement->expression.get()))
      {
        if (auto *idTarget = dynamic_cast<IdExpression *>(assignmentExpr->target.get()))
        {
          movedBindings.erase(idTarget->id);
        }
      }
    }

    void visit(CompoundStatement *compoundStatement) override
    {
      auto outerNames = scopeNames(locals);
      TypeChecker checker{locals, contextRequirement, expectedType, movedBindings};
      CheckingRef<TypeInfo> returnType = nullptr;
      for (auto stmt : compoundStatement->statements)
      {
        checker.result = nullptr;
        stmt->accept(&checker);
        if (checker.result)
        {
          if (returnType)
          {
            if (!typeMatch(*returnType, *checker.result))
            {
              if (typeMatch(*checker.result, *returnType))
              {
                returnType = checker.result;
              }
              else
              {
                throw TypeCheckingException("Mismatched return types in compound statement: " + returnType->repr() +
                                                ", " + checker.result->repr(),
                                            compoundStatement->pos);
              }
            }
          }
          else
          {
            returnType = checker.result;
          }
        }
      }
      movedBindings = filterMovedBindings(checker.movedBindings, outerNames);
      result = returnType;
    }

    void visit(ReturnStatement *returnStatement) override
    {
      if (returnStatement->expression)
      {
        TypeChecker checker{locals, {}, nullptr, movedBindings};
        returnStatement->expression->accept(&checker);
        movedBindings = checker.movedBindings;
        result = checker.result;
      }
      else
      {
        result = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
      }
    }

    void visit(NextStatement *nextStatement) override
    {
      // Resolve argument types, expanding spreads
      Vec<CheckingRef<TypeInfo>> resolvedTypes;
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      for (auto &expr : nextStatement->expressions)
      {
        checker.spreadResult.clear();
        expr->accept(&checker);
        if (!checker.spreadResult.empty())
        {
          for (auto &&type : checker.spreadResult)
          {
            resolvedTypes.push_back(type);
          }
        }
        else
        {
          resolvedTypes.push_back(checker.result);
        }
      }

      // Expand VarargsType entries in contextRequirement so the count check
      // works correctly for pack parameters (e.g. `next ...tail` in a variadic generic function).
      Vec<CheckingRef<TypeInfo>> expandedContext;
      bool hasVarargs = false;
      for (auto &req : contextRequirement)
      {
        if (req->tag() == typeinfo_tag::VARARGS)
        {
          hasVarargs = true;
          auto &varargs = static_cast<VarargsType &>(*req);
          for (auto &elem : varargs.elementTypes)
          {
            expandedContext.push_back(elem);
          }
        }
        else
        {
          expandedContext.push_back(req);
        }
      }

      // For variadic functions (hasVarargs), allow the next statement to provide
      // fewer or equal arguments than the expanded context (e.g. `next ...tail`
      // in a recursive variadic function passes the tail, which has fewer elements).
      // When args are fewer, match as a suffix (tail) of the expanded context.
      if (hasVarargs ? (resolvedTypes.size() > expandedContext.size())
                     : (resolvedTypes.size() != expandedContext.size()))
      {
        throw TypeCheckingException(
            "Next statement argument count mismatch: " + std::to_string(resolvedTypes.size()) + " to " +
                std::to_string(expandedContext.size()),
            nextStatement->pos);
      }
      size_t offset = 0;
      if (hasVarargs && resolvedTypes.size() < expandedContext.size())
      {
        offset = expandedContext.size() - resolvedTypes.size();
      }
      for (size_t i = 0; i < resolvedTypes.size(); ++i)
      {
        auto exprType = resolvedTypes[i];
        auto reqType = expandedContext[offset + i];
        if (exprType->tag() != typeinfo_tag::UNTYPED && !typeMatch(*reqType, *exprType))
        {
          throw TypeCheckingException("Next statement argument type mismatch: " + exprType->repr() + " to " +
                                          reqType->repr(),
                                      nextStatement->pos);
        }
      }
      movedBindings = checker.movedBindings;
    }

    void visit(IfStatement *ifStatement) override
    {
      ifStatement->evaluatedCondition.reset();
      if (ifStatement->isConst)
      {
        auto condResult = tryEvalConstCondition(ifStatement->testing.get());
        if (condResult.has_value())
        {
          ifStatement->evaluatedCondition = condResult.value();
          if (condResult.value())
          {
            auto outerNames = scopeNames(locals);
            TypeChecker thenChecker{locals, contextRequirement, expectedType, movedBindings};
            ifStatement->consequence->accept(&thenChecker);
            movedBindings = filterMovedBindings(thenChecker.movedBindings, outerNames);
            result = thenChecker.result;
          }
          else if (ifStatement->alternative)
          {
            auto outerNames = scopeNames(locals);
            TypeChecker elseChecker{locals, contextRequirement, expectedType, movedBindings};
            ifStatement->alternative->accept(&elseChecker);
            movedBindings = filterMovedBindings(elseChecker.movedBindings, outerNames);
            result = elseChecker.result;
          }
          else
          {
            result = makecheck<PrimitiveType>(typeinfo_tag::UNIT);
          }
          return;
        }
        // If we can't resolve at compile time, fall through to runtime if behavior
      }

      TypeChecker condChecker{locals, contextRequirement, expectedType, movedBindings};
      ifStatement->testing->accept(&condChecker);
      auto condType = condChecker.result;
      if (!condType || (condType->tag() != typeinfo_tag::BOOL && condType->tag() != typeinfo_tag::UNTYPED))
      {
        throw TypeCheckingException("Condition expression must be boolean: " + ifStatement->testing->repr(),
                                    ifStatement->testing->pos);
      }
      auto outerNames = scopeNames(locals);
      auto entryMovedBindings = filterMovedBindings(condChecker.movedBindings, outerNames);
      CheckingRef<TypeInfo> returnType = nullptr;
      if (ifStatement->consequence)
      {
        TypeChecker thenChecker{locals, contextRequirement, expectedType, entryMovedBindings};
        ifStatement->consequence->accept(&thenChecker);
        returnType = thenChecker.result;
        result = returnType;
        movedBindings = filterMovedBindings(thenChecker.movedBindings, outerNames);
      }
      if (ifStatement->alternative)
      {
        TypeChecker elseChecker{locals, contextRequirement, expectedType, entryMovedBindings};
        ifStatement->alternative->accept(&elseChecker);
        auto consequenceType = elseChecker.result;
        if (returnType && consequenceType)
        {
          if (typeMatch(*returnType, *consequenceType))
          {
            result = returnType;
          }
          else if (typeMatch(*consequenceType, *returnType))
          {
            result = consequenceType;
          }
          else
          {
            throw TypeCheckingException("Mismatched return types in if-else branches: " + returnType->repr() + ", " +
                                        consequenceType->repr());
          }
        }
        else if (consequenceType)
        {
          result = consequenceType;
        }
        auto thenMovedBindings = ifStatement->consequence ? movedBindings : entryMovedBindings;
        auto elseMovedBindings = filterMovedBindings(elseChecker.movedBindings, outerNames);
        thenMovedBindings.insert(elseMovedBindings.begin(), elseMovedBindings.end());
        movedBindings = std::move(thenMovedBindings);
      }
      else
      {
        movedBindings.insert(entryMovedBindings.begin(), entryMovedBindings.end());
      }
    }

    void visit(LoopStatement *loopStatement) override
    {
      auto outerNames = scopeNames(locals);
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      Vec<CheckingRef<TypeInfo>> paramTypes;
      for (auto binding : loopStatement->bindings)
      {
        binding.target->accept(&checker);
        auto bindingType = checker.result;
        if (binding.annotation)
        {
          binding.annotation->accept(&checker);
          auto annoType = checker.result;
          if (!typeMatch(*annoType, *bindingType))
          {
            throw TypeCheckingException("Loop Binding Type Mismatch: " + bindingType->repr() + " to " +
                                        annoType->repr());
          }
          bindingType = annoType;
        }
        // add to local scope
        checker.locals.insert_or_assign(binding.name, bindingType);
        paramTypes.push_back(bindingType);
      }
      checker.contextRequirement = paramTypes;
      loopStatement->loopBody->accept(&checker);
      movedBindings = filterMovedBindings(checker.movedBindings, outerNames);
      result = checker.result;
    }

    void visit(ValDefStatement *valDefStatement) override
    {
      CheckingRef<TypeInfo> annoType = nullptr;
      if (valDefStatement->typeAnnotation)
      {
        TypeChecker annoChecker{locals, {}, nullptr, movedBindings};
        valDefStatement->typeAnnotation->accept(&annoChecker);
        annoType = annoChecker.result;
      }

      // Bidirectional inference: pass annotation type as expectedType to value expression
      TypeChecker valChecker{locals, {}, annoType, movedBindings};
      valDefStatement->value->accept(&valChecker);
      auto valType = valChecker.result;
      movedBindings = valChecker.movedBindings;

      if (annoType)
      {
        if (typeMatch(*annoType, *valType))
        {
          locals.insert_or_assign(valDefStatement->name, annoType);
        }
        else
        {
          throw TypeCheckingException("Value Define Type Mismatch: " + valType->repr() + " to " + annoType->repr());
        }
      }
      else
      {
        locals.insert_or_assign(valDefStatement->name, valType);
      }
      movedBindings.erase(valDefStatement->name);
    }

    void visit(ValueBindingStatement *valBind) override
    {
      switch (valBind->type)
      {
      // TODO: migrate ValDefStatement to ValueBindingStatement
      // case BindingType::DIRECT:
      // {
      //     if (valBind->bindings.size() != 1) [[unlikely]]
      //     {
      //         throw TypeCheckingException("Direct binding allows only 1 value");
      //     }
      //     else
      //     {
      //         TypeChecker checker{locals};
      //         valBind->value->accept(&checker);
      //         auto valType = checker.result;
      //         auto binding = valBind->bindings[0];
      //         if (binding->annotation)
      //         {
      //             binding->annotation->accept(&checker);
      //             auto annoType = checker.result;
      //             if (annoType->match(*valType))
      //             {
      //                 result = annoType;
      //                 locals.insert_or_assign(binding->name, annoType);
      //             }
      //             else
      //             {
      //                 throw TypeCheckingException("Value Binding Type Mismatch: " +
      //                                             valType->repr() + " to " +
      //                                             annoType->repr());
      //             }
      //         }
      //         else
      //         {
      //             result = valType;
      //             locals.insert_or_assign(binding->name, valType);
      //         }
      //     }
      // }
      // break;
      case BindingType::TUPLE_UNPACK:
      {
        TypeChecker checker{locals, {}, nullptr, movedBindings};
        valBind->value->accept(&checker);
        auto valType = checker.result;
        movedBindings = checker.movedBindings;
        // Both TupleType and VarargsType have elementTypes and can be unpacked
        Vec<CheckingRef<TypeInfo>> *elementTypesPtr = nullptr;
        if (auto tupleType = std::dynamic_pointer_cast<TupleType>(valType); tupleType)
        {
          elementTypesPtr = &tupleType->elementTypes;
        }
        else if (auto varargsType = std::dynamic_pointer_cast<VarargsType>(valType); varargsType)
        {
          elementTypesPtr = &varargsType->elementTypes;
        }
        if (elementTypesPtr)
        {
          auto &elementTypes = *elementTypesPtr;
          if (valBind->bindings.size() > elementTypes.size())
          {
            throw TypeCheckingException(
                "Too many bindings in tuple unpack: " + std::to_string(valBind->bindings.size()) + " to " +
                std::to_string(elementTypes.size()));
          }
          for (size_t i = 0; i < valBind->bindings.size(); ++i)
          {
            auto binding = valBind->bindings[i];
            if (binding->spreadReceiver)
            {
              if (i != valBind->bindings.size() - 1) [[unlikely]]
              {
                throw TypeCheckingException("Spread receiver must be the last binding in tuple unpack.");
              }
              auto restTypes = Vec<CheckingRef<TypeInfo>>{};
              for (size_t j = i; j < elementTypes.size(); ++j)
              {
                restTypes.push_back(elementTypes[j]);
              }
              auto restTupleType = makecheck<TupleType>(restTypes);
              if (binding->annotation)
              {
                binding->annotation->accept(&checker);
                auto annoType = checker.result;
                if (typeMatch(*annoType, *restTupleType))
                {
                  locals.insert_or_assign(binding->name, annoType);
                  movedBindings.erase(binding->name);
                }
                else
                {
                  throw TypeCheckingException("Value Binding Type Mismatch: " + restTupleType->repr() + " to " +
                                              annoType->repr());
                }
              }
              else if (!binding->name.empty())
              {
                locals.insert_or_assign(binding->name, restTupleType);
                movedBindings.erase(binding->name);
              }
              break;
            }
            if (binding->annotation)
            {
              binding->annotation->accept(&checker);
              auto annoType = checker.result;
              if (typeMatch(*annoType, *elementTypes[i]))
              {
                locals.insert_or_assign(binding->name, annoType);
                movedBindings.erase(binding->name);
              }
              else
              {
                throw TypeCheckingException("Value Binding Type Mismatch: " + elementTypes[i]->repr() +
                                            " to " + annoType->repr());
              }
            }
            else
            {
              locals.insert_or_assign(binding->name, (elementTypes[i]));
              movedBindings.erase(binding->name);
            }
          }
        }
        else
        {
          throw TypeCheckingException("Value Binding Type Mismatch: " + valType->repr() + " to tuple");
        }
      }
      break;
      case BindingType::ARRAY_UNPACK:
      {
        TypeChecker checker{locals, {}, nullptr, movedBindings};
        valBind->value->accept(&checker);
        auto valType = checker.result;
        movedBindings = checker.movedBindings;
        if (auto arrayType = std::dynamic_pointer_cast<ArrayType>(valType); arrayType)
        {
          for (size_t i = 0; i < valBind->bindings.size(); ++i)
          {
            auto binding = valBind->bindings[i];
            if (binding->spreadReceiver)
            {
              if (i != valBind->bindings.size() - 1) [[unlikely]]
              {
                throw TypeCheckingException("Spread receiver must be the last binding in array unpack.");
              }
              auto restArrayType = makecheck<ArrayType>(arrayType->elementType);
              if (binding->annotation)
              {
                binding->annotation->accept(&checker);
                auto annoType = checker.result;
                if (typeMatch(*annoType, *restArrayType))
                {
                  locals.insert_or_assign(binding->name, annoType);
                  movedBindings.erase(binding->name);
                }
                else
                {
                  throw TypeCheckingException("Value Binding Type Mismatch: " + restArrayType->repr() + " to " +
                                              annoType->repr());
                }
              }
              else if (!binding->name.empty())
              {
                locals.insert_or_assign(binding->name, restArrayType);
                movedBindings.erase(binding->name);
              }

              break;
            }
            if (binding->annotation)
            {
              binding->annotation->accept(&checker);
              auto annoType = checker.result;
              if (typeMatch(*annoType, *arrayType->elementType))
              {
                locals.insert_or_assign(binding->name, annoType);
                movedBindings.erase(binding->name);
              }
              else
              {
                throw TypeCheckingException("Value Binding Type Mismatch: " + arrayType->elementType->repr() + " to " +
                                            annoType->repr());
              }
            }
            else
            {
              locals.insert_or_assign(binding->name, arrayType->elementType);
              movedBindings.erase(binding->name);
            }
          }
        }
        else
        {
          throw TypeCheckingException("Value Binding Type Mismatch: " + valType->repr() + " to array");
        }
      }
      break;
      default:
        throw TypeCheckingException("Unexpected binding type");
        break;
      }
    }

    void visit(StringValue *value) override { result = makecheck<PrimitiveType>(typeinfo_tag::STRING); }

    void visit(BooleanValue *value) override { result = makecheck<PrimitiveType>(typeinfo_tag::BOOL); }
    void visit(IntegralValue<int8_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::I8); }
    void visit(IntegralValue<uint8_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::U8); }
    void visit(IntegralValue<int16_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::I16); }
    void visit(IntegralValue<uint16_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::U16); }
    void visit(IntegralValue<int32_t> *intVal) override
    {
      // Bidirectional inference: use expectedType if it's an integral type
      if (expectedType && isPrimitive(expectedType->tag()) && isIntegralType(expectedType->tag()))
      {
        result = expectedType;
      }
      else
      {
        result = makecheck<PrimitiveType>(typeinfo_tag::I32);
      }
    }
    void visit(IntegralValue<uint32_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::U32); }
    void visit(IntegralValue<int64_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::I64); }
    void visit(IntegralValue<uint64_t> *intVal) override { result = makecheck<PrimitiveType>(typeinfo_tag::U64); }
    // void visit(FloatingPointValue<float16_t> *floatVal) override {}
    void visit(FloatingPointValue<float /* float32_t */> *floatVal) override
    {
      result = makecheck<PrimitiveType>(typeinfo_tag::F32);
    }
    void visit(FloatingPointValue<double /* float64_t */> *floatVal) override
    {
      // Bidirectional inference: use expectedType if it's a floating point type
      if (expectedType && isPrimitive(expectedType->tag()) && isFloatingPoint(expectedType->tag()))
      {
        result = expectedType;
      }
      else
      {
        result = makecheck<PrimitiveType>(typeinfo_tag::F64);
      }
    }
    // void AstVisitor::visit(FloatingPointValue<float128_t> *floatVal) override {}

    void visit(TypeCheckingExpression *typeCheck) override
    {
      result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
    }

    void visit(TypeOfExpression * /*typeofExpr*/) override
    {
      result = makecheck<Untyped>();
    }

    void visit(UnaryExpression *unoExpr) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      unoExpr->operand->accept(&checker);
      auto operandType = checker.result;
      movedBindings = checker.movedBindings;
      switch (unoExpr->optr->type)
      {
      case TokenType::MINUS:
      {
        if (isPrimitive(operandType->tag()))
        {
          PrimitiveType &primitive = static_cast<PrimitiveType &>(*operandType);
          if (isSigned(primitive.tag()) || isFloatingPoint(primitive.tag()))
          {
            result = operandType;
            return;
          }
        }

        throw TypeCheckingException("Invalid operand type for negate operation.");
      }
      case TokenType::NOT:
      {
        result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
        return;
      }
      case TokenType::QUERY:
      {
        throw TypeCheckingException("Not supported operator QUERY (?).");
      }
      case TokenType::KEYWORD_REF:
      case TokenType::AMPERSAND:
      {
        if (!isReferenceableExpression(unoExpr->operand.get()))
        {
          throw TypeCheckingException("Reference operator requires an lvalue.");
        }
        result = makecheck<ReferenceType>(widenVariantToUnionType(locals, operandType));
        return;
      }
      case TokenType::TIMES:
      {
        auto refType = std::dynamic_pointer_cast<ReferenceType>(unwrap(operandType));
        if (!refType)
        {
          throw TypeCheckingException("Cannot dereference non-reference type: " + operandType->repr());
        }
        result = refType->referencedType;
        return;
      }
      case TokenType::KEYWORD_MOVE:
      {
        if (!isMovableExpression(unoExpr->operand.get()))
        {
          throw TypeCheckingException("Move operator requires a movable place.");
        }
      if (auto *id = dynamic_cast<IdExpression *>(unoExpr->operand.get()))
      {
          movedBindings.insert(id->id);
      }
      result = operandType;
      return;
      }
      default:
        throw TypeCheckingException("Unsupported unary operator.");
      }
    }

    void visit(BinaryExpression *expression) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      expression->left->accept(&checker);
      auto leftType = checker.result;
      expression->right->accept(&checker);
      auto rightType = checker.result;
      movedBindings = checker.movedBindings;

      if (!leftType || !rightType || leftType->tag() == typeinfo_tag::UNTYPED ||
          rightType->tag() == typeinfo_tag::UNTYPED)
      {
        result = makecheck<Untyped>();
        return;
      }

      if (isPrimitive(leftType->tag()))
      {
        PrimitiveType &leftPrimitive = static_cast<PrimitiveType &>(*leftType);
        switch (expression->optr->type)
        {
        case TokenType::MODULUS:
        case TokenType::LSHIFT:
        case TokenType::RSHIFT:
          if (!isIntegralType(leftPrimitive.tag()))
          {
            throw TypeCheckingException("Invalid type for modulus: " + leftPrimitive.repr());
          }
          if (expression->optr->type != TokenType::MODULUS)
          {
            result = leftType;
            return;
          }
          [[fallthrough]];
        case TokenType::PLUS:
        case TokenType::MINUS:
        case TokenType::TIMES:
        case TokenType::DIVIDE:
          if (typeMatch(leftPrimitive, *rightType))
          {
            result = leftType;
          }
          else if (typeMatch(*rightType, leftPrimitive))
          {
            result = rightType;
          }
          else if (isIntegralType(leftPrimitive.tag()) && rightType->tag() == typeinfo_tag::UNTYPED)
          {
            result = leftType;
          }
          else if (isIntegralType(leftPrimitive.tag()) && isPrimitive(rightType->tag()) &&
                   isIntegralType(rightType->tag()))
          {
            // Loose arithmetic for integral types
            result = leftType;
          }
          else if (isFloatingPoint(leftPrimitive.tag()) && isPrimitive(rightType->tag()) &&
                   isFloatingPoint(rightType->tag()))
          {
            // Loose arithmetic for floating point types
            result = leftType;
          }
          else
          {
            throw TypeCheckingException("Mismatch type on arithmetic operation: " + leftPrimitive.repr() + ", " +
                                        rightType->repr());
          }
          return;
        case TokenType::EQUAL:
        case TokenType::NOT_EQUAL:
        case TokenType::GE:
        case TokenType::GT:
        case TokenType::LE:
        case TokenType::LT:
          if (typeMatch(leftPrimitive, *rightType) || typeMatch(*rightType, leftPrimitive) ||
              (isPrimitive(rightType->tag()) &&
               ((isIntegralType(leftPrimitive.tag()) && isIntegralType(rightType->tag())) ||
                (isFloatingPoint(leftPrimitive.tag()) && isFloatingPoint(rightType->tag())))))
          {
            result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
          }
          else
          {
            throw TypeCheckingException("Mismatch type on comparison operators: " + leftPrimitive.repr() + ", " +
                                        rightType->repr());
          }
          return;
        case TokenType::AND:
        case TokenType::OR:
          if (leftPrimitive.tag() == typeinfo_tag::BOOL && rightType->tag() == typeinfo_tag::BOOL)
          {
            result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
            return;
          }
          throw TypeCheckingException("Logical operators require boolean operands", expression->pos);
        default:
          throw TypeCheckingException("Unsupported operator for primitive types", expression->pos);
        }
      }
      else if (leftType->tag() == typeinfo_tag::ARRAY)
      {
        ArrayType &arrayType = static_cast<ArrayType &>(*leftType);
        switch (expression->optr->type)
        {
        case TokenType::LSHIFT: // push to array
          if (typeMatch(*arrayType.elementType, *rightType) || rightType->tag() == typeinfo_tag::UNTYPED)
          {
            result = leftType;
            return;
          }
          else
          {
            throw TypeCheckingException("Invalid element type for array push: " + rightType->repr(), expression->pos);
          }
        default:
          throw TypeCheckingException("Unsupported operator for array types", expression->pos);
        }
      }
      else
      {
        throw TypeCheckingException("Unsupported type for binary expression: " + leftType->repr(), expression->pos);
      }
    }

    void visit(Param *param) override
    {
      if (param->type == ParamType::Simple || param->annotatedType)
      {
        result = makecheck<Untyped>();
      }
      TypeChecker checker{locals};
      if (param->annotatedType)
      {
        param->annotatedType->accept(&checker);
        if (!checker.result)
        {
          throw TypeCheckingException("Unknown type annotation for parameter: " + param->paramName);
        }
        result = checker.result;
      }
      if (param->value)
      {
        param->value->accept(&checker);
        auto valueType = checker.result;
        if (valueType)
        {
          if (result->tag() != typeinfo_tag::UNTYPED)
          {
            if (!typeMatch(*result, *valueType))
            {
              throw TypeCheckingException("Invalid default value for type: " + result->repr());
            }
            result = makecheck<ParamWithDefaultValueType>(result);
          }
          else
          {
            result = makecheck<ParamWithDefaultValueType>(valueType);
          }
        }
        else
        {
          throw TypeCheckingException("Unexpected default expression type for parameter: " + param->paramName);
        }
      }
    }

    void visit(TypeAnnotation *annotation) override
    {
      auto typecode = code(annotation->type);
      if (typecode > code(TypeAnnotationType::BUILTIN) && typecode < code(TypeAnnotationType::END_OF_BUILTIN))
      {
        result = PrimitiveType::from(annotation->type);
        return;
      }
      else if (annotation->type == TypeAnnotationType::ARRAY)
      {
        if (annotation->arguments.size() == 1)
        {
          auto arg = annotation->arguments[0];
          TypeChecker checker{locals};
          arg->accept(&checker);
          auto argType = checker.result;
          if (argType)
          {
            result = makecheck<ArrayType>(argType);
            return;
          }
          throw TypeCheckingException("Unknown element type for array");
        }
        else
        {
          throw TypeCheckingException("Array type expects exactly 1 type argument");
        }
      }
      else if (annotation->type == TypeAnnotationType::TUPLE)
      {
        Vec<CheckingRef<TypeInfo>> types{};
        TypeChecker checker{locals};

        for (auto &&anno : annotation->arguments)
        {
          anno->accept(&checker);
          auto &&type = checker.result;
          types.push_back(type);
        }
        result = makecheck<TupleType>(types);
        return;
      }
      else if (annotation->type == TypeAnnotationType::UNION)
      {
        Vec<CheckingRef<TypeInfo>> types{};
        TypeChecker checker{locals};

        for (auto &&anno : annotation->arguments)
        {
          anno->accept(&checker);
          auto &&type = checker.result;
          types.push_back(type);
        }
        result = makecheck<UnionType>(types);
        return;
      }
      else
      {
        if (annotation->name == "ref")
        {
          if (annotation->genericArgs.size() != 1)
          {
            throw TypeCheckingException("Reference type expects exactly 1 type argument");
          }

          TypeChecker checker{locals};
          annotation->genericArgs[0]->accept(&checker);
          auto innerType = checker.result;
          if (!innerType)
          {
            throw TypeCheckingException("Unknown referenced type for ref");
          }
          result = makecheck<ReferenceType>(innerType);
          return;
        }

        // Handle suffix generic "array" syntax: T array => ArrayType<T>
        // This can come from bracket syntax [T] (arguments) or suffix syntax T array (genericArgs)
        if (annotation->name == "array")
        {
          if (annotation->arguments.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->arguments[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<ArrayType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for array");
          }
          if (annotation->genericArgs.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->genericArgs[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<ArrayType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for array");
          }
        }

        // Handle variadic type annotations: T...
        if (annotation->name.size() > 3 && annotation->name.ends_with("..."))
        {
          Str innerName = annotation->name.substr(0, annotation->name.size() - 3);
          auto it = locals.find(innerName);
          if (it != locals.end())
          {
            // If the resolved type is already a VarargsType (e.g. from pack monomorphization),
            // return it directly to avoid nesting VarargsType(VarargsType(...))
            if (it->second->tag() == typeinfo_tag::VARARGS)
            {
              result = it->second;
            }
            else
            {
              result = makecheck<VarargsType>(it->second);
            }
            return;
          }
          throw TypeCheckingException("Unknown type: " + innerName);
        }
        auto it = locals.find(annotation->name);
        if (it != locals.end())
        {
          if (!annotation->genericArgs.empty())
          {
            auto *genericType = dynamic_cast<GenericTypeDef *>(&(*it->second));
            if (!genericType)
            {
              throw TypeCheckingException("Type '" + annotation->name + "' is not generic");
            }

            Vec<CheckingRef<TypeInfo>> typeArgs;
            TypeChecker checker{locals};
            for (auto &arg : annotation->genericArgs)
            {
              arg->accept(&checker);
              typeArgs.push_back(checker.result);
            }
            result = instantiateGenericType(*genericType, typeArgs);
            return;
          }

          if (it->second->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
          {
            throw TypeCheckingException("Generic type '" + annotation->name + "' requires type arguments");
          }
          result = it->second;
        }
        else
        {
          throw TypeCheckingException("Unknown type: " + annotation->name);
        }
      }
    }

    void visit(AssignmentExpression *assignmentExpr) override
    {
      CheckingRef<TypeInfo> targetType;
      Set<Str> targetMovedBindings = movedBindings;
      if (auto *idTarget = dynamic_cast<IdExpression *>(assignmentExpr->target.get()))
      {
        auto it = locals.find(idTarget->id);
        if (it == locals.end())
        {
          throw TypeCheckingException("Unknown type for object: " + idTarget->id, idTarget->pos);
        }
        targetType = it->second;
      }
      else
      {
        TypeChecker targetChecker{locals, {}, nullptr, movedBindings, true};
        assignmentExpr->target->accept(&targetChecker);
        targetType = targetChecker.result;
        targetMovedBindings = targetChecker.movedBindings;
      }

      TypeChecker valueChecker{locals, {}, nullptr, targetMovedBindings};
      assignmentExpr->value->accept(&valueChecker);
      auto valueType = valueChecker.result;
      movedBindings = valueChecker.movedBindings;

      if (!targetType || !valueType)
      {
        throw TypeCheckingException("Invalid assignment expression: " + assignmentExpr->repr(), assignmentExpr->pos);
      }
      if (!typeMatch(*targetType, *valueType))
      {
        throw TypeCheckingException("Invalid assignment type: " + valueType->repr() + " to " + targetType->repr(),
                                    assignmentExpr->pos);
      }
      if (auto *id = dynamic_cast<IdExpression *>(assignmentExpr->target.get()))
      {
        movedBindings.erase(id->id);
      }
      result = targetType;
    }

    void visit(ArrayLiteral *arrayLit) override
    {
      if (arrayLit->elements.empty())
      {
        // Bidirectional inference: use expectedType if it's an array type
        if (expectedType && expectedType->tag() == typeinfo_tag::ARRAY)
        {
          result = expectedType;
        }
        else
        {
          result = makecheck<ArrayType>(makecheck<Untyped>());
        }
        return;
      }
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      arrayLit->elements[0]->accept(&checker);
      auto elemType = checker.result;
      for (size_t i = 1; i < arrayLit->elements.size(); ++i)
      {
        arrayLit->elements[i]->accept(&checker);
        auto nextType = checker.result;
        if (!typeMatch(*elemType, *nextType))
        {
          if (typeMatch(*nextType, *elemType))
          {
            elemType = nextType;
          }
          else
          {
            throw TypeCheckingException("Mismatched element type in array literal: " + elemType->repr() + ", " +
                                        nextType->repr());
          }
        }
      }
      movedBindings = checker.movedBindings;
      result = makecheck<ArrayType>(elemType);
    }

    void visit(TupleLiteral *tuple) override
    {
      if (tuple->elements.empty()) [[unlikely]]
      {
        result = makecheck<TupleType>(Vec<CheckingRef<TypeInfo>>{});
        return;
      }
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      Vec<CheckingRef<TypeInfo>> types{};
      for (size_t i = 0; i < tuple->elements.size(); ++i)
      {
        checker.spreadResult.clear();
        tuple->elements[i]->accept(&checker);
        if (!checker.spreadResult.empty())
        {
          for (auto &&type : checker.spreadResult)
          {
            types.push_back(type);
          }
        }
        else
        {
          types.push_back(std::move(checker.result));
        }
      }
      movedBindings = checker.movedBindings;
      result = makecheck<TupleType>(types);
    }

    void visit(UnitLiteral *unitLiteral) override { result = makecheck<PrimitiveType>(typeinfo_tag::UNIT); }

    void visit(SpreadExpression *spread) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      spread->expression->accept(&checker);
      auto type = checker.result;
      movedBindings = checker.movedBindings;
      spreadResult.clear();
      if (auto tup = std::dynamic_pointer_cast<TupleType>(type); tup)
      {
        result = tup;
        for (auto &&elemType : tup->elementTypes)
        {
          spreadResult.push_back(elemType);
        }
      }
      else if (auto varargs = std::dynamic_pointer_cast<VarargsType>(type); varargs)
      {
        result = varargs;
        for (auto &&elemType : varargs->elementTypes)
        {
          spreadResult.push_back(elemType);
        }
      }
      else if (auto arr = std::dynamic_pointer_cast<ArrayType>(type); arr)
      {
        // Array spread does not expand compile-time arity.
        result = arr->elementType;
      }
      else
      {
        throw TypeCheckingException("Invalid spread expression on type, expect tuple, varargs, or array, got " + type->repr());
      }
    }

    void visit(IndexAccessorExpression *indexAccess) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      indexAccess->primary->accept(&checker);
      auto primaryType = checker.result;
      primaryType = deref_reference_type(primaryType);
      if (!primaryType)
      {
        throw TypeCheckingException("Invalid index accessor expression: " + indexAccess->primary->repr());
      }
      if (primaryType->tag() == typeinfo_tag::UNTYPED)
      {
        result = makecheck<Untyped>();
        return;
      }
      if (primaryType->tag() == typeinfo_tag::TUPLE)
      {
        auto &tupleType = static_cast<TupleType &>(*primaryType);
        indexAccess->accessor->accept(&checker);
        auto indexType = checker.result;
        // If index is a compile-time constant integer, return the element type
        if (auto intLit = dynamic_ast_cast<IntegralValue<int32_t>>(indexAccess->accessor))
        {
          size_t idx = static_cast<size_t>(intLit->value);
          if (idx < tupleType.elementTypes.size())
          {
            result = tupleType.elementTypes[idx];
            return;
          }
        }
        // Otherwise, return Untyped for dynamic index
        movedBindings = checker.movedBindings;
        result = makecheck<Untyped>();
        return;
      }
      if (primaryType->tag() != typeinfo_tag::ARRAY)
      {
        throw TypeCheckingException("Index accessor on non-array type: " + primaryType->repr());
      }
      indexAccess->accessor->accept(&checker);
      auto indexType = checker.result;
      if (!indexType || (!isIntegralType(indexType->tag()) && indexType->tag() != typeinfo_tag::UNTYPED))
      {
        throw TypeCheckingException("Invalid index type for array: " + indexAccess->accessor->repr());
      }
      ArrayType &arrayType = static_cast<ArrayType &>(*primaryType);
      movedBindings = checker.movedBindings;
      result = arrayType.elementType;
    }

    void visit(IdAccessorExpression *idAccExpr) override
    {
      if (auto *typeOfExpr = dynamic_cast<TypeOfExpression *>(idAccExpr->primaryExpression.get()))
      {
        TypeChecker typeChecker{locals, {}, nullptr, movedBindings};
        typeOfExpr->expression->accept(&typeChecker);
        movedBindings = typeChecker.movedBindings;
        result = typeQueryPropertyType(typeChecker.result, idAccExpr->accessor->repr());
        return;
      }

      TypeChecker checker{locals, {}, nullptr, movedBindings};
      idAccExpr->primaryExpression->accept(&checker);
      auto primaryType = checker.result;
      primaryType = deref_reference_type(primaryType);
      movedBindings = checker.movedBindings;

      if (!primaryType || primaryType->tag() == typeinfo_tag::UNTYPED)
      {
        result = makecheck<Untyped>();
        return;
      }

      Str memberName = idAccExpr->accessor->repr();

      CheckingRef<TypeInfo> memberType = makecheck<Untyped>();

      // Adhoc polymorphic member access on built-in collection types
      // (TupleType, VarargsType, ArrayType) support common members like .size
      auto tag = primaryType->tag();
      bool isCollectionType = (tag == typeinfo_tag::TUPLE || tag == typeinfo_tag::VARARGS ||
                               tag == typeinfo_tag::ARRAY);
      if (isCollectionType)
      {
        if (memberName == "size")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::U32);
        }
      }

      if (auto customType = std::dynamic_pointer_cast<CustomizedType>(primaryType))
      {
        if (customType->memberFunctions.contains(memberName))
        {
          memberType = customType->memberFunctions[memberName];
        }
        else if (customType->properties.contains(memberName))
        {
          memberType = customType->properties[memberName];
        }
      }

      // Property access on tagged union variant types (after switch/case narrowing)
      if (auto variantType = std::dynamic_pointer_cast<VariantType>(primaryType))
      {
        if (memberName == "tag")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::STRING);
        }
        else if (memberName == "index")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::I32);
        }
        else if (!variantType->payloadNames.empty())
        {
          // Look up named payload field
          for (size_t i = 0; i < variantType->payloadNames.size(); ++i)
          {
            if (i < variantType->payloadTypes.size() && variantType->payloadNames[i] == memberName)
            {
              memberType = variantType->payloadTypes[i];
              break;
            }
          }
        }
      }

      if (!idAccExpr->arguments.empty() ||
          (idAccExpr->pos.line != 0)) // Hack to detect if it's potentially a call
      {
        if (auto funcType = std::dynamic_pointer_cast<FunctionType>(memberType))
        {
          result = funcType->returnType;
          return;
        }
      }

      result = memberType;
    }

    void visit(NewObjectExpression *newObj) override
    {
      CheckingRef<TypeInfo> objectType;
      auto variantInfo = std::optional<TaggedVariantLookup>{};
      if (newObj->targetType)
      {
        if (newObj->targetType->type == TypeAnnotationType::CUSTOMIZED && newObj->targetType->genericArgs.empty() &&
            !locals.contains(newObj->targetType->name))
        {
          variantInfo = findTaggedVariant(locals, newObj->targetType->name);
        }
        else
        {
          TypeChecker checker{locals, {}, nullptr, movedBindings};
          newObj->targetType->accept(&checker);
          objectType = checker.result;
          movedBindings = checker.movedBindings;
        }
      }
      else
      {
        auto it = locals.find(newObj->typeName);
        if (it != locals.end())
        {
          objectType = it->second;
        }
      }

      if (!objectType)
      {
        variantInfo = findTaggedVariant(locals, newObj->typeName);
      }

      if (!objectType || objectType->tag() == typeinfo_tag::UNTYPED)
      {
        if (!variantInfo.has_value())
        {
          result = makecheck<Untyped>();
          return;
        }
      }

      auto customType = std::dynamic_pointer_cast<CustomizedType>(objectType);
      if (customType)
      {
        TypeChecker checker{locals, {}, nullptr, movedBindings};
        for (auto &&[name, expr] : newObj->properties)
        {
          if (!customType->properties.contains(name))
          {
            throw TypeCheckingException("Unknown property '" + name + "' for type " + customType->name, expr->pos);
          }
          expr->accept(&checker);
          if (checker.result->tag() != typeinfo_tag::UNTYPED &&
              !typeMatch(*customType->properties[name], *checker.result))
          {
            throw TypeCheckingException("Property type mismatch for '" + name + "': " + checker.result->repr() +
                                            " to " + customType->properties[name]->repr(),
                                        expr->pos);
          }
        }
        for (const auto &[name, expectedType] : customType->properties)
        {
          if (!newObj->properties.contains(name))
          {
            throw TypeCheckingException("Missing property '" + name + "' for type " + customType->name, newObj->pos);
          }
        }

        movedBindings = checker.movedBindings;
        result = makecheck<ReferenceType>(customType);
        return;
      }

      if (!variantInfo.has_value())
      {
        throw TypeCheckingException("Invalid type for new object: " +
                                        (newObj->targetType ? newObj->targetType->repr() : newObj->typeName),
                                    newObj->pos);
      }

      if ((!variantInfo->payloadTypes.empty() && variantInfo->payloadNames.empty()) ||
          variantInfo->payloadNames.size() != variantInfo->payloadTypes.size())
      {
        throw TypeCheckingException("new on tagged union variant '" + newObj->typeName +
                                        "' requires named payload fields for every payload",
                                    newObj->pos);
      }

      TypeChecker checker{locals, {}, nullptr, movedBindings};
      for (auto &&[name, expr] : newObj->properties)
      {
        auto payloadIt = std::find(variantInfo->payloadNames.begin(), variantInfo->payloadNames.end(), name);
        if (payloadIt == variantInfo->payloadNames.end())
        {
          throw TypeCheckingException("Unknown payload property '" + name + "' for variant " + newObj->typeName, expr->pos);
        }

        auto index = static_cast<size_t>(std::distance(variantInfo->payloadNames.begin(), payloadIt));
        expr->accept(&checker);
        if (checker.result->tag() != typeinfo_tag::UNTYPED &&
            !typeMatch(*variantInfo->payloadTypes[index], *checker.result))
        {
          throw TypeCheckingException("Payload type mismatch for '" + name + "': " + checker.result->repr() +
                                          " to " + variantInfo->payloadTypes[index]->repr(),
                                      expr->pos);
        }
      }

      for (const auto &payloadName : variantInfo->payloadNames)
      {
        if (!newObj->properties.contains(payloadName))
        {
          throw TypeCheckingException("Missing payload property '" + payloadName + "' for variant " + newObj->typeName,
                                      newObj->pos);
        }
      }

      movedBindings = checker.movedBindings;
      result = makecheck<ReferenceType>(
          makecheck<VariantType>(variantInfo->unionType->name, newObj->typeName, 0, variantInfo->payloadTypes,
                                 variantInfo->payloadNames));
    }

    void visit(IndexAssignmentExpression *indexAssign) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      indexAssign->primary->accept(&checker);
      auto primaryType = deref_reference_type(checker.result);
      if (!primaryType)
      {
        throw TypeCheckingException("Invalid index assignment expression: " + indexAssign->primary->repr());
      }
      if (primaryType->tag() != typeinfo_tag::ARRAY)
      {
        throw TypeCheckingException("Index assignment on non-array type: " + primaryType->repr());
      }
      indexAssign->accessor->accept(&checker);
      auto indexType = checker.result;
      if (!indexType || !isIntegralType(indexType->tag()))
      {
        throw TypeCheckingException("Invalid index type for array: " + indexAssign->accessor->repr());
      }
      indexAssign->value->accept(&checker);
      auto valueType = checker.result;
      ArrayType &arrayType = static_cast<ArrayType &>(*primaryType);
      if (!typeMatch(*arrayType.elementType, *valueType))
      {
        throw TypeCheckingException("Invalid value type for array assignment: " + valueType->repr());
      }
      movedBindings = checker.movedBindings;
      result = arrayType.elementType;
    }

    void visit(IdExpression *id) override
    {
      auto it = locals.find(id->id);
      if (it != locals.end())
      {
        if (movedBindings.contains(id->id) && !allowMovedLvalueRead)
        {
          throw TypeCheckingException("Use after move: " + id->id, id->pos);
        }
        result = it->second;
      }
      else if (hasWildcardImportFlag())
      {
        // Wildcard import: resolve to Untyped since we can't enumerate exports at type-check time
        result = makecheck<Untyped>();
      }
      else
      {
        throw TypeCheckingException("Unknown type for object: " + id->id);
      }
    }

    void visit(CastExpression *castExpr) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      castExpr->expression->accept(&checker);
      auto exprType = checker.result;
      castExpr->targetType->accept(&checker);
      auto targetType = checker.result;
      movedBindings = checker.movedBindings;

      if (!exprType || !targetType)
      {
        throw TypeCheckingException("Invalid cast expression", castExpr->pos);
      }

      // Allow casts between primitive types
      if (isPrimitive(exprType->tag()) && isPrimitive(targetType->tag()))
      {
        result = targetType;
        return;
      }

      // Allow wrap: T -> NewType(T)
      if (auto nt = std::dynamic_pointer_cast<NewTypeType>(targetType))
      {
        if (nt->wrappedType->match(*exprType) || exprType->tag() == typeinfo_tag::UNTYPED)
        {
          result = targetType;
          return;
        }
      }

      // Allow unwrap: NewType(T) -> T
      if (auto nt = std::dynamic_pointer_cast<NewTypeType>(exprType))
      {
        if (targetType->match(*nt->wrappedType) || targetType->tag() == typeinfo_tag::UNTYPED)
        {
          result = targetType;
          return;
        }
      }

      // Allow cast through type alias (transparent)
      if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(exprType))
      {
        if (alias->underlyingType->match(*targetType) || targetType->match(*alias->underlyingType))
        {
          result = targetType;
          return;
        }
      }
      if (auto alias = std::dynamic_pointer_cast<TypeAliasType>(targetType))
      {
        if (alias->underlyingType->match(*exprType) || exprType->match(*alias->underlyingType))
        {
          result = targetType;
          return;
        }
      }

      throw TypeCheckingException("Invalid cast from " + exprType->repr() + " to " + targetType->repr(), castExpr->pos);
    }

    void visit(ImportDecl *importDecl) override
    {
      // Basic support for imports in type checker: 
      // Mark the module or its alias as Untyped for now
      if (!importDecl->alias.empty())
      {
        locals.insert_or_assign(importDecl->alias, makecheck<Untyped>());
      }
      else
      {
        locals.insert_or_assign(importDecl->module, makecheck<Untyped>());
      }
      
      // If importing specific symbols, mark them as Untyped too
      for (auto &&imp : importDecl->imports)
      {
        if (imp == "*")
        {
          // Wildcard import: store sentinel in locals so it propagates to child checkers
          locals[WILDCARD_IMPORT_KEY] = makecheck<Untyped>();
        }
        else
        {
          locals.insert_or_assign(imp, makecheck<Untyped>());
        }
      }
    }

    void visit(TaggedUnionDef * /*taggedUnion*/) override
    {
      // Already registered in Module first pass
    }

    void visit(SwitchStatement *switchStmt) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      switchStmt->scrutinee->accept(&checker);
      auto scrutineeType = checker.result;
      auto outerNames = scopeNames(locals);
      auto entryMovedBindings = filterMovedBindings(checker.movedBindings, outerNames);

      if (!scrutineeType || scrutineeType->tag() == typeinfo_tag::UNTYPED)
      {
        // Untyped scrutinee — check bodies loosely
        Set<Str> mergedMovedBindings = entryMovedBindings;
        for (auto &c : switchStmt->cases)
        {
          TypeChecker caseChecker{locals, {}, nullptr, entryMovedBindings};
          c.body->accept(&caseChecker);
          auto caseMovedBindings = filterMovedBindings(caseChecker.movedBindings, outerNames);
          mergedMovedBindings.insert(caseMovedBindings.begin(), caseMovedBindings.end());
        }
        movedBindings = std::move(mergedMovedBindings);
        result = makecheck<Untyped>();
        return;
      }

      TaggedUnionType *tuType = nullptr;
      if (scrutineeType->tag() == typeinfo_tag::TAGGED_UNION)
      {
        tuType = dynamic_cast<TaggedUnionType *>(&(*scrutineeType));
      }
      else if (scrutineeType->tag() == typeinfo_tag::VARIANT)
      {
        // Variant type — look up the parent tagged union
        auto *varType = dynamic_cast<VariantType *>(&(*scrutineeType));
        if (locals.contains(varType->unionName))
        {
          tuType = dynamic_cast<TaggedUnionType *>(&(*locals[varType->unionName]));
        }
      }

      if (!tuType)
      {
        throw TypeCheckingException("Switch scrutinee must be a tagged union type", switchStmt->pos);
      }

      // Type-check each case body and collect covered variants
      Set<Str> coveredVariants;
      bool hasOtherwise = false;
      Set<Str> mergedMovedBindings = entryMovedBindings;

      for (auto &c : switchStmt->cases)
      {
        TypeChecker caseChecker{locals, {}, nullptr, entryMovedBindings};

        if (c.isOtherwise)
        {
          hasOtherwise = true;
        }
        else
        {
          coveredVariants.insert(c.variantName);

          // Validate that the variant exists in the tagged union
          if (!tuType->variants.contains(c.variantName))
          {
            throw TypeCheckingException(
                "Unknown variant '" + c.variantName + "' for tagged union '" + tuType->name + "'",
                switchStmt->pos);
          }

          // Bind payload variables from the variant
          auto &payloadTypes = tuType->variants[c.variantName];
          for (size_t j = 0; j < c.bindings.size() && j < payloadTypes.size(); ++j)
          {
            if (!c.bindings[j].empty())
            {
              caseChecker.locals[c.bindings[j]] = payloadTypes[j];
            }
          }
        }

        c.body->accept(&caseChecker);
        auto caseMovedBindings = filterMovedBindings(caseChecker.movedBindings, outerNames);
        mergedMovedBindings.insert(caseMovedBindings.begin(), caseMovedBindings.end());
      }

      // Exhaustiveness check: all variants must be covered or an otherwise branch must exist
      if (!hasOtherwise)
      {
        for (auto &[variantName, _] : tuType->variants)
        {
          if (!coveredVariants.contains(variantName))
          {
            throw TypeCheckingException(
                "Non-exhaustive switch: variant '" + variantName + "' of '" + tuType->name + "' is not covered",
                switchStmt->pos);
          }
        }
      }

      movedBindings = std::move(mergedMovedBindings);
      result = makecheck<Untyped>();
    }

    void visit(FunCallExpression *funCall) override
    {
      // Check if this is a tagged value construction (e.g. Ok(42))
      if (auto idExpr = dynamic_ast_cast<IdExpression>(funCall->primaryExpression))
      {
        // Look through all TaggedUnionType in locals to find if this ID is a variant
        for (auto &[name, type] : locals)
        {
          if (type->tag() == typeinfo_tag::TAGGED_UNION)
          {
            auto *tuType = dynamic_cast<TaggedUnionType *>(&(*type));
            if (tuType && tuType->variants.contains(idExpr->id))
            {
              // It's a tagged value construction
              auto &payloadTypes = tuType->variants[idExpr->id];
              TypeChecker argChecker{locals, {}, nullptr, movedBindings};
              Vec<CheckingRef<TypeInfo>> argumentTypes;
              for (size_t i = 0; i < funCall->arguments.size(); ++i)
              {
                funCall->arguments[i]->accept(&argChecker);
                argumentTypes.push_back(argChecker.result);
              }
              movedBindings = argChecker.movedBindings;
              if (payloadTypes.size() != argumentTypes.size())
              {
                throw TypeCheckingException("Invalid payload arity for variant " + idExpr->id + ": expected " +
                                                std::to_string(payloadTypes.size()) + " argument(s), got " +
                                                std::to_string(argumentTypes.size()),
                                            funCall->pos);
              }
              for (size_t i = 0; i < payloadTypes.size(); ++i)
              {
                if (argumentTypes[i] && argumentTypes[i]->tag() != typeinfo_tag::UNTYPED &&
                    !typeMatch(*payloadTypes[i], *argumentTypes[i]))
                {
                  throw TypeCheckingException("Payload type mismatch for variant " + idExpr->id + " at argument " +
                                                  std::to_string(i + 1) + ": " + argumentTypes[i]->repr() + " to " +
                                                  payloadTypes[i]->repr(),
                                              funCall->arguments[i]->pos);
                }
              }
              // Find payload names for this variant
              Vec<Str> payloadNames;
              if (tuType->variantPayloadNames.contains(idExpr->id))
              {
                payloadNames = tuType->variantPayloadNames[idExpr->id];
              }
              result = makecheck<VariantType>(tuType->name, idExpr->id, 0, payloadTypes, payloadNames);
              return;
            }
          }
          else if (type->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
          {
            auto *genericType = dynamic_cast<GenericTypeDef *>(&(*type));
            if (!genericType || genericType->kind != GenericTypeKind::TAGGED_UNION)
            {
              continue;
            }

            auto resolveExpectedUnion = [&]() -> TaggedUnionType * {
              if (!expectedType)
              {
                return nullptr;
              }

              if (auto *expectedUnion = dynamic_cast<TaggedUnionType *>(&(*expectedType)))
              {
                if (stripTypeInstanceSuffix(expectedUnion->name) == genericType->name)
                {
                  return expectedUnion;
                }
              }

              if (auto *expectedVariant = dynamic_cast<VariantType *>(&(*expectedType)))
              {
                if (stripTypeInstanceSuffix(expectedVariant->unionName) == genericType->name)
                {
                  auto it = locals.find(expectedVariant->unionName);
                  if (it != locals.end())
                  {
                    return dynamic_cast<TaggedUnionType *>(&(*it->second));
                  }
                }
              }

              return nullptr;
            };

            auto inferFromVariant = [&](const Vec<CheckingRef<TypeInfo>> &expectedPayloadTypes,
                                        const Vec<CheckingRef<TypeInfo>> &argumentTypes)
                -> std::optional<Vec<CheckingRef<TypeInfo>>> {
              if (expectedPayloadTypes.size() != argumentTypes.size())
              {
                return std::nullopt;
              }

              Vec<CheckingRef<TypeInfo>> inferred(genericType->typeParamNames.size(), nullptr);
              for (size_t i = 0; i < expectedPayloadTypes.size(); ++i)
              {
                auto expected = expectedPayloadTypes[i];
                auto actual = argumentTypes[i];
                if (!expected || !actual)
                {
                  return std::nullopt;
                }

                Map<Str, CheckingRef<TypeInfo>> nestedSubstitution;
                extractGenericBindings(expected, actual, nestedSubstitution);
                if (!nestedSubstitution.empty())
                {
                  for (const auto &[name, inferredType] : nestedSubstitution)
                  {
                    auto it = std::find(genericType->typeParamNames.begin(), genericType->typeParamNames.end(), name);
                    if (it == genericType->typeParamNames.end())
                    {
                      return std::nullopt;
                    }
                    auto index = static_cast<size_t>(std::distance(genericType->typeParamNames.begin(), it));
                    if (inferred[index] && !typeMatch(*inferred[index], *inferredType))
                    {
                      return std::nullopt;
                    }
                    inferred[index] = inferredType;
                  }
                }
                else if (expected->tag() == typeinfo_tag::GENERIC_PARAM)
                {
                  auto &param = static_cast<GenericParamType &>(*expected);
                  auto it = std::find(genericType->typeParamNames.begin(), genericType->typeParamNames.end(), param.name);
                  if (it == genericType->typeParamNames.end())
                  {
                    return std::nullopt;
                  }
                  auto index = static_cast<size_t>(std::distance(genericType->typeParamNames.begin(), it));
                  if (inferred[index] && !typeMatch(*inferred[index], *actual))
                  {
                    return std::nullopt;
                  }
                  inferred[index] = actual;
                }
                else if (!typeMatch(*expected, *actual))
                {
                  return std::nullopt;
                }
              }

              if (std::any_of(inferred.begin(), inferred.end(), [](auto &item) { return item == nullptr; }))
              {
                return std::nullopt;
              }
              return inferred;
            };

            TypeChecker instChecker{genericType->capturedLocals};
            for (size_t i = 0; i < genericType->typeParamNames.size(); ++i)
            {
              instChecker.locals[genericType->typeParamNames[i]] =
                  makecheck<GenericParamType>(genericType->typeParamNames[i], "", genericType->typeParamIsPack[i]);
            }

            for (auto &variant : genericType->taggedUnionDef->variants)
            {
              if (variant.variantName != idExpr->id)
              {
                continue;
              }

              Vec<CheckingRef<TypeInfo>> expectedPayloadTypes;
              for (auto &payloadType : variant.payloadTypes)
              {
                payloadType->accept(&instChecker);
                expectedPayloadTypes.push_back(instChecker.result);
              }

              TypeChecker argChecker{locals, {}, nullptr, movedBindings};
              Vec<CheckingRef<TypeInfo>> argumentTypes;
              for (auto &arg : funCall->arguments)
              {
                arg->accept(&argChecker);
                argumentTypes.push_back(argChecker.result);
              }
              movedBindings = argChecker.movedBindings;
              if (expectedPayloadTypes.size() != argumentTypes.size())
              {
                throw TypeCheckingException("Invalid payload arity for variant " + idExpr->id + ": expected " +
                                                std::to_string(expectedPayloadTypes.size()) + " argument(s), got " +
                                                std::to_string(argumentTypes.size()),
                                            funCall->pos);
              }

              auto inferredArgs = inferFromVariant(expectedPayloadTypes, argumentTypes);
              if (!inferredArgs.has_value())
              {
                if (funCall->arguments.empty())
                {
                  if (auto *expectedUnion = resolveExpectedUnion(); expectedUnion && expectedUnion->variants.contains(idExpr->id))
                  {
                    Vec<Str> payloadNames;
                    if (expectedUnion->variantPayloadNames.contains(idExpr->id))
                    {
                      payloadNames = expectedUnion->variantPayloadNames[idExpr->id];
                    }
                    if (!expectedUnion->variants[idExpr->id].empty() || !payloadNames.empty())
                    {
                      continue;
                    }
                    result = makecheck<VariantType>(expectedUnion->name, idExpr->id, 0,
                                                    expectedUnion->variants[idExpr->id], payloadNames);
                    return;
                  }
                }
                throw TypeCheckingException("Payload type mismatch for variant " + idExpr->id, funCall->pos);
              }

              auto instantiatedUnion = instantiateGenericType(*genericType, inferredArgs.value());
              auto *tuType = dynamic_cast<TaggedUnionType *>(&(*instantiatedUnion));
              if (!tuType)
              {
                throw TypeCheckingException("Invalid instantiated tagged union type: " + instantiatedUnion->repr(),
                                            funCall->pos);
              }

              Vec<Str> payloadNames;
              if (tuType->variantPayloadNames.contains(idExpr->id))
              {
                payloadNames = tuType->variantPayloadNames[idExpr->id];
              }
              result = makecheck<VariantType>(tuType->name, idExpr->id, 0, tuType->variants[idExpr->id],
                                              payloadNames);
              return;
            }
          }
        }
      }

      TypeChecker checker{locals, {}, nullptr, movedBindings};
      funCall->primaryExpression->accept(&checker);
      auto primaryType = checker.result;
      if (!primaryType || primaryType->tag() == typeinfo_tag::UNTYPED)
      {
        movedBindings = checker.movedBindings;
        result = makecheck<Untyped>();
        return;
      }

      // --- Generic function call: monomorphize ---
      if (primaryType->tag() == typeinfo_tag::GENERIC_DEF)
      {
        auto &genericDef = static_cast<GenericDefType &>(*primaryType);
        result = monomorphizeGenericCall(genericDef, funCall);
        return;
      }

      auto funcType = dynamic_cast<FunctionType *>(&(*primaryType));

      if (!funcType)
      {
        throw TypeCheckingException("Invalid function type: " + primaryType->repr(), funCall->pos);
      }

      Vec<CheckingRef<TypeInfo>> argumentTypes;
      for (auto arg : funCall->arguments)
      {
        arg->accept(&checker);
        argumentTypes.push_back(checker.result);
      }
      movedBindings = checker.movedBindings;

      if (!funcType->applyWith(argumentTypes))
      {
        // Check if any argument is untyped, if so, allow it
        bool hasUntyped = std::any_of(argumentTypes.begin(), argumentTypes.end(),
                                      [](auto &&t) { return t->tag() == typeinfo_tag::UNTYPED; });
        if (!hasUntyped)
        {
          throw TypeCheckingException("Invalid argument types for function: " + funcType->repr(), funCall->pos);
        }
      }
      // Bidirectional inference: infer Untyped parameter types from arguments
      for (size_t i = 0; i < funcType->parametersType.size() && i < argumentTypes.size(); ++i)
      {
        auto &paramType = funcType->parametersType[i];
        auto argType = argumentTypes[i];
        // Unwrap ParamWithDefaultValueType if needed
        if (paramType->tag() == typeinfo_tag::PARAM_WITH_DEFAULT_VALUE)
        {
          auto &pwDef = static_cast<ParamWithDefaultValueType &>(*paramType);
          if (pwDef.paramType->tag() == typeinfo_tag::UNTYPED && argType->tag() != typeinfo_tag::UNTYPED)
          {
            pwDef.paramType = argType;
          }
        }
        else if (paramType->tag() == typeinfo_tag::UNTYPED && argType->tag() != typeinfo_tag::UNTYPED)
        {
          paramType = argType;
        }
      }

      result = funcType->returnType;
    }

    /**
     * @brief Recursively extract generic parameter bindings by unifying a resolved
     *        parameter type (which may contain GenericParamType) with a concrete argument type.
     */
    void extractGenericBindingsImpl(CheckingRef<TypeInfo> paramType, CheckingRef<TypeInfo> argType,
                                    Map<Str, CheckingRef<TypeInfo>> &substitution, Set<uintptr_t> &seen)
    {
      if (!paramType || !argType) return;
      auto key = reinterpret_cast<uintptr_t>(paramType.get()) ^ (reinterpret_cast<uintptr_t>(argType.get()) << 1U);
      if (!seen.insert(key).second)
      {
        return;
      }

      paramType = unwrap(paramType);
      argType = unwrap(argType);

      if (auto paramAlias = std::dynamic_pointer_cast<TypeAliasType>(paramType))
      {
        extractGenericBindingsImpl(paramAlias->underlyingType, argType, substitution, seen);
        return;
      }
      if (auto argAlias = std::dynamic_pointer_cast<TypeAliasType>(argType))
      {
        extractGenericBindingsImpl(paramType, argAlias->underlyingType, substitution, seen);
        return;
      }

      if (paramType->tag() == typeinfo_tag::GENERIC_PARAM)
      {
        auto &gp = static_cast<GenericParamType &>(*paramType);
        substitution[gp.name] = argType;
        return;
      }

      if (paramType->tag() == typeinfo_tag::ARRAY && argType->tag() == typeinfo_tag::ARRAY)
      {
        auto &paramArr = static_cast<ArrayType &>(*paramType);
        auto &argArr = static_cast<ArrayType &>(*argType);
        extractGenericBindingsImpl(paramArr.elementType, argArr.elementType, substitution, seen);
        return;
      }

      if (paramType->tag() == typeinfo_tag::TUPLE && argType->tag() == typeinfo_tag::TUPLE)
      {
        auto &paramTup = static_cast<TupleType &>(*paramType);
        auto &argTup = static_cast<TupleType &>(*argType);
        for (size_t i = 0; i < paramTup.elementTypes.size() && i < argTup.elementTypes.size(); ++i)
        {
          extractGenericBindingsImpl(paramTup.elementTypes[i], argTup.elementTypes[i], substitution, seen);
        }
        return;
      }

      if (paramType->tag() == typeinfo_tag::REFERENCE && argType->tag() == typeinfo_tag::REFERENCE)
      {
        auto &paramRef = static_cast<ReferenceType &>(*paramType);
        auto &argRef = static_cast<ReferenceType &>(*argType);
        extractGenericBindingsImpl(paramRef.referencedType, argRef.referencedType, substitution, seen);
        return;
      }

      if (paramType->tag() == typeinfo_tag::TAGGED_UNION)
      {
        auto &paramUnion = static_cast<TaggedUnionType &>(*paramType);
        if (argType->tag() == typeinfo_tag::TAGGED_UNION)
        {
          auto &argUnion = static_cast<TaggedUnionType &>(*argType);
          for (const auto &[variantName, paramPayload] : paramUnion.variants)
          {
            if (!argUnion.variants.contains(variantName))
            {
              continue;
            }
            const auto &argPayload = argUnion.variants.at(variantName);
            for (size_t i = 0; i < paramPayload.size() && i < argPayload.size(); ++i)
            {
              extractGenericBindingsImpl(paramPayload[i], argPayload[i], substitution, seen);
            }
          }
          return;
        }
        if (argType->tag() == typeinfo_tag::VARIANT)
        {
          auto &argVariant = static_cast<VariantType &>(*argType);
          if (paramUnion.variants.contains(argVariant.variantName))
          {
            const auto &paramPayload = paramUnion.variants.at(argVariant.variantName);
            for (size_t i = 0; i < paramPayload.size() && i < argVariant.payloadTypes.size(); ++i)
            {
              extractGenericBindingsImpl(paramPayload[i], argVariant.payloadTypes[i], substitution, seen);
            }
          }
          return;
        }
      }

      if (paramType->tag() == typeinfo_tag::VARIANT && argType->tag() == typeinfo_tag::VARIANT)
      {
        auto &paramVariant = static_cast<VariantType &>(*paramType);
        auto &argVariant = static_cast<VariantType &>(*argType);
        if (paramVariant.variantName != argVariant.variantName)
        {
          return;
        }
        for (size_t i = 0; i < paramVariant.payloadTypes.size() && i < argVariant.payloadTypes.size(); ++i)
        {
          extractGenericBindingsImpl(paramVariant.payloadTypes[i], argVariant.payloadTypes[i], substitution, seen);
        }
        return;
      }

      // Handle VarargsType: recurse into element types if argType is also VarargsType/TupleType
      if (paramType->tag() == typeinfo_tag::VARARGS)
      {
        auto &paramVar = static_cast<VarargsType &>(*paramType);
        if (argType->tag() == typeinfo_tag::VARARGS)
        {
          auto &argVar = static_cast<VarargsType &>(*argType);
          for (size_t i = 0; i < paramVar.elementTypes.size() && i < argVar.elementTypes.size(); ++i)
          {
            extractGenericBindingsImpl(paramVar.elementTypes[i], argVar.elementTypes[i], substitution, seen);
          }
        }
        else if (argType->tag() == typeinfo_tag::TUPLE)
        {
          auto &argTup = static_cast<TupleType &>(*argType);
          for (size_t i = 0; i < paramVar.elementTypes.size() && i < argTup.elementTypes.size(); ++i)
          {
            extractGenericBindingsImpl(paramVar.elementTypes[i], argTup.elementTypes[i], substitution, seen);
          }
        }
        return;
      }

      // For other types, no generic params to extract
    }

    void extractGenericBindings(CheckingRef<TypeInfo> paramType, CheckingRef<TypeInfo> argType,
                                Map<Str, CheckingRef<TypeInfo>> &substitution)
    {
      Set<uintptr_t> seen;
      extractGenericBindingsImpl(std::move(paramType), std::move(argType), substitution, seen);
    }

    /**
     * @brief Monomorphize a generic function call.
     *
     * Steps:
     * 1. Type-check the arguments to get concrete types.
     * 2. Build a substitution map: type param name -> concrete TypeInfo.
     * 3. Substitute in the function's parameter type annotations and return type.
     * 4. Type-check the function body with the substituted types.
     * 5. Return the instantiated return type.
     */
    CheckingRef<TypeInfo> monomorphizeGenericCall(GenericDefType &genericDef, FunCallExpression *funCall)
    {
      auto &funcDef = genericDef.funcDef;
      auto &typeParamNames = genericDef.typeParamNames;
      auto &typeParamIsPack = genericDef.typeParamIsPack;

      // 1. Type-check arguments
      TypeChecker argChecker{locals, {}, nullptr, movedBindings};
      Vec<CheckingRef<TypeInfo>> argumentTypes;
      for (auto arg : funCall->arguments)
      {
        arg->accept(&argChecker);
        argumentTypes.push_back(argChecker.result);
      }
      movedBindings = argChecker.movedBindings;

      // 2. Inject GenericParamType entries (with pack flags) into a working scope
      Map<Str, CheckingRef<TypeInfo>> substitution;
      for (size_t pi = 0; pi < typeParamNames.size(); ++pi)
      {
        bool isPack = (pi < typeParamIsPack.size()) ? typeParamIsPack[pi] : false;
        substitution[typeParamNames[pi]] = makecheck<GenericParamType>(typeParamNames[pi], "", isPack);
      }

      // 3. Arity check: verify argument count matches parameter expectations
      {
        // Count non-pack parameters; detect if there's a pack parameter
        size_t nonPackParamCount = 0;
        bool hasPackParam = false;
        for (size_t i = 0; i < funcDef->params.size(); ++i)
        {
          auto &param = funcDef->params[i];
          bool isPackParam = false;
          if (param->annotatedType)
          {
            // Resolve the annotation through substitution to check for pack
            TypeChecker annoCheck{locals};
            for (auto &[name, type] : substitution)
            {
              annoCheck.locals[name] = type;
            }
            param->annotatedType->accept(&annoCheck);
            auto pType = annoCheck.result;
            if (pType && pType->tag() == typeinfo_tag::VARARGS)
            {
              isPackParam = true;
            }
            else if (pType && pType->tag() == typeinfo_tag::GENERIC_PARAM)
            {
              auto &gp = static_cast<GenericParamType &>(*pType);
              if (gp.isPack) isPackParam = true;
            }
          }
          if (isPackParam)
          {
            hasPackParam = true;
          }
          else
          {
            ++nonPackParamCount;
          }
        }

        if (hasPackParam)
        {
          if (argumentTypes.size() < nonPackParamCount)
          {
            throw TypeCheckingException(
                "Too few arguments for generic function: expected at least " +
                    std::to_string(nonPackParamCount) + ", got " + std::to_string(argumentTypes.size()),
                funCall->pos);
          }
        }
        else
        {
          if (argumentTypes.size() != nonPackParamCount)
          {
            throw TypeCheckingException(
                "Arity mismatch in generic function call: expected " +
                    std::to_string(nonPackParamCount) + " arguments, got " +
                    std::to_string(argumentTypes.size()),
                funCall->pos);
          }
        }
      }

      // 4. Build substitution map by unifying parameter annotations with argument types.
      //    Pack parameters collect all remaining arguments into a VarargsType.
      for (size_t i = 0; i < funcDef->params.size(); ++i)
      {
        auto &param = funcDef->params[i];
        if (param->annotatedType)
        {
          TypeChecker annoChecker{locals};
          for (auto &[name, type] : substitution)
          {
            annoChecker.locals[name] = type;
          }
          param->annotatedType->accept(&annoChecker);
          auto paramType = annoChecker.result;

          if (paramType && paramType->tag() != typeinfo_tag::UNTYPED)
          {
            // Check if this parameter's type is a pack generic param
            // It could be either GenericParamType directly, or VarargsType(GenericParamType(...))
            // when the annotation uses T... syntax
            GenericParamType *packGp = nullptr;
            if (paramType->tag() == typeinfo_tag::GENERIC_PARAM)
            {
              auto &gp = static_cast<GenericParamType &>(*paramType);
              if (gp.isPack) packGp = &gp;
            }
            else if (paramType->tag() == typeinfo_tag::VARARGS)
            {
              auto &varargs = static_cast<VarargsType &>(*paramType);
              if (!varargs.elementTypes.empty() && varargs.elementTypes[0]->tag() == typeinfo_tag::GENERIC_PARAM)
              {
                auto &gp = static_cast<GenericParamType &>(*varargs.elementTypes[0]);
                if (gp.isPack) packGp = &gp;
              }
            }
            if (packGp)
            {
              // Collect remaining arguments from position i onward into VarargsType
              Vec<CheckingRef<TypeInfo>> packElements;
              for (size_t j = i; j < argumentTypes.size(); ++j)
              {
                packElements.push_back(argumentTypes[j]);
              }
              substitution[packGp->name] = makecheck<VarargsType>(packElements);
              break; // Pack consumes all remaining args; no more params to process
            }
            // Non-pack: unify param type with argument type
            if (i < argumentTypes.size())
            {
              extractGenericBindings(paramType, argumentTypes[i], substitution);
            }
          }
        }
      }

      // Fill in any unsubstituted type params with Untyped
      for (auto &name : typeParamNames)
      {
        if (!substitution.contains(name))
        {
          substitution[name] = makecheck<Untyped>();
        }
      }

      Vec<CheckingRef<TypeInfo>> instantiatedArgs;
      instantiatedArgs.reserve(typeParamNames.size());
      for (const auto &name : typeParamNames)
      {
        instantiatedArgs.push_back(substitution[name]);
      }
      Str instanceName = formatTypeInstanceName(genericDef.name, instantiatedArgs);
      if (genericDef.instances.contains(instanceName))
      {
        return genericDef.instances.at(instanceName);
      }

      // 4. Resolve the return type with substitution
      CheckingRef<TypeInfo> returnType = makecheck<Untyped>();
      if (funcDef->returnType)
      {
        TypeChecker retChecker{locals};
        for (auto &[name, type] : substitution)
        {
          retChecker.locals[name] = type;
        }
        funcDef->returnType->accept(&retChecker);
        returnType = retChecker.result;
      }
      genericDef.instances[instanceName] = returnType;
      try
      {

      // 5. Type-check the function body with substituted types
      TypeChecker bodyChecker{locals};
      for (auto &[name, type] : substitution)
      {
        bodyChecker.locals[name] = type;
      }

      // Set up parameter bindings in the body scope
      // For pack params, bind the parameter name to the VarargsType
      bool packHandled = false;
      for (size_t i = 0; i < funcDef->params.size(); ++i)
      {
        auto &param = funcDef->params[i];
        if (packHandled)
        {
          // Params after pack: shouldn't exist, but bind Untyped to be safe
          bodyChecker.locals[param->paramName] = makecheck<Untyped>();
          continue;
        }

        // Resolve the param's annotated type through substitution
        if (param->annotatedType)
        {
          TypeChecker annoChecker{locals};
          for (auto &[name, type] : substitution)
          {
            annoChecker.locals[name] = type;
          }
          param->annotatedType->accept(&annoChecker);
          auto resolvedType = annoChecker.result;

          if (resolvedType && resolvedType->tag() == typeinfo_tag::VARARGS)
          {
            // This is a pack parameter — bind to VarargsType
            bodyChecker.locals[param->paramName] = resolvedType;
            packHandled = true;
          }
          else if (resolvedType)
          {
            bodyChecker.locals[param->paramName] = resolvedType;
          }
          else
          {
            bodyChecker.locals[param->paramName] = makecheck<Untyped>();
          }
        }
        else if (i < argumentTypes.size())
        {
          bodyChecker.locals[param->paramName] = argumentTypes[i];
        }
        else
        {
          bodyChecker.locals[param->paramName] = makecheck<Untyped>();
        }
      }

      // Build contextRequirement from resolved parameter types (not expanded argumentTypes).
      // For pack params, the resolved type is a single VarargsType entry,
      // so `next ...tail` won't have a count mismatch with the expanded argument count.
      Vec<CheckingRef<TypeInfo>> resolvedParamTypes;
      {
        bool packSeen = false;
        for (size_t i = 0; i < funcDef->params.size(); ++i)
        {
          if (packSeen)
            break;
          auto &param = funcDef->params[i];
          if (param->annotatedType)
          {
            TypeChecker annoChecker{locals};
            for (auto &[name, type] : substitution)
            {
              annoChecker.locals[name] = type;
            }
            param->annotatedType->accept(&annoChecker);
            auto resolvedType = annoChecker.result;
            if (resolvedType && resolvedType->tag() == typeinfo_tag::VARARGS)
            {
              packSeen = true;
              resolvedParamTypes.push_back(resolvedType);
            }
            else if (resolvedType)
            {
              resolvedParamTypes.push_back(resolvedType);
            }
          }
          else if (i < argumentTypes.size())
          {
            resolvedParamTypes.push_back(argumentTypes[i]);
          }
        }
      }
      bodyChecker.contextRequirement = resolvedParamTypes;

      // Set expectedType for return value bidirectional inference
      if (returnType->tag() != typeinfo_tag::UNTYPED)
      {
        bodyChecker.expectedType = returnType;
      }

      if (funcDef->body)
      {
        funcDef->body->accept(&bodyChecker);
        auto bodyReturnType = bodyChecker.result;
        if (bodyReturnType && bodyReturnType->tag() != typeinfo_tag::UNTYPED &&
            returnType->tag() != typeinfo_tag::UNTYPED)
        {
          if (!typeMatch(*returnType, *bodyReturnType))
          {
            throw TypeCheckingException("Return Type Mismatch in generic instantiation: " +
                                            bodyReturnType->repr() + " to " + returnType->repr(),
                                        funcDef->pos);
          }
        }
        // Use the body's inferred return type if it's more specific
        if (bodyReturnType && bodyReturnType->tag() != typeinfo_tag::UNTYPED)
        {
          returnType = bodyReturnType;
        }
      }

      genericDef.instances[instanceName] = returnType;
      return returnType;
      }
      catch (...)
      {
        genericDef.instances.erase(instanceName);
        throw;
      }
    }
  };

  TypeIndex type_check(ASTRef<ASTNode> ast, TypeIndex initial_index)
  {
    TypeChecker checker{initial_index};
    checker.type_index = initial_index;
    ast->accept(&checker);

    return checker.type_index;
  }

  TypeIndex build_prelude_type_index()
  {
    TypeIndex result;

    // Try to locate and load the prelude source file from known lib paths.
    // This mirrors the search logic used by the interpreter's module loader.
    namespace fs = std::filesystem;

    Vec<Str> libPaths = {"lib", "../lib", "../../lib"};
    fs::path preludePath;

    for (const auto &base : libPaths)
    {
      fs::path candidate = fs::path(base) / "std" / "prelude.ng";
      if (fs::exists(candidate))
      {
        preludePath = candidate;
        break;
      }
    }

    if (preludePath.empty())
    {
      // Prelude not found — return empty index (tests/CLI that don't use
      // the prelude will still work).
      return result;
    }

    try
    {
      std::ifstream file{preludePath};
      std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

      using namespace NG::parsing;
      auto ast = Parser(ParseState(Lexer(LexState{source}).lex())).parse(preludePath.string());

      if (ast)
      {
        result = type_check(ast);
        destroyast(ast);
      }
    }
    catch (...)
    {
      // If prelude parsing/type-checking fails, return empty index.
      // This keeps the rest of the program functional.
    }

    return result;
  }
} // namespace NG::typecheck
