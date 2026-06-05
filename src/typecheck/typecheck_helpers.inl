
#include <debug.hpp>
#include <intp/intp.hpp>
#include <intp/runtime_numerals.hpp>
#include <token.hpp>
#include <typecheck/pattern_matching.hpp>
#include <typecheck/overload_resolver.hpp>
#include <typecheck/type_environment.hpp>
#include <typecheck/trait_registry.hpp>
#include <typecheck/mangling.hpp>
#include <typecheck/typecheck.hpp>
#include <runtime/value_access.hpp>
#include <module.hpp>
#include <parser.hpp>
#include <algorithm>
#include <cctype>
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
    if (t.tag() == typeinfo_tag::TYPE_ALIAS)
    {
      return unwrapAlias(*static_cast<const TypeAliasType &>(t).underlyingType);
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
    if (!type) return type;
    switch (type->tag())
    {
    case typeinfo_tag::PARAM_WITH_DEFAULT_VALUE:
      return unwrap(static_cast<ParamWithDefaultValueType &>(*type).paramType);
    case typeinfo_tag::TYPE_ALIAS:
      return unwrap(static_cast<TypeAliasType &>(*type).underlyingType);
    default:
      return type;
    }
  }

  static auto deref_reference_type(CheckingRef<TypeInfo> type) -> CheckingRef<TypeInfo>
  {
    auto unwrapped = unwrap(type);
    if (unwrapped && unwrapped->tag() == typeinfo_tag::REFERENCE)
    {
      return static_cast<ReferenceType &>(*unwrapped).referencedType;
    }
    return type;
  }



  static auto builtin_sequence_element_type(const CheckingRef<TypeInfo> &type) -> CheckingRef<TypeInfo>
  {
    auto unwrapped = unwrap(type);
    if (!unwrapped) return nullptr;
    switch (unwrapped->tag())
    {
    case typeinfo_tag::ARRAY:  return static_cast<const ArrayType &>(*unwrapped).elementType;
    case typeinfo_tag::VECTOR: return static_cast<const VectorType &>(*unwrapped).elementType;
    case typeinfo_tag::SPAN:   return static_cast<const SpanType &>(*unwrapped).elementType;
    case typeinfo_tag::RANGE:  return static_cast<const RangeType &>(*unwrapped).elementType;
    case typeinfo_tag::VARARGS:
    {
      const auto &varargs = static_cast<const VarargsType &>(*unwrapped);
      if (varargs.elementTypes.empty()) return nullptr;
      auto elementType = varargs.elementTypes.front();
      for (auto &nextType : varargs.elementTypes)
      {
        if (!typeMatch(*elementType, *nextType)) return makecheck<Untyped>();
      }
      return elementType;
    }
    default: return nullptr;
    }
  }

  static auto is_builtin_sequence_type(const CheckingRef<TypeInfo> &type) -> bool
  {
    return builtin_sequence_element_type(type) != nullptr;
  }

  static auto const_value_equals_size(const CheckingRef<TypeInfo> &type, size_t value) -> bool
  {
    auto constValue = std::dynamic_pointer_cast<ConstValueType>(unwrap(type));
    if (!constValue || constValue->isParam)
    {
      return true;
    }
    try
    {
      return std::stoull(constValue->value) == value;
    }
    catch (...)
    {
      return false;
    }
  }

  static auto is_self_type(const CheckingRef<TypeInfo> &type) -> bool
  {
    auto generic = std::dynamic_pointer_cast<GenericParamType>(unwrap(type));
    return generic && generic->name == "Self";
  }

  static auto is_ref_self_type(const CheckingRef<TypeInfo> &type) -> bool
  {
    auto ref = std::dynamic_pointer_cast<ReferenceType>(unwrap(type));
    return ref && is_self_type(ref->referencedType);
  }

  static auto contains_non_receiver_self(const CheckingRef<TypeInfo> &type) -> bool
  {
    auto unwrapped = unwrap(type);
    if (!unwrapped)
    {
      return false;
    }
    if (is_self_type(unwrapped))
    {
      return true;
    }
    switch (unwrapped->tag())
    {
    case typeinfo_tag::REFERENCE:
      return contains_non_receiver_self(static_cast<const ReferenceType &>(*unwrapped).referencedType);
    case typeinfo_tag::ARRAY:
      return contains_non_receiver_self(static_cast<const ArrayType &>(*unwrapped).elementType);
    case typeinfo_tag::VECTOR:
      return contains_non_receiver_self(static_cast<const VectorType &>(*unwrapped).elementType);
    case typeinfo_tag::SPAN:
      return contains_non_receiver_self(static_cast<const SpanType &>(*unwrapped).elementType);
    case typeinfo_tag::TUPLE:
      return std::ranges::any_of(static_cast<const TupleType &>(*unwrapped).elementTypes, contains_non_receiver_self);
    case typeinfo_tag::UNION:
      return std::ranges::any_of(static_cast<const UnionType &>(*unwrapped).types, contains_non_receiver_self);
    default:
      return false;
    }
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
    if (dynamic_cast<const IdAccessorExpression *>(expr) != nullptr)
    {
      return true;
    }
    if (dynamic_cast<const IndexAccessorExpression *>(expr) != nullptr)
    {
      return true;
    }
    auto unaryExpr = dynamic_cast<const UnaryExpression *>(expr);
    return unaryExpr != nullptr && unaryExpr->optr != nullptr && unaryExpr->optr->type == TokenType::TIMES;
  }

  static auto movedPlaceRoot(const Str &place) -> Str
  {
    auto borrowSeparator = place.find("->");
    if (place.starts_with("$borrow:") && borrowSeparator != Str::npos)
    {
      return place.substr(std::string_view{"$borrow:"}.size(), borrowSeparator - std::string_view{"$borrow:"}.size());
    }
    auto dot = place.find('.');
    auto bracket = place.find('[');
    auto end = std::min(dot == Str::npos ? place.size() : dot,
                        bracket == Str::npos ? place.size() : bracket);
    return place.substr(0, end);
  }

  static auto isBorrowEntry(const Str &entry) -> bool
  {
    return entry.starts_with("$borrow:");
  }

  static auto borrowedAliasName(const Str &entry) -> Str
  {
    if (!isBorrowEntry(entry))
    {
      return {};
    }
    auto start = std::string_view{"$borrow:"}.size();
    auto separator = entry.find("->", start);
    return separator == Str::npos ? Str{} : entry.substr(start, separator - start);
  }

  static auto borrowedTargetPlace(const Str &entry) -> Str
  {
    if (!isBorrowEntry(entry))
    {
      return {};
    }
    auto separator = entry.find("->");
    return separator == Str::npos ? Str{} : entry.substr(separator + 2);
  }

  static auto borrowEntry(Str alias, Str place) -> Str
  {
    return "$borrow:" + std::move(alias) + "->" + std::move(place);
  }

  static auto isMovedDescendantOf(const Str &moved, const Str &place) -> bool
  {
    if (isBorrowEntry(moved))
    {
      return false;
    }
    return moved.size() > place.size() && moved.starts_with(place) &&
           (moved[place.size()] == '.' || moved[place.size()] == '[');
  }

  static auto previousPlaceComponent(const Str &place) -> std::optional<Str>
  {
    auto dot = place.rfind('.');
    auto bracket = place.rfind('[');
    if (dot == Str::npos && bracket == Str::npos)
    {
      return std::nullopt;
    }
    auto pos = std::max(dot == Str::npos ? size_t{0} : dot,
                        bracket == Str::npos ? size_t{0} : bracket);
    return place.substr(0, pos);
  }

  static auto movedAncestorOrSelf(const Set<Str> &moved, const Str &place) -> std::optional<Str>
  {
    auto current = std::optional<Str>{place};
    while (current.has_value() && !current->empty())
    {
      if (moved.contains(*current))
      {
        return current;
      }
      current = previousPlaceComponent(*current);
    }
    return std::nullopt;
  }

  static auto hasMovedDescendant(const Set<Str> &moved, const Str &place) -> bool
  {
    return std::ranges::any_of(moved, [&](const auto &entry) { return isMovedDescendantOf(entry, place); });
  }

  static auto placesConflict(const Str &lhs, const Str &rhs) -> bool
  {
    return lhs == rhs || isMovedDescendantOf(lhs, rhs) || isMovedDescendantOf(rhs, lhs);
  }

  static auto borrowedConflict(const Set<Str> &state, const Str &place) -> std::optional<Str>
  {
    for (const auto &entry : state)
    {
      if (!isBorrowEntry(entry))
      {
        continue;
      }
      auto target = borrowedTargetPlace(entry);
      if (!target.empty() && placesConflict(target, place))
      {
        return target;
      }
    }
    return std::nullopt;
  }

  static auto borrowedAliasTarget(const Set<Str> &state, const Str &alias) -> std::optional<Str>
  {
    for (const auto &entry : state)
    {
      if (isBorrowEntry(entry) && borrowedAliasName(entry) == alias)
      {
        return borrowedTargetPlace(entry);
      }
    }
    return std::nullopt;
  }

  static void clearMovedPlace(Set<Str> &moved, const Str &place)
  {
    for (auto it = moved.begin(); it != moved.end();)
    {
      if (*it == place || isMovedDescendantOf(*it, place) ||
          (isBorrowEntry(*it) && borrowedAliasName(*it) == place))
      {
        it = moved.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  static auto staticPlaceKey(const Expression *expr) -> std::optional<Str>
  {
    if (auto id = dynamic_cast<const IdExpression *>(expr))
    {
      return id->id;
    }
    if (auto idAcc = dynamic_cast<const IdAccessorExpression *>(expr))
    {
      if (!idAcc->arguments.empty())
      {
        return std::nullopt;
      }
      auto primary = staticPlaceKey(idAcc->primaryExpression.get());
      if (!primary.has_value())
      {
        return std::nullopt;
      }
      return *primary + "." + idAcc->accessor->repr();
    }
    if (auto index = dynamic_cast<const IndexAccessorExpression *>(expr))
    {
      auto primary = staticPlaceKey(index->primary.get());
      if (!primary.has_value())
      {
        return std::nullopt;
      }
      if (auto intLit = dynamic_cast<const IntegralValue<int32_t> *>(index->accessor.get()))
      {
        return *primary + "[" + std::to_string(intLit->value) + "]";
      }
      return std::nullopt;
    }
    if (auto index = dynamic_cast<const IndexAssignmentExpression *>(expr))
    {
      auto primary = staticPlaceKey(index->primary.get());
      if (!primary.has_value())
      {
        return std::nullopt;
      }
      if (auto intLit = dynamic_cast<const IntegralValue<int32_t> *>(index->accessor.get()))
      {
        return *primary + "[" + std::to_string(intLit->value) + "]";
      }
      return std::nullopt;
    }
    return std::nullopt;
  }

  static auto borrowedPlaceFromRefExpression(const Expression *expr) -> std::optional<Str>
  {
    auto unary = dynamic_cast<const UnaryExpression *>(expr);
    if (!unary || !unary->optr ||
        (unary->optr->type != TokenType::KEYWORD_REF && unary->optr->type != TokenType::AMPERSAND))
    {
      return std::nullopt;
    }
    return staticPlaceKey(unary->operand.get());
  }

  static auto relativeReceiverPlace(const Expression *expr, const Str &receiverName) -> std::optional<Str>
  {
    auto place = staticPlaceKey(expr);
    if (!place.has_value() || movedPlaceRoot(*place) != receiverName)
    {
      return std::nullopt;
    }
    if (*place == receiverName)
    {
      return Str{};
    }
    auto relative = place->substr(receiverName.size());
    if (!relative.empty() && (relative.front() == '.' || relative.front() == '['))
    {
      relative.erase(relative.begin());
    }
    return relative;
  }

  static auto absoluteReceiverPlace(const Str &receiverPlace, const Str &relativePlace) -> Str
  {
    if (relativePlace.empty())
    {
      return receiverPlace;
    }
    if (relativePlace.front() == '[')
    {
      return receiverPlace + relativePlace;
    }
    return receiverPlace + "." + relativePlace;
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
      if (isBorrowEntry(name))
      {
        if (allowed.contains(borrowedAliasName(name)))
        {
          filtered.insert(name);
        }
      }
      else if (allowed.contains(name) || allowed.contains(movedPlaceRoot(name)))
      {
        filtered.insert(name);
      }
    }
    return filtered;
  }

  struct ReceiverEffectCollector : DummyVisitor
  {
    Str receiverName;
    Vec<PlaceEffect> effects;
    bool unknown = false;

    explicit ReceiverEffectCollector(Str receiverName) : receiverName(std::move(receiverName)) {}

    void recordRead(Expression *expr)
    {
      if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
      {
        effects.push_back(PlaceEffect{PlaceEffectKind::Read, *place});
      }
    }

    void recordWrite(Expression *expr)
    {
      if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
      {
        effects.push_back(PlaceEffect{PlaceEffectKind::Write, *place});
      }
    }

    void recordMove(Expression *expr)
    {
      if (auto place = relativeReceiverPlace(expr, receiverName); place.has_value())
      {
        effects.push_back(PlaceEffect{PlaceEffectKind::Move, *place});
      }
    }

    void visit(SimpleStatement *stmt) override
    {
      if (stmt->expression)
      {
        stmt->expression->accept(this);
      }
    }

    void visit(CompoundStatement *stmt) override
    {
      for (auto &child : stmt->statements)
      {
        child->accept(this);
      }
    }

    void visit(ReturnStatement *stmt) override
    {
      if (stmt->expression)
      {
        stmt->expression->accept(this);
      }
    }

    void visit(IfStatement *stmt) override
    {
      if (stmt->testing)
      {
        stmt->testing->accept(this);
      }
      if (stmt->consequence)
      {
        stmt->consequence->accept(this);
      }
      if (stmt->alternative)
      {
        stmt->alternative->accept(this);
      }
    }

    void visit(LoopStatement *stmt) override
    {
      for (auto &binding : stmt->bindings)
      {
        if (binding.target)
        {
          binding.target->accept(this);
        }
      }
      if (stmt->loopBody)
      {
        stmt->loopBody->accept(this);
      }
    }

    void visit(NextStatement *stmt) override
    {
      for (auto &expr : stmt->expressions)
      {
        expr->accept(this);
      }
    }

    void visit(ValDefStatement *stmt) override
    {
      if (stmt->value)
      {
        stmt->value->accept(this);
      }
    }

    void visit(ValueBindingStatement *stmt) override
    {
      if (stmt->value)
      {
        stmt->value->accept(this);
      }
    }

    void visit(UnaryExpression *expr) override
    {
      if (!expr->optr)
      {
        return;
      }
      if (expr->optr->type == TokenType::KEYWORD_MOVE)
      {
        recordMove(expr->operand.get());
        return;
      }
      if (expr->optr->type == TokenType::KEYWORD_REF || expr->optr->type == TokenType::AMPERSAND)
      {
        if (relativeReceiverPlace(expr->operand.get(), receiverName).has_value())
        {
          unknown = true;
        }
        return;
      }
      if (expr->operand)
      {
        expr->operand->accept(this);
      }
    }

    void visit(BinaryExpression *expr) override
    {
      if (expr->left)
      {
        expr->left->accept(this);
      }
      if (expr->right)
      {
        expr->right->accept(this);
      }
    }

    void visit(AssignmentExpression *expr) override
    {
      if (expr->value)
      {
        expr->value->accept(this);
      }
      recordWrite(expr->target.get());
    }

    void visit(IndexAssignmentExpression *expr) override
    {
      if (expr->value)
      {
        expr->value->accept(this);
      }
      recordWrite(expr);
    }

    void visit(IdExpression *expr) override { recordRead(expr); }

    void visit(IdAccessorExpression *expr) override
    {
      const bool receiverPlace = relativeReceiverPlace(expr, receiverName).has_value();
      recordRead(expr);
      if (!receiverPlace && expr->primaryExpression)
      {
        expr->primaryExpression->accept(this);
      }
      for (auto &arg : expr->arguments)
      {
        arg->accept(this);
      }
    }

    void visit(IndexAccessorExpression *expr) override
    {
      const bool receiverPlace = relativeReceiverPlace(expr, receiverName).has_value();
      recordRead(expr);
      if (!receiverPlace && expr->primary)
      {
        expr->primary->accept(this);
      }
      if (expr->accessor)
      {
        expr->accessor->accept(this);
      }
    }

    void visit(FunCallExpression *expr) override
    {
      if (relativeReceiverPlace(expr->primaryExpression.get(), receiverName).has_value())
      {
        unknown = true;
      }
      else if (expr->primaryExpression)
      {
        expr->primaryExpression->accept(this);
      }
      for (auto &arg : expr->arguments)
      {
        arg->accept(this);
      }
    }

    void visit(QualifiedTraitCallExpression *expr) override
    {
      if (expr->receiver && relativeReceiverPlace(expr->receiver.get(), receiverName).has_value())
      {
        unknown = true;
      }
      if (expr->receiver)
      {
        expr->receiver->accept(this);
      }
      for (auto &arg : expr->arguments)
      {
        arg->accept(this);
      }
    }

    void visit(NewObjectExpression *expr) override
    {
      for (auto &[_, value] : expr->properties)
      {
        value->accept(this);
      }
    }

    void visit(ArrayLiteral *expr) override
    {
      for (auto &element : expr->elements)
      {
        element->accept(this);
      }
    }

    void visit(TupleLiteral *expr) override
    {
      for (auto &element : expr->elements)
      {
        element->accept(this);
      }
    }

    void visit(SpreadExpression *expr) override
    {
      if (expr->expression)
      {
        expr->expression->accept(this);
      }
    }

    void visit(TypeCheckingExpression *expr) override
    {
      if (expr->value)
      {
        expr->value->accept(this);
      }
    }

    void visit(CastExpression *expr) override
    {
      if (expr->expression)
      {
        expr->expression->accept(this);
      }
    }
  };

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
    // Use tag() dispatch instead of dynamic_pointer_cast for better performance.
    auto safeTypeName = [](const CheckingRef<TypeInfo> &type, const auto &self) -> Str {
      if (!type) return "?";
      switch (type->tag())
      {
      case typeinfo_tag::TAGGED_UNION: return static_cast<const TaggedUnionType &>(*type).name;
      case typeinfo_tag::VARIANT:
      {
        const auto &v = static_cast<const VariantType &>(*type);
        return v.unionName + "." + v.variantName;
      }
      case typeinfo_tag::CUSTOMIZED:  return static_cast<const CustomizedType &>(*type).name;
      case typeinfo_tag::TYPE_ALIAS:  return static_cast<const TypeAliasType &>(*type).name;
      case typeinfo_tag::NEW_TYPE:    return static_cast<const NewTypeType &>(*type).name;
      case typeinfo_tag::REFERENCE:
        return "ref<" + self(static_cast<const ReferenceType &>(*type).referencedType, self) + ">";
      case typeinfo_tag::ARRAY:
      {
        const auto &a = static_cast<const ArrayType &>(*type);
        if (a.length) return "array<" + self(a.elementType, self) + ", " + self(a.length, self) + ">";
        return "array<" + self(a.elementType, self) + ", ?>";
      }
      case typeinfo_tag::VECTOR:
        return "vector<" + self(static_cast<const VectorType &>(*type).elementType, self) + ">";
      case typeinfo_tag::SPAN:
        return "span<" + self(static_cast<const SpanType &>(*type).elementType, self) + ">";
      case typeinfo_tag::RANGE:
        return "Range<" + self(static_cast<const RangeType &>(*type).elementType, self) + ">";
      case typeinfo_tag::TUPLE:
      {
        const auto &t = static_cast<const TupleType &>(*type);
        Str out = "(";
        for (size_t i = 0; i < t.elementTypes.size(); ++i)
        {
          if (i > 0) out += ", ";
          out += self(t.elementTypes[i], self);
        }
        return out + ")";
      }
      default: return type->repr();
      }
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
    case typeinfo_tag::VECTOR:
      return "vector";
    case typeinfo_tag::SPAN:
      return "span";
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
    // ── Submodules ──────────────────────────────────────────────────────
    TypeEnvironment env;
    TraitRegistry traits;

    // ── Type checking state ─────────────────────────────────────────────
    Map<Str, CheckingRef<TypeInfo>> type_index{};
    CheckingRef<TypeInfo> result;
    Vec<CheckingRef<TypeInfo>> spreadResult{};
    Vec<CheckingRef<TypeInfo>> contextRequirement;
    CheckingRef<TypeInfo> expectedType;
    Str currentModuleId = "default";
    Str activeGenericInstanceName;

    // ── Static global state ─────────────────────────────────────────────
    inline static Map<Str, Vec<TypeAliasDef *>> activeTypeAliasSpecializations{};
    inline static Map<Str, Vec<TypeAliasDef *>> preludeTypeAliasSpecializations{};
    inline static Map<Str, Vec<ConstDef *>> activeConstPredicates{};
    inline static Map<Str, Vec<ConstDef *>> preludeConstPredicates{};
    inline static Map<Str, Vec<FunctionDef *>> activeConstFunctions{};
    inline static Map<Str, Vec<FunctionDef *>> preludeConstFunctions{};
    inline static Set<Str> activeAutoTraits{};
    inline static Set<Str> preludeAutoTraits{};
    inline static Set<Str> activeDerivedTraitImplKeys{};
    inline static Vec<ASTRef<ASTNode>> retainedPreludeImportAsts{};

    // ── Module artifacts ────────────────────────────────────────────────
    struct TraitImplRecord
    {
      Str traitName;
      Str targetPattern;
      Str moduleId;
      Set<Str> genericParamNames;
      Vec<Str> whereBounds;
      Map<Str, Str> methods;
      ImplDef *definition = nullptr;
      TokenPosition pos;
    };
    struct ModuleArtifacts
    {
      TypeIndex exportedTypes;
      Vec<TraitImplRecord> exportedImpls;
      Set<Str> exports;
      Map<Str, Vec<TypeAliasDef *>> exportedTypeAliasSpecializations;
      Map<Str, Vec<ConstDef *>> exportedConstPredicates;
      Map<Str, Vec<FunctionDef *>> exportedConstFunctions;
    };
    Vec<TraitImplRecord> localTraitImpls;
    Map<Str, Vec<UseImplDecl *>> selectedTraitImpls;
    Set<Str> matchedSelectedTraitImpls;
    Vec<Str> modulePaths;
    inline static Map<Str, ModuleArtifacts> moduleArtifactsById{};
    inline static Set<Str> activeModuleChecks{};

    // ── Convenience aliases to env/traits members ───────────────────────
    // These allow existing code to work without mass-renaming locals -> env.locals
    Map<Str, CheckingRef<TypeInfo>> &locals = env.locals;
    Set<Str> &movedBindings = env.movedBindings;
    bool &allowMovedLvalueRead = env.allowMovedLvalueRead;
    Map<Str, Vec<Str>> &trait_impls_by_type = traits.trait_impls_by_type;
    Set<Str> &autoTraitNames = traits.autoTraitNames;
    Set<Str> &derivedTraitImplKeys = traits.derivedTraitImplKeys;
    Set<Str> &importedSymbolNames = env.importedSymbolNames;
    Set<Str> &importedImplNames = env.importedImplNames;
    Set<Str> &exportedImportNames = env.exportedImportNames;
    Map<Str, Str> &importAliases = env.importAliases;
    Vec<Str> &importedModuleIds = env.importedModuleIds;

    // ── Constants ───────────────────────────────────────────────────────
    static constexpr const char *WILDCARD_IMPORT_KEY = "$$wildcard_import$$";
    static constexpr const char *COPY_TRAIT_NAME = "Copy";
    static constexpr const char *CLONE_TRAIT_NAME = "Clone";
    static constexpr const char *DROP_TRAIT_NAME = "Drop";

    explicit TypeChecker(Map<Str, CheckingRef<TypeInfo>> locals, Vec<CheckingRef<TypeInfo>> contextRequirement = {},
                         CheckingRef<TypeInfo> expectedType = nullptr, Set<Str> movedBindings = {},
                         bool allowMovedLvalueRead = false, Str activeGenericInstanceName = "",
                         Vec<Str> modulePaths = {})
        : env(std::move(locals), std::move(movedBindings), allowMovedLvalueRead),
          contextRequirement(std::move(contextRequirement)), expectedType(std::move(expectedType)),
          activeGenericInstanceName(std::move(activeGenericInstanceName)), modulePaths(std::move(modulePaths))
    {
    }

    bool hasWildcardImportFlag() const { return locals.contains(WILDCARD_IMPORT_KEY); }

    auto resolveTypeAnnotationWithBindings(const TypeAnnotation *annotation,
                                           const Map<Str, CheckingRef<TypeInfo>> &bindings) const
        -> CheckingRef<TypeInfo>
    {
      if (!annotation)
      {
        return nullptr;
      }
      auto scope = locals;
      for (const auto &[name, type] : bindings)
      {
        scope[name] = type;
      }
      TypeChecker checker{scope, {}, nullptr, {}, false, activeGenericInstanceName, modulePaths};
      checker.trait_impls_by_type = trait_impls_by_type;
      checker.localTraitImpls = localTraitImpls;
      const_cast<TypeAnnotation *>(annotation)->accept(&checker);
      return checker.result;
    }

    auto sequenceElementType(const CheckingRef<TypeInfo> &type) const -> CheckingRef<TypeInfo>
    {
      if (auto builtin = builtin_sequence_element_type(type))
      {
        return builtin;
      }

      auto unwrapped = unwrap(deref_reference_type(type));
      auto custom = std::dynamic_pointer_cast<CustomizedType>(unwrapped);
      if (!custom)
      {
        return nullptr;
      }
      if (custom->memberFunctions.contains("size") && custom->memberFunctions.contains("get"))
      {
        return custom->memberFunctions.at("get")->returnType;
      }

      for (const auto &impl : localTraitImpls)
      {
        if (impl.traitName != "Sequence" || !impl.definition || !impl.definition->targetType ||
            !impl.definition->trait)
        {
          continue;
        }
        auto genericParamNames = impl.genericParamNames.empty()
                                     ? genericParamNameSet(impl.definition->genericParams)
                                     : impl.genericParamNames;
        Map<Str, CheckingRef<TypeInfo>> bindings;
        const bool patternMatches =
            typePatternMatch(impl.definition->targetType.get(), unwrapped, genericParamNames, bindings);
        if (!patternMatches &&
            (stripTypeInstanceSuffix(impl.definition->targetType->name) != stripTypeInstanceSuffix(custom->name) ||
             impl.definition->targetType->genericArgs.empty()))
        {
          continue;
        }
        if (!impl.definition->targetType->genericArgs.empty())
        {
          Vec<CheckingRef<TypeInfo>> actualArgs = custom->typeArgs;
          if (actualArgs.empty())
          {
            for (auto &argName : parseTypeInstanceArgs(custom->name))
            {
              if (auto primitive = PrimitiveType::from(argName))
              {
                actualArgs.push_back(primitive);
              }
              else
              {
                actualArgs.push_back(makecheck<CustomizedType>(argName));
              }
            }
          }
          (void)typePatternArgListMatches(impl.definition->targetType->genericArgs, actualArgs,
                                          genericParamNames, bindings);
        }
        if (impl.definition->trait->genericArgs.size() != 1)
        {
          continue;
        }
        auto traitElement = impl.definition->trait->genericArgs.front().get();
        if (traitElement && traitElement->genericArgs.empty() && traitElement->arguments.empty())
        {
          if (auto binding = bindings.find(traitElement->name); binding != bindings.end())
          {
            return binding->second;
          }
        }
        return resolveTypeAnnotationWithBindings(impl.definition->trait->genericArgs.front().get(), bindings);
      }

      for (const auto &impl : localTraitImpls)
      {
        if (impl.traitName != "Sequence" ||
            stripTypeInstanceSuffix(impl.targetPattern) != stripTypeInstanceSuffix(custom->name))
        {
          continue;
        }
        Vec<CheckingRef<TypeInfo>> actualArgs = custom->typeArgs;
        if (actualArgs.empty())
        {
          for (auto &argName : parseTypeInstanceArgs(custom->name))
          {
            if (auto primitive = PrimitiveType::from(argName))
            {
              actualArgs.push_back(primitive);
            }
            else
            {
              actualArgs.push_back(makecheck<CustomizedType>(argName));
            }
          }
        }
        auto patternArgs = parseTypeInstanceArgs(impl.targetPattern);
        if (actualArgs.empty() || patternArgs.size() != actualArgs.size())
        {
          continue;
        }
        if (impl.genericParamNames.size() == 1)
        {
          auto genericName = *impl.genericParamNames.begin();
          for (size_t i = 0; i < patternArgs.size(); ++i)
          {
            if (patternArgs[i] == genericName)
            {
              return actualArgs[i];
            }
          }
        }
        if (actualArgs.size() == 1)
        {
          return actualArgs.front();
        }
      }

      if (auto traitIt = custom->traitMemberFunctions.find("Sequence");
          traitIt != custom->traitMemberFunctions.end())
      {
        if (auto getIt = traitIt->second.find("get"); getIt != traitIt->second.end() && getIt->second)
        {
          return getIt->second->returnType;
        }
      }
      return nullptr;
    }

    auto isSequenceType(const CheckingRef<TypeInfo> &type) const -> bool
    {
      return sequenceElementType(type) != nullptr;
    }

    void rejectBorrowConflict(const Str &operation, const Str &place, TokenPosition pos) const
    {
      if (auto borrowed = borrowedConflict(movedBindings, place); borrowed.has_value())
      {
        throw TypeCheckingException("Cannot " + operation + " borrowed place: " + place +
                                        " conflicts with active ref " + *borrowed,
                                    pos);
      }
    }

    void recordBorrowAlias(const Str &alias, const std::optional<Str> &place)
    {
      clearMovedPlace(movedBindings, alias);
      if (place.has_value())
      {
        movedBindings.insert(borrowEntry(alias, *place));
      }
    }

    static void attachReceiverEffects(FunctionType &funType, FunctionDef *method, const Str &receiverName)
    {
      if (!method || !method->body || receiverName.empty())
      {
        funType.unknownPlaceEffects = true;
        return;
      }
      ReceiverEffectCollector collector{receiverName};
      method->body->accept(&collector);
      funType.placeEffects = std::move(collector.effects);
      funType.unknownPlaceEffects = collector.unknown;
    }

    void validateAndApplyMethodEffects(const FunctionType &funcType, const Str &receiverPlace, TokenPosition pos)
    {
      if (receiverPlace.empty())
      {
        return;
      }
      if (funcType.unknownPlaceEffects)
      {
        rejectBorrowConflict("call", receiverPlace, pos);
        if (movedBindings.contains(receiverPlace) || hasMovedDescendant(movedBindings, receiverPlace))
        {
          throw TypeCheckingException("Use after partial move: " + receiverPlace, pos);
        }
        return;
      }

      if (movedBindings.contains(receiverPlace))
      {
        throw TypeCheckingException("Use after move: " + receiverPlace, pos);
      }

      auto rejectMovedConflict = [&](const Str &absolute) {
        if (auto moved = movedAncestorOrSelf(movedBindings, absolute); moved.has_value())
        {
          throw TypeCheckingException("Use after move: " + *moved, pos);
        }
        if (hasMovedDescendant(movedBindings, absolute))
        {
          throw TypeCheckingException("Use after partial move: " + absolute, pos);
        }
      };

      for (const auto &effect : funcType.placeEffects)
      {
        auto absolute = absoluteReceiverPlace(receiverPlace, effect.place);
        switch (effect.kind)
        {
        case PlaceEffectKind::Read:
          rejectMovedConflict(absolute);
          break;
        case PlaceEffectKind::Move:
          rejectMovedConflict(absolute);
          rejectBorrowConflict("move", absolute, pos);
          movedBindings.insert(absolute);
          break;
        case PlaceEffectKind::Write:
          rejectBorrowConflict("assign", absolute, pos);
          clearMovedPlace(movedBindings, absolute);
          break;
        }
      }

    }

    static auto implSelectionKey(const Str &traitName, const Str &targetPattern) -> Str
    {
      return traitName + " for " + targetPattern;
    }

    static auto moduleIdFromPath(const Vec<Str> &modulePath) -> Str
    {
      return NG::module::canonical_module_id(modulePath);
    }

    static auto moduleIdTail(const Str &moduleId) -> Str
    {
      auto pos = moduleId.rfind('.');
      return pos == Str::npos ? moduleId : moduleId.substr(pos + 1);
    }

    auto selectionMatchesRecord(const UseImplDecl *selection, const TraitImplRecord &record) const -> bool
    {
      if (!selection || !selection->targetType || selection->targetType->repr() != record.targetPattern)
      {
        return false;
      }
      if (selection->moduleQualifier.empty())
      {
        return true;
      }
      auto qualifier = selection->moduleQualifier;
      if (auto alias = importAliases.find(qualifier); alias != importAliases.end())
      {
        qualifier = alias->second;
      }
      return qualifier == record.moduleId || qualifier == moduleIdTail(record.moduleId);
    }

    auto recordIsSelected(const TraitImplRecord &record) const -> bool
    {
      auto it = selectedTraitImpls.find(record.traitName);
      if (it == selectedTraitImpls.end())
      {
        return false;
      }
      return std::ranges::any_of(it->second, [&](auto *selection) {
        return selectionMatchesRecord(selection, record);
      });
    }

    auto selectedRecordKey(const TraitImplRecord &record) const -> Str
    {
      return record.moduleId + "::" + implSelectionKey(record.traitName, record.targetPattern);
    }

    static auto toModuleImplEvidence(const TraitImplRecord &record) -> NG::module::ModuleImplEvidence
    {
      return NG::module::ModuleImplEvidence{
          .traitName = record.traitName,
          .targetPattern = record.targetPattern,
          .moduleId = record.moduleId,
          .genericParamNames = record.genericParamNames,
          .whereBounds = record.whereBounds,
          .methods = record.methods,
          .definition = nullptr,
          .pos = record.pos,
      };
    }

    static auto toTraitImplRecord(const NG::module::ModuleImplEvidence &evidence) -> TraitImplRecord
    {
      return TraitImplRecord{
          .traitName = evidence.traitName,
          .targetPattern = evidence.targetPattern,
          .moduleId = evidence.moduleId,
          .genericParamNames = evidence.genericParamNames,
          .whereBounds = evidence.whereBounds,
          .methods = evidence.methods,
          .definition = evidence.definition.get(),
          .pos = evidence.pos,
      };
    }

    static auto hasPublishedTypeMetadata(const NG::module::ModuleArtifact &artifact) -> bool
    {
      return !artifact.typeIndex.empty() || !artifact.exports.types.empty() || !artifact.impls.empty() ||
             !artifact.traits.empty();
    }

    static auto toLocalModuleArtifacts(const NG::module::ModuleArtifact &artifact) -> ModuleArtifacts
    {
      ModuleArtifacts artifacts;
      artifacts.exportedTypes = artifact.exports.types;
      artifacts.exports = artifact.exports.declared;
      artifacts.exportedTypeAliasSpecializations = artifact.typeAliasSpecializations;
      artifacts.exportedConstPredicates = artifact.constPredicates;
      artifacts.exportedConstFunctions = artifact.constFunctions;
      for (const auto &impl : artifact.impls)
      {
        artifacts.exportedImpls.push_back(toTraitImplRecord(impl));
      }
      return artifacts;
    }

    static auto byValueAbstractReason(const CheckingRef<TypeInfo> &type) -> Str
    {
      auto unwrapped = unwrap(type);
      if (!unwrapped) return "";
      if (unwrapped->tag() == typeinfo_tag::CUSTOMIZED)
      {
        auto &custom = static_cast<CustomizedType &>(*unwrapped);
        if (custom.abstract) return "abstract type '" + custom.name + "'";
      }
      if (unwrapped->tag() == typeinfo_tag::TRAIT)
      {
        return "trait type '" + static_cast<TraitType &>(*unwrapped).name + "'";
      }
      return "";
    }

    static void rejectInvalidByValueType(const CheckingRef<TypeInfo> &type, const Str &context,
                                         const TokenPosition &pos)
    {
      auto reason = byValueAbstractReason(type);
      if (!reason.empty())
      {
        throw TypeCheckingException(reason + " cannot be used by value in " + context, pos);
      }
    }

    static auto typeKindArity(const CheckingRef<TypeInfo> &type) -> size_t
    {
      if (!type) return 0;
      if (type->tag() == typeinfo_tag::GENERIC_PARAM)
        return static_cast<GenericParamType &>(*type).kindArity;
      if (type->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
        return genericTypeConstructorFixedArity(static_cast<GenericTypeDef &>(*type));
      return 0;
    }

    static auto typeKindVariadicTail(const CheckingRef<TypeInfo> &type) -> bool
    {
      if (!type) return false;
      if (type->tag() == typeinfo_tag::GENERIC_PARAM)
        return static_cast<GenericParamType &>(*type).kindVariadicTail;
      if (type->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
        return genericTypeConstructorVariadicTail(static_cast<GenericTypeDef &>(*type));
      return false;
    }

    static void validateTypeArgumentKind(const Str &paramName, size_t expectedArity, bool expectedVariadicTail,
                                         const CheckingRef<TypeInfo> &actual, const TokenPosition &pos)
    {
      const size_t actualArity = typeKindArity(actual);
      const bool actualVariadicTail = typeKindVariadicTail(actual);
      if (expectedArity == actualArity && expectedVariadicTail == actualVariadicTail)
      {
        return;
      }
      if (expectedArity == 0 && !expectedVariadicTail)
      {
        throw TypeCheckingException("Generic parameter '" + paramName + "' expects a concrete type, got type constructor '" +
                                        (actual ? actual->repr() : "?") + "'",
                                    pos);
      }
      throw TypeCheckingException("Generic parameter '" + paramName + "' expects a type constructor with " +
                                      std::to_string(expectedArity) + " fixed argument(s)" +
                                      (expectedVariadicTail ? " and a variadic tail" : "") + ", got " +
                                      std::to_string(actualArity) + " fixed argument(s)" +
                                      (actualVariadicTail ? " and a variadic tail" : ""),
                                  pos);
    }

    static auto typeConstructorApplicationArityValid(size_t fixedArity, bool variadicTail, size_t actualArity) -> bool
    {
      return variadicTail ? actualArity >= fixedArity : actualArity == fixedArity;
    }

    auto resolveGenericTypeArgument(TypeAnnotation *annotation, size_t expectedArity, bool expectedVariadicTail,
                                    const Str &paramName) const -> CheckingRef<TypeInfo>
    {
      if (!annotation)
      {
        throw TypeCheckingException("Missing generic type argument");
      }
      if (expectedArity == 0 && !expectedVariadicTail)
      {
        TypeChecker checker{locals};
        annotation->accept(&checker);
        validateTypeArgumentKind(paramName, expectedArity, expectedVariadicTail, checker.result, annotation->pos);
        return checker.result;
      }

      if (!annotation->genericArgs.empty())
      {
        throw TypeCheckingException("Generic parameter '" + paramName +
                                        "' expects a type constructor, not an instantiated type: " +
                                        annotation->repr(),
                                    annotation->pos);
      }
      auto it = locals.find(annotation->name);
      if (it == locals.end())
      {
        throw TypeCheckingException("Unknown type constructor: " + annotation->name, annotation->pos);
      }
      validateTypeArgumentKind(paramName, expectedArity, expectedVariadicTail, it->second, annotation->pos);
      return it->second;
    }

    auto resolveGenericArgument(TypeAnnotation *annotation, bool expectedConst, const Str &expectedConstType,
                                size_t expectedArity, bool expectedVariadicTail,
                                const Str &paramName) const -> CheckingRef<TypeInfo>
    {
      auto resolved = resolveGenericTypeArgument(annotation, expectedArity, expectedVariadicTail, paramName);
      const bool isConstValue = resolved && resolved->tag() == typeinfo_tag::CONST_VALUE;
      if (expectedConst && !isConstValue)
      {
        throw TypeCheckingException("Generic parameter '" + paramName + "' expects a compile-time constant argument",
                                    annotation ? annotation->pos : TokenPosition{});
      }
      if (expectedConst && !expectedConstType.empty())
      {
        auto constValueType = std::dynamic_pointer_cast<ConstValueType>(unwrap(resolved));
        auto value = constValueFromType(constValueType);
        auto expectedType = type_from_repr(expectedConstType);
        if (value.has_value() && expectedType && !constValueMatchesType(*value, expectedType))
        {
          throw TypeCheckingException("Generic parameter '" + paramName + "' expects const " + expectedConstType,
                                      annotation ? annotation->pos : TokenPosition{});
        }
      }
      if (!expectedConst && isConstValue)
      {
        throw TypeCheckingException("Generic parameter '" + paramName + "' expects a type argument",
                                    annotation ? annotation->pos : TokenPosition{});
      }
      return resolved;
    }

    static auto isBuiltinLifecycleTraitName(const Str &name) -> bool
    {
      return name == COPY_TRAIT_NAME || name == CLONE_TRAIT_NAME || name == DROP_TRAIT_NAME;
    }

    static auto unitType() -> CheckingRef<TypeInfo>
    {
      return makecheck<PrimitiveType>(typeinfo_tag::UNIT);
    }

    static auto selfType() -> CheckingRef<TypeInfo>
    {
      return makecheck<GenericParamType>("Self");
    }

    static auto refSelfType() -> CheckingRef<TypeInfo>
    {
      return makecheck<ReferenceType>(selfType());
    }

    static auto isUniquePtrType(const CheckingRef<TypeInfo> &type) -> bool
    {
      auto custom = std::dynamic_pointer_cast<CustomizedType>(unwrap(type));
      return custom && stripTypeInstanceSuffix(custom->name) == "UniquePtr";
    }

    static auto isUniquePtrAnnotation(const TypeAnnotation *annotation) -> bool
    {
      return annotation && stripTypeInstanceSuffix(annotation->repr()) == "UniquePtr";
    }

    static auto builtinLifecycleTrait(Str name) -> CheckingRef<TraitType>
    {
      auto trait = makecheck<TraitType>(std::move(name), Vec<Str>{}, "std.prelude");
      if (trait->name == CLONE_TRAIT_NAME)
      {
        trait->methods["clone"] = makecheck<FunctionType>(selfType(), Vec<CheckingRef<TypeInfo>>{refSelfType()});
      }
      else if (trait->name == DROP_TRAIT_NAME)
      {
        trait->methods["drop"] = makecheck<FunctionType>(unitType(), Vec<CheckingRef<TypeInfo>>{refSelfType()});
      }
      trait->allMethods = trait->methods;
      return trait;
    }

    void installBuiltinLifecycleTraits()
    {
      for (const auto &name : Vec<Str>{COPY_TRAIT_NAME, CLONE_TRAIT_NAME, DROP_TRAIT_NAME})
      {
        if (!locals.contains(name))
        {
          locals[name] = builtinLifecycleTrait(name);
        }
      }
    }

    static auto isSelfTypeAnnotation(const TypeAnnotation *annotation) -> bool
    {
      return annotation && annotation->name == "Self" && annotation->genericArgs.empty();
    }

    static auto isRefSelfTypeAnnotation(const TypeAnnotation *annotation) -> bool
    {
      return annotation && annotation->name == "ref" && annotation->genericArgs.size() == 1 &&
             isSelfTypeAnnotation(annotation->genericArgs[0].get());
    }

    static auto isReceiverParam(const Param *param) -> bool
    {
      return param && (isSelfTypeAnnotation(param->annotatedType.get()) ||
                       isRefSelfTypeAnnotation(param->annotatedType.get()));
    }

    static auto isObjectSafeTraitMethod(const FunctionType &methodType) -> bool
    {
      if (methodType.parametersType.empty() || !is_ref_self_type(methodType.parametersType.front()))
      {
        return false;
      }
      if (contains_non_receiver_self(methodType.returnType))
      {
        return false;
      }
      for (size_t i = 1; i < methodType.parametersType.size(); ++i)
      {
        if (contains_non_receiver_self(methodType.parametersType[i]))
        {
          return false;
        }
      }
      return true;
    }

    static auto isObjectSafeTrait(const TraitType &trait) -> bool
    {
      if (!trait.typeParamNames.empty())
      {
        return false;
      }
      const auto &methods = trait.allMethods.empty() ? trait.methods : trait.allMethods;
      return std::ranges::all_of(methods, [](const auto &entry) {
        return isObjectSafeTraitMethod(*entry.second);
      });
    }

    static void validateObjectSafeTraitRefs(const CheckingRef<TypeInfo> &type)
    {
      auto unwrapped = unwrap(type);
      if (!unwrapped)
      {
        return;
      }
      switch (unwrapped->tag())
      {
      case typeinfo_tag::REFERENCE:
      {
        const auto &ref = static_cast<const ReferenceType &>(*unwrapped);
        auto refUnwrapped = unwrap(ref.referencedType);
        if (refUnwrapped && refUnwrapped->tag() == typeinfo_tag::TRAIT)
        {
          const auto &trait = static_cast<const TraitType &>(*refUnwrapped);
          if (!isObjectSafeTrait(trait))
          {
            throw TypeCheckingException("Trait is not object-safe for ref<" + trait.repr() + ">");
          }
        }
        validateObjectSafeTraitRefs(ref.referencedType);
        return;
      }
      case typeinfo_tag::ARRAY:
        validateObjectSafeTraitRefs(static_cast<const ArrayType &>(*unwrapped).elementType);
        return;
      case typeinfo_tag::VECTOR:
        validateObjectSafeTraitRefs(static_cast<const VectorType &>(*unwrapped).elementType);
        return;
      case typeinfo_tag::SPAN:
        validateObjectSafeTraitRefs(static_cast<const SpanType &>(*unwrapped).elementType);
        return;
      case typeinfo_tag::TUPLE:
        for (auto &element : static_cast<const TupleType &>(*unwrapped).elementTypes)
          validateObjectSafeTraitRefs(element);
        return;
      case typeinfo_tag::UNION:
        for (auto &member : static_cast<const UnionType &>(*unwrapped).types)
          validateObjectSafeTraitRefs(member);
        return;
      default:
        return;
      }
    }

    auto refTraitCoercionMatches(const TypeInfo &expected, const TypeInfo &actual) const -> bool
    {
      const auto &unwrappedExpected = unwrapAlias(expected);
      const auto &unwrappedActual = unwrapAlias(actual);
      if (unwrappedExpected.tag() != typeinfo_tag::REFERENCE || unwrappedActual.tag() != typeinfo_tag::REFERENCE)
      {
        return false;
      }
      const auto &expectedRef = static_cast<const ReferenceType &>(unwrappedExpected);
      const auto &actualRef = static_cast<const ReferenceType &>(unwrappedActual);
      if (!expectedRef.referencedType || !actualRef.referencedType)
      {
        return false;
      }
      auto unwrappedRef = unwrap(expectedRef.referencedType);
      if (!unwrappedRef || unwrappedRef->tag() != typeinfo_tag::TRAIT)
      {
        return false;
      }
      auto trait = std::static_pointer_cast<TraitType>(unwrappedRef);
      if (!isObjectSafeTrait(*trait))
      {
        return false;
      }
      return typeSatisfiesTrait(actualRef.referencedType, *trait);
    }

    auto typeMatches(const TypeInfo &expected, const TypeInfo &actual) const -> bool
    {
      return typeMatch(expected, actual) || refTraitCoercionMatches(expected, actual);
    }

    static auto genericTypeConstructorFixedArity(const GenericTypeDef &genericType) -> size_t
    {
      auto packIt = std::find(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(), true);
      if (packIt == genericType.typeParamIsPack.end())
      {
        return genericType.typeParamNames.size();
      }
      return static_cast<size_t>(std::distance(genericType.typeParamIsPack.begin(), packIt));
    }

    static auto genericTypeConstructorVariadicTail(const GenericTypeDef &genericType) -> bool
    {
      return std::any_of(genericType.typeParamIsPack.begin(), genericType.typeParamIsPack.end(), [](bool isPack) {
        return isPack;
      });
    }

    static auto typeSpecializationMatches(const TypeAliasDef &specialization,
                                          const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                          Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      if (!specialization.specializationPattern)
      {
        return false;
      }
      auto genericNames = genericParamNameSet(specialization.genericParams);
      return typePatternArgListMatches(specialization.specializationPattern->genericArgs, typeArgs,
                                       genericNames, bindings);
    }

    auto functionApplyWithCoercions(const FunctionType &funcType,
                                    const Vec<CheckingRef<TypeInfo>> &argumentTypes) const -> bool
    {
      auto requiredSize = std::count_if(funcType.parametersType.begin(), funcType.parametersType.end(),
                                        [](const auto &type) {
                                          return type->tag() != typeinfo_tag::PARAM_WITH_DEFAULT_VALUE;
                                        });
      if (argumentTypes.size() < static_cast<size_t>(requiredSize) ||
          argumentTypes.size() > funcType.parametersType.size())
      {
        return false;
      }
      for (size_t i = 0; i < argumentTypes.size(); ++i)
      {
        if (!typeMatches(*funcType.parametersType[i], *argumentTypes[i]))
        {
          return false;
        }
      }
      return true;
    }

    auto typeAliasSpecializationWhereMatches(const TypeAliasDef &specialization,
                                             const Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      if (specialization.whereBounds.empty())
      {
        return true;
      }
      TypeChecker whereChecker{locals};
      whereChecker.trait_impls_by_type = trait_impls_by_type;
      for (auto &[name, type] : bindings)
      {
        whereChecker.locals[name] = type;
      }
      for (auto &bound : specialization.whereBounds)
      {
        if (!bound)
        {
          return false;
        }
        if (bound->predicate)
        {
          auto value = whereChecker.tryEvalWherePredicate(bound->predicate.get());
          if (!value.has_value() || !*value)
          {
            return false;
          }
          continue;
        }
        if (!bound->subject || !bound->trait || !bound->subject->genericArgs.empty())
        {
          return false;
        }
        auto subIt = whereChecker.locals.find(bound->subject->name);
        auto traitIt = whereChecker.locals.find(bound->trait->repr());
        auto trait = traitIt == whereChecker.locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
        if (subIt == whereChecker.locals.end() || !trait || !whereChecker.typeSatisfiesTrait(subIt->second, *trait))
        {
          return false;
        }
      }
      return true;
    }

    static auto constSpecializationMatches(const ConstDef &specialization,
                                           const Vec<CheckingRef<TypeInfo>> &typeArgs,
                                           Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      if (!specialization.specializationPattern)
      {
        return false;
      }
      auto genericNames = genericParamNameSet(specialization.genericParams);
      return typePatternArgListMatches(specialization.specializationPattern->genericArgs, typeArgs,
                                       genericNames, bindings);
    }

    auto constSpecializationWhereMatches(const ConstDef &specialization,
                                         const Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      if (specialization.whereBounds.empty())
      {
        return true;
      }
      TypeChecker whereChecker{locals};
      whereChecker.trait_impls_by_type = trait_impls_by_type;
      for (auto &[name, type] : bindings)
      {
        whereChecker.locals[name] = type;
      }
      for (auto &bound : specialization.whereBounds)
      {
        if (!bound)
        {
          return false;
        }
        if (bound->predicate)
        {
          auto value = whereChecker.tryEvalWherePredicate(bound->predicate.get());
          if (!value.has_value() || !*value)
          {
            return false;
          }
          continue;
        }
        if (!bound->subject || !bound->trait || !bound->subject->genericArgs.empty())
        {
          return false;
        }
        auto subIt = whereChecker.locals.find(bound->subject->name);
        auto traitIt = whereChecker.locals.find(bound->trait->repr());
        auto trait = traitIt == whereChecker.locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
        if (subIt == whereChecker.locals.end() || !trait || !whereChecker.typeSatisfiesTrait(subIt->second, *trait))
        {
          return false;
        }
      }
      return true;
    }

    auto functionCandidateWhereMatches(const FunctionDef &candidate,
                                       const Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      if (candidate.whereBounds.empty())
      {
        return true;
      }
      TypeChecker whereChecker{locals};
      whereChecker.trait_impls_by_type = trait_impls_by_type;
      for (auto &[name, type] : bindings)
      {
        whereChecker.locals[name] = type;
      }
      for (auto &bound : candidate.whereBounds)
      {
        if (!bound)
        {
          return false;
        }
        if (bound->predicate)
        {
          auto value = whereChecker.tryEvalWherePredicate(bound->predicate.get());
          if (!value.has_value() || !*value)
          {
            return false;
          }
          continue;
        }
        if (!bound->subject || !bound->trait || !bound->subject->genericArgs.empty())
        {
          return false;
        }
        auto subIt = whereChecker.locals.find(bound->subject->name);
        auto traitIt = whereChecker.locals.find(bound->trait->repr());
        auto trait = traitIt == whereChecker.locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
        if (subIt == whereChecker.locals.end() || !trait || !whereChecker.typeSatisfiesTrait(subIt->second, *trait))
        {
          return false;
        }
      }
      return true;
    }

    auto functionCandidateMatches(FunctionDef &candidate, const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                  Map<Str, CheckingRef<TypeInfo>> &bindings) -> bool
    {
      auto genericNames = genericParamNameSet(candidate.genericParams);
      const bool hasPackTailParam = !candidate.params.empty() &&
                                    candidate.params.back() &&
                                    candidate.params.back()->annotatedType &&
                                    isPackTypePattern(candidate.params.back()->annotatedType.get(), genericNames);
      if (!hasPackTailParam && candidate.params.size() != argumentTypes.size())
      {
        return false;
      }
      if (hasPackTailParam && argumentTypes.size() + 1 < candidate.params.size())
      {
        return false;
      }
      for (auto &genericParam : candidate.genericParams)
      {
        if (genericParam)
        {
          if (genericParam->isConst)
          {
            bindings[genericParam->name] = makecheck<ConstValueType>(
                genericParam->name, genericParam->constType ? genericParam->constType->repr() : "", true);
          }
          else
          {
            bindings[genericParam->name] = makecheck<GenericParamType>(
                genericParam->name, typeParamBoundName(*genericParam), genericParam->isPack,
                genericParam->kindArity, genericParam->kindVariadicTail);
          }
        }
      }
      for (size_t i = 0; i < candidate.params.size(); ++i)
      {
        auto &param = candidate.params[i];
        if (!param || !param->annotatedType)
        {
          continue;
        }
        if (hasPackTailParam && i + 1 == candidate.params.size())
        {
          Vec<CheckingRef<TypeInfo>> packArgs;
          packArgs.insert(packArgs.end(), argumentTypes.begin() + static_cast<std::ptrdiff_t>(i), argumentTypes.end());
          return bindPackPattern(param->annotatedType.get(), packArgs, bindings) &&
                 functionCandidateWhereMatches(candidate, bindings);
        }
        if (!typePatternMatch(param->annotatedType.get(), argumentTypes[i], genericNames, bindings))
        {
          return false;
        }
      }
      return functionCandidateWhereMatches(candidate, bindings);
    }

    auto selectGenericFunctionCandidate(GenericDefType &genericDef,
                                        const Vec<CheckingRef<TypeInfo>> &argumentTypes,
                                        size_t explicitGenericArgCount = Str::npos) -> FunctionDef *
    {
      FunctionDef *best = genericDef.funcDef.get();
      size_t bestSpecificity = 0;
      bool found = false;
      for (auto &candidateRef : genericDef.overloads)
      {
        auto *candidate = candidateRef.get();
        if (!candidate)
        {
          continue;
        }
        if (explicitGenericArgCount != Str::npos &&
            candidate->genericParams.size() != explicitGenericArgCount)
        {
          continue;
        }
        Map<Str, CheckingRef<TypeInfo>> bindings;
        if (!functionCandidateMatches(*candidate, argumentTypes, bindings))
        {
          continue;
        }
        auto specificity = functionPatternSpecificity(*candidate);
        if (!found || specificity > bestSpecificity)
        {
          best = candidate;
          bestSpecificity = specificity;
          found = true;
        }
      }
      if (found)
      {
        return best;
      }
      if (genericDef.overloads.size() == 1 && genericDef.funcDef && !genericDef.funcDef->deleted)
      {
        return genericDef.funcDef.get();
      }
      return nullptr;
    }

    auto resolveAliasSpecializationBody(const TypeAliasDef &specialization,
                                        const Map<Str, CheckingRef<TypeInfo>> &bindings,
                                        const Map<Str, CheckingRef<TypeInfo>> &scope,
                                        const Str &instanceName) -> CheckingRef<TypeInfo>
    {
      if (specialization.deleted)
      {
        throw TypeCheckingException("Type specialization is deleted: " + specialization.repr(),
                                    specialization.pos);
      }
      if (specialization.abstract)
      {
        throw TypeCheckingException("Abstract type alias cannot be instantiated without a matching specialization: " +
                                        specialization.repr(),
                                    specialization.pos);
      }
      if (specialization.nativeOpaque)
      {
        return makecheck<CustomizedType>(instanceName, true, false, currentModuleId);
      }
      auto specializedScope = scope;
      for (auto &[name, type] : bindings)
      {
        specializedScope[name] = type;
      }
      TypeChecker checker{specializedScope};
      specialization.underlyingType->accept(&checker);
      return checker.result;
    }

    static auto whereBoundPatterns(const Vec<ASTRef<TraitBound>> &bounds) -> Vec<Str>
    {
      Vec<Str> patterns;
      for (auto &bound : bounds)
      {
        if (bound && bound->subject && bound->trait)
        {
          patterns.push_back(bound->subject->repr() + ": " + bound->trait->repr());
        }
        else if (bound && bound->predicate)
        {
          patterns.push_back(bound->predicate->repr());
        }
      }
      return patterns;
    }

    static auto implMethodMap(const ImplDef &implDef, const TraitType &trait) -> Map<Str, Str>
    {
      Map<Str, Str> methods;
      for (auto &method : implDef.methods)
      {
        methods[method->funName] = trait.name + "::" + method->funName;
      }
      return methods;
    }

    static auto isGenericPatternWildcard(const TypeAnnotation *annotation, const Set<Str> &genericParamNames) -> bool
    {
      return annotation && annotation->genericArgs.empty() && annotation->arguments.empty() &&
             genericParamNames.contains(annotation->name);
    }

    static auto typePatternChildren(const TypeAnnotation *annotation) -> Vec<const TypeAnnotation *>
    {
      Vec<const TypeAnnotation *> children;
      if (!annotation)
      {
        return children;
      }
      for (auto &arg : annotation->genericArgs)
      {
        children.push_back(arg.get());
      }
      for (auto &arg : annotation->arguments)
      {
        if (auto child = dynamic_ast_cast<TypeAnnotation>(arg))
        {
          children.push_back(child.get());
        }
      }
      return children;
    }

    static auto typePatternSpecificity(const TypeAnnotation *annotation,
                                       const Set<Str> &genericParamNames) -> size_t
    {
      if (!annotation || isGenericPatternWildcard(annotation, genericParamNames))
      {
        return 0;
      }
      size_t score = 1;
      for (auto *child : typePatternChildren(annotation))
      {
        score += typePatternSpecificity(child, genericParamNames);
      }
      return score;
    }

    static auto constSpecializationSpecificity(const ConstDef &specialization) -> size_t
    {
      if (!specialization.specializationPattern)
      {
        return 0;
      }
      auto genericNames = genericParamNameSet(specialization.genericParams);
      size_t score = 0;
      for (auto &arg : specialization.specializationPattern->genericArgs)
      {
        score += typePatternSpecificity(arg.get(), genericNames);
      }
      return score;
    }

    static auto typeAliasSpecializationSpecificity(const TypeAliasDef &specialization) -> size_t
    {
      if (!specialization.specializationPattern)
      {
        return 0;
      }
      auto genericNames = genericParamNameSet(specialization.genericParams);
      size_t score = 0;
      for (auto &arg : specialization.specializationPattern->genericArgs)
      {
        score += typePatternSpecificity(arg.get(), genericNames);
      }
      return score;
    }

    auto selectTypeAliasSpecialization(const Str &name,
                                       const Vec<CheckingRef<TypeInfo>> &typeArgs)
        -> std::optional<std::pair<TypeAliasDef *, Map<Str, CheckingRef<TypeInfo>>>>
    {
      auto it = activeTypeAliasSpecializations.find(name);
      if (it == activeTypeAliasSpecializations.end())
      {
        return std::nullopt;
      }

      TypeAliasDef *bestSpecialization = nullptr;
      Map<Str, CheckingRef<TypeInfo>> bestBindings;
      size_t bestSpecificity = 0;
      for (auto *candidate : it->second)
      {
        Map<Str, CheckingRef<TypeInfo>> bindings;
        if (!candidate || !typeSpecializationMatches(*candidate, typeArgs, bindings) ||
            !typeAliasSpecializationWhereMatches(*candidate, bindings))
        {
          continue;
        }
        auto specificity = typeAliasSpecializationSpecificity(*candidate);
        if (!bestSpecialization || specificity > bestSpecificity)
        {
          bestSpecialization = candidate;
          bestBindings = std::move(bindings);
          bestSpecificity = specificity;
        }
      }

      if (!bestSpecialization)
      {
        return std::nullopt;
      }
      return std::make_pair(bestSpecialization, std::move(bestBindings));
    }

    static auto typePatternsMayOverlap(const TypeAnnotation *left, const Set<Str> &leftGenericParams,
                                       const TypeAnnotation *right, const Set<Str> &rightGenericParams) -> bool
    {
      if (!left || !right)
      {
        return false;
      }
      if (isGenericPatternWildcard(left, leftGenericParams) ||
          isGenericPatternWildcard(right, rightGenericParams))
      {
        return true;
      }
      if (left->name != right->name)
      {
        return false;
      }
      auto leftChildren = typePatternChildren(left);
      auto rightChildren = typePatternChildren(right);
      if (leftChildren.size() != rightChildren.size())
      {
        return leftChildren.empty() && rightChildren.empty();
      }
      for (size_t i = 0; i < leftChildren.size(); ++i)
      {
        if (!typePatternsMayOverlap(leftChildren[i], leftGenericParams, rightChildren[i], rightGenericParams))
        {
          return false;
        }
      }
      return true;
    }

    auto registerTraitImplRecord(TraitImplRecord candidate, const TokenPosition &pos) -> bool
    {
      const bool candidateSelected = recordIsSelected(candidate);

      for (const auto &existing : localTraitImpls)
      {
        if (existing.traitName != candidate.traitName)
        {
          continue;
        }
        const bool overlaps = existing.definition && candidate.definition
                                  ? typePatternsMayOverlap(existing.definition->targetType.get(),
                                                           existing.genericParamNames,
                                                           candidate.definition->targetType.get(),
                                                           candidate.genericParamNames)
                                  : existing.targetPattern == candidate.targetPattern;
        if (!overlaps)
        {
          continue;
        }
        const bool existingSelected = recordIsSelected(existing);
        if (candidateSelected && !existingSelected)
        {
          matchedSelectedTraitImpls.insert(selectedRecordKey(candidate));
          continue;
        }
        if (!candidateSelected && existingSelected)
        {
          return false;
        }
        if (existing.targetPattern == candidate.targetPattern)
        {
          throw TypeCheckingException("Duplicate impl for trait '" + candidate.traitName +
                                          "' and type '" + candidate.targetPattern + "' from modules '" +
                                          existing.moduleId + "' and '" + candidate.moduleId + "'",
                                      pos);
        }
        throw TypeCheckingException("Overlapping impl for trait '" + candidate.traitName +
                                        "' between '" + existing.targetPattern + "' and '" +
                                        candidate.targetPattern + "' from modules '" + existing.moduleId +
                                        "' and '" + candidate.moduleId + "'",
                                    pos);
      }

      if (!candidateSelected)
      {
        for (auto *selection : selectedTraitImpls[candidate.traitName])
        {
          if (!selection || !selection->targetType ||
              selection->targetType->repr() == candidate.targetPattern)
          {
            continue;
          }
          Set<Str> emptyGenericParams;
          const bool overlapsSelection = candidate.definition
                                             ? typePatternsMayOverlap(candidate.definition->targetType.get(),
                                                                      candidate.genericParamNames,
                                                                      selection->targetType.get(),
                                                                      emptyGenericParams)
                                             : candidate.targetPattern == selection->targetType->repr();
          if (overlapsSelection)
          {
            return false;
          }
        }
      }

      localTraitImpls.push_back(std::move(candidate));
      if (candidateSelected)
      {
        matchedSelectedTraitImpls.insert(selectedRecordKey(localTraitImpls.back()));
      }
      return true;
    }

    auto registerLocalTraitImpl(ImplDef *implDef, const TraitType &trait) -> bool
    {
      return registerTraitImplRecord(TraitImplRecord{
                                         .traitName = trait.name,
                                         .targetPattern = implDef->targetType ? implDef->targetType->repr() : "",
                                         .moduleId = currentModuleId,
                                         .genericParamNames = genericParamNameSet(implDef->genericParams),
                                         .whereBounds = whereBoundPatterns(implDef->whereBounds),
                                         .methods = implMethodMap(*implDef, trait),
                                         .definition = implDef,
                                         .pos = implDef->pos,
                                     },
                                     implDef->pos);
    }

    void addGenericParamsToScope(Map<Str, CheckingRef<TypeInfo>> &scope,
                                 const Vec<ASTRef<GenericParam>> &genericParams) const
    {
      for (auto &gp : genericParams)
      {
        if (gp->isConst)
        {
          scope[gp->name] = makecheck<ConstValueType>(gp->name, gp->constType ? gp->constType->repr() : "", true);
        }
        else
        {
          scope[gp->name] = makecheck<GenericParamType>(gp->name, typeParamBoundName(*gp), gp->isPack,
                                                        gp->kindArity, gp->kindVariadicTail);
        }
      }
    }

    void addWhereBoundsToScope(Map<Str, CheckingRef<TypeInfo>> &scope,
                               const Vec<ASTRef<TraitBound>> &bounds) const
    {
      for (auto &bound : bounds)
      {
        if (bound && bound->predicate)
        {
          continue;
        }
        if (!bound || !bound->subject || !bound->trait || !bound->subject->genericArgs.empty())
        {
          throw TypeCheckingException("Phase 1 where clauses only support `T: Trait` bounds");
        }
        auto it = scope.find(bound->subject->name);
        if (it == scope.end())
        {
          throw TypeCheckingException("Unknown type parameter in where clause: " + bound->subject->name,
                                      bound->pos);
        }
        auto generic = std::dynamic_pointer_cast<GenericParamType>(it->second);
        if (!generic)
        {
          throw TypeCheckingException("Where clause subject must be a generic parameter: " +
                                      bound->subject->repr(), bound->pos);
        }
        generic->bound = bound->trait->repr();
      }
    }

    auto resolveConstPredicateTypeArg(TypeAnnotation *annotation,
                                      const Map<Str, CheckingRef<TypeInfo>> &scope) -> CheckingRef<TypeInfo>
    {
      if (!annotation)
      {
        return nullptr;
      }
      TypeChecker checker{scope};
      annotation->accept(&checker);
      return checker.result;
    }

    auto tryEvalNativeConstPredicate(const Str &name, const Vec<CheckingRef<TypeInfo>> &typeArgs)
        -> std::optional<ConstValue>
    {
      if (typeArgs.size() != 1 || !typeArgs[0])
      {
        return std::nullopt;
      }
      auto type = unwrap(typeArgs[0]);
      if (name == "is_ref")
      {
        return std::dynamic_pointer_cast<ReferenceType>(type) != nullptr;
      }
      if (name == "is_trait")
      {
        return std::dynamic_pointer_cast<TraitType>(type) != nullptr;
      }
      if (name == "is_abstract")
      {
        if (std::dynamic_pointer_cast<TraitType>(type))
        {
          return true;
        }
        if (auto custom = std::dynamic_pointer_cast<CustomizedType>(type))
        {
          return custom->abstract;
        }
        return false;
      }
      return std::nullopt;
    }

    auto tryEvalConstPredicateCall(const FunCallExpression *funCall) -> std::optional<ConstValue>
    {
      auto idExpr = dynamic_ast_cast<IdExpression>(funCall->primaryExpression);
      if (!idExpr || !activeConstPredicates.contains(idExpr->id))
      {
        return std::nullopt;
      }

      Vec<CheckingRef<TypeInfo>> typeArgs;
      if (funCall->genericArgs.empty())
      {
        return std::nullopt;
      }
      typeArgs.reserve(funCall->genericArgs.size());
      for (auto &arg : funCall->genericArgs)
      {
        auto resolved = resolveConstPredicateTypeArg(arg.get(), locals);
        if (!resolved)
        {
          return std::nullopt;
        }
        if (auto varargs = std::dynamic_pointer_cast<VarargsType>(unwrap(resolved)))
        {
          typeArgs.insert(typeArgs.end(), varargs->elementTypes.begin(), varargs->elementTypes.end());
        }
        else
        {
          typeArgs.push_back(resolved);
        }
      }

      ConstDef *primary = nullptr;
      ConstDef *bestSpecialization = nullptr;
      Map<Str, CheckingRef<TypeInfo>> bestBindings;
      size_t bestSpecificity = 0;
      if (auto it = activeConstPredicates.find(idExpr->id); it != activeConstPredicates.end())
      {
        for (auto *candidate : it->second)
        {
          if (!candidate)
          {
            continue;
          }
          if (!candidate->specializationPattern)
          {
            primary = candidate;
            continue;
          }
          Map<Str, CheckingRef<TypeInfo>> bindings;
          if (constSpecializationMatches(*candidate, typeArgs, bindings) &&
              constSpecializationWhereMatches(*candidate, bindings))
          {
            auto specificity = constSpecializationSpecificity(*candidate);
            if (!bestSpecialization || specificity > bestSpecificity)
            {
              bestSpecialization = candidate;
              bestBindings = std::move(bindings);
              bestSpecificity = specificity;
            }
          }
        }
      }

      if (bestSpecialization)
      {
        if (bestSpecialization->deleted)
        {
          throw TypeCheckingException("Const specialization is deleted: " + bestSpecialization->repr(),
                                      bestSpecialization->pos);
        }
        TypeChecker valueChecker{locals};
        valueChecker.trait_impls_by_type = trait_impls_by_type;
        for (auto &[name, type] : bestBindings)
        {
          valueChecker.locals[name] = type;
        }
        if (bestSpecialization->native)
        {
          return tryEvalNativeConstPredicate(bestSpecialization->constName, typeArgs);
        }
        return valueChecker.tryEvalConstValue(bestSpecialization->value.get());
      }

      if (primary)
      {
        if (primary->deleted)
        {
          throw TypeCheckingException("Const predicate is deleted: " + primary->repr(), primary->pos);
        }
        TypeChecker valueChecker{locals};
        valueChecker.trait_impls_by_type = trait_impls_by_type;
        for (size_t i = 0; i < primary->genericParams.size() && i < typeArgs.size(); ++i)
        {
          valueChecker.locals[primary->genericParams[i]->name] = typeArgs[i];
        }
        if (primary->native)
        {
          return tryEvalNativeConstPredicate(primary->constName, typeArgs);
        }
        return valueChecker.tryEvalConstValue(primary->value.get());
      }
      return std::nullopt;
    }

    auto tryEvalWherePredicate(Expression *expr) -> std::optional<bool>
    {
      if (auto *funCall = dynamic_cast<FunCallExpression *>(expr))
      {
        auto value = tryEvalConstPredicateCall(funCall);
        if (value.has_value() && std::holds_alternative<bool>(*value))
        {
          return std::get<bool>(*value);
        }
        value = tryEvalConstValue(expr);
        if (value.has_value() && std::holds_alternative<bool>(*value))
        {
          return std::get<bool>(*value);
        }
        return std::nullopt;
      }
      if (auto *unaryExpr = dynamic_cast<UnaryExpression *>(expr))
      {
        auto operand = tryEvalWherePredicate(unaryExpr->operand.get());
        if (operand.has_value() && unaryExpr->optr && unaryExpr->optr->type == TokenType::NOT)
        {
          return !*operand;
        }
        return std::nullopt;
      }
      if (auto *binaryExpr = dynamic_cast<BinaryExpression *>(expr))
      {
        auto left = tryEvalWherePredicate(binaryExpr->left.get());
        auto right = tryEvalWherePredicate(binaryExpr->right.get());
        if (!left.has_value() || !right.has_value())
        {
          return std::nullopt;
        }
        if (binaryExpr->optr->type == TokenType::AND)
        {
          return *left && *right;
        }
        if (binaryExpr->optr->type == TokenType::OR)
        {
          return *left || *right;
        }
      }
      return tryEvalConstCondition(expr);
    }

    static auto constValueTypeName(const ConstValue &value) -> Str
    {
      if (std::holds_alternative<bool>(value))
      {
        return "bool";
      }
      if (std::holds_alternative<Str>(value))
      {
        return "string";
      }
      return "i64";
    }

    static auto primitiveTypeForConstValue(const ConstValue &value) -> CheckingRef<TypeInfo>
    {
      if (std::holds_alternative<bool>(value))
      {
        return makecheck<PrimitiveType>(typeinfo_tag::BOOL);
      }
      if (std::holds_alternative<Str>(value))
      {
        return makecheck<PrimitiveType>(typeinfo_tag::STRING);
      }
      return makecheck<PrimitiveType>(typeinfo_tag::I64);
    }

    static auto constValueLiteral(const ConstValue &value) -> Str
    {
      if (std::holds_alternative<bool>(value))
      {
        return std::get<bool>(value) ? "true" : "false";
      }
      if (std::holds_alternative<Str>(value))
      {
        return std::get<Str>(value);
      }
      return std::to_string(std::get<int64_t>(value));
    }

    static auto constValueFromType(const CheckingRef<TypeInfo> &type) -> std::optional<ConstValue>
    {
      auto constValue = std::dynamic_pointer_cast<ConstValueType>(unwrap(type));
      if (!constValue || constValue->isParam)
      {
        return std::nullopt;
      }
      if (constValue->valueType == "bool" || constValue->value == "true" || constValue->value == "false")
      {
        if (constValue->value == "true")
        {
          return true;
        }
        if (constValue->value == "false")
        {
          return false;
        }
        return std::nullopt;
      }
      if (constValue->valueType == "string")
      {
        return constValue->value;
      }
      try
      {
        return static_cast<int64_t>(std::stoll(constValue->value));
      }
      catch (const std::exception &)
      {
        return std::nullopt;
      }
    }

    auto predicateHasUnresolvedGeneric(Expression *expr) const -> bool
    {
      if (!expr)
      {
        return false;
      }
      if (auto *idExpr = dynamic_cast<IdExpression *>(expr))
      {
        auto it = locals.find(idExpr->id);
        if (it == locals.end())
        {
          return false;
        }
        if (auto constValue = std::dynamic_pointer_cast<ConstValueType>(unwrap(it->second)))
        {
          return constValue->isParam;
        }
        return unwrap(it->second)->tag() == typeinfo_tag::GENERIC_PARAM;
      }
      if (auto *funCall = dynamic_cast<FunCallExpression *>(expr))
      {
        for (auto &arg : funCall->genericArgs)
        {
          if (typeAnnotationHasUnresolvedGeneric(arg.get()))
          {
            return true;
          }
        }
        for (auto &arg : funCall->arguments)
        {
          if (predicateHasUnresolvedGeneric(arg.get()))
          {
            return true;
          }
        }
        return false;
      }
      if (auto *unary = dynamic_cast<UnaryExpression *>(expr))
      {
        return predicateHasUnresolvedGeneric(unary->operand.get());
      }
      if (auto *binary = dynamic_cast<BinaryExpression *>(expr))
      {
        return predicateHasUnresolvedGeneric(binary->left.get()) ||
               predicateHasUnresolvedGeneric(binary->right.get());
      }
      return false;
    }

    auto typeAnnotationHasUnresolvedGeneric(TypeAnnotation *annotation) const -> bool
    {
      if (!annotation)
      {
        return false;
      }
      if (auto it = locals.find(annotation->name); it != locals.end())
      {
        auto unwrapped = unwrap(it->second);
        if (unwrapped && (unwrapped->tag() == typeinfo_tag::GENERIC_PARAM ||
                          unwrapped->tag() == typeinfo_tag::CONST_VALUE))
        {
          return true;
        }
      }
      return std::ranges::any_of(annotation->genericArgs, [&](const auto &arg) {
        return typeAnnotationHasUnresolvedGeneric(arg.get());
      });
    }

    void validateConstFunctionExpression(Expression *expr)
    {
      if (!expr)
      {
        return;
      }
      if (auto *call = dynamic_cast<FunCallExpression *>(expr))
      {
        auto id = dynamic_cast<IdExpression *>(call->primaryExpression.get());
        if (!id || !activeConstFunctions.contains(id->id))
        {
          throw TypeCheckingException("Non-const function cannot be called from const function: " +
                                          call->primaryExpression->repr(),
                                      call->pos);
        }
        for (auto &arg : call->arguments)
        {
          validateConstFunctionExpression(arg.get());
        }
        return;
      }
      if (auto *unary = dynamic_cast<UnaryExpression *>(expr))
      {
        validateConstFunctionExpression(unary->operand.get());
        return;
      }
      if (auto *binary = dynamic_cast<BinaryExpression *>(expr))
      {
        validateConstFunctionExpression(binary->left.get());
        validateConstFunctionExpression(binary->right.get());
        return;
      }
    }

    void validateConstFunctionStatement(Statement *stmt)
    {
      if (!stmt)
      {
        return;
      }
      if (auto *ret = dynamic_cast<ReturnStatement *>(stmt))
      {
        validateConstFunctionExpression(ret->expression.get());
        return;
      }
      if (auto *compound = dynamic_cast<CompoundStatement *>(stmt))
      {
        for (auto &child : compound->statements)
        {
          validateConstFunctionStatement(child.get());
        }
        return;
      }
      if (auto *val = dynamic_cast<ValDefStatement *>(stmt))
      {
        validateConstFunctionExpression(val->value.get());
        return;
      }
      if (auto *ifStmt = dynamic_cast<IfStatement *>(stmt))
      {
        validateConstFunctionExpression(ifStmt->testing.get());
        validateConstFunctionStatement(ifStmt->consequence.get());
        validateConstFunctionStatement(ifStmt->alternative.get());
        return;
      }
      throw TypeCheckingException("Unsupported statement in const function: " + stmt->repr(), stmt->pos);
    }

    void validateWherePredicates(const Vec<ASTRef<TraitBound>> &bounds, const TokenPosition &pos)
    {
      for (auto &bound : bounds)
      {
        if (!bound || !bound->predicate)
        {
          continue;
        }
        auto value = tryEvalWherePredicate(bound->predicate.get());
        if (!value.has_value())
        {
          if (predicateHasUnresolvedGeneric(bound->predicate.get()))
          {
            continue;
          }
          throw TypeCheckingException("Unable to evaluate where predicate: " + bound->predicate->repr(),
                                      bound->pos);
        }
        if (!*value)
        {
          throw TypeCheckingException("Where predicate is not satisfied: " + bound->predicate->repr(), pos);
        }
      }
    }

    auto functionTypeFor(FunctionDef *funDef, const Map<Str, CheckingRef<TypeInfo>> &scope) -> CheckingRef<FunctionType>
    {
      TypeChecker checker{scope};
      Vec<CheckingRef<TypeInfo>> paramTypes;
      for (auto param : funDef->params)
      {
        param->accept(&checker);
        rejectInvalidByValueType(checker.result, "function parameter '" + param->paramName + "'", param->pos);
        paramTypes.push_back(checker.result);
      }

      CheckingRef<TypeInfo> returnType = makecheck<Untyped>();
      if (funDef->returnType)
      {
        funDef->returnType->accept(&checker);
        returnType = checker.result;
        rejectInvalidByValueType(returnType, "function return type", funDef->returnType->pos);
      }
      auto funType = makecheck<FunctionType>(returnType, paramTypes);
      funType->deleted = funDef->deleted;
      if (funDef->deleted)
      {
        funType->deletedRepr = funDef->repr();
      }
      if (!funDef->params.empty() && isReceiverParam(funDef->params.front().get()))
      {
        attachReceiverEffects(*funType, funDef, funDef->params.front()->paramName);
      }
      return funType;
    }

    static auto functionSignaturesMatch(const FunctionType &expected, const FunctionType &actual) -> bool
    {
      if (expected.parametersType.size() != actual.parametersType.size())
      {
        return false;
      }
      if (!typeMatch(*expected.returnType, *actual.returnType))
      {
        return false;
      }
      for (size_t i = 0; i < expected.parametersType.size(); ++i)
      {
        if (!typeMatch(*expected.parametersType[i], *actual.parametersType[i]))
        {
          return false;
        }
      }
      return true;
    }

    static auto typeMatchesReplacingSelf(const CheckingRef<TypeInfo> &expected,
                                         const CheckingRef<TypeInfo> &actual,
                                         const CheckingRef<TypeInfo> &selfType) -> bool
    {
      if (auto generic = std::dynamic_pointer_cast<GenericParamType>(unwrap(expected));
          generic && generic->name == "Self")
      {
        return typeMatch(*selfType, *actual);
      }
      auto expectedRef = std::dynamic_pointer_cast<ReferenceType>(unwrap(expected));
      auto actualRef = std::dynamic_pointer_cast<ReferenceType>(unwrap(actual));
      if (expectedRef || actualRef)
      {
        return expectedRef && actualRef &&
               typeMatchesReplacingSelf(expectedRef->referencedType, actualRef->referencedType, selfType);
      }
      return typeMatch(*expected, *actual);
    }

    static auto functionSignaturesMatchReplacingSelf(const FunctionType &expected,
                                                     const FunctionType &actual,
                                                     const CheckingRef<TypeInfo> &selfType) -> bool
    {
      if (expected.parametersType.size() != actual.parametersType.size())
      {
        return false;
      }
      if (!typeMatchesReplacingSelf(expected.returnType, actual.returnType, selfType))
      {
        return false;
      }
      for (size_t i = 0; i < expected.parametersType.size(); ++i)
      {
        if (!typeMatchesReplacingSelf(expected.parametersType[i], actual.parametersType[i], selfType))
        {
          return false;
        }
      }
      return true;
    }

    auto typeSatisfiesAutoTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait,
                                Set<Str> &seen) const -> bool
    {
      auto candidate = unwrap(type);
      if (!candidate)
      {
        return false;
      }
      if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
          candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
      {
        return true;
      }
      switch (candidate->tag())
      {
      case typeinfo_tag::REFERENCE:
        return typeSatisfiesAutoTrait(static_cast<const ReferenceType &>(*candidate).referencedType, trait, seen);
      case typeinfo_tag::TUPLE:
        return std::ranges::all_of(static_cast<const TupleType &>(*candidate).elementTypes, [&](const auto &element) {
          return typeSatisfiesAutoTrait(element, trait, seen);
        });
      case typeinfo_tag::ARRAY:
        return typeSatisfiesAutoTrait(static_cast<const ArrayType &>(*candidate).elementType, trait, seen);
      case typeinfo_tag::VECTOR:
        return typeSatisfiesAutoTrait(static_cast<const VectorType &>(*candidate).elementType, trait, seen);
      case typeinfo_tag::SPAN:
        return typeSatisfiesAutoTrait(static_cast<const SpanType &>(*candidate).elementType, trait, seen);
      default:
        break;
      }
      auto custom = std::dynamic_pointer_cast<CustomizedType>(candidate);
      if (!custom)
      {
        return false;
      }
      if (auto implIt = trait_impls_by_type.find(custom->name); implIt != trait_impls_by_type.end())
      {
        if (std::ranges::any_of(implIt->second, [&](const Str &implemented) {
              return implemented == trait.name || traitImplies(implemented, trait.name);
            }))
        {
          return true;
        }
      }
      if (!seen.insert(custom->name + "::" + trait.name).second)
      {
        return true;
      }
      for (const auto &[_, fieldType] : custom->properties)
      {
        if (!typeSatisfiesAutoTrait(fieldType, trait, seen))
        {
          return false;
        }
      }
      return true;
    }

    auto typeSatisfiesAutoTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait) const -> bool
    {
      Set<Str> seen;
      return typeSatisfiesAutoTrait(type, trait, seen);
    }

    auto typeCanDeriveTrait(const CheckingRef<TypeInfo> &type, const Str &traitName,
                            Set<Str> &seen) const -> bool
    {
      auto candidate = unwrap(type);
      if (!candidate)
      {
        return false;
      }
      if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
          candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
      {
        return true;
      }
      switch (candidate->tag())
      {
      case typeinfo_tag::REFERENCE:
        return traitName == COPY_TRAIT_NAME ||
               typeCanDeriveTrait(static_cast<const ReferenceType &>(*candidate).referencedType, traitName, seen);
      case typeinfo_tag::TUPLE:
        return std::ranges::all_of(static_cast<const TupleType &>(*candidate).elementTypes, [&](const auto &element) {
          return typeCanDeriveTrait(element, traitName, seen);
        });
      case typeinfo_tag::ARRAY:
        return typeCanDeriveTrait(static_cast<const ArrayType &>(*candidate).elementType, traitName, seen);
      case typeinfo_tag::SPAN:
        return typeCanDeriveTrait(static_cast<const SpanType &>(*candidate).elementType, traitName, seen);
      case typeinfo_tag::VECTOR:
        return false;
      default:
        break;
      }
      auto custom = std::dynamic_pointer_cast<CustomizedType>(candidate);
      if (!custom)
      {
        return false;
      }
      if (!seen.insert(custom->name + "::" + traitName).second)
      {
        return true;
      }
      if (auto implIt = trait_impls_by_type.find(custom->name); implIt != trait_impls_by_type.end())
      {
        if (std::ranges::find(implIt->second, traitName) != implIt->second.end())
        {
          return true;
        }
        if (traitName == COPY_TRAIT_NAME &&
            std::ranges::find(implIt->second, DROP_TRAIT_NAME) != implIt->second.end())
        {
          return false;
        }
      }
      return std::ranges::all_of(custom->properties, [&](const auto &entry) {
        return typeCanDeriveTrait(entry.second, traitName, seen);
      });
    }

    auto typeCanDeriveTrait(const CheckingRef<TypeInfo> &type, const Str &traitName) const -> bool
    {
      Set<Str> seen;
      return typeCanDeriveTrait(type, traitName, seen);
    }

    auto typeSatisfiesTrait(const CheckingRef<TypeInfo> &type, const TraitType &trait) const -> bool
    {
      if (activeAutoTraits.contains(trait.name))
      {
        return typeSatisfiesAutoTrait(type, trait);
      }
      auto candidate = unwrap(type);
      if (trait.name == COPY_TRAIT_NAME || trait.name == CLONE_TRAIT_NAME)
      {
        if (isPrimitive(candidate->tag()) || candidate->tag() == typeinfo_tag::UNIT ||
            candidate->tag() == typeinfo_tag::BOOL || candidate->tag() == typeinfo_tag::STRING)
        {
          return true;
        }
        switch (candidate->tag())
        {
        case typeinfo_tag::REFERENCE:
          return trait.name == COPY_TRAIT_NAME ||
                 typeSatisfiesTrait(static_cast<ReferenceType &>(*candidate).referencedType, trait);
        case typeinfo_tag::TUPLE:
          return std::ranges::all_of(static_cast<TupleType &>(*candidate).elementTypes, [&](const auto &element) {
            return typeSatisfiesTrait(element, trait);
          });
        case typeinfo_tag::ARRAY:
          return typeSatisfiesTrait(static_cast<ArrayType &>(*candidate).elementType, trait);
        case typeinfo_tag::SPAN:
          return typeSatisfiesTrait(static_cast<SpanType &>(*candidate).elementType, trait);
        case typeinfo_tag::VECTOR:
          return false;
        default:
          break;
        }
      }
      if (candidate->tag() == typeinfo_tag::REFERENCE)
      {
        candidate = unwrap(static_cast<ReferenceType &>(*candidate).referencedType);
      }
      if (candidate && candidate->tag() == typeinfo_tag::GENERIC_PARAM)
      {
        auto &generic = static_cast<GenericParamType &>(*candidate);
        return generic.bound == trait.name || traitImplies(generic.bound, trait.name);
      }
      if (trait.name == "Sequence" && isSequenceType(candidate))
      {
        return true;
      }
      if (!candidate || candidate->tag() != typeinfo_tag::CUSTOMIZED)
      {
        return false;
      }
      auto &custom = static_cast<CustomizedType &>(*candidate);
      if (auto implIt = trait_impls_by_type.find(custom.name); implIt != trait_impls_by_type.end())
      {
        if (std::ranges::any_of(implIt->second, [&](const Str &implemented) {
              return implemented == trait.name || traitImplies(implemented, trait.name);
            }))
        {
          return true;
        }
      }
      if (activeDerivedTraitImplKeys.contains(custom.name + "::" + trait.name))
      {
        return true;
      }
      if (trait.name == COPY_TRAIT_NAME || trait.name == CLONE_TRAIT_NAME)
      {
        return false;
      }
      auto &methods = trait.allMethods.empty() ? trait.methods : trait.allMethods;
      for (auto &[methodName, methodType] : methods)
      {
        if (!custom.memberFunctions.contains(methodName) &&
            (!custom.traitMemberFunctions.contains(trait.name) ||
             !custom.traitMemberFunctions.at(trait.name).contains(methodName)))
        {
          return false;
        }
      }
      return true;
    }

    auto traitImplies(const Str &candidateName, const Str &requiredName) const -> bool
    {
      if (candidateName == requiredName)
      {
        return true;
      }
      auto it = locals.find(candidateName);
      auto trait = it == locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(it->second);
      if (!trait)
      {
        return false;
      }
      Set<Str> seen;
      return traitImplies(*trait, requiredName, seen);
    }

    auto traitImplies(const TraitType &candidate, const Str &requiredName, Set<Str> &seen) const -> bool
    {
      if (!seen.insert(candidate.name).second)
      {
        return false;
      }
      for (auto &superTrait : candidate.superTraits)
      {
        if (!superTrait)
        {
          continue;
        }
        if (superTrait->name == requiredName || traitImplies(*superTrait, requiredName, seen))
        {
          return true;
        }
      }
      return false;
    }

    void resolveTraitClosure(TraitType &trait, Set<Str> &visiting, Set<Str> &visited, TokenPosition pos)
    {
      if (visited.contains(trait.name))
      {
        return;
      }
      if (!visiting.insert(trait.name).second)
      {
        throw TypeCheckingException("Cyclic trait inheritance involving " + trait.name, pos);
      }
      Map<Str, CheckingRef<FunctionType>> allMethods;
      Map<Str, FunctionDef *> allDefaults;
      Map<Str, Str> allOrigins;
      Map<Str, Str> allDefaultOrigins;
      for (auto &superTrait : trait.superTraits)
      {
        resolveTraitClosure(*superTrait, visiting, visited, pos);
        for (auto &[methodName, methodType] : superTrait->allMethods)
        {
          if (allMethods.contains(methodName) && !functionSignaturesMatch(*allMethods[methodName], *methodType))
          {
            throw TypeCheckingException("Conflicting inherited trait method: " + methodName, pos);
          }
          allMethods[methodName] = methodType;
          allOrigins[methodName] = superTrait->allMethodOrigins.contains(methodName)
                                       ? superTrait->allMethodOrigins[methodName]
                                       : superTrait->name;
        }
        for (auto &[methodName, defaultMethod] : superTrait->allDefaultMethods)
        {
          if (allDefaults.contains(methodName))
          {
            throw TypeCheckingException("Conflicting default trait method " + methodName + " inherited by " +
                                            trait.name,
                                        pos);
          }
          allDefaults[methodName] = defaultMethod;
          allDefaultOrigins[methodName] = superTrait->allDefaultOrigins.contains(methodName)
                                             ? superTrait->allDefaultOrigins[methodName]
                                             : superTrait->name;
        }
      }
      for (auto &[methodName, methodType] : trait.methods)
      {
        if (allMethods.contains(methodName) && !functionSignaturesMatch(*allMethods[methodName], *methodType))
        {
          throw TypeCheckingException("Conflicting trait method: " + methodName, pos);
        }
        allMethods[methodName] = methodType;
        allOrigins[methodName] = trait.name;
        if (trait.defaultMethods.contains(methodName))
        {
          allDefaults[methodName] = trait.defaultMethods[methodName];
          allDefaultOrigins[methodName] = trait.name;
        }
        else
        {
          allDefaults.erase(methodName);
          allDefaultOrigins.erase(methodName);
        }
      }
      trait.allMethods = std::move(allMethods);
      trait.allDefaultMethods = std::move(allDefaults);
      trait.allMethodOrigins = std::move(allOrigins);
      trait.allDefaultOrigins = std::move(allDefaultOrigins);
      visiting.erase(trait.name);
      visited.insert(trait.name);
    }

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
      for (size_t i = 0; i < typeArgs.size(); ++i)
      {
        const bool expectedConst = i < genericDef.typeParamIsConst.size() && genericDef.typeParamIsConst[i];
        const bool actualConst = typeArgs[i] && typeArgs[i]->tag() == typeinfo_tag::CONST_VALUE;
        if (expectedConst)
        {
          if (!actualConst)
          {
            throw TypeCheckingException("Generic parameter '" + genericDef.typeParamNames[i] +
                                        "' expects a compile-time constant argument");
          }
          auto constValueType = std::dynamic_pointer_cast<ConstValueType>(unwrap(typeArgs[i]));
          auto value = constValueFromType(constValueType);
          Str expectedConstType;
          if (genericDef.typeDef && i < genericDef.typeDef->genericParams.size() &&
              genericDef.typeDef->genericParams[i]->constType)
          {
            expectedConstType = genericDef.typeDef->genericParams[i]->constType->repr();
          }
          else if (genericDef.typeAliasDef && i < genericDef.typeAliasDef->genericParams.size() &&
                   genericDef.typeAliasDef->genericParams[i]->constType)
          {
            expectedConstType = genericDef.typeAliasDef->genericParams[i]->constType->repr();
          }
          else if (genericDef.newTypeDef && i < genericDef.newTypeDef->genericParams.size() &&
                   genericDef.newTypeDef->genericParams[i]->constType)
          {
            expectedConstType = genericDef.newTypeDef->genericParams[i]->constType->repr();
          }
          else if (genericDef.taggedUnionDef && i < genericDef.taggedUnionDef->genericParams.size() &&
                   genericDef.taggedUnionDef->genericParams[i]->constType)
          {
            expectedConstType = genericDef.taggedUnionDef->genericParams[i]->constType->repr();
          }
          if (value.has_value() && !expectedConstType.empty())
          {
            auto expectedType = type_from_repr(expectedConstType);
            if (expectedType && !constValueMatchesType(*value, expectedType))
            {
              throw TypeCheckingException("Generic parameter '" + genericDef.typeParamNames[i] +
                                          "' expects const " + expectedConstType);
            }
          }
          continue;
        }
        if (actualConst)
        {
          throw TypeCheckingException("Generic parameter '" + genericDef.typeParamNames[i] +
                                      "' expects a type argument");
        }
        const size_t expectedArity =
            i < genericDef.typeParamKindArities.size() ? genericDef.typeParamKindArities[i] : 0;
        const bool expectedVariadicTail = i < genericDef.typeParamKindVariadicTails.size()
                                              ? genericDef.typeParamKindVariadicTails[i]
                                              : false;
        validateTypeArgumentKind(genericDef.typeParamNames[i], expectedArity, expectedVariadicTail,
                                 typeArgs[i], TokenPosition{});
      }

      Str instanceName = formatTypeInstanceName(genericDef.name, typeArgs);
      if (genericDef.instances.contains(instanceName))
      {
        return genericDef.instances.at(instanceName);
      }

      if (auto selected = selectTypeAliasSpecialization(genericDef.name, typeArgs); selected.has_value())
      {
        auto [specialization, bindings] = std::move(*selected);
        auto resultType = resolveAliasSpecializationBody(*specialization, bindings, genericDef.capturedLocals,
                                                         instanceName);
        genericDef.instances[instanceName] = resultType;
        return resultType;
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
        auto customType = makecheck<CustomizedType>(instanceName, false, false, genericDef.moduleId);
        customType->typeArgs = typeArgs;
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
          const bool hasExplicitReceiver = !memFn->params.empty() && isReceiverParam(memFn->params.front().get());
          if (!hasExplicitReceiver)
          {
            paramTypes.push_back(customType);
          }
          auto methodScope = instLocals;
          methodScope["Self"] = customType;
          TypeChecker checker{methodScope};
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
          attachReceiverEffects(*funType, memFn.get(), hasExplicitReceiver ? memFn->params.front()->paramName : "self");
          customType->memberFunctions[memFn->funName] = funType;

          TypeChecker bodyChecker{methodScope};
          if (!hasExplicitReceiver)
          {
            bodyChecker.locals.insert_or_assign("self", customType);
            for (auto &&[name, type] : customType->properties)
            {
              bodyChecker.locals.insert_or_assign(name, type);
            }
          }
          for (size_t i = 0; i < memFn->params.size(); ++i)
          {
            auto paramIndex = i + (hasExplicitReceiver ? 0 : 1);
            bodyChecker.locals.insert_or_assign(memFn->params[i]->paramName, unwrap(funType->parametersType[paramIndex]));
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
        if (genericDef.typeAliasDef->deleted)
        {
          throw TypeCheckingException("Type alias is deleted: " + genericDef.typeAliasDef->repr(),
                                      genericDef.typeAliasDef->pos);
        }
        if (genericDef.typeAliasDef->abstract)
        {
          throw TypeCheckingException("Abstract type alias cannot be instantiated without a matching specialization: " +
                                      genericDef.name);
        }
        if (genericDef.typeAliasDef->nativeOpaque)
        {
          auto nativeType = makecheck<CustomizedType>(instanceName, true, false, genericDef.moduleId);
          genericDef.instances[instanceName] = nativeType;
          return nativeType;
        }
        auto aliasType = makecheck<TypeAliasType>(instanceName, makecheck<Untyped>(), genericDef.moduleId);
        genericDef.instances[instanceName] = aliasType;
        TypeChecker checker{instLocals};
        genericDef.typeAliasDef->underlyingType->accept(&checker);
        aliasType->underlyingType = checker.result;
        return aliasType;
      }
      case GenericTypeKind::NEW_TYPE:
      {
        auto newType = makecheck<NewTypeType>(instanceName, makecheck<Untyped>(), genericDef.moduleId);
        genericDef.instances[instanceName] = newType;
        TypeChecker checker{instLocals};
        genericDef.newTypeDef->wrappedType->accept(&checker);
        newType->wrappedType = checker.result;
        return newType;
      }
      case GenericTypeKind::TAGGED_UNION:
      {
        auto unionType = makecheck<TaggedUnionType>(instanceName, genericDef.moduleId);
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

    auto tryEvalConstFunctionCall(const FunCallExpression *funCall) -> std::optional<ConstValue>
    {
      auto idExpr = dynamic_cast<IdExpression *>(funCall->primaryExpression.get());
      if (!idExpr || !activeConstFunctions.contains(idExpr->id))
      {
        return std::nullopt;
      }
      auto candidates = activeConstFunctions.at(idExpr->id);
      if (candidates.empty())
      {
        return std::nullopt;
      }
      auto *fn = candidates.front();
      if (!fn)
      {
        return std::nullopt;
      }
      if (!fn->genericParams.empty())
      {
        throw TypeCheckingException("Generic const functions are not supported yet: " + fn->funName, fn->pos);
      }
      if (fn->native || fn->deleted)
      {
        throw TypeCheckingException("Const function cannot be native or deleted: " + fn->funName, fn->pos);
      }
      if (fn->params.size() != funCall->arguments.size())
      {
        throw TypeCheckingException("Const function argument count mismatch: " + fn->funName, funCall->pos);
      }

      Vec<NG::runtime::RuntimeRef<NG::runtime::StorageCell>> runtimeArgs;
      runtimeArgs.reserve(fn->params.size());
      for (size_t i = 0; i < fn->params.size(); ++i)
      {
        auto argValue = tryEvalConstValue(funCall->arguments[i].get());
        if (!argValue.has_value())
        {
          if (predicateHasUnresolvedGeneric(funCall->arguments[i].get()))
          {
            return std::nullopt;
          }
          throw TypeCheckingException("Const function argument is not compile-time evaluable: " + fn->funName,
                                      funCall->arguments[i]->pos);
        }
        if (fn->params[i]->annotatedType)
        {
          TypeChecker paramChecker{locals};
          fn->params[i]->annotatedType->accept(&paramChecker);
          if (!constValueMatchesType(*argValue, paramChecker.result))
          {
            throw TypeCheckingException("Const function argument type mismatch: " + fn->funName,
                                        funCall->arguments[i]->pos);
          }
        }
        if (std::holds_alternative<bool>(*argValue))
        {
          runtimeArgs.push_back(NG::runtime::make_runtime_boolean(std::get<bool>(*argValue)));
        }
        else if (std::holds_alternative<Str>(*argValue))
        {
          runtimeArgs.push_back(NG::runtime::make_runtime_string(std::get<Str>(*argValue)));
        }
        else
        {
          runtimeArgs.push_back(NG::runtime::numeral_cell_from_value<int64_t>(std::get<int64_t>(*argValue)));
        }
      }

      Vec<FunctionDef *> constFunctions;
      for (const auto &[_, functions] : activeConstFunctions)
      {
        constFunctions.insert(constFunctions.end(), functions.begin(), functions.end());
      }
      for (auto *constFunction : Vec<FunctionDef *>{fn})
      {
        if (constFunction)
        {
          validateConstFunctionStatement(constFunction->body.get());
        }
      }
      auto resultCell = NG::intp::eval_const_function(fn, constFunctions, runtimeArgs, modulePaths);
      std::optional<ConstValue> resultValue = std::nullopt;
      if (auto boolValue = NG::runtime::runtime_boolean_value(resultCell); boolValue.has_value())
      {
        resultValue = *boolValue;
      }
      else if (NG::runtime::runtime_is_string_value(resultCell))
      {
        resultValue = NG::runtime::runtime_string_value(resultCell);
      }
      else if (resultCell && resultCell->runtimeType)
      {
        try
        {
          resultValue = NG::runtime::read_numeric_cell_as<int64_t>(resultCell);
        }
        catch (const std::exception &)
        {
          resultValue = std::nullopt;
        }
      }
      if (!resultValue.has_value())
      {
        throw TypeCheckingException("Const function does not return a compile-time value: " + fn->funName, fn->pos);
      }
      if (fn->returnType)
      {
        TypeChecker returnChecker{locals};
        fn->returnType->accept(&returnChecker);
        if (!constValueMatchesType(*resultValue, returnChecker.result))
        {
          throw TypeCheckingException("Const function return type mismatch: " + fn->funName, fn->pos);
        }
      }
      return resultValue;
    }

    auto tryEvalConstValue(Expression *expr) -> std::optional<ConstValue>
    {
      if (auto *funCall = dynamic_cast<FunCallExpression *>(expr))
      {
        if (auto predicate = tryEvalConstPredicateCall(funCall); predicate.has_value())
        {
          return *predicate;
        }
        if (auto constFun = tryEvalConstFunctionCall(funCall); constFun.has_value())
        {
          return *constFun;
        }
      }
      if (auto *idExpr = dynamic_cast<IdExpression *>(expr))
      {
        if (auto it = locals.find(idExpr->id); it != locals.end())
        {
          return constValueFromType(it->second);
        }
      }
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
        if (!operand.has_value())
        {
          if (auto predicate = tryEvalWherePredicate(unaryExpr->operand.get()); predicate.has_value())
          {
            operand = *predicate;
          }
        }
        if (operand.has_value() && unaryExpr->optr && unaryExpr->optr->type == TokenType::NOT &&
            std::holds_alternative<bool>(*operand))
        {
          return !std::get<bool>(*operand);
        }
        if (operand.has_value() && unaryExpr->optr && unaryExpr->optr->type == TokenType::MINUS &&
            std::holds_alternative<int64_t>(*operand))
        {
          return -std::get<int64_t>(*operand);
        }
        return std::nullopt;
      }
      if (auto *binaryExpr = dynamic_cast<BinaryExpression *>(expr))
      {
        auto left = tryEvalConstValue(binaryExpr->left.get());
        auto right = tryEvalConstValue(binaryExpr->right.get());
        if (!left.has_value())
        {
          if (auto predicate = tryEvalWherePredicate(binaryExpr->left.get()); predicate.has_value())
          {
            left = *predicate;
          }
        }
        if (!right.has_value())
        {
          if (auto predicate = tryEvalWherePredicate(binaryExpr->right.get()); predicate.has_value())
          {
            right = *predicate;
          }
        }
        if (!left.has_value() || !right.has_value())
        {
          return std::nullopt;
        }

        switch (binaryExpr->optr->type)
        {
        case TokenType::PLUS:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) + std::get<int64_t>(*right);
          }
          break;
        case TokenType::MINUS:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) - std::get<int64_t>(*right);
          }
          break;
        case TokenType::TIMES:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) * std::get<int64_t>(*right);
          }
          break;
        case TokenType::DIVIDE:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right) &&
              std::get<int64_t>(*right) != 0)
          {
            return std::get<int64_t>(*left) / std::get<int64_t>(*right);
          }
          break;
        case TokenType::MODULUS:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right) &&
              std::get<int64_t>(*right) != 0)
          {
            return std::get<int64_t>(*left) % std::get<int64_t>(*right);
          }
          break;
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
        case TokenType::GT:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) > std::get<int64_t>(*right);
          }
          break;
        case TokenType::GE:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) >= std::get<int64_t>(*right);
          }
          break;
        case TokenType::LT:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) < std::get<int64_t>(*right);
          }
          break;
        case TokenType::LE:
          if (std::holds_alternative<int64_t>(*left) && std::holds_alternative<int64_t>(*right))
          {
            return std::get<int64_t>(*left) <= std::get<int64_t>(*right);
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

    static auto constValueMatchesType(const ConstValue &value, const CheckingRef<TypeInfo> &type) -> bool
    {
      auto unwrapped = unwrap(type);
      if (!unwrapped)
      {
        return false;
      }
      if (std::holds_alternative<bool>(value))
      {
        return unwrapped->tag() == typeinfo_tag::BOOL;
      }
      if (std::holds_alternative<Str>(value))
      {
        return unwrapped->tag() == typeinfo_tag::STRING;
      }
      if (std::holds_alternative<int64_t>(value))
      {
        auto tag = unwrapped->tag();
        return (tag >= typeinfo_tag::I8 && tag <= typeinfo_tag::I128) ||
               (tag >= typeinfo_tag::U8 && tag <= typeinfo_tag::U128);
      }
      return false;
    }

    void publishModuleArtifacts(Module *module)
    {
      ModuleArtifacts artifacts;
      artifacts.exports.insert(module->exports.begin(), module->exports.end());
      artifacts.exports.insert(exportedImportNames.begin(), exportedImportNames.end());
      const bool exportsAll = artifacts.exports.contains("*");
      for (const auto &[name, type] : locals)
      {
        if (name == WILDCARD_IMPORT_KEY)
        {
          continue;
        }
        const bool explicitlyExported = artifacts.exports.contains(name);
        if (isBuiltinLifecycleTraitName(name) && !explicitlyExported)
        {
          continue;
        }
        if ((exportsAll && !importedSymbolNames.contains(name)) || explicitlyExported)
        {
          artifacts.exportedTypes.insert_or_assign(name, type);
        }
      }
      for (const auto &impl : localTraitImpls)
      {
        auto implName = "impl " + impl.traitName + " for " + impl.targetPattern;
        auto explicitImplName = impl.definition && impl.definition->trait
                                    ? "impl " + impl.definition->trait->repr() + " for " + impl.targetPattern
                                    : implName;
        const bool explicitlyExported = artifacts.exports.contains(implName) ||
                                        artifacts.exports.contains(explicitImplName);
        if ((exportsAll && !importedImplNames.contains(implName)) || explicitlyExported)
        {
          artifacts.exportedImpls.push_back(impl);
        }
      }
      for (auto def : module->definitions)
      {
        if (auto typeAlias = dynamic_ast_cast<TypeAliasDef>(def); typeAlias && typeAlias->specializationPattern)
        {
          if (exportsAll || artifacts.exports.contains(typeAlias->aliasName))
          {
            artifacts.exportedTypeAliasSpecializations[typeAlias->aliasName].push_back(typeAlias.get());
          }
        }
        else if (auto constDef = dynamic_ast_cast<ConstDef>(def))
        {
          if (exportsAll || artifacts.exports.contains(constDef->constName))
          {
            artifacts.exportedConstPredicates[constDef->constName].push_back(constDef.get());
          }
        }
        else if (auto funDef = dynamic_ast_cast<FunctionDef>(def); funDef && funDef->constEval)
        {
          if (exportsAll || artifacts.exports.contains(funDef->funName))
          {
            artifacts.exportedConstFunctions[funDef->funName].push_back(funDef.get());
          }
        }
      }

      const bool shouldPublishSharedArtifact =
          !currentModuleId.empty() && currentModuleId != "default" && currentModuleId != "[noname]" &&
          currentModuleId != "[interpreter]";
      auto sharedArtifact = runtime::makert<NG::module::ModuleArtifact>();
      if (shouldPublishSharedArtifact)
      {
        if (auto moduleInfo = NG::module::get_module_registry().queryModuleById(currentModuleId))
        {
          if (moduleInfo->artifact)
          {
            sharedArtifact = moduleInfo->artifact;
          }
          else
          {
            moduleInfo->artifact = sharedArtifact;
          }
          if (!moduleInfo->moduleAst && sharedArtifact->ast)
          {
            moduleInfo->moduleAst = sharedArtifact->ast;
          }
        }
      }

      if (shouldPublishSharedArtifact)
      {
        auto artifactTypeIndex = type_index;
        for (const auto &[name, type] : locals)
        {
          if (name != WILDCARD_IMPORT_KEY)
          {
            artifactTypeIndex.insert_or_assign(name, type);
          }
        }
        const bool wasNativeArtifact = sharedArtifact->format == NG::module::ModuleFormat::Native;
        sharedArtifact->id = NG::module::module_id_from_name(currentModuleId);
        sharedArtifact->format = wasNativeArtifact ? NG::module::ModuleFormat::Native
                                                   : NG::module::ModuleFormat::SourceNg;
        sharedArtifact->typeIndex = std::move(artifactTypeIndex);
        sharedArtifact->exports.declared = artifacts.exports;
        sharedArtifact->exports.types = artifacts.exportedTypes;
        sharedArtifact->imports.moduleIds = importedModuleIds;
        sharedArtifact->traits.clear();
        for (const auto &[name, type] : artifacts.exportedTypes)
        {
          if (type && unwrap(type)->tag() == typeinfo_tag::TRAIT)
          {
            sharedArtifact->traits.insert_or_assign(name, type);
          }
        }
        sharedArtifact->impls.clear();
        for (const auto &impl : artifacts.exportedImpls)
        {
          sharedArtifact->impls.push_back(toModuleImplEvidence(impl));
        }
        if (sharedArtifact->ast)
        {
          sharedArtifact->typeAliasSpecializations = artifacts.exportedTypeAliasSpecializations;
          sharedArtifact->constPredicates = artifacts.exportedConstPredicates;
          sharedArtifact->constFunctions = artifacts.exportedConstFunctions;
        }
        else
        {
          sharedArtifact->typeAliasSpecializations.clear();
          sharedArtifact->constPredicates.clear();
          sharedArtifact->constFunctions.clear();
        }
        NG::module::get_module_registry().addModuleArtifact(sharedArtifact);
      }
      moduleArtifactsById[currentModuleId] = std::move(artifacts);
    }

