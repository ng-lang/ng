
#include <debug.hpp>
#include <intp/intp.hpp>
#include <intp/runtime_numerals.hpp>
#include <token.hpp>
#include <typecheck/pattern_matching.hpp>
#include <typecheck/overload_resolver.hpp>
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
    Map<Str, CheckingRef<TypeInfo>> type_index{};

    Map<Str, CheckingRef<TypeInfo>> locals{};

    CheckingRef<TypeInfo> result;

    Vec<CheckingRef<TypeInfo>> spreadResult{};

    Vec<CheckingRef<TypeInfo>> contextRequirement;

    CheckingRef<TypeInfo> expectedType; // For bidirectional type inference

    Set<Str> movedBindings{};

    bool allowMovedLvalueRead = false;
    Map<Str, Vec<Str>> trait_impls_by_type;
    Str currentModuleId = "default";
    Str activeGenericInstanceName;
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
    Set<Str> autoTraitNames;
    Set<Str> derivedTraitImplKeys;
    Set<Str> importedSymbolNames;
    Set<Str> importedImplNames;
    Set<Str> exportedImportNames;
    Map<Str, Str> importAliases;
    Vec<Str> importedModuleIds;
    Vec<Str> modulePaths;
    inline static Map<Str, ModuleArtifacts> moduleArtifactsById{};
    inline static Set<Str> activeModuleChecks{};

    // Sentinel key stored in locals to indicate wildcard imports are active.
    // This propagates automatically when locals are copied to child checkers.
    static constexpr const char *WILDCARD_IMPORT_KEY = "$$wildcard_import$$";
    static constexpr const char *COPY_TRAIT_NAME = "Copy";
    static constexpr const char *CLONE_TRAIT_NAME = "Clone";
    static constexpr const char *DROP_TRAIT_NAME = "Drop";

    explicit TypeChecker(Map<Str, CheckingRef<TypeInfo>> locals, Vec<CheckingRef<TypeInfo>> contextRequirement = {},
                         CheckingRef<TypeInfo> expectedType = nullptr, Set<Str> movedBindings = {},
                         bool allowMovedLvalueRead = false, Str activeGenericInstanceName = "",
                         Vec<Str> modulePaths = {})
        : locals(std::move(locals)), contextRequirement(std::move(contextRequirement)), expectedType(std::move(expectedType)),
          movedBindings(std::move(movedBindings)), allowMovedLvalueRead(allowMovedLvalueRead),
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

    void visit(CompileUnit *compileUnit) override { compileUnit->module->accept(this); }

    // ── Module-level type checking ──────────────────────────────────────
    void visit(Module *module) override
    {
      installBuiltinLifecycleTraits();
      if (currentModuleId == "default")
      {
        currentModuleId = module->name;
      }
      // First pass: collect function signatures and type definitions
      for (auto def : module->definitions)
      {
        if (auto traitDef = dynamic_ast_cast<TraitDef>(def))
        {
          if (isBuiltinLifecycleTraitName(traitDef->traitName))
          {
            continue;
          }
          Vec<Str> typeParamNames;
          for (auto &gp : traitDef->genericParams)
          {
            typeParamNames.push_back(gp->name);
          }
          locals[traitDef->traitName] = makecheck<TraitType>(traitDef->traitName, typeParamNames, currentModuleId);
        }
        else if (auto constDef = dynamic_ast_cast<ConstDef>(def))
        {
          activeConstPredicates[constDef->constName].push_back(constDef.get());
        }
        else if (auto funDef = dynamic_ast_cast<FunctionDef>(def))
        {
          if (funDef->constEval && !funDef->genericParams.empty())
          {
            throw TypeCheckingException("Generic const functions are not supported yet: " + funDef->funName,
                                        funDef->pos);
          }
          if (funDef->constEval)
          {
            activeConstFunctions[funDef->funName].push_back(funDef.get());
          }
          // Check if this is a generic function (has type parameters)
          if (!funDef->genericParams.empty())
          {
            auto validateGenericFunctionAnnotations = [&](FunctionDef *target) {
              Map<Str, CheckingRef<TypeInfo>> genericLocals = locals;
              addGenericParamsToScope(genericLocals, target->genericParams);
              addWhereBoundsToScope(genericLocals, target->whereBounds);
              for (auto param : target->params)
              {
                if (param->annotatedType)
                {
                  TypeChecker annoChecker{genericLocals};
                  param->annotatedType->accept(&annoChecker);
                }
              }
            };
            if (auto existing = std::dynamic_pointer_cast<GenericDefType>(locals[funDef->funName]))
            {
              existing->overloads.push_back(funDef);
              validateGenericFunctionAnnotations(funDef.get());
              continue;
            }
            Vec<Str> typeParamNames;
            Vec<bool> typeParamIsPack;
            for (auto &gp : funDef->genericParams)
            {
              typeParamNames.push_back(gp->name);
              typeParamIsPack.push_back(gp->isPack);
            }
            auto genericDef = makecheck<GenericDefType>(
                funDef->funName, typeParamNames, typeParamIsPack, funDef, locals, currentModuleId);
            genericDef->typeParamIsConst = genericParamIsConst(funDef->genericParams);
            genericDef->typeParamKindArities = genericParamKindArities(funDef->genericParams);
            genericDef->typeParamKindVariadicTails =
                genericParamKindVariadicTails(funDef->genericParams);
            locals[funDef->funName] = genericDef;

            // Register generic type params in a temporary scope so parameter
            // type annotations (e.g. `T vector`) can resolve them during
            // monomorphization later.  We don't need to fully type-check the
            // body here, but the params must be visible for annotation parsing.
            validateGenericFunctionAnnotations(funDef.get());
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
          if (typeAlias->specializationPattern)
          {
            activeTypeAliasSpecializations[typeAlias->aliasName].push_back(typeAlias.get());
            continue;
          }
          if (typeAlias->abstract && typeAlias->genericParams.empty())
          {
            locals.insert_or_assign(typeAlias->aliasName, makecheck<CustomizedType>(typeAlias->aliasName, false, true, currentModuleId));
            continue;
          }
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
                makecheck<GenericTypeDef>(typeAlias->aliasName, typeParamNames, typeParamIsPack, typeAlias, locals, currentModuleId);
            std::static_pointer_cast<GenericTypeDef>(locals[typeAlias->aliasName])->typeParamIsConst =
                genericParamIsConst(typeAlias->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[typeAlias->aliasName])->typeParamKindArities =
                genericParamKindArities(typeAlias->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[typeAlias->aliasName])->typeParamKindVariadicTails =
                genericParamKindVariadicTails(typeAlias->genericParams);
          }
          else
          {
            if (typeAlias->abstract)
            {
              locals.insert_or_assign(typeAlias->aliasName, makecheck<CustomizedType>(typeAlias->aliasName, false, true, currentModuleId));
              continue;
            }
            if (typeAlias->nativeOpaque)
            {
              locals.insert_or_assign(typeAlias->aliasName, makecheck<CustomizedType>(typeAlias->aliasName, true, false, currentModuleId));
              continue;
            }
            TypeChecker checker{locals};
            typeAlias->underlyingType->accept(&checker);
            auto aliasType = makecheck<TypeAliasType>(typeAlias->aliasName, checker.result, currentModuleId);
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
                makecheck<GenericTypeDef>(newTypeDef->typeName, typeParamNames, typeParamIsPack, newTypeDef, locals, currentModuleId);
            std::static_pointer_cast<GenericTypeDef>(locals[newTypeDef->typeName])->typeParamIsConst =
                genericParamIsConst(newTypeDef->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[newTypeDef->typeName])->typeParamKindArities =
                genericParamKindArities(newTypeDef->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[newTypeDef->typeName])->typeParamKindVariadicTails =
                genericParamKindVariadicTails(newTypeDef->genericParams);
          }
          else
          {
            TypeChecker checker{locals};
            newTypeDef->wrappedType->accept(&checker);
            auto ntType = makecheck<NewTypeType>(newTypeDef->typeName, checker.result, currentModuleId);
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
                makecheck<GenericTypeDef>(typeDef->typeName, typeParamNames, typeParamIsPack, typeDef, locals, currentModuleId);
            std::static_pointer_cast<GenericTypeDef>(locals[typeDef->typeName])->typeParamIsConst =
                genericParamIsConst(typeDef->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[typeDef->typeName])->typeParamKindArities =
                genericParamKindArities(typeDef->genericParams);
            std::static_pointer_cast<GenericTypeDef>(locals[typeDef->typeName])->typeParamKindVariadicTails =
                genericParamKindVariadicTails(typeDef->genericParams);
          }
          else
          {
            auto customType = makecheck<CustomizedType>(typeDef->typeName, false, false, currentModuleId);
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
                                                        typeParamIsPack, taggedUnion, locals, currentModuleId);
            genericDef->typeParamIsConst = genericParamIsConst(taggedUnion->genericParams);
            genericDef->typeParamKindArities = genericParamKindArities(taggedUnion->genericParams);
            genericDef->typeParamKindVariadicTails =
                genericParamKindVariadicTails(taggedUnion->genericParams);
            locals[taggedUnion->typeName] = genericDef;
            genericDef->capturedLocals = locals;
          }
          else
          {
            auto tuType = makecheck<TaggedUnionType>(taggedUnion->typeName, currentModuleId);
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

      for (auto def : module->definitions)
      {
        if (auto useImpl = dynamic_ast_cast<UseImplDecl>(def))
        {
          useImpl->accept(this);
        }
      }
      // Process imports after recording use-impl selections so imported impl
      // conflicts can be resolved deterministically.
      for (auto imp : module->imports)
      {
        imp->accept(this);
      }
      for (auto def : module->definitions)
      {
        if (auto traitDef = dynamic_ast_cast<TraitDef>(def))
        {
          traitDef->accept(this);
        }
      }
      for (auto def : module->definitions)
      {
        if (auto useImpl = dynamic_ast_cast<UseImplDecl>(def))
        {
          validateUseImplDecl(useImpl.get());
        }
      }
      for (auto def : module->definitions)
      {
        if (dynamic_ast_cast<TraitDef>(def) || dynamic_ast_cast<UseImplDecl>(def))
        {
          continue;
        }
        def->accept(this);
      }
      for (const auto &[traitName, selections] : selectedTraitImpls)
      {
        for (auto *selection : selections)
        {
          if (!selection || !selection->targetType)
          {
            continue;
          }
          auto unqualifiedKey = implSelectionKey(traitName, selection->targetType->repr());
          const bool matched = std::ranges::any_of(matchedSelectedTraitImpls, [&](const Str &key) {
            return key.ends_with("::" + unqualifiedKey);
          });
          if (!matched)
          {
            throw TypeCheckingException("Selected impl does not exist: " + unqualifiedKey, selection->pos);
          }
        }
      }
      for (auto stmt : module->statements)
      {
        stmt->accept(this);
      }
      publishModuleArtifacts(module);
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
        Vec<CheckingRef<TypeInfo>> paramTypes;
        const bool hasExplicitReceiver = !memFn->params.empty() && isReceiverParam(memFn->params.front().get());
        if (!hasExplicitReceiver)
        {
          paramTypes.push_back(customType);
        }
        auto methodScope = locals;
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

        // Check member function body
        TypeChecker bodyChecker{methodScope};
        if (!hasExplicitReceiver)
        {
          bodyChecker.locals.insert_or_assign("self", customType);
          // Flatten properties into body scope for legacy implicit-receiver methods.
          for (auto &&[name, type] : customType->properties)
          {
            bodyChecker.locals.insert_or_assign(name, type);
          }
        }
        for (size_t i = 0; i < memFn->params.size(); ++i)
        {
          auto paramIndex = i + (hasExplicitReceiver ? 0 : 1);
          bodyChecker.locals.insert_or_assign(memFn->params[i]->paramName,
                                              unwrap(funType->parametersType[paramIndex]));
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

      for (auto &derivedTraitAnnotation : typeDef->derivedTraits)
      {
        TypeChecker traitChecker{locals};
        derivedTraitAnnotation->accept(&traitChecker);
        auto trait = std::dynamic_pointer_cast<TraitType>(traitChecker.result);
        if (!trait)
        {
          throw TypeCheckingException("derive target is not a trait: " + derivedTraitAnnotation->repr(),
                                      derivedTraitAnnotation->pos);
        }
        if (trait->name != COPY_TRAIT_NAME && trait->name != CLONE_TRAIT_NAME)
        {
          throw TypeCheckingException("derive currently supports Copy and Clone only: " + trait->name,
                                      derivedTraitAnnotation->pos);
        }
        auto derivedKey = customType->name + "::" + trait->name;
        if (derivedTraitImplKeys.contains(derivedKey))
        {
          throw TypeCheckingException("Duplicate derive for trait '" + trait->name + "' on type '" +
                                      customType->name + "'", derivedTraitAnnotation->pos);
        }
        if (auto implIt = trait_impls_by_type.find(customType->name);
            implIt != trait_impls_by_type.end() &&
            std::ranges::find(implIt->second, trait->name) != implIt->second.end())
        {
          throw TypeCheckingException("derive conflicts with explicit impl for trait '" + trait->name +
                                      "' on type '" + customType->name + "'", derivedTraitAnnotation->pos);
        }
        if (trait->name == COPY_TRAIT_NAME)
        {
          if (auto implIt = trait_impls_by_type.find(customType->name);
              implIt != trait_impls_by_type.end() &&
              std::ranges::find(implIt->second, DROP_TRAIT_NAME) != implIt->second.end())
          {
            throw TypeCheckingException("Copy cannot be derived for Drop type '" + customType->name + "'",
                                        derivedTraitAnnotation->pos);
          }
        }
        for (const auto &[fieldName, fieldType] : customType->properties)
        {
          if (!typeCanDeriveTrait(fieldType, trait->name))
          {
            throw TypeCheckingException("Cannot derive " + trait->name + " for '" + customType->name +
                                            "': field '" + fieldName + "' does not satisfy " + trait->name,
                                        derivedTraitAnnotation->pos);
          }
        }
        derivedTraitImplKeys.insert(derivedKey);
        activeDerivedTraitImplKeys.insert(derivedKey);
        auto &implTraits = trait_impls_by_type[customType->name];
        if (std::ranges::find(implTraits, trait->name) == implTraits.end())
        {
          implTraits.push_back(trait->name);
        }
        if (trait->name == CLONE_TRAIT_NAME)
        {
          auto cloneType =
              makecheck<FunctionType>(customType, Vec<CheckingRef<TypeInfo>>{makecheck<ReferenceType>(customType)});
          customType->traitMemberFunctions[CLONE_TRAIT_NAME]["clone"] = cloneType;
          customType->memberFunctions[CLONE_TRAIT_NAME + Str{"::clone"}] = cloneType;
          if (!customType->memberFunctions.contains("clone"))
          {
            customType->memberFunctions["clone"] = cloneType;
          }
        }
      }
    }

    void visit(ConstDef *constDef) override
    {
      Map<Str, CheckingRef<TypeInfo>> constScope = locals;
      addGenericParamsToScope(constScope, constDef->genericParams);

      TypeChecker returnChecker{constScope};
      constDef->returnType->accept(&returnChecker);
      auto returnType = returnChecker.result;

      if (constDef->deleted)
      {
        return;
      }

      if (constDef->native)
      {
        if (!returnType || unwrap(returnType)->tag() != typeinfo_tag::BOOL)
        {
          throw TypeCheckingException("Native const predicate must return bool: " + constDef->constName,
                                      constDef->pos);
        }
        return;
      }

      if (!constDef->value)
      {
        throw TypeCheckingException("Const definition requires a compile-time value: " + constDef->constName,
                                    constDef->pos);
      }

      TypeChecker valueChecker{constScope};
      valueChecker.trait_impls_by_type = trait_impls_by_type;
      auto value = valueChecker.tryEvalConstValue(constDef->value.get());
      if (!value.has_value())
      {
        if (!constDef->genericParams.empty() || constDef->specializationPattern)
        {
          return;
        }
        throw TypeCheckingException("Const definition is not compile-time evaluable: " + constDef->constName,
                                    constDef->value->pos);
      }
      if (!constValueMatchesType(*value, returnType))
      {
        throw TypeCheckingException("Const definition type mismatch: " + constDef->constName,
                                    constDef->value->pos);
      }
    }

    void visit(TraitDef *traitDef) override
    {
      if (isBuiltinLifecycleTraitName(traitDef->traitName))
      {
        auto builtin = std::dynamic_pointer_cast<TraitType>(locals[traitDef->traitName]);
        if (!builtin)
        {
          throw TypeCheckingException("Internal lifecycle trait is missing: " + traitDef->traitName, traitDef->pos);
        }
        if (!traitDef->genericParams.empty() || !traitDef->superTraits.empty())
        {
          throw TypeCheckingException("Builtin lifecycle trait '" + traitDef->traitName +
                                          "' cannot declare generics or supertraits",
                                      traitDef->pos);
        }
        if (traitDef->methods.size() != builtin->methods.size())
        {
          throw TypeCheckingException("Builtin lifecycle trait '" + traitDef->traitName +
                                          "' must match the reserved builtin shape",
                                      traitDef->pos);
        }
        Map<Str, CheckingRef<TypeInfo>> traitScope = locals;
        traitScope["Self"] = makecheck<GenericParamType>("Self", traitDef->traitName);
        for (auto &&method : traitDef->methods)
        {
          if (!builtin->methods.contains(method->funName))
          {
            throw TypeCheckingException("Builtin lifecycle trait '" + traitDef->traitName +
                                            "' cannot declare method '" + method->funName + "'",
                                        method->pos);
          }
          auto actual = functionTypeFor(method.get(), traitScope);
          if (!functionSignaturesMatch(*builtin->methods[method->funName], *actual))
          {
            throw TypeCheckingException("Builtin lifecycle trait '" + traitDef->traitName +
                                            "' method signature mismatch for '" + method->funName + "'",
                                        method->pos);
          }
        }
        return;
      }

      if (traitDef->autoTrait)
      {
        if (!traitDef->genericParams.empty())
        {
          throw TypeCheckingException("auto trait cannot declare generic parameters: " + traitDef->traitName,
                                      traitDef->pos);
        }
        if (!traitDef->methods.empty())
        {
          throw TypeCheckingException("auto trait cannot declare methods: " + traitDef->traitName,
                                      traitDef->pos);
        }
        activeAutoTraits.insert(traitDef->traitName);
        autoTraitNames.insert(traitDef->traitName);
      }

      auto trait = std::dynamic_pointer_cast<TraitType>(locals[traitDef->traitName]);
      if (!trait)
      {
        trait = makecheck<TraitType>(traitDef->traitName, Vec<Str>{}, currentModuleId);
        locals[traitDef->traitName] = trait;
      }

      Map<Str, CheckingRef<TypeInfo>> traitScope = locals;
      addGenericParamsToScope(traitScope, traitDef->genericParams);
      traitScope["Self"] = makecheck<GenericParamType>("Self", traitDef->traitName);
      trait->superTraits.clear();
      for (auto &superTraitAnnotation : traitDef->superTraits)
      {
        TypeChecker superChecker{traitScope};
        superTraitAnnotation->accept(&superChecker);
        auto superTrait = std::dynamic_pointer_cast<TraitType>(superChecker.result);
        if (!superTrait)
        {
          throw TypeCheckingException("Unknown trait: " + superTraitAnnotation->repr(), superTraitAnnotation->pos);
        }
        trait->superTraits.push_back(superTrait);
      }

      trait->methods.clear();
      trait->defaultMethods.clear();
      for (auto &&method : traitDef->methods)
      {
        if (method->params.empty() || !isReceiverParam(method->params.front().get()))
        {
          throw TypeCheckingException("Trait method '" + method->funName +
                                          "' must declare an explicit Self receiver in Phase 1",
                                      method->pos);
        }
        auto funType = functionTypeFor(method.get(), traitScope);
        trait->methods[method->funName] = funType;
        if (method->body)
        {
          trait->defaultMethods[method->funName] = method.get();
        }
      }

      Set<Str> visiting;
      Set<Str> visited;
      resolveTraitClosure(*trait, visiting, visited, traitDef->pos);

      for (auto &&method : traitDef->methods)
      {
        if (!method->body)
        {
          continue;
        }
        auto funType = trait->methods[method->funName];
        TypeChecker bodyChecker{traitScope};
        bodyChecker.trait_impls_by_type = trait_impls_by_type;
        for (size_t i = 0; i < method->params.size(); ++i)
        {
          bodyChecker.locals.insert_or_assign(method->params[i]->paramName, unwrap(funType->parametersType[i]));
        }
        bodyChecker.contextRequirement = funType->parametersType;
        if (funType->returnType->tag() != typeinfo_tag::UNTYPED)
        {
          bodyChecker.expectedType = funType->returnType;
        }
        method->body->accept(&bodyChecker);
        auto bodyReturnType = bodyChecker.result ? bodyChecker.result : makecheck<PrimitiveType>(typeinfo_tag::UNIT);
        if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && funType->returnType->tag() != typeinfo_tag::UNTYPED &&
            !typeMatch(*funType->returnType, *bodyReturnType))
        {
          throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                          funType->returnType->repr(),
                                      method->pos);
        }
      }
    }

    void visit(ImplDef *implDef) override
    {
      Map<Str, CheckingRef<TypeInfo>> implScope = locals;
      addGenericParamsToScope(implScope, implDef->genericParams);
      addWhereBoundsToScope(implScope, implDef->whereBounds);
      {
        TypeChecker whereChecker{implScope};
        whereChecker.validateWherePredicates(implDef->whereBounds, implDef->pos);
      }

      TypeChecker traitChecker{implScope};
      implDef->trait->accept(&traitChecker);
      auto trait = std::dynamic_pointer_cast<TraitType>(traitChecker.result);
      if (!trait)
      {
        throw TypeCheckingException("Impl target trait is not a trait: " + implDef->trait->repr(), implDef->pos);
      }

      TypeChecker targetChecker{implScope};
      implDef->targetType->accept(&targetChecker);
      auto targetType = unwrap(targetChecker.result);
      auto customType = std::dynamic_pointer_cast<CustomizedType>(targetType);
      if (!customType)
      {
        throw TypeCheckingException("Impl target must be a structural or native opaque type: " +
                                    implDef->targetType->repr(), implDef->pos);
      }
      auto implKey = customType->name + "::" + trait->name;
      if (derivedTraitImplKeys.contains(implKey))
      {
        throw TypeCheckingException("Explicit impl conflicts with derived impl for trait '" + trait->name +
                                    "' on type '" + customType->name + "'", implDef->pos);
      }
      if (trait->name == DROP_TRAIT_NAME &&
          derivedTraitImplKeys.contains(customType->name + "::" + COPY_TRAIT_NAME))
      {
        throw TypeCheckingException("Drop impl conflicts with derived Copy for type '" + customType->name + "'",
                                    implDef->pos);
      }
      if (!registerLocalTraitImpl(implDef, *trait))
      {
        return;
      }

      implScope["Self"] = customType;
      Map<Str, FunctionDef *> methods;
      for (auto &&method : implDef->methods)
      {
        methods[method->funName] = method.get();
      }

      auto &requiredMethods = trait->allMethods.empty() ? trait->methods : trait->allMethods;
      for (auto &&[methodName, expectedMethodType] : requiredMethods)
      {
        if (!methods.contains(methodName))
        {
          if (!trait->allDefaultMethods.contains(methodName))
          {
            throw TypeCheckingException("Impl for trait '" + trait->name + "' is missing method '" + methodName + "'",
                                        implDef->pos);
          }
          continue;
        }
        auto actualMethod = methods[methodName];
        if (actualMethod->params.empty() || !isReceiverParam(actualMethod->params.front().get()))
        {
          throw TypeCheckingException("Impl method '" + methodName + "' must declare an explicit Self receiver",
                                      actualMethod->pos);
        }
        auto expected = functionTypeFor(actualMethod, implScope);
        if (!functionSignaturesMatchReplacingSelf(*expectedMethodType, *expected, customType))
        {
          throw TypeCheckingException("Impl method signature mismatch for '" + methodName + "'", actualMethod->pos);
        }
      }

      for (auto &&[methodName, actualMethod] : methods)
      {
        if (!requiredMethods.contains(methodName))
        {
          throw TypeCheckingException("Impl method '" + methodName + "' is not a member of trait '" +
                                          trait->name + "'",
                                      actualMethod->pos);
        }
      }

      auto &implTraits = trait_impls_by_type[customType->name];
      if (std::ranges::find(implTraits, trait->name) == implTraits.end())
      {
        implTraits.push_back(trait->name);
      }

      for (auto &&method : implDef->methods)
      {
        auto funType = functionTypeFor(method.get(), implScope);
        auto traitMethodName = trait->name + "::" + method->funName;
        customType->traitMemberFunctions[trait->name][method->funName] = funType;
        customType->memberFunctions[traitMethodName] = funType;

        TypeChecker bodyChecker{implScope};
        bodyChecker.trait_impls_by_type = trait_impls_by_type;
        for (size_t i = 0; i < method->params.size(); ++i)
        {
          bodyChecker.locals.insert_or_assign(method->params[i]->paramName, unwrap(funType->parametersType[i]));
        }
        bodyChecker.contextRequirement = funType->parametersType;
        if (funType->returnType->tag() != typeinfo_tag::UNTYPED)
        {
          bodyChecker.expectedType = funType->returnType;
        }
        if (method->body)
        {
          method->body->accept(&bodyChecker);
          auto bodyReturnType = bodyChecker.result ? bodyChecker.result : makecheck<PrimitiveType>(typeinfo_tag::UNIT);
          if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && funType->returnType->tag() != typeinfo_tag::UNTYPED &&
              !typeMatch(*funType->returnType, *bodyReturnType))
          {
            throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                            funType->returnType->repr(),
                                        method->pos);
          }
        }
      }

      for (auto &&[methodName, defaultMethod] : trait->allDefaultMethods)
      {
        if (methods.contains(methodName))
        {
          continue;
        }
        auto funType = requiredMethods[methodName];
        auto originTraitName = trait->allDefaultOrigins.contains(methodName) ? trait->allDefaultOrigins[methodName] : trait->name;
        auto traitMethodName = originTraitName + "::" + methodName;
        customType->traitMemberFunctions[originTraitName][methodName] = funType;
        customType->memberFunctions[traitMethodName] = funType;

        TypeChecker bodyChecker{implScope};
        bodyChecker.trait_impls_by_type = trait_impls_by_type;
        for (size_t i = 0; i < defaultMethod->params.size(); ++i)
        {
          bodyChecker.locals.insert_or_assign(defaultMethod->params[i]->paramName, unwrap(funType->parametersType[i]));
        }
        bodyChecker.contextRequirement = funType->parametersType;
        if (funType->returnType->tag() != typeinfo_tag::UNTYPED)
        {
          bodyChecker.expectedType = funType->returnType;
        }
        if (defaultMethod->body)
        {
          defaultMethod->body->accept(&bodyChecker);
          auto bodyReturnType = bodyChecker.result ? bodyChecker.result : makecheck<PrimitiveType>(typeinfo_tag::UNIT);
          if (bodyReturnType->tag() != typeinfo_tag::UNTYPED && funType->returnType->tag() != typeinfo_tag::UNTYPED &&
              !typeMatch(*funType->returnType, *bodyReturnType))
          {
            throw TypeCheckingException("Return Type Mismatch: " + bodyReturnType->repr() + " to " +
                                            funType->returnType->repr(),
                                        defaultMethod->pos);
          }
        }
      }
    }

    void visit(UseImplDecl *useImplDecl) override
    {
      auto traitName = useImplDecl->trait ? useImplDecl->trait->repr() : Str{};
      selectedTraitImpls[traitName].push_back(useImplDecl);
    }

    void validateUseImplDecl(UseImplDecl *useImplDecl)
    {
      TypeChecker traitChecker{locals};
      useImplDecl->trait->accept(&traitChecker);
      auto trait = std::dynamic_pointer_cast<TraitType>(traitChecker.result);
      if (!trait)
      {
        throw TypeCheckingException("Selected impl trait is not a trait: " + useImplDecl->trait->repr(),
                                    useImplDecl->pos);
      }
      TypeChecker targetChecker{locals};
      useImplDecl->targetType->accept(&targetChecker);
      auto target = unwrap(targetChecker.result);
      auto custom = std::dynamic_pointer_cast<CustomizedType>(target);
      if (!custom)
      {
        throw TypeCheckingException("Selected impl target must be a structural or native opaque type: " +
                                        useImplDecl->targetType->repr(),
                                    useImplDecl->pos);
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
      if (funDef->constEval && (funDef->native || funDef->deleted))
      {
        throw TypeCheckingException("Const function cannot be native or deleted: " + funDef->funName,
                                    funDef->pos);
      }
      // Skip generic functions — already registered as GenericDefType in Module first pass
      if (!funDef->genericParams.empty())
      {
        return;
      }

      CheckingRef<TypeInfo> funType;
      if (auto it = locals.find(funDef->funName); it != locals.end())
      {
        funType = it->second;
        if (auto existingFunction = std::dynamic_pointer_cast<FunctionType>(funType); existingFunction && funDef->deleted)
        {
          existingFunction->deleted = true;
          existingFunction->deletedRepr = funDef->repr();
        }
      }
      else
      {
        TypeChecker checker{locals};
        Vec<CheckingRef<TypeInfo>> paramTypes;
        for (auto param : funDef->params)
        {
          param->accept(&checker);
          rejectInvalidByValueType(checker.result, "function parameter '" + param->paramName + "'", param->pos);
          paramTypes.push_back(checker.result);
        }
        CheckingRef<TypeInfo> returnType;
        if (funDef->returnType)
        {
          funDef->returnType->accept(&checker);
          returnType = checker.result;
          rejectInvalidByValueType(returnType, "function return type", funDef->returnType->pos);
        }
        else
        {
          returnType = makecheck<Untyped>();
        }
        funType = makecheck<FunctionType>(returnType, paramTypes);
        auto &createdFunctionType = static_cast<FunctionType &>(*funType);
        createdFunctionType.deleted = funDef->deleted;
        if (funDef->deleted)
        {
          createdFunctionType.deletedRepr = funDef->repr();
        }
        if (!funDef->funName.empty())
        {
          locals.insert_or_assign(funDef->funName, funType);
        }
      }

      auto &funcInfo = static_cast<FunctionType &>(*funType);
      if (funcInfo.deleted)
      {
        return;
      }
      for (auto &paramType : funcInfo.parametersType)
      {
        validateObjectSafeTraitRefs(paramType);
      }
      validateObjectSafeTraitRefs(funcInfo.returnType);
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
            !typeMatches(*funcInfo.returnType, *bodyReturnType))
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
      TypeChecker checker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
      simpleStatement->expression->accept(&checker);
      movedBindings = checker.movedBindings;
      if (auto *assignmentExpr = dynamic_cast<AssignmentExpression *>(simpleStatement->expression.get()))
      {
        if (auto *idTarget = dynamic_cast<IdExpression *>(assignmentExpr->target.get()))
        {
          clearMovedPlace(movedBindings, idTarget->id);
        }
      }
    }

    void visit(CompoundStatement *compoundStatement) override
    {
      auto outerNames = scopeNames(locals);
      TypeChecker checker{locals, contextRequirement, expectedType, movedBindings, allowMovedLvalueRead,
                          activeGenericInstanceName};
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
        TypeChecker checker{locals, {}, expectedType, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
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
      TypeChecker checker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
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
      if (activeGenericInstanceName.empty())
      {
        ifStatement->evaluatedCondition.reset();
      }
      if (ifStatement->isConst)
      {
        auto condResult = tryEvalConstCondition(ifStatement->testing.get());
        if (condResult.has_value())
        {
          ifStatement->evaluatedCondition = condResult.value();
          if (!activeGenericInstanceName.empty())
          {
            ifStatement->evaluatedConditionByInstance[activeGenericInstanceName] = condResult.value();
          }
          if (condResult.value())
          {
            auto outerNames = scopeNames(locals);
            TypeChecker thenChecker{locals, contextRequirement, expectedType, movedBindings, allowMovedLvalueRead,
                                    activeGenericInstanceName};
            thenChecker.trait_impls_by_type = trait_impls_by_type;
            ifStatement->consequence->accept(&thenChecker);
            movedBindings = filterMovedBindings(thenChecker.movedBindings, outerNames);
            result = thenChecker.result;
          }
          else if (ifStatement->alternative)
          {
            auto outerNames = scopeNames(locals);
            TypeChecker elseChecker{locals, contextRequirement, expectedType, movedBindings, allowMovedLvalueRead,
                                    activeGenericInstanceName};
            elseChecker.trait_impls_by_type = trait_impls_by_type;
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

      TypeChecker condChecker{locals, contextRequirement, expectedType, movedBindings, allowMovedLvalueRead,
                              activeGenericInstanceName};
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
        TypeChecker thenChecker{locals, contextRequirement, expectedType, entryMovedBindings, allowMovedLvalueRead,
                                activeGenericInstanceName};
        ifStatement->consequence->accept(&thenChecker);
        returnType = thenChecker.result;
        result = returnType;
        movedBindings = filterMovedBindings(thenChecker.movedBindings, outerNames);
      }
      if (ifStatement->alternative)
      {
        TypeChecker elseChecker{locals, contextRequirement, expectedType, entryMovedBindings, allowMovedLvalueRead,
                                activeGenericInstanceName};
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
      TypeChecker checker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
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
        TypeChecker annoChecker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
        annoChecker.trait_impls_by_type = trait_impls_by_type;
        valDefStatement->typeAnnotation->accept(&annoChecker);
        annoType = annoChecker.result;
        rejectInvalidByValueType(annoType, "value annotation '" + valDefStatement->name + "'",
                                 valDefStatement->typeAnnotation->pos);
      }

      // Bidirectional inference: pass annotation type as expectedType to value expression
      TypeChecker valChecker{locals, {}, annoType, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
      valChecker.trait_impls_by_type = trait_impls_by_type;
      valDefStatement->value->accept(&valChecker);
      auto valType = valChecker.result;
      movedBindings = valChecker.movedBindings;

      if (annoType)
      {
        if (typeMatches(*annoType, *valType))
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
      clearMovedPlace(movedBindings, valDefStatement->name);
      recordBorrowAlias(valDefStatement->name, borrowedPlaceFromRefExpression(valDefStatement->value.get()));
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
        if (valType && valType->tag() == typeinfo_tag::TUPLE)
        {
          elementTypesPtr = &static_cast<TupleType &>(*valType).elementTypes;
        }
        else if (valType && valType->tag() == typeinfo_tag::VARARGS)
        {
          elementTypesPtr = &static_cast<VarargsType &>(*valType).elementTypes;
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
                  clearMovedPlace(movedBindings, binding->name);
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
                clearMovedPlace(movedBindings, binding->name);
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
                clearMovedPlace(movedBindings, binding->name);
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
              clearMovedPlace(movedBindings, binding->name);
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
        if (auto elementType = sequenceElementType(valType); elementType)
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
              auto restArrayType = makecheck<VectorType>(elementType);
              if (binding->annotation)
              {
                binding->annotation->accept(&checker);
                auto annoType = checker.result;
                if (typeMatch(*annoType, *restArrayType))
                {
                  locals.insert_or_assign(binding->name, annoType);
                  clearMovedPlace(movedBindings, binding->name);
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
                clearMovedPlace(movedBindings, binding->name);
              }

              break;
            }
            if (binding->annotation)
            {
              binding->annotation->accept(&checker);
              auto annoType = checker.result;
              if (typeMatch(*annoType, *elementType))
              {
                locals.insert_or_assign(binding->name, annoType);
                clearMovedPlace(movedBindings, binding->name);
              }
              else
              {
                throw TypeCheckingException("Value Binding Type Mismatch: " + elementType->repr() + " to " +
                                            annoType->repr());
              }
            }
            else
            {
              locals.insert_or_assign(binding->name, elementType);
              clearMovedPlace(movedBindings, binding->name);
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
      TypeChecker checker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
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
        if (unwrap(operandType) && unwrap(operandType)->tag() == typeinfo_tag::REFERENCE)
        {
          throw TypeCheckingException("Reference operator cannot take a reference value.");
        }
        result = makecheck<ReferenceType>(widenVariantToUnionType(locals, operandType));
        return;
      }
      case TokenType::TIMES:
      {
        auto unwrappedOp = unwrap(operandType);
        if (!unwrappedOp || unwrappedOp->tag() != typeinfo_tag::REFERENCE)
        {
          throw TypeCheckingException("Cannot dereference non-reference type: " + operandType->repr());
        }
        result = static_cast<ReferenceType &>(*unwrappedOp).referencedType;
        return;
      }
      case TokenType::KEYWORD_MOVE:
      {
        if (!isMovableExpression(unoExpr->operand.get()))
        {
          throw TypeCheckingException("Move operator requires a movable place.");
        }
        if (auto *deref = dynamic_cast<UnaryExpression *>(unoExpr->operand.get());
            deref && deref->optr && deref->optr->type == TokenType::TIMES)
        {
          if (auto *id = dynamic_cast<IdExpression *>(deref->operand.get()))
          {
            if (auto target = borrowedAliasTarget(movedBindings, id->id); target.has_value())
            {
              throw TypeCheckingException("Cannot move borrowed place through ref alias: " + id->id +
                                              " aliases " + *target,
                                          unoExpr->pos);
            }
          }
        }
        if (auto *index = dynamic_cast<IndexAccessorExpression *>(unoExpr->operand.get()))
        {
          TypeChecker primaryChecker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
          index->primary->accept(&primaryChecker);
          auto primaryType = deref_reference_type(primaryChecker.result);
          if (!primaryType || primaryType->tag() != typeinfo_tag::TUPLE ||
              !dynamic_cast<IntegralValue<int32_t> *>(index->accessor.get()))
          {
            throw TypeCheckingException("Move from indexed place only supports tuple constant indexes.");
          }
        }
        if (auto place = staticPlaceKey(unoExpr->operand.get()); place.has_value())
        {
          rejectBorrowConflict("move", *place, unoExpr->pos);
          movedBindings.insert(*place);
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
      else if (leftType->tag() == typeinfo_tag::VECTOR)
      {
        VectorType &vectorType = static_cast<VectorType &>(*leftType);
        switch (expression->optr->type)
        {
        case TokenType::LSHIFT:
          if (typeMatch(*vectorType.elementType, *rightType) || rightType->tag() == typeinfo_tag::UNTYPED)
          {
            result = leftType;
            return;
          }
          else
          {
            throw TypeCheckingException("Invalid element type for array push: " + rightType->repr(), expression->pos);
          }
        default:
          throw TypeCheckingException("Unsupported operator for vector types", expression->pos);
        }
      }
      else if (leftType->tag() == typeinfo_tag::ARRAY || leftType->tag() == typeinfo_tag::SPAN)
      {
        throw TypeCheckingException("Unsupported operator for " + typeKindName(*leftType) + " types",
                                    expression->pos);
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
        rejectInvalidByValueType(result, "parameter '" + param->paramName + "'", param->pos);
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

    // ── Type annotation resolution ──────────────────────────────────────
    void visit(TypeAnnotation *annotation) override
    {
      if (annotation->constLiteral)
      {
        result = makecheck<ConstValueType>(annotation->name, annotation->constLiteralType, false);
        return;
      }
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
          throw TypeCheckingException("Legacy array type expects exactly 1 element type argument");
        }
      }
      else if (annotation->type == TypeAnnotationType::VECTOR)
      {
        if (annotation->arguments.size() == 1)
        {
          auto arg = annotation->arguments[0];
          TypeChecker checker{locals};
          arg->accept(&checker);
          auto argType = checker.result;
          if (argType)
          {
            result = makecheck<VectorType>(argType);
            return;
          }
          throw TypeCheckingException("Unknown element type for vector");
        }
        else
        {
          throw TypeCheckingException("Vector type expects exactly 1 type argument");
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
          if (auto varargs = std::dynamic_pointer_cast<VarargsType>(unwrap(type)))
          {
            types.insert(types.end(), varargs->elementTypes.begin(), varargs->elementTypes.end());
          }
          else
          {
            types.push_back(type);
          }
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
          if (auto selected = selectTypeAliasSpecialization("ref", Vec<CheckingRef<TypeInfo>>{innerType});
              selected.has_value())
          {
            auto [specialization, bindings] = std::move(*selected);
            result = resolveAliasSpecializationBody(*specialization, bindings, locals,
                                                    "ref<" + innerType->repr() + ">");
            return;
          }
          if (auto trait = std::dynamic_pointer_cast<TraitType>(unwrap(innerType)); trait && !isObjectSafeTrait(*trait))
          {
            throw TypeCheckingException("Trait is not object-safe for ref<" + innerType->repr() + ">");
          }
          result = makecheck<ReferenceType>(innerType);
          return;
        }

        // Fixed array type: array<T, N>
        if (annotation->name == "array")
        {
          if (annotation->genericArgs.size() == 2)
          {
            TypeChecker checker{locals};
            annotation->genericArgs[0]->accept(&checker);
            auto argType = checker.result;
            annotation->genericArgs[1]->accept(&checker);
            auto lengthType = checker.result;
            if (!argType)
            {
              throw TypeCheckingException("Unknown element type for array");
            }
            if (!lengthType || lengthType->tag() != typeinfo_tag::CONST_VALUE)
            {
              throw TypeCheckingException("Array length expects a compile-time constant argument", annotation->pos);
            }
            result = makecheck<ArrayType>(argType, lengthType);
            return;
          }
          if (annotation->genericArgs.size() == 1)
          {
            throw TypeCheckingException("Fixed array type expects 2 generic arguments: array<T, N>; use vector<T> for dynamic arrays",
                                        annotation->pos);
          }
        }

        if (annotation->name == "vector")
        {
          if (annotation->arguments.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->arguments[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<VectorType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for vector");
          }
          if (annotation->genericArgs.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->genericArgs[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<VectorType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for vector");
          }
        }

        if (annotation->name == "span")
        {
          if (annotation->genericArgs.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->genericArgs[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<SpanType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for span");
          }
        }

        if (annotation->name == "Range")
        {
          if (annotation->genericArgs.size() == 1)
          {
            TypeChecker checker{locals};
            annotation->genericArgs[0]->accept(&checker);
            auto argType = checker.result;
            if (argType)
            {
              result = makecheck<RangeType>(argType);
              return;
            }
            throw TypeCheckingException("Unknown element type for Range");
          }
        }

        if (annotation->name == "tuple_element")
        {
          if (annotation->genericArgs.size() != 2)
          {
            throw TypeCheckingException("tuple_element<T, I> expects exactly 2 generic arguments",
                                        annotation->pos);
          }
          TypeChecker checker{locals};
          annotation->genericArgs[0]->accept(&checker);
          auto unwrappedTuple = unwrap(checker.result);
          bool isTuple = unwrappedTuple && unwrappedTuple->tag() == typeinfo_tag::TUPLE;
          annotation->genericArgs[1]->accept(&checker);
          auto unwrappedIndex = unwrap(checker.result);
          bool isConstValue = unwrappedIndex && unwrappedIndex->tag() == typeinfo_tag::CONST_VALUE;
          if (!isTuple)
          {
            throw TypeCheckingException("tuple_element<T, I> expects a tuple type as T", annotation->pos);
          }
          auto &indexValue = static_cast<ConstValueType &>(*unwrappedIndex);
          if (!isConstValue || indexValue.isParam)
          {
            throw TypeCheckingException("tuple_element<T, I> expects a concrete const index", annotation->pos);
          }
          size_t index = 0;
          try
          {
            auto parsed = std::stoll(indexValue.value);
            if (parsed < 0)
            {
              throw TypeCheckingException("tuple_element<T, I> index cannot be negative", annotation->pos);
            }
            index = static_cast<size_t>(parsed);
          }
          catch (const TypeCheckingException &)
          {
            throw;
          }
          catch (const std::exception &)
          {
            throw TypeCheckingException("tuple_element<T, I> expects an integer const index", annotation->pos);
          }
          auto &tupleRef = static_cast<TupleType &>(*unwrappedTuple);
          if (index >= tupleRef.elementTypes.size())
          {
            throw TypeCheckingException("tuple_element<T, I> index out of range", annotation->pos);
          }
          result = tupleRef.elementTypes[index];
          return;
        }

        if (annotation->name == "tuple_concat")
        {
          if (annotation->genericArgs.size() != 2)
          {
            throw TypeCheckingException("tuple_concat<A, B> expects exactly 2 generic arguments", annotation->pos);
          }
          TypeChecker checker{locals};
          annotation->genericArgs[0]->accept(&checker);
          auto unwrappedLeft = unwrap(checker.result);
          annotation->genericArgs[1]->accept(&checker);
          auto unwrappedRight = unwrap(checker.result);
          if (!unwrappedLeft || unwrappedLeft->tag() != typeinfo_tag::TUPLE ||
              !unwrappedRight || unwrappedRight->tag() != typeinfo_tag::TUPLE)
          {
            throw TypeCheckingException("tuple_concat<A, B> expects tuple type arguments", annotation->pos);
          }
          auto &leftTuple = static_cast<TupleType &>(*unwrappedLeft);
          auto &rightTuple = static_cast<TupleType &>(*unwrappedRight);
          Vec<CheckingRef<TypeInfo>> elements = leftTuple.elementTypes;
          elements.insert(elements.end(), rightTuple.elementTypes.begin(), rightTuple.elementTypes.end());
          result = makecheck<TupleType>(elements);
          return;
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
        if (!annotation->genericArgs.empty())
        {
          Vec<CheckingRef<TypeInfo>> typeArgs;
          TypeChecker checker{locals};
          for (auto &arg : annotation->genericArgs)
          {
            arg->accept(&checker);
            typeArgs.push_back(checker.result);
          }
          if (auto selected = selectTypeAliasSpecialization(annotation->name, typeArgs); selected.has_value())
          {
            auto [specialization, bindings] = std::move(*selected);
            result = resolveAliasSpecializationBody(*specialization, bindings, locals,
                                                    formatTypeInstanceName(annotation->name, typeArgs));
            return;
          }
        }
        if (it != locals.end())
        {
          if (!annotation->genericArgs.empty())
          {
            Vec<CheckingRef<TypeInfo>> typeArgs;
            TypeChecker checker{locals};
            for (auto &arg : annotation->genericArgs)
            {
              arg->accept(&checker);
              typeArgs.push_back(checker.result);
            }

            if (it->second && it->second->tag() == typeinfo_tag::GENERIC_PARAM)
            {
              auto &genericParam = static_cast<GenericParamType &>(*it->second);
              if (genericParam.kindArity == 0 && !genericParam.kindVariadicTail)
              {
                throw TypeCheckingException("Type parameter '" + annotation->name +
                                                "' is not a type constructor",
                                            annotation->pos);
              }
              if (!typeConstructorApplicationArityValid(genericParam.kindArity,
                                                        genericParam.kindVariadicTail, typeArgs.size()))
              {
                throw TypeCheckingException("Type constructor parameter '" + annotation->name + "' expects " +
                                                std::to_string(genericParam.kindArity) +
                                                " fixed type argument(s)" +
                                                (genericParam.kindVariadicTail ? " and a variadic tail" : "") +
                                                ", got " + std::to_string(typeArgs.size()),
                                            annotation->pos);
              }
              result = makecheck<TypeConstructorApplicationType>(it->second, typeArgs);
              return;
            }

            if (it->second && it->second->tag() == typeinfo_tag::TRAIT)
            {
              auto &traitType = static_cast<TraitType &>(*it->second);
              if (traitType.typeParamNames.size() != typeArgs.size())
              {
                throw TypeCheckingException("Trait '" + annotation->name + "' expects " +
                                                std::to_string(traitType.typeParamNames.size()) +
                                                " type argument(s), got " + std::to_string(typeArgs.size()),
                                            annotation->pos);
              }
              result = it->second;
              return;
            }

            auto *genericType = dynamic_cast<GenericTypeDef *>(&(*it->second));
            if (!genericType)
            {
              throw TypeCheckingException("Type '" + annotation->name + "' is not generic");
            }
            result = instantiateGenericType(*genericType, typeArgs);
            return;
          }

          if (it->second->tag() == typeinfo_tag::GENERIC_TYPE_DEF)
          {
            throw TypeCheckingException("Generic type '" + annotation->name + "' requires type arguments");
          }
          if (auto genericParam = std::dynamic_pointer_cast<GenericParamType>(it->second);
              genericParam && (genericParam->kindArity > 0 || genericParam->kindVariadicTail))
          {
            throw TypeCheckingException("Type constructor parameter '" + annotation->name +
                                            "' requires type arguments",
                                        annotation->pos);
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
        TypeChecker targetChecker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
        assignmentExpr->target->accept(&targetChecker);
        targetType = targetChecker.result;
        targetMovedBindings = targetChecker.movedBindings;
      }

      TypeChecker valueChecker{locals, {}, nullptr, targetMovedBindings, allowMovedLvalueRead,
                               activeGenericInstanceName};
      assignmentExpr->value->accept(&valueChecker);
      auto valueType = valueChecker.result;
      movedBindings = valueChecker.movedBindings;

      if (!targetType || !valueType)
      {
        throw TypeCheckingException("Invalid assignment expression: " + assignmentExpr->repr(), assignmentExpr->pos);
      }
      if (!typeMatches(*targetType, *valueType))
      {
        throw TypeCheckingException("Invalid assignment type: " + valueType->repr() + " to " + targetType->repr(),
                                    assignmentExpr->pos);
      }
      if (auto *id = dynamic_cast<IdExpression *>(assignmentExpr->target.get()))
      {
        rejectBorrowConflict("assign", id->id, assignmentExpr->pos);
        clearMovedPlace(movedBindings, id->id);
        recordBorrowAlias(id->id, borrowedPlaceFromRefExpression(assignmentExpr->value.get()));
      }
      else if (auto place = staticPlaceKey(assignmentExpr->target.get()); place.has_value())
      {
        rejectBorrowConflict("assign", *place, assignmentExpr->pos);
        clearMovedPlace(movedBindings, *place);
      }
      result = targetType;
    }

    void visit(ArrayLiteral *arrayLit) override
    {
      if (arrayLit->elements.empty())
      {
        if (expectedType && isSequenceType(expectedType))
        {
          auto unwrappedExpected = unwrap(expectedType);
          if (unwrappedExpected && unwrappedExpected->tag() == typeinfo_tag::ARRAY)
          {
            auto &expectedArray = static_cast<ArrayType &>(*unwrappedExpected);
            if (expectedArray.length && !const_value_equals_size(expectedArray.length, 0))
            {
              throw TypeCheckingException("Array literal length mismatch: expected " + expectedArray.length->repr() +
                                              ", got 0",
                                          arrayLit->pos);
            }
          }
          result = expectedType;
        }
        else
        {
          result = makecheck<VectorType>(makecheck<Untyped>());
        }
        return;
      }
      auto foldElementType = [&](const ASTRef<PostfixFoldExpression> &fold) -> CheckingRef<TypeInfo> {
        auto call = dynamic_ast_cast<FunCallExpression>(fold->expression);
        if (!call || call->arguments.size() != 1)
        {
          throw TypeCheckingException("Map/filter fold expects a single-argument function call", fold->pos);
        }
        auto driver = dynamic_ast_cast<IdExpression>(call->arguments[0]);
        if (!driver)
        {
          throw TypeCheckingException("Map/filter fold driver must be a single sequence identifier", fold->pos);
        }

        TypeChecker sequenceChecker{locals, {}, nullptr, movedBindings};
        call->arguments[0]->accept(&sequenceChecker);
        auto sequenceType = sequenceChecker.result;
        auto elementType = sequenceElementType(sequenceType);
        if (!elementType)
        {
          throw TypeCheckingException("Map/filter fold driver must be a Sequence-compatible type: " +
                                          sequenceType->repr(),
                                      fold->pos);
        }

        auto elementLocals = locals;
        elementLocals[driver->id] = elementType;
        TypeChecker bodyChecker{elementLocals, {}, nullptr, sequenceChecker.movedBindings};
        fold->expression->accept(&bodyChecker);
        movedBindings = bodyChecker.movedBindings;
        if (fold->filter)
        {
          if (!bodyChecker.result || bodyChecker.result->tag() != typeinfo_tag::BOOL)
          {
            throw TypeCheckingException("Filter fold expression must return bool", fold->pos);
          }
          return elementType;
        }
        return bodyChecker.result;
      };

      if (arrayLit->elements.size() == 1)
      {
        if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(arrayLit->elements[0]))
        {
          result = makecheck<VectorType>(foldElementType(fold));
          return;
        }
      }
      auto expectedElementType = sequenceElementType(expectedType);
      TypeChecker checker{locals, {}, nullptr, movedBindings};
      checker.expectedType = expectedElementType;
      CheckingRef<TypeInfo> elemType;
      if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(arrayLit->elements[0]))
      {
        elemType = foldElementType(fold);
      }
      else
      {
        arrayLit->elements[0]->accept(&checker);
        elemType = checker.result;
      }
      for (size_t i = 1; i < arrayLit->elements.size(); ++i)
      {
        checker.expectedType = expectedElementType;
        auto nextType = [&]() -> CheckingRef<TypeInfo> {
          if (auto fold = dynamic_ast_cast<PostfixFoldExpression>(arrayLit->elements[i]))
          {
            return foldElementType(fold);
          }
          arrayLit->elements[i]->accept(&checker);
          return checker.result;
        }();
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
      auto unwrappedExpectedArr = unwrap(expectedType);
      if (unwrappedExpectedArr && unwrappedExpectedArr->tag() == typeinfo_tag::ARRAY)
      {
        auto &expectedArray = static_cast<ArrayType &>(*unwrappedExpectedArr);
        if (expectedArray.length && !const_value_equals_size(expectedArray.length, arrayLit->elements.size()))
        {
          throw TypeCheckingException("Array literal length mismatch: expected " + expectedArray.length->repr() +
                                          ", got " + std::to_string(arrayLit->elements.size()),
                                      arrayLit->pos);
        }
        result = expectedType;
        return;
      }
      if (expectedType && expectedType->tag() == typeinfo_tag::VECTOR)
      {
        result = expectedType;
        return;
      }
      result = makecheck<VectorType>(elemType);
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
      checker.trait_impls_by_type = trait_impls_by_type;
      checker.localTraitImpls = localTraitImpls;
      spread->expression->accept(&checker);
      auto type = checker.result;
      movedBindings = checker.movedBindings;
      spreadResult.clear();
      if (type && type->tag() == typeinfo_tag::TUPLE)
      {
        auto &tup = static_cast<TupleType &>(*type);
        result = type;
        for (auto &&elemType : tup.elementTypes)
        {
          spreadResult.push_back(elemType);
        }
      }
      else if (type && type->tag() == typeinfo_tag::VARARGS)
      {
        auto &varargs = static_cast<VarargsType &>(*type);
        result = type;
        for (auto &&elemType : varargs.elementTypes)
        {
          spreadResult.push_back(elemType);
        }
      }
      else if (auto elementType = sequenceElementType(type); elementType)
      {
        // Contiguous sequence spread does not expand compile-time arity.
        result = elementType;
      }
      else
      {
        throw TypeCheckingException("Invalid spread expression on type, expect tuple, varargs, array, vector, or span, got " + type->repr());
      }
    }

    void visit(PostfixFoldExpression *fold) override
    {
      throw TypeCheckingException("Postfix fold expression is only supported in array literals or fold calls: " +
                                      fold->repr(),
                                  fold->pos);
    }

    void visit(IndexAccessorExpression *indexAccess) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
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
      if (auto range = dynamic_ast_cast<RangeExpression>(indexAccess->accessor))
      {
        auto validateBound = [&](const ASTRef<Expression> &bound) {
          if (!bound)
          {
            return;
          }
          if (auto fromEnd = dynamic_ast_cast<FromEndIndexExpression>(bound))
          {
            if (fromEnd->index)
            {
              fromEnd->index->accept(&checker);
            }
          }
          else
          {
            bound->accept(&checker);
          }
          auto boundType = checker.result;
          if (boundType && boundType->tag() != typeinfo_tag::UNTYPED && !isIntegralType(boundType->tag()))
          {
            throw TypeCheckingException("Range bound must be integral: " + bound->repr(), bound->pos);
          }
        };
        validateBound(range->start);
        validateBound(range->end);

        if (primaryType->tag() == typeinfo_tag::TUPLE)
        {
          auto &tupleType = static_cast<TupleType &>(*primaryType);
          auto staticBound = [&](const ASTRef<Expression> &bound, size_t defaultValue) -> std::optional<size_t> {
            if (!bound)
            {
              return defaultValue;
            }
            auto literal = dynamic_ast_cast<IntegralValue<int32_t>>(bound);
            if (literal)
            {
              if (literal->value < 0) return std::nullopt;
              return static_cast<size_t>(literal->value);
            }
            if (auto fromEnd = dynamic_ast_cast<FromEndIndexExpression>(bound))
            {
              auto fromEndLiteral = dynamic_ast_cast<IntegralValue<int32_t>>(fromEnd->index);
              if (!fromEndLiteral || fromEndLiteral->value < 0)
              {
                return std::nullopt;
              }
              auto offset = static_cast<size_t>(fromEndLiteral->value);
              if (offset > tupleType.elementTypes.size())
              {
                return std::nullopt;
              }
              return tupleType.elementTypes.size() - offset;
            }
            return std::nullopt;
          };
          auto start = staticBound(range->start, 0);
          auto end = staticBound(range->end, tupleType.elementTypes.size());
          if (!start || !end)
          {
            throw TypeCheckingException("Tuple slice bounds must be compile-time non-negative integers",
                                        indexAccess->accessor->pos);
          }
          auto exclusiveEnd = *end + (range->inclusive ? 1 : 0);
          if (*start > exclusiveEnd || exclusiveEnd > tupleType.elementTypes.size())
          {
            throw TypeCheckingException("Tuple slice bounds out of range: " + range->repr(), indexAccess->accessor->pos);
          }
          Vec<CheckingRef<TypeInfo>> elements;
          for (size_t i = *start; i < exclusiveEnd; ++i)
          {
            elements.push_back(tupleType.elementTypes[i]);
          }
          movedBindings = checker.movedBindings;
          result = makecheck<TupleType>(std::move(elements));
          return;
        }

        auto elementType = sequenceElementType(primaryType);
        if (!elementType)
        {
          throw TypeCheckingException("Range index on non-contiguous sequence type: " + primaryType->repr());
        }
        movedBindings = checker.movedBindings;
        result = makecheck<SpanType>(elementType);
        return;
      }
      if (auto fromEnd = dynamic_ast_cast<FromEndIndexExpression>(indexAccess->accessor))
      {
        if (fromEnd->index)
        {
          fromEnd->index->accept(&checker);
          auto indexType = checker.result;
          if (!indexType || (!isIntegralType(indexType->tag()) && indexType->tag() != typeinfo_tag::UNTYPED))
          {
            throw TypeCheckingException("From-end index must be integral: " + fromEnd->repr(), fromEnd->pos);
          }
        }
        auto elementType = primaryType->tag() == typeinfo_tag::TUPLE
                               ? makecheck<Untyped>()
                               : sequenceElementType(primaryType);
        if (primaryType->tag() == typeinfo_tag::TUPLE)
        {
          auto &tupleType = static_cast<TupleType &>(*primaryType);
          if (auto literal = dynamic_ast_cast<IntegralValue<int32_t>>(fromEnd->index);
              literal && literal->value > 0 && static_cast<size_t>(literal->value) <= tupleType.elementTypes.size())
          {
            elementType = tupleType.elementTypes[tupleType.elementTypes.size() - static_cast<size_t>(literal->value)];
          }
        }
        if (!elementType)
        {
          throw TypeCheckingException("Index accessor on non-contiguous sequence type: " + primaryType->repr());
        }
        movedBindings = checker.movedBindings;
        result = elementType;
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
            if (auto place = staticPlaceKey(indexAccess); place.has_value() && !allowMovedLvalueRead)
            {
              if (auto moved = movedAncestorOrSelf(movedBindings, *place); moved.has_value())
              {
                throw TypeCheckingException("Use after move: " + *moved, indexAccess->pos);
              }
              if (hasMovedDescendant(movedBindings, *place))
              {
                throw TypeCheckingException("Use after partial move: " + *place, indexAccess->pos);
              }
            }
            result = tupleType.elementTypes[idx];
            return;
          }
        }
        // Otherwise, return Untyped for dynamic index
        movedBindings = checker.movedBindings;
        result = makecheck<Untyped>();
        return;
      }
      auto elementType = sequenceElementType(primaryType);
      if (!elementType)
      {
        throw TypeCheckingException("Index accessor on non-contiguous sequence type: " + primaryType->repr());
      }
      indexAccess->accessor->accept(&checker);
      auto indexType = checker.result;
      if (!indexType || (!isIntegralType(indexType->tag()) && indexType->tag() != typeinfo_tag::UNTYPED))
      {
        throw TypeCheckingException("Invalid index type for " + typeKindName(*primaryType) + ": " + indexAccess->accessor->repr());
      }
      movedBindings = checker.movedBindings;
      if (auto place = staticPlaceKey(indexAccess); place.has_value() && !allowMovedLvalueRead)
      {
        if (auto moved = movedAncestorOrSelf(movedBindings, *place); moved.has_value())
        {
          throw TypeCheckingException("Use after move: " + *moved, indexAccess->pos);
        }
      }
      result = elementType;
    }

    void visit(RangeExpression *range) override
    {
      if (!range->start || !range->end)
      {
        throw TypeCheckingException("Open range expressions are only supported inside index access", range->pos);
      }
      TypeChecker checker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
      range->start->accept(&checker);
      auto startType = checker.result;
      range->end->accept(&checker);
      auto endType = checker.result;
      movedBindings = checker.movedBindings;
      if ((startType && startType->tag() != typeinfo_tag::UNTYPED && !isIntegralType(startType->tag())) ||
          (endType && endType->tag() != typeinfo_tag::UNTYPED && !isIntegralType(endType->tag())))
      {
        throw TypeCheckingException("Range expression bounds must be integral", range->pos);
      }
      CheckingRef<TypeInfo> elementType = makecheck<PrimitiveType>(typeinfo_tag::I32);
      if (startType && endType && startType->tag() == endType->tag() && isIntegralType(startType->tag()))
      {
        elementType = startType;
      }
      result = makecheck<RangeType>(elementType);
    }

    void visit(FromEndIndexExpression *fromEnd) override
    {
      throw TypeCheckingException("From-end index is only supported inside index access: " + fromEnd->repr(),
                                  fromEnd->pos);
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

      TypeChecker checker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
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
      auto numericMemberIndex = [&]() -> std::optional<size_t> {
        if (memberName.empty() ||
            !std::ranges::all_of(memberName, [](unsigned char ch) { return std::isdigit(ch) != 0; }))
        {
          return std::nullopt;
        }
        try
        {
          return static_cast<size_t>(std::stoull(memberName));
        }
        catch (const std::exception &)
        {
          throw TypeCheckingException("Tuple element index out of range: " + memberName, idAccExpr->pos);
        }
      };

      // Adhoc polymorphic member access on built-in collection types
      // Tuple, varargs, and contiguous sequence types support common members like .size.
      auto tag = primaryType->tag();
      bool isCollectionType = (tag == typeinfo_tag::TUPLE || tag == typeinfo_tag::VARARGS ||
                               isSequenceType(primaryType));
      if (isCollectionType)
      {
        if (tag == typeinfo_tag::TUPLE)
        {
          auto &tupleType = static_cast<TupleType &>(*primaryType);
          if (auto index = numericMemberIndex(); index.has_value())
          {
            if (*index >= tupleType.elementTypes.size())
            {
              throw TypeCheckingException("Tuple element index out of range: " + memberName, idAccExpr->pos);
            }
            memberType = tupleType.elementTypes[*index];
          }
        }
        else if (tag == typeinfo_tag::VARARGS)
        {
          auto &varargsType = static_cast<VarargsType &>(*primaryType);
          if (auto index = numericMemberIndex(); index.has_value())
          {
            if (*index >= varargsType.elementTypes.size())
            {
              throw TypeCheckingException("Tuple element index out of range: " + memberName, idAccExpr->pos);
            }
            memberType = varargsType.elementTypes[*index];
          }
        }
        if (memberName == "size")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::U32);
        }
        else if (memberName == "get")
        {
          if (auto elementType = sequenceElementType(primaryType))
          {
            memberType = makecheck<FunctionType>(
                elementType, Vec<CheckingRef<TypeInfo>>{primaryType, makecheck<PrimitiveType>(typeinfo_tag::I32)});
          }
        }
      }

      if (primaryType && primaryType->tag() == typeinfo_tag::CUSTOMIZED)
      {
        auto customPtr = std::dynamic_pointer_cast<CustomizedType>(primaryType);
        if (auto localType = locals.find(customPtr->name); localType != locals.end())
        {
          auto unwrappedLocal = unwrap(localType->second);
          if (unwrappedLocal && unwrappedLocal->tag() == typeinfo_tag::CUSTOMIZED)
          {
            customPtr = std::static_pointer_cast<CustomizedType>(unwrappedLocal);
          }
        }
        if (customPtr->properties.contains(memberName))
        {
          memberType = customPtr->properties[memberName];
        }
        else if (customPtr->memberFunctions.contains(memberName))
        {
          memberType = customPtr->memberFunctions[memberName];
        }
        if (!customPtr->properties.contains(memberName))
        {
          Vec<Str> traitCandidates;
          for (auto &[traitName, methods] : customPtr->traitMemberFunctions)
          {
            if (methods.contains(memberName))
            {
              traitCandidates.push_back(traitName);
            }
          }
          if (traitCandidates.size() > 1 && !customPtr->memberFunctions.contains(memberName))
          {
            Str candidates;
            for (size_t i = 0; i < traitCandidates.size(); ++i)
            {
              if (i > 0) candidates += ", ";
              candidates += traitCandidates[i] + "::" + memberName;
            }
            throw TypeCheckingException("Ambiguous trait method call " + memberName + ": candidates " + candidates,
                                        idAccExpr->pos);
          }
          if (traitCandidates.size() == 1 && !customPtr->memberFunctions.contains(memberName))
          {
            memberType = customPtr->traitMemberFunctions[traitCandidates.front()][memberName];
          }
        }
      }
      else if (primaryType && primaryType->tag() == typeinfo_tag::GENERIC_PARAM)
      {
        auto &genericType = static_cast<GenericParamType &>(*primaryType);
        if (!genericType.bound.empty())
        {
          auto traitIt = locals.find(genericType.bound);
          if (traitIt != locals.end() && traitIt->second && traitIt->second->tag() == typeinfo_tag::TRAIT)
          {
            auto &traitType = static_cast<TraitType &>(*traitIt->second);
            auto &methods = !traitType.allMethods.empty() ? traitType.allMethods : traitType.methods;
            if (methods.contains(memberName))
            {
              memberType = methods[memberName];
            }
          }
        }
        if (genericType.name == "Self" && (!memberType || memberType->tag() != typeinfo_tag::FUNCTION))
        {
          throw TypeCheckingException("Trait default method cannot access member '" + memberName +
                                          "' on abstract Self",
                                      idAccExpr->pos);
        }
      }

      // Property access on tagged union variant types (after switch/case narrowing)
      if (primaryType && primaryType->tag() == typeinfo_tag::VARIANT)
      {
        if (memberName == "tag")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::STRING);
        }
        else if (memberName == "index")
        {
          memberType = makecheck<PrimitiveType>(typeinfo_tag::I32);
        }
        else
        {
          auto &variantType = static_cast<VariantType &>(*primaryType);
          if (!variantType.payloadNames.empty())
          {
            // Look up named payload field
            for (size_t i = 0; i < variantType.payloadNames.size(); ++i)
            {
              if (i < variantType.payloadTypes.size() && variantType.payloadNames[i] == memberName)
              {
                memberType = variantType.payloadTypes[i];
                break;
              }
            }
          }
        }
      }

      if (!idAccExpr->arguments.empty() ||
          (idAccExpr->pos.line != 0)) // Hack to detect if it's potentially a call
      {
        if (auto funcType = std::dynamic_pointer_cast<FunctionType>(memberType))
        {
          if (auto receiverPlace = staticPlaceKey(idAccExpr->primaryExpression.get());
              receiverPlace.has_value() && !allowMovedLvalueRead)
          {
            validateAndApplyMethodEffects(*funcType, *receiverPlace, idAccExpr->pos);
          }
          result = funcType->returnType;
          return;
        }
      }

      if (auto place = staticPlaceKey(idAccExpr); place.has_value() && !allowMovedLvalueRead)
      {
        if (auto moved = movedAncestorOrSelf(movedBindings, *place); moved.has_value())
        {
          throw TypeCheckingException("Use after move: " + *moved, idAccExpr->pos);
        }
        if (hasMovedDescendant(movedBindings, *place))
        {
          throw TypeCheckingException("Use after partial move: " + *place, idAccExpr->pos);
        }
      }

      result = memberType;
    }

    void visit(QualifiedTraitCallExpression *qualifiedCall) override
    {
      auto traitIt = locals.find(qualifiedCall->traitName);
      auto trait = traitIt == locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
      if (!trait)
      {
        throw TypeCheckingException("Unknown trait: " + qualifiedCall->traitName, qualifiedCall->pos);
      }
      auto &methods = trait->allMethods.empty() ? trait->methods : trait->allMethods;
      if (!methods.contains(qualifiedCall->methodName))
      {
        throw TypeCheckingException("Trait " + trait->name + " has no method " + qualifiedCall->methodName,
                                    qualifiedCall->pos);
      }

      TypeChecker checker{locals, {}, nullptr, movedBindings};
      CheckingRef<TypeInfo> receiverType;
      size_t firstRegularArg = 0;
      if (qualifiedCall->receiver)
      {
        qualifiedCall->receiver->accept(&checker);
        receiverType = checker.result;
      }
      else
      {
        if (qualifiedCall->arguments.empty())
        {
          throw TypeCheckingException("Trait-qualified call requires a receiver argument", qualifiedCall->pos);
        }
        qualifiedCall->arguments.front()->accept(&checker);
        receiverType = checker.result;
        firstRegularArg = 1;
      }
      receiverType = deref_reference_type(receiverType);
      if (!typeSatisfiesTrait(receiverType, *trait))
      {
        throw TypeCheckingException("Type " + (receiverType ? receiverType->repr() : Str{"?"}) +
                                        " does not implement trait " + trait->name,
                                    qualifiedCall->pos);
      }

      Vec<CheckingRef<TypeInfo>> argumentTypes;
      for (size_t i = firstRegularArg; i < qualifiedCall->arguments.size(); ++i)
      {
        qualifiedCall->arguments[i]->accept(&checker);
        if (dynamic_ast_cast<SpreadExpression>(qualifiedCall->arguments[i]))
        {
          if (checker.spreadResult.empty())
          {
            throw TypeCheckingException("Spread call arguments require compile-time tuple arity", qualifiedCall->pos);
          }
          argumentTypes.insert(argumentTypes.end(), checker.spreadResult.begin(), checker.spreadResult.end());
          checker.spreadResult.clear();
        }
        else
        {
          argumentTypes.push_back(checker.result);
        }
      }
      movedBindings = checker.movedBindings;

      auto funcType = methods[qualifiedCall->methodName];
      Vec<CheckingRef<TypeInfo>> expectedArgs;
      for (size_t i = 1; i < funcType->parametersType.size(); ++i)
      {
        expectedArgs.push_back(funcType->parametersType[i]);
      }
      auto expectedFunction = makecheck<FunctionType>(funcType->returnType, expectedArgs);
      if (!functionApplyWithCoercions(*expectedFunction, argumentTypes))
      {
        throw TypeCheckingException("Invalid argument types for trait-qualified call: " + qualifiedCall->repr(),
                                    qualifiedCall->pos);
      }
      const Expression *receiverExpr = qualifiedCall->receiver ? qualifiedCall->receiver.get()
                                                               : qualifiedCall->arguments.front().get();
      if (auto receiverPlace = staticPlaceKey(receiverExpr); receiverPlace.has_value() && !allowMovedLvalueRead)
      {
        validateAndApplyMethodEffects(*funcType, *receiverPlace, qualifiedCall->pos);
      }
      result = funcType->returnType;
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
        if (customType->abstract)
        {
          throw TypeCheckingException("Abstract type cannot be constructed with new: " + customType->name,
                                      newObj->pos);
        }
        if (customType->nativeOpaque)
        {
          throw TypeCheckingException("Native opaque type cannot be constructed with new: " + customType->name,
                                      newObj->pos);
        }
        TypeChecker checker{locals, {}, nullptr, movedBindings};
        for (auto &&[name, expr] : newObj->properties)
        {
          if (!customType->properties.contains(name))
          {
            throw TypeCheckingException("Unknown property '" + name + "' for type " + customType->name, expr->pos);
          }
          checker.expectedType = customType->properties[name];
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
                                 variantInfo->payloadNames, variantInfo->unionType->moduleId));
    }

    void visit(IndexAssignmentExpression *indexAssign) override
    {
      TypeChecker checker{locals, {}, nullptr, movedBindings, true, activeGenericInstanceName};
      indexAssign->primary->accept(&checker);
      auto primaryType = deref_reference_type(checker.result);
      if (!primaryType)
      {
        throw TypeCheckingException("Invalid index assignment expression: " + indexAssign->primary->repr());
      }
      auto sequenceElementType = this->sequenceElementType(primaryType);
      if (!sequenceElementType && primaryType->tag() != typeinfo_tag::TUPLE)
      {
        throw TypeCheckingException("Index assignment on non-contiguous sequence type: " + primaryType->repr());
      }
      indexAssign->accessor->accept(&checker);
      auto indexType = checker.result;
      if (!indexType || !isIntegralType(indexType->tag()))
      {
        const auto targetName = sequenceElementType ? typeKindName(*primaryType) : Str{"tuple"};
        throw TypeCheckingException("Invalid index type for " + targetName + ": " + indexAssign->accessor->repr());
      }
      indexAssign->value->accept(&checker);
      auto valueType = checker.result;
      CheckingRef<TypeInfo> expectedElementType;
      if (sequenceElementType)
      {
        expectedElementType = sequenceElementType;
      }
      else
      {
        auto &tupleType = static_cast<TupleType &>(*primaryType);
        auto intLit = dynamic_ast_cast<IntegralValue<int32_t>>(indexAssign->accessor);
        if (!intLit)
        {
          throw TypeCheckingException("Tuple index assignment requires a constant integer index.",
                                      indexAssign->accessor->pos);
        }
        auto idx = static_cast<size_t>(intLit->value);
        if (idx >= tupleType.elementTypes.size())
        {
          throw TypeCheckingException("Tuple index out of range: " + std::to_string(idx), indexAssign->accessor->pos);
        }
        expectedElementType = tupleType.elementTypes[idx];
      }
      if (!typeMatch(*expectedElementType, *valueType))
      {
        const auto targetName = sequenceElementType ? typeKindName(*primaryType) : Str{"tuple"};
        throw TypeCheckingException("Invalid value type for " + targetName + " assignment: " + valueType->repr());
      }
      movedBindings = checker.movedBindings;
      if (auto place = staticPlaceKey(indexAssign); place.has_value())
      {
        rejectBorrowConflict("assign", *place, indexAssign->pos);
        clearMovedPlace(movedBindings, *place);
      }
      result = expectedElementType;
    }

    // ── Expression visitors ─────────────────────────────────────────────
    void visit(IdExpression *id) override
    {
      auto it = locals.find(id->id);
      if (it != locals.end())
      {
        if (!allowMovedLvalueRead)
        {
          if (movedBindings.contains(id->id))
          {
            throw TypeCheckingException("Use after move: " + id->id, id->pos);
          }
          if (hasMovedDescendant(movedBindings, id->id))
          {
            throw TypeCheckingException("Use after partial move: " + id->id, id->pos);
          }
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
      if (targetType && targetType->tag() == typeinfo_tag::NEW_TYPE)
      {
        auto &nt = static_cast<NewTypeType &>(*targetType);
        if (nt.wrappedType->match(*exprType) || exprType->tag() == typeinfo_tag::UNTYPED)
        {
          result = targetType;
          return;
        }
      }

      // Allow unwrap: NewType(T) -> T
      if (exprType && exprType->tag() == typeinfo_tag::NEW_TYPE)
      {
        auto &nt = static_cast<NewTypeType &>(*exprType);
        if (targetType->match(*nt.wrappedType) || targetType->tag() == typeinfo_tag::UNTYPED)
        {
          result = targetType;
          return;
        }
      }

      // Allow cast through type alias (transparent)
      if (exprType && exprType->tag() == typeinfo_tag::TYPE_ALIAS)
      {
        auto &alias = static_cast<TypeAliasType &>(*exprType);
        if (alias.underlyingType->match(*targetType) || targetType->match(*alias.underlyingType))
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

    auto loadModuleArtifacts(const ImportDecl &importDecl, const Str &moduleId) -> ModuleArtifacts
    {
      if (auto cached = moduleArtifactsById.find(moduleId); cached != moduleArtifactsById.end())
      {
        return cached->second;
      }
      auto &registry = NG::module::get_module_registry();
      const bool forceSourceLoad =
          std::ranges::find(modulePaths, "[force-source-module-loader]") != modulePaths.end();
      if (!activeModuleChecks.insert(moduleId).second)
      {
        return {};
      }
      struct ActiveGuard
      {
        Str moduleId;
        ~ActiveGuard()
        {
          TypeChecker::activeModuleChecks.erase(moduleId);
        }
      } guard{moduleId};

      auto moduleInfo = registry.queryModuleById(moduleId);
      if (forceSourceLoad)
      {
        moduleInfo = {};
      }
      if (!moduleInfo || !moduleInfo->moduleAst)
      {
        NG::module::FileBasedExternalModuleLoader loader{modulePaths};
        moduleInfo = loader.load(importDecl.modulePath);
        if (moduleInfo)
        {
          if (forceSourceLoad && moduleInfo->moduleAst)
          {
            retainedPreludeImportAsts.push_back(moduleInfo->moduleAst);
          }
          registry.addModuleInfo(moduleInfo);
        }
      }
      if (!moduleInfo || !moduleInfo->moduleAst)
      {
        if (!forceSourceLoad)
        {
          if (auto artifact = registry.queryArtifactById(moduleId); artifact && hasPublishedTypeMetadata(*artifact))
          {
            auto artifacts = toLocalModuleArtifacts(*artifact);
            moduleArtifactsById[moduleId] = artifacts;
            return artifacts;
          }
        }
        return {};
      }

      TypeChecker checker{locals, {}, nullptr, {}, false, "", modulePaths};
      checker.currentModuleId = moduleInfo->moduleId.empty() ? moduleId : moduleInfo->moduleId;
      moduleInfo->moduleAst->accept(&checker);
      moduleInfo->moduleTypeIndex = checker.type_index;

      if (auto artifact = registry.queryArtifactById(checker.currentModuleId); artifact && hasPublishedTypeMetadata(*artifact))
      {
        auto artifacts = toLocalModuleArtifacts(*artifact);
        moduleArtifactsById[checker.currentModuleId] = artifacts;
        if (checker.currentModuleId != moduleId)
        {
          moduleArtifactsById[moduleId] = artifacts;
        }
        return artifacts;
      }

      if (auto cached = moduleArtifactsById.find(checker.currentModuleId); cached != moduleArtifactsById.end())
      {
        if (checker.currentModuleId != moduleId)
        {
          moduleArtifactsById[moduleId] = cached->second;
        }
        return cached->second;
      }
      return {};
    }

    void importCheckedModuleArtifacts(const ImportDecl &importDecl, const Str & /*moduleId*/,
                                      const ModuleArtifacts &artifacts)
    {
      if (!importDecl.alias.empty())
      {
        locals.insert_or_assign(importDecl.alias, makecheck<Untyped>());
        importedSymbolNames.insert(importDecl.alias);
      }
      else
      {
        locals.insert_or_assign(importDecl.module, makecheck<Untyped>());
        importedSymbolNames.insert(importDecl.module);
      }

      const bool importAll = std::ranges::find(importDecl.imports, "*") != importDecl.imports.end();
      Set<Str> namesToImport;
      if (importAll)
      {
        for (const auto &[name, _type] : artifacts.exportedTypes)
        {
          namesToImport.insert(name);
        }
      }
      else
      {
        namesToImport.insert(importDecl.imports.begin(), importDecl.imports.end());
      }

      for (const auto &name : namesToImport)
      {
        if (auto it = artifacts.exportedTypes.find(name); it != artifacts.exportedTypes.end())
        {
          locals.insert_or_assign(name, it->second);
        }
        else
        {
          locals.insert_or_assign(name, makecheck<Untyped>());
        }
        importedSymbolNames.insert(name);
        if (importDecl.exported)
        {
          exportedImportNames.insert(name);
        }
      }

      Set<Str> compileTimeNamesToImport;
      if (importAll)
      {
        for (const auto &[name, _defs] : artifacts.exportedTypeAliasSpecializations)
        {
          compileTimeNamesToImport.insert(name);
        }
        for (const auto &[name, _defs] : artifacts.exportedConstPredicates)
        {
          compileTimeNamesToImport.insert(name);
        }
        for (const auto &[name, _defs] : artifacts.exportedConstFunctions)
        {
          compileTimeNamesToImport.insert(name);
        }
      }
      else
      {
        compileTimeNamesToImport.insert(importDecl.imports.begin(), importDecl.imports.end());
      }

      auto importCompileTimeDefs = [&compileTimeNamesToImport](auto &active, const auto &exported) {
        for (const auto &name : compileTimeNamesToImport)
        {
          if (auto it = exported.find(name); it != exported.end())
          {
            auto &defs = active[name];
            defs.insert(defs.end(), it->second.begin(), it->second.end());
          }
        }
      };
      importCompileTimeDefs(activeTypeAliasSpecializations, artifacts.exportedTypeAliasSpecializations);
      importCompileTimeDefs(activeConstPredicates, artifacts.exportedConstPredicates);
      importCompileTimeDefs(activeConstFunctions, artifacts.exportedConstFunctions);

      if (!importAll)
      {
        return;
      }

      for (const auto &impl : artifacts.exportedImpls)
      {
        auto implName = "impl " + impl.traitName + " for " + impl.targetPattern;
        importedImplNames.insert(implName);
        auto registered = registerTraitImplRecord(impl, importDecl.pos);
        if (!registered)
        {
          continue;
        }
        CheckingRef<TypeInfo> targetType;
        if (impl.definition)
        {
          auto targetScope = locals;
          addGenericParamsToScope(targetScope, impl.definition->genericParams);
          TypeChecker targetChecker{targetScope, {}, nullptr, {}, false, "", modulePaths};
          impl.definition->targetType->accept(&targetChecker);
          targetType = targetChecker.result;
        }
        else
        {
          targetType = type_from_repr(impl.targetPattern);
        }
        auto custom = std::dynamic_pointer_cast<CustomizedType>(unwrap(targetType));
        if (custom)
        {
          if (auto localTarget = locals.find(custom->name); localTarget != locals.end())
          {
            if (auto localCustom = std::dynamic_pointer_cast<CustomizedType>(unwrap(localTarget->second)))
            {
              custom = localCustom;
            }
          }
        }
        if (!custom)
        {
          continue;
        }
        auto &implTraits = trait_impls_by_type[custom->name];
        if (std::ranges::find(implTraits, impl.traitName) == implTraits.end())
        {
          implTraits.push_back(impl.traitName);
        }
        auto traitIt = locals.find(impl.traitName);
        auto trait = traitIt == locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
        if (trait)
        {
          auto &methods = trait->allMethods.empty() ? trait->methods : trait->allMethods;
          for (const auto &[methodName, methodType] : methods)
          {
            custom->traitMemberFunctions[trait->name][methodName] = methodType;
            custom->memberFunctions[trait->name + "::" + methodName] = methodType;
          }
        }
      }
    }

    void visit(ImportDecl *importDecl) override
    {
      auto moduleId = moduleIdFromPath(importDecl->modulePath);
      if (moduleId.empty())
      {
        moduleId = importDecl->module;
      }
      if (!importDecl->alias.empty())
      {
        importAliases[importDecl->alias] = moduleId;
      }
      importAliases[importDecl->module] = moduleId;
      if (!moduleId.empty() && std::ranges::find(importedModuleIds, moduleId) == importedModuleIds.end())
      {
        importedModuleIds.push_back(moduleId);
      }

      if (!modulePaths.empty())
      {
        try
        {
          auto artifacts = loadModuleArtifacts(*importDecl, moduleId);
          importCheckedModuleArtifacts(*importDecl, moduleId, artifacts);
          return;
        }
        catch (const TypeCheckingException &)
        {
          throw;
        }
        catch (const std::exception &)
        {
          // Preserve the historical loose import behavior for modules that are
          // intentionally provided by native/compiler test harnesses.
        }
      }

      if (auto artifact = NG::module::get_module_registry().queryArtifactById(moduleId);
          artifact && hasPublishedTypeMetadata(*artifact))
      {
        auto artifacts = toLocalModuleArtifacts(*artifact);
        moduleArtifactsById[moduleId] = artifacts;
        importCheckedModuleArtifacts(*importDecl, moduleId, artifacts);
        return;
      }

      // Basic support for imports in type checker:
      // Mark the module or its alias as Untyped for now
      if (!importDecl->alias.empty())
      {
        locals.insert_or_assign(importDecl->alias, makecheck<Untyped>());
        importedSymbolNames.insert(importDecl->alias);
      }
      else
      {
        locals.insert_or_assign(importDecl->module, makecheck<Untyped>());
        importedSymbolNames.insert(importDecl->module);
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
          if (importDecl->exported)
          {
            exportedImportNames.insert(imp);
          }
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
          TypeChecker caseChecker{locals, {}, nullptr, entryMovedBindings, allowMovedLvalueRead,
                                  activeGenericInstanceName};
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
        TypeChecker caseChecker{locals, {}, nullptr, entryMovedBindings, allowMovedLvalueRead,
                                activeGenericInstanceName};

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

    // ── Function call resolution ────────────────────────────────────────
    void visit(FunCallExpression *funCall) override
    {
      if (auto predicate = tryEvalConstPredicateCall(funCall); predicate.has_value())
      {
        result = makecheck<PrimitiveType>(typeinfo_tag::BOOL);
        return;
      }
      if (auto idExpr = dynamic_cast<IdExpression *>(funCall->primaryExpression.get());
          idExpr && activeConstFunctions.contains(idExpr->id) && !activeConstFunctions.at(idExpr->id).empty())
      {
        auto *fn = activeConstFunctions.at(idExpr->id).front();
        if (!fn || !fn->returnType)
        {
          throw TypeCheckingException("Const function must declare a return type: " + idExpr->id, funCall->pos);
        }
        if (fn->params.size() != funCall->arguments.size())
        {
          throw TypeCheckingException("Const function argument count mismatch: " + fn->funName, funCall->pos);
        }
        for (size_t i = 0; i < funCall->arguments.size(); ++i)
        {
          TypeChecker argChecker{locals};
          funCall->arguments[i]->accept(&argChecker);
          if (fn->params[i]->annotatedType)
          {
            TypeChecker paramChecker{locals};
            fn->params[i]->annotatedType->accept(&paramChecker);
            if (!typeMatches(*paramChecker.result, *argChecker.result))
            {
              throw TypeCheckingException("Const function argument type mismatch: " + fn->funName,
                                          funCall->arguments[i]->pos);
            }
          }
        }
        TypeChecker returnChecker{locals};
        fn->returnType->accept(&returnChecker);
        result = returnChecker.result;
        return;
      }
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
              result = makecheck<VariantType>(tuType->name, idExpr->id, 0, payloadTypes, payloadNames,
                                              tuType->moduleId);
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
              if (i < genericType->typeParamIsConst.size() && genericType->typeParamIsConst[i])
              {
                instChecker.locals[genericType->typeParamNames[i]] =
                    makecheck<ConstValueType>(genericType->typeParamNames[i], "", true);
              }
              else
              {
                instChecker.locals[genericType->typeParamNames[i]] =
                    makecheck<GenericParamType>(
                        genericType->typeParamNames[i], "",
                        i < genericType->typeParamIsPack.size() ? genericType->typeParamIsPack[i] : false,
                        i < genericType->typeParamKindArities.size() ? genericType->typeParamKindArities[i] : 0,
                        i < genericType->typeParamKindVariadicTails.size()
                            ? genericType->typeParamKindVariadicTails[i]
                            : false);
              }
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
                                                    expectedUnion->variants[idExpr->id], payloadNames,
                                                    expectedUnion->moduleId);
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
                                              payloadNames, tuType->moduleId);
              return;
            }
          }
        }
      }

      auto foldArgIt = std::find_if(funCall->arguments.begin(), funCall->arguments.end(), [](const auto &arg) {
        return dynamic_ast_cast<PostfixFoldExpression>(arg) != nullptr;
      });
      if (foldArgIt != funCall->arguments.end())
      {
        auto secondFoldArg = std::find_if(std::next(foldArgIt), funCall->arguments.end(), [](const auto &arg) {
          return dynamic_ast_cast<PostfixFoldExpression>(arg) != nullptr;
        });
        if (secondFoldArg != funCall->arguments.end())
        {
          throw TypeCheckingException("Fold call supports only one postfix fold pack", funCall->pos);
        }
        auto foldIndex = static_cast<size_t>(std::distance(funCall->arguments.begin(), foldArgIt));
        if (funCall->arguments.size() != 2 || (foldIndex != 0 && foldIndex != 1))
        {
          throw TypeCheckingException("Fold call expects `op(xs..., init)` or `op(init, xs...)`", funCall->pos);
        }
        auto fold = dynamic_ast_cast<PostfixFoldExpression>(*foldArgIt);
        if (fold->filter)
        {
          throw TypeCheckingException("Filter marker `?...` is only supported in array literals", fold->pos);
        }

        TypeChecker checker{locals, {}, nullptr, movedBindings};
        funCall->primaryExpression->accept(&checker);
        auto primaryType = checker.result;
        auto funcType = primaryType ? dynamic_cast<FunctionType *>(&(*primaryType)) : nullptr;
        if (!funcType)
        {
          throw TypeCheckingException("Fold call requires a non-generic function value", funCall->pos);
        }
        if (funcType->parametersType.size() != 2)
        {
          throw TypeCheckingException("Fold call operator must be binary: " + funcType->repr(), funCall->pos);
        }

        fold->expression->accept(&checker);
        auto sequenceType = checker.result;
        auto elementType = sequenceElementType(sequenceType);
        if (!elementType)
        {
          throw TypeCheckingException("Fold pack must be a Sequence-compatible value: " + sequenceType->repr(),
                                      fold->pos);
        }

        auto initIndex = foldIndex == 0 ? 1UZ : 0UZ;
        funCall->arguments[initIndex]->accept(&checker);
        auto accumulatorType = checker.result;
        movedBindings = checker.movedBindings;

        Vec<CheckingRef<TypeInfo>> argumentTypes =
            foldIndex == 0 ? Vec<CheckingRef<TypeInfo>>{elementType, accumulatorType}
                           : Vec<CheckingRef<TypeInfo>>{accumulatorType, elementType};
        if (!functionApplyWithCoercions(*funcType, argumentTypes))
        {
          throw TypeCheckingException("Invalid fold operator argument types for function: " + funcType->repr(),
                                      funCall->pos);
        }
        if (!typeMatches(*accumulatorType, *funcType->returnType))
        {
          throw TypeCheckingException("Fold operator return type must match accumulator type: " +
                                          funcType->returnType->repr() + " to " + accumulatorType->repr(),
                                      funCall->pos);
        }
        result = accumulatorType;
        return;
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
        for (size_t i = 0; i < funCall->arguments.size() && i < genericDef.funcDef->params.size(); ++i)
        {
          auto param = genericDef.funcDef->params[i].get();
          if (!param || !param->annotatedType)
          {
            continue;
          }
          if (!isUniquePtrAnnotation(param->annotatedType.get()))
          {
            continue;
          }
          auto movedArg = dynamic_cast<UnaryExpression *>(funCall->arguments[i].get());
          if (!movedArg || !movedArg->optr || movedArg->optr->type != TokenType::KEYWORD_MOVE)
          {
            throw TypeCheckingException("UniquePtr value must be passed with move", funCall->arguments[i]->pos);
          }
        }
        result = monomorphizeGenericCall(genericDef, funCall);
        return;
      }

      auto funcType = dynamic_cast<FunctionType *>(&(*primaryType));

      if (!funcType)
      {
        throw TypeCheckingException("Invalid function type: " + primaryType->repr(), funCall->pos);
      }
      if (funcType->deleted)
      {
        throw TypeCheckingException("Function overload is deleted: " +
                                        (funcType->deletedRepr.empty() ? funcType->repr() : funcType->deletedRepr),
                                    funCall->pos);
      }

      Vec<CheckingRef<TypeInfo>> argumentTypes;
      for (auto arg : funCall->arguments)
      {
        if (funcType && argumentTypes.size() < funcType->parametersType.size() &&
            isUniquePtrType(funcType->parametersType[argumentTypes.size()]))
        {
          auto movedArg = dynamic_cast<UnaryExpression *>(arg.get());
          if (!movedArg || !movedArg->optr || movedArg->optr->type != TokenType::KEYWORD_MOVE)
          {
            throw TypeCheckingException("UniquePtr value must be passed with move", arg->pos);
          }
        }
        arg->accept(&checker);
        if (dynamic_ast_cast<SpreadExpression>(arg))
        {
          if (checker.spreadResult.empty())
          {
            throw TypeCheckingException("Spread call arguments require compile-time tuple arity", arg->pos);
          }
          argumentTypes.insert(argumentTypes.end(), checker.spreadResult.begin(), checker.spreadResult.end());
          checker.spreadResult.clear();
        }
        else
        {
          argumentTypes.push_back(checker.result);
        }
      }
      movedBindings = checker.movedBindings;

      if (!functionApplyWithCoercions(*funcType, argumentTypes))
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

      if (paramType->tag() == typeinfo_tag::CONST_VALUE)
      {
        auto &constParam = static_cast<ConstValueType &>(*paramType);
        if (!constParam.isParam)
        {
          return;
        }
        if (auto existing = substitution.contains(constParam.value) ? substitution[constParam.value] : nullptr)
        {
          if (existing->tag() == typeinfo_tag::CONST_VALUE)
          {
            auto existingConst = std::static_pointer_cast<ConstValueType>(existing);
            if (existingConst->isParam)
            {
              substitution[constParam.value] = argType;
            }
            else if (!existing->match(*argType))
            {
              throw TypeCheckingException("Inconsistent bindings for const generic parameter '" + constParam.value +
                                          "': " + existing->repr() + " vs " + argType->repr());
            }
          }
        }
        else
        {
          substitution[constParam.value] = argType;
        }
        return;
      }

      if (paramType->tag() == typeinfo_tag::GENERIC_PARAM)
      {
        auto &gp = static_cast<GenericParamType &>(*paramType);
        if (auto existing = substitution.contains(gp.name) ? substitution[gp.name] : nullptr)
        {
          if (existing->tag() == typeinfo_tag::GENERIC_PARAM)
          {
            substitution[gp.name] = argType;
          }
          else if (argType && argType->tag() != typeinfo_tag::GENERIC_PARAM && !typeMatch(*existing, *argType))
          {
            throw TypeCheckingException("Inconsistent bindings for generic parameter '" + gp.name + "': " +
                                        existing->repr() + " vs " + argType->repr());
          }
        }
        else
        {
          substitution[gp.name] = argType;
        }
        return;
      }

      if (paramType->tag() == typeinfo_tag::ARRAY && argType->tag() == typeinfo_tag::ARRAY)
      {
        auto &paramArr = static_cast<ArrayType &>(*paramType);
        auto &argArr = static_cast<ArrayType &>(*argType);
        extractGenericBindingsImpl(paramArr.elementType, argArr.elementType, substitution, seen);
        if (paramArr.length && argArr.length)
        {
          extractGenericBindingsImpl(paramArr.length, argArr.length, substitution, seen);
        }
        return;
      }

      if (paramType->tag() == typeinfo_tag::VECTOR && argType->tag() == typeinfo_tag::VECTOR)
      {
        auto &paramVec = static_cast<VectorType &>(*paramType);
        auto &argVec = static_cast<VectorType &>(*argType);
        extractGenericBindingsImpl(paramVec.elementType, argVec.elementType, substitution, seen);
        return;
      }

      if (paramType->tag() == typeinfo_tag::SPAN && argType->tag() == typeinfo_tag::SPAN)
      {
        auto &paramSpan = static_cast<SpanType &>(*paramType);
        auto &argSpan = static_cast<SpanType &>(*argType);
        extractGenericBindingsImpl(paramSpan.elementType, argSpan.elementType, substitution, seen);
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

      if (paramType->tag() == typeinfo_tag::TYPE_CONSTRUCTOR_APPLICATION)
      {
        auto &paramApp = static_cast<TypeConstructorApplicationType &>(*paramType);
        Str argBase;
        Vec<Str> argArgs;
        if (argType)
        {
          Str name;
          switch (argType->tag())
          {
          case typeinfo_tag::CUSTOMIZED:    name = static_cast<const CustomizedType &>(*argType).name; break;
          case typeinfo_tag::TYPE_ALIAS:    name = static_cast<const TypeAliasType &>(*argType).name; break;
          case typeinfo_tag::NEW_TYPE:      name = static_cast<const NewTypeType &>(*argType).name; break;
          case typeinfo_tag::TAGGED_UNION:  name = static_cast<const TaggedUnionType &>(*argType).name; break;
          default: break;
          }
          if (!name.empty())
          {
            argBase = stripTypeInstanceSuffix(name);
            argArgs = parseTypeInstanceArgs(name);
          }
        }

        if (argBase.empty() || argArgs.size() != paramApp.typeArgs.size())
        {
          return;
        }

        if (auto constructorParam = std::dynamic_pointer_cast<GenericParamType>(paramApp.constructorType))
        {
          if (auto constructorIt = locals.find(argBase); constructorIt != locals.end())
          {
            const size_t actualArity = typeKindArity(constructorIt->second);
            const bool actualVariadicTail = typeKindVariadicTail(constructorIt->second);
            if (actualArity == constructorParam->kindArity &&
                actualVariadicTail == constructorParam->kindVariadicTail)
            {
              if (auto existing = substitution.contains(constructorParam->name) ? substitution[constructorParam->name] : nullptr;
                  !existing || existing->tag() == typeinfo_tag::GENERIC_PARAM)
              {
                substitution[constructorParam->name] = constructorIt->second;
              }
            }
          }
        }

        for (size_t i = 0; i < paramApp.typeArgs.size() && i < argArgs.size(); ++i)
        {
          CheckingRef<TypeInfo> argConcrete;
          if (auto argIt = locals.find(argArgs[i]); argIt != locals.end())
          {
            argConcrete = argIt->second;
          }
          else if (auto primitive = PrimitiveType::from(argArgs[i]))
          {
            argConcrete = primitive;
          }
          else
          {
            argConcrete = makecheck<CustomizedType>(argArgs[i]);
          }
          extractGenericBindingsImpl(paramApp.typeArgs[i], argConcrete, substitution, seen);
        }
        return;
      }

      if (paramType->tag() == typeinfo_tag::CUSTOMIZED && argType->tag() == typeinfo_tag::CUSTOMIZED)
      {
        auto &paramCustom = static_cast<CustomizedType &>(*paramType);
        auto &argCustom = static_cast<CustomizedType &>(*argType);
        auto paramBase = stripTypeInstanceSuffix(paramCustom.name);
        auto argBase = stripTypeInstanceSuffix(argCustom.name);
        if (paramBase != argBase || paramCustom.name == argCustom.name)
        {
          return;
        }

        auto paramArgs = parseTypeInstanceArgs(paramCustom.name);
        auto argArgs = parseTypeInstanceArgs(argCustom.name);
        for (size_t i = 0; i < paramArgs.size() && i < argArgs.size(); ++i)
        {
          auto paramIt = substitution.find(paramArgs[i]);
          CheckingRef<TypeInfo> argConcrete;
          if (auto argIt = locals.find(argArgs[i]); argIt != locals.end())
          {
            argConcrete = argIt->second;
          }
          else
          {
            argConcrete = PrimitiveType::from(argArgs[i]);
          }
          if (paramIt != substitution.end() && argConcrete)
          {
            extractGenericBindingsImpl(paramIt->second, argConcrete, substitution, seen);
          }
        }
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
      // 1. Type-check arguments
      TypeChecker argChecker{locals, {}, nullptr, movedBindings, allowMovedLvalueRead, activeGenericInstanceName};
      Vec<CheckingRef<TypeInfo>> argumentTypes;
      for (auto arg : funCall->arguments)
      {
        arg->accept(&argChecker);
        if (dynamic_ast_cast<SpreadExpression>(arg))
        {
          if (argChecker.spreadResult.empty())
          {
            throw TypeCheckingException("Spread call arguments require compile-time tuple arity", arg->pos);
          }
          argumentTypes.insert(argumentTypes.end(), argChecker.spreadResult.begin(), argChecker.spreadResult.end());
          argChecker.spreadResult.clear();
        }
        else
        {
          argumentTypes.push_back(argChecker.result);
        }
      }
      movedBindings = argChecker.movedBindings;

      auto *funcDef = selectGenericFunctionCandidate(
          genericDef, argumentTypes, funCall->genericArgs.empty() ? Str::npos : funCall->genericArgs.size());
      if (!funcDef)
      {
        throw TypeCheckingException("No matching generic function overload: " + genericDef.name, funCall->pos);
      }
      if (funcDef->deleted)
      {
        throw TypeCheckingException("Function overload is deleted: " + funcDef->repr(), funCall->pos);
      }

      Vec<Str> typeParamNames;
      Vec<bool> typeParamIsPack;
      Vec<bool> typeParamIsConst;
      for (auto &genericParam : funcDef->genericParams)
      {
        typeParamNames.push_back(genericParam->name);
        typeParamIsPack.push_back(genericParam->isPack);
        typeParamIsConst.push_back(genericParam->isConst);
      }
      auto typeParamKindArities = genericParamKindArities(funcDef->genericParams);
      auto typeParamKindVariadicTails = genericParamKindVariadicTails(funcDef->genericParams);

      // 2. Inject GenericParamType entries (with pack flags) into a working scope
      Map<Str, CheckingRef<TypeInfo>> substitution;
      for (size_t pi = 0; pi < typeParamNames.size(); ++pi)
      {
        bool isPack = (pi < typeParamIsPack.size()) ? typeParamIsPack[pi] : false;
        size_t kindArity = (pi < typeParamKindArities.size()) ? typeParamKindArities[pi] : 0;
        bool kindVariadicTail = pi < typeParamKindVariadicTails.size()
                                    ? typeParamKindVariadicTails[pi]
                                    : false;
        Str bound;
        if (pi < funcDef->genericParams.size())
        {
          bound = typeParamBoundName(*funcDef->genericParams[pi]);
        }
        if (pi < typeParamIsConst.size() && typeParamIsConst[pi])
        {
          substitution[typeParamNames[pi]] = makecheck<ConstValueType>(
              typeParamNames[pi],
              pi < funcDef->genericParams.size() && funcDef->genericParams[pi]->constType
                  ? funcDef->genericParams[pi]->constType->repr()
                  : "",
              true);
        }
        else
        {
          substitution[typeParamNames[pi]] = makecheck<GenericParamType>(typeParamNames[pi], bound, isPack,
                                                                         kindArity, kindVariadicTail);
        }
      }
      addWhereBoundsToScope(substitution, funcDef->whereBounds);
      if (!funCall->genericArgs.empty())
      {
        if (funCall->genericArgs.size() != typeParamNames.size())
        {
          throw TypeCheckingException("Generic function '" + genericDef.name + "' expects " +
                                          std::to_string(typeParamNames.size()) + " type argument(s), got " +
                                          std::to_string(funCall->genericArgs.size()),
                                      funCall->pos);
        }
        for (size_t pi = 0; pi < funCall->genericArgs.size(); ++pi)
        {
          const size_t expectedArity =
              pi < typeParamKindArities.size() ? typeParamKindArities[pi] : 0;
          const bool expectedVariadicTail = pi < typeParamKindVariadicTails.size()
                                                ? typeParamKindVariadicTails[pi]
                                                : false;
          const bool expectedConst = pi < typeParamIsConst.size() && typeParamIsConst[pi];
          substitution[typeParamNames[pi]] =
              resolveGenericArgument(funCall->genericArgs[pi].get(), expectedConst,
                                     pi < funcDef->genericParams.size() && funcDef->genericParams[pi]->constType
                                         ? funcDef->genericParams[pi]->constType->repr()
                                         : "",
                                     expectedArity, expectedVariadicTail, typeParamNames[pi]);
        }
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
      for (auto &bound : funcDef->whereBounds)
      {
        if (!bound || !bound->subject || !bound->trait)
        {
          continue;
        }
        auto subIt = substitution.find(bound->subject->name);
        auto traitIt = locals.find(bound->trait->repr());
        auto trait = traitIt == locals.end() ? nullptr : std::dynamic_pointer_cast<TraitType>(traitIt->second);
        if (subIt != substitution.end() && trait && !typeSatisfiesTrait(subIt->second, *trait))
        {
          throw TypeCheckingException("Type '" + subIt->second->repr() + "' does not implement trait '" +
                                          trait->name + "'",
                                      funCall->pos);
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
            if (auto varargsParam = std::dynamic_pointer_cast<VarargsType>(unwrap(paramType)); varargsParam)
            {
              if (varargsParam->elementTypes.empty())
              {
                break;
              }
              auto elementPattern = varargsParam->elementTypes.front();
              for (size_t j = i; j < argumentTypes.size(); ++j)
              {
                extractGenericBindings(elementPattern, argumentTypes[j], substitution);
              }
              break; // Homogeneous varargs consume all remaining args.
            }
            // Non-pack: unify param type with argument type
            if (i < argumentTypes.size())
            {
              extractGenericBindings(paramType, argumentTypes[i], substitution);
            }
          }
        }
      }

      if (expectedType && funcDef->returnType)
      {
        TypeChecker retPatternChecker{locals};
        for (auto &[name, type] : substitution)
        {
          retPatternChecker.locals[name] = type;
        }
        funcDef->returnType->accept(&retPatternChecker);
        if (retPatternChecker.result)
        {
          extractGenericBindings(retPatternChecker.result, expectedType, substitution);
        }
      }

      for (size_t i = 0; i < funcDef->params.size() && i < argumentTypes.size(); ++i)
      {
        auto &param = funcDef->params[i];
        if (!param->annotatedType)
        {
          continue;
        }
        auto usesHigherKindedParam = [&](const TypeAnnotation *annotation, const auto &self) -> bool {
          if (!annotation)
          {
            return false;
          }
          auto paramIt = std::find(typeParamNames.begin(), typeParamNames.end(), annotation->name);
          if (paramIt != typeParamNames.end())
          {
            auto index = static_cast<size_t>(std::distance(typeParamNames.begin(), paramIt));
            auto kindArity = index < typeParamKindArities.size()
                                 ? typeParamKindArities[index]
                                 : 0;
            auto kindVariadicTail = index < typeParamKindVariadicTails.size()
                                        ? typeParamKindVariadicTails[index]
                                        : false;
            if ((kindArity > 0 || kindVariadicTail) && !annotation->genericArgs.empty())
            {
              return true;
            }
          }
          for (auto &arg : annotation->genericArgs)
          {
            if (self(arg.get(), self))
            {
              return true;
            }
          }
          for (auto &arg : annotation->arguments)
          {
            if (auto nested = dynamic_ast_cast<TypeAnnotation>(arg); nested && self(nested.get(), self))
            {
              return true;
            }
          }
          return false;
        };
        if (!usesHigherKindedParam(param->annotatedType.get(), usesHigherKindedParam))
        {
          continue;
        }
        TypeChecker finalParamChecker{locals};
        for (auto &[name, type] : substitution)
        {
          finalParamChecker.locals[name] = type;
        }
        param->annotatedType->accept(&finalParamChecker);
        auto paramType = finalParamChecker.result;
        if (!paramType || paramType->tag() == typeinfo_tag::UNTYPED || argumentTypes[i]->tag() == typeinfo_tag::UNTYPED)
        {
          continue;
        }
        if (paramType->tag() == typeinfo_tag::VARARGS)
        {
          continue;
        }
        if (!typeMatches(*paramType, *argumentTypes[i]))
        {
          throw TypeCheckingException("Invalid argument type for generic function '" + genericDef.name + "': " +
                                          argumentTypes[i]->repr() + " to " + paramType->repr(),
                                      funCall->arguments[i]->pos);
        }
      }

      // Fill in any unsubstituted type params with Untyped
      for (auto &name : typeParamNames)
      {
        auto subIt = substitution.find(name);
        if (subIt != substitution.end() && subIt->second && subIt->second->tag() == typeinfo_tag::CONST_VALUE)
        {
          auto constParam = std::static_pointer_cast<ConstValueType>(subIt->second);
          if (constParam->isParam)
          {
            throw TypeCheckingException("Could not infer const generic parameter '" + name + "'", funCall->pos);
          }
        }
        if (!substitution.contains(name))
        {
          substitution[name] = makecheck<Untyped>();
        }
      }
      {
        TypeChecker whereChecker{locals};
        whereChecker.trait_impls_by_type = trait_impls_by_type;
        for (auto &[name, type] : substitution)
        {
          whereChecker.locals[name] = type;
        }
        whereChecker.validateWherePredicates(funcDef->whereBounds, funCall->pos);
      }

      Vec<CheckingRef<TypeInfo>> instantiatedArgs;
      instantiatedArgs.reserve(typeParamNames.size());
      for (const auto &name : typeParamNames)
      {
        instantiatedArgs.push_back(substitution[name]);
      }
      for (size_t pi = 0; pi < typeParamNames.size(); ++pi)
      {
        const auto &name = typeParamNames[pi];
        Str bound;
        if (pi < funcDef->genericParams.size())
        {
          bound = typeParamBoundName(*funcDef->genericParams[pi]);
        }
        if (bound.empty())
        {
          if (substitution[name] && substitution[name]->tag() == typeinfo_tag::GENERIC_PARAM)
          {
            bound = static_cast<GenericParamType &>(*substitution[name]).bound;
          }
        }
        if (!bound.empty())
        {
          auto traitIt = locals.find(bound);
          if (traitIt != locals.end() && traitIt->second && traitIt->second->tag() == typeinfo_tag::TRAIT)
          {
            auto &trait = static_cast<TraitType &>(*traitIt->second);
            if (pi < instantiatedArgs.size() && !typeSatisfiesTrait(instantiatedArgs[pi], trait))
            {
              throw TypeCheckingException("Type '" + instantiatedArgs[pi]->repr() + "' does not implement trait '" +
                                              trait.name + "'",
                                          funCall->pos);
            }
          }
        }
      }
      Str instanceName = formatTypeInstanceName(genericDef.name, instantiatedArgs);
      funCall->genericInstanceName = instanceName;
      funCall->mangledCalleeName =
          mangle_symbol(MangledSymbolKind::Function, genericDef.moduleId, genericDef.name, instantiatedArgs);
      funCall->resolvedCalleeName = funCall->mangledCalleeName;
      if (!activeGenericInstanceName.empty())
      {
        funCall->mangledCalleeNameByInstance[activeGenericInstanceName] = funCall->mangledCalleeName;
      }
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
      bodyChecker.trait_impls_by_type = trait_impls_by_type;
      bodyChecker.activeGenericInstanceName = funCall->mangledCalleeName;
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

  namespace
  {
    auto trim_copy(Str value) -> Str
    {
      auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };
      value.erase(value.begin(), std::find_if(value.begin(), value.end(), isNotSpace));
      value.erase(std::find_if(value.rbegin(), value.rend(), isNotSpace).base(), value.end());
      return value;
    }

    auto split_top_level(const Str &value) -> Vec<Str>
    {
      Vec<Str> parts;
      int depth = 0;
      size_t start = 0;
      for (size_t i = 0; i < value.size(); ++i)
      {
        char ch = value[i];
        if (ch == '<' || ch == '(' || ch == '[')
        {
          ++depth;
        }
        else if (ch == '>' || ch == ')' || ch == ']')
        {
          --depth;
        }
        else if (ch == ',' && depth == 0)
        {
          parts.push_back(trim_copy(value.substr(start, i - start)));
          start = i + 1;
        }
      }
      auto tail = trim_copy(value.substr(start));
      if (!tail.empty())
      {
        parts.push_back(std::move(tail));
      }
      return parts;
    }

    auto find_top_level_arrow(const Str &value) -> size_t
    {
      int depth = 0;
      for (size_t i = 0; i + 1 < value.size(); ++i)
      {
        char ch = value[i];
        if (ch == '<' || ch == '(' || ch == '[')
        {
          ++depth;
        }
        else if (ch == '>' || ch == ')' || ch == ']')
        {
          --depth;
        }
        else if (ch == '-' && value[i + 1] == '>' && depth == 0)
        {
          return i;
        }
      }
      return Str::npos;
    }

    auto type_from_repr_impl(const Str &raw) -> CheckingRef<TypeInfo>
    {
      auto repr = trim_copy(raw);
      if (repr.empty() || repr == "untyped" || repr == "[untyped]")
      {
        return makecheck<Untyped>();
      }
      if (auto primitive = PrimitiveType::from(repr))
      {
        return primitive;
      }
      if (repr.starts_with("fun ("))
      {
        auto paramsStart = Str{"fun ("}.size();
        auto paramsEnd = repr.find(") -> ", paramsStart);
        if (paramsEnd != Str::npos)
        {
          Vec<CheckingRef<TypeInfo>> params;
          auto paramsRepr = repr.substr(paramsStart, paramsEnd - paramsStart);
          if (!trim_copy(paramsRepr).empty())
          {
            for (auto &&part : split_top_level(paramsRepr))
            {
              params.push_back(type_from_repr_impl(part));
            }
          }
          auto returnType = type_from_repr_impl(repr.substr(paramsEnd + Str{") -> "}.size()));
          return makecheck<FunctionType>(returnType, params);
        }
      }
      if (repr.size() >= 2 && repr.front() == '(' && repr.back() == ')')
      {
        Vec<CheckingRef<TypeInfo>> elements;
        auto inner = repr.substr(1, repr.size() - 2);
        if (!trim_copy(inner).empty())
        {
          for (auto &&part : split_top_level(inner))
          {
            elements.push_back(type_from_repr_impl(part));
          }
        }
        return makecheck<TupleType>(elements);
      }
      auto parseUnaryGeneric = [&](const Str &prefix, auto makeType) -> CheckingRef<TypeInfo> {
        if (repr.starts_with(prefix) && repr.ends_with(">"))
        {
          auto inner = repr.substr(prefix.size(), repr.size() - prefix.size() - 1);
          return makeType(type_from_repr_impl(inner));
        }
        return nullptr;
      };
      if (auto ref = parseUnaryGeneric("ref<", [](CheckingRef<TypeInfo> inner) {
            return makecheck<ReferenceType>(std::move(inner));
          }))
      {
        return ref;
      }
      if (auto vector = parseUnaryGeneric("vector<", [](CheckingRef<TypeInfo> inner) {
            return makecheck<VectorType>(std::move(inner));
          }))
      {
        return vector;
      }
      if (auto span = parseUnaryGeneric("span<", [](CheckingRef<TypeInfo> inner) {
            return makecheck<SpanType>(std::move(inner));
          }))
      {
        return span;
      }
      if (auto range = parseUnaryGeneric("Range<", [](CheckingRef<TypeInfo> inner) {
            return makecheck<RangeType>(std::move(inner));
          }))
      {
        return range;
      }
      if (repr.starts_with("array<") && repr.ends_with(">"))
      {
        auto inner = repr.substr(Str{"array<"}.size(), repr.size() - Str{"array<"}.size() - 1);
        auto parts = split_top_level(inner);
        if (parts.size() == 2)
        {
          return makecheck<ArrayType>(type_from_repr_impl(parts[0]), makecheck<ConstValueType>(parts[1]));
        }
      }
      if (auto arrow = find_top_level_arrow(repr); arrow != Str::npos)
      {
        return makecheck<Untyped>();
      }
      return makecheck<CustomizedType>(repr);
    }
  } // namespace

  CheckingRef<TypeInfo> type_from_repr(const Str &repr)
  {
    return type_from_repr_impl(repr);
  }

  TypeIndex type_check(ASTRef<ASTNode> ast, TypeIndex initial_index, Vec<Str> module_paths)
  {
    TypeChecker::activeTypeAliasSpecializations.clear();
    TypeChecker::activeConstPredicates.clear();
    TypeChecker::activeConstFunctions.clear();
    TypeChecker::activeAutoTraits.clear();
    TypeChecker::activeDerivedTraitImplKeys.clear();
    TypeChecker::moduleArtifactsById.clear();
    TypeChecker::activeModuleChecks.clear();
    if (!initial_index.empty())
    {
      TypeChecker::activeTypeAliasSpecializations = TypeChecker::preludeTypeAliasSpecializations;
      TypeChecker::activeConstPredicates = TypeChecker::preludeConstPredicates;
      TypeChecker::activeConstFunctions = TypeChecker::preludeConstFunctions;
      TypeChecker::activeAutoTraits = TypeChecker::preludeAutoTraits;
    }
    TypeChecker checker{initial_index, {}, nullptr, {}, false, "", std::move(module_paths)};
    checker.type_index = initial_index;
    ast->accept(&checker);

    return checker.type_index;
  }

  TypeIndex build_prelude_type_index()
  {
    static TypeIndex cachedResult;
    static ASTRef<ASTNode> retainedPreludeAst = nullptr;
    static std::once_flag initFlag;
    static bool initSucceeded = false;

    std::call_once(initFlag, [&]()
    {
      TypeIndex result;

      // Try to locate and load the prelude source file from known lib paths.
      namespace fs = std::filesystem;

      Vec<Str> libPaths = {"[force-source-module-loader]", "lib", "../lib", "../../lib"};
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

      if (preludePath.empty()) return;

      try
      {
        std::ifstream file{preludePath};
        std::string source{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};

        using namespace NG::parsing;
        auto ast = Parser(ParseState(Lexer(LexState{source}).lex())).parse(preludePath.string());

        if (ast)
        {
          TypeChecker::retainedPreludeImportAsts.clear();
          result = type_check(ast, {}, libPaths);
          TypeChecker::preludeTypeAliasSpecializations = TypeChecker::activeTypeAliasSpecializations;
          TypeChecker::preludeConstPredicates = TypeChecker::activeConstPredicates;
          TypeChecker::preludeConstFunctions = TypeChecker::activeConstFunctions;
          TypeChecker::preludeAutoTraits = TypeChecker::activeAutoTraits;
          retainedPreludeAst = ast;
          cachedResult = result;
          initSucceeded = true;
        }
      }
      catch (...)
      {
        // If prelude parsing/type-checking fails, do not cache.
      }
    });

    return initSucceeded ? cachedResult : TypeIndex{};
  }
} // namespace NG::typecheck
