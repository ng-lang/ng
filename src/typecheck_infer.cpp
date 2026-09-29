// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck_infer.cpp — synthesis-mode expression inference: literals,
// names, operators, references/spans, indexing, and members. Checking-mode
// inference lives in typecheck_expected.cpp; calls, method calls, and closures
// in typecheck_call.cpp.
#include "typecheck_internal.hpp"

namespace NG::typecheck::detail
{

  namespace
  {
    /// True when a List<T> Cons payload's second field is the recursive tail:
    /// either `ref<List<T>>` (legacy representation) or `Box<List<T>>`
    /// (INC-20c representation, `Box.handle` is the recursive target).
    [[nodiscard]] auto isListTail(const TypeDescriptor &field, TypeId list) -> bool
    {
      if (field.kind == TypeKind::Reference && field.element == list)
        return true;
      return field.kind == TypeKind::Struct && field.name == "Box" && field.typeArguments.size() == 1 &&
             field.typeArguments[0] == list;
    }
  } // namespace

  auto Checker::listElementType(TypeId list) const -> std::optional<TypeId>
  {
    const auto &descriptor = interner_.descriptor(list);
    if (descriptor.kind != TypeKind::Enum)
      return std::nullopt;
    std::optional<TypeId> element;
    for (const auto payload : descriptor.elements)
    {
      const auto &cell = interner_.descriptor(payload);
      if (cell.kind == TypeKind::Tuple && cell.elements.size() == 2 &&
          isListTail(interner_.descriptor(cell.elements[1]), list))
        element = cell.elements[0];
    }
    return element;
  }

  void Checker::rejectScopedView(TypeId type, syntax::SourceSpan span, std::string_view container) const
  {
    const auto kind = interner_.descriptor(type).kind;
    if (kind == TypeKind::Reference)
      throw TypeError(std::format("references cannot be stored in {}", container), span);
    if (kind == TypeKind::Span)
      throw TypeError(std::format("spans cannot be stored in {} (INC-21 views are scoped)", container), span);
  }

  void Checker::recordBorrow(const hir::Expression &site, uint32_t root, bool mutableBorrow, std::string_view name)
  {
    if (!borrowExpressions_.insert(&site).second)
      return;
    const auto &counts = borrowState_.counts[root];
    if (mutableBorrow)
    {
      if (counts.shared != 0)
        throw TypeError("E-borrow-conflict",
                        std::format("cannot mutably borrow `{}` while it is shared-borrowed", name), site.span);
      if (counts.mutableRefs != 0)
        throw TypeError("E-borrow-conflict",
                        std::format("cannot mutably borrow `{}` while it is already mutably borrowed", name),
                        site.span);
    }
    else
    {
      if (counts.mutableRefs != 0)
        throw TypeError("E-borrow-conflict",
                        std::format("cannot shared-borrow `{}` while it is mutably borrowed", name), site.span);
      if (counts.shared >= BorrowState::MaxShared)
        throw TypeError("E-borrow-too-many", "too many shared borrows of one binding", site.span);
    }
    borrowState_.add(&site, root, 0, mutableBorrow);
  }

  auto Checker::infer(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    TypeId type;
    switch (expression.kind)
    {
    case hir::ExpressionKind::IntegerLiteral:
      type = expression.numericSuffix.empty() ? builtin::I64 : typeFromNumericSuffix(expression.numericSuffix);
      if (isIntegerBuiltin(type))
        checkIntegerLiteralMagnitude(parseIntegerLiteralMagnitude(expression.text, type, expression.span),
                                     expression.text, type, expression.span);
      break;
    case hir::ExpressionKind::FloatLiteral:
      type = expression.numericSuffix.empty() ? builtin::F64 : typeFromNumericSuffix(expression.numericSuffix);
      static_cast<void>(std::stod(expression.text));
      break;
    case hir::ExpressionKind::StringLiteral:
      type = builtin::String;
      break;
    case hir::ExpressionKind::BooleanLiteral:
      type = builtin::Bool;
      break;
    case hir::ExpressionKind::ArrayLiteral:
      type = inferArrayLiteral(expression, locals);
      break;
    case hir::ExpressionKind::StructLiteral:
      return inferStructLiteral(expression, locals);
    case hir::ExpressionKind::EnumLiteral:
      type = inferEnumLiteral(expression, locals);
      break;
    case hir::ExpressionKind::TupleLiteral:
      type = inferTupleLiteral(expression, locals);
      break;
    case hir::ExpressionKind::ResolvedName:
      type = inferName(expression, locals);
      break;
    case hir::ExpressionKind::Grouped:
      type = infer(*expression.operands[0], locals);
      break;
    case hir::ExpressionKind::GenericApplication:
      type = inferGenericApplication(expression);
      break;
    case hir::ExpressionKind::Prefix:
      type = inferPrefix(expression, locals);
      break;
    case hir::ExpressionKind::Binary:
      type = inferBinary(expression, locals);
      break;
    case hir::ExpressionKind::Call:
      if (expression.methodCall)
        return inferMethodCall(expression, locals);
      type = inferCall(expression, locals);
      break;
    case hir::ExpressionKind::Index:
      type = inferIndex(expression, locals);
      break;
    case hir::ExpressionKind::Closure:
      type = inferClosureLiteral(expression, locals);
      break;
    case hir::ExpressionKind::Member:
      type = inferMember(expression, locals);
      break;
    }
    record(expression, type);
    return type;
  }

  auto Checker::inferName(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    if (expression.resolvedName->kind == hir::ResolvedNameKind::Function)
      throw TypeError("function name cannot be used as a value", expression.span);
    if (expression.resolvedName->kind == hir::ResolvedNameKind::ConstParameter)
      throw TypeError(std::format("const parameter `{}` is not a runtime value", expression.text), expression.span);
    if (moveState_.isWholeMoved(expression.resolvedName->id))
      throw TypeError("E-move-used", std::format("use of moved value `{}`", expression.text), expression.span);
    return locals.at(expression.resolvedName->id);
  }

  // ---- Aggregate literals ------------------------------------------------------

  auto Checker::inferArrayLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    if (expression.operands.empty())
      throw TypeError("cannot infer the type of an empty array literal", expression.span);
    size_t spreads = 0;
    TypeId element{};
    for (const auto &candidate : expression.operands)
    {
      TypeId candidateType;
      std::string_view context = "array element";
      if (isSpread(*candidate))
      {
        ++spreads;
        const ArraySpread spread = inferArraySpread(*candidate, locals);
        candidateType = spread.element;
        context = spread.map ? "map spread result" : "array spread";
      }
      else
      {
        candidateType = infer(*candidate, locals);
        rejectScopedView(candidateType, candidate->span, "arrays");
      }
      if (element.value == 0)
        element = candidateType;
      else
        requireType(element, candidateType, candidate->span, context);
    }
    if (spreads > 1)
      throw TypeError("array literals support at most one spread", expression.span);
    return interner_.internDynamicArray(element);
  }

  /// One array-literal spread: `xs...` over an array, range, or recursive list,
  /// or a map spread `f(xs)...` (optionally filtered, `?p(xs)...`).
  auto Checker::inferArraySpread(const hir::Expression &spread, const LocalTypes &locals) -> ArraySpread
  {
    const hir::Expression *inner = spread.operands[0].get();
    const bool filter = inner->kind == hir::ExpressionKind::Prefix && inner->text == "?";
    if (filter)
      inner = inner->operands[0].get();
    const bool mapSpread = inner->kind == hir::ExpressionKind::Call && !inner->operands.empty() &&
                           inner->operands[0]->resolvedName.has_value() &&
                           inner->operands[0]->resolvedName->kind == hir::ResolvedNameKind::Function;
    if (mapSpread)
      return ArraySpread{.element = inferMapSpread(spread, *inner, filter, locals), .map = true};
    if (filter)
      throw TypeError("the filter marker `?` requires a map spread", spread.span);

    const TypeId source = infer(*inner, locals);
    const auto &descriptor = interner_.descriptor(source);
    if (descriptor.kind == TypeKind::Range && descriptor.element != builtin::I64)
      throw TypeError("spreading ranges currently requires i64 elements", spread.span);
    if (isArrayKind(descriptor.kind) || descriptor.kind == TypeKind::Range)
      return ArraySpread{.element = descriptor.element};
    const auto listElement = listElementType(source);
    if (!listElement.has_value())
      throw TypeError(std::format("cannot spread value of type {}", interner_.display(source)), spread.span);
    // INC-20c S2: list spreads lower as calls to the std list walkers.
    listSpreadCalls_.emplace(
        inner, std::pair<hir::DefId, hir::DefId>{resolveListSpreadCall("spreadLength", source, spread.span),
                                                 resolveListSpreadCall("spreadGet", source, spread.span)});
    return ArraySpread{.element = *listElement};
  }

  /// `f(xs)...` maps each source element through a one-argument function; the
  /// filter form `?p(xs)...` keeps the source elements whose predicate holds.
  auto Checker::inferMapSpread(const hir::Expression &spread, const hir::Expression &call, bool filter,
                               const LocalTypes &locals) -> TypeId
  {
    if (call.operands.size() != 2)
      throw TypeError("map spread function must take exactly one argument", spread.span);
    const auto &sourceExpression = *call.operands[1];
    const TypeId source = infer(sourceExpression, locals);
    const auto &descriptor = interner_.descriptor(source);
    if (!isArrayKind(descriptor.kind) && descriptor.kind != TypeKind::Range)
      throw TypeError(std::format("map spread source must be an array or range, got {}", interner_.display(source)),
                      sourceExpression.span);
    if (descriptor.kind == TypeKind::Range && descriptor.element != builtin::I64)
      throw TypeError("map spread over ranges currently requires i64 elements", sourceExpression.span);
    const hir::DefId target{call.operands[0]->resolvedName->id};
    const auto &signature = signatures_.at(target.value);
    if (signature.parameters.size() != 1)
      throw TypeError("map spread function must take exactly one argument", spread.span);
    Substitution substitution;
    unify(signature.parameters.front(), descriptor.element, substitution, sourceExpression.span);
    requireConstArguments(signature, substitution, call.operands[0]->text, call.span);
    requireWhereClause(target, substitution, signature, call.span);
    const TypeId mapped = specializeReturnType(signature, substitution, 0);
    if (filter)
      requireType(builtin::Bool, mapped, spread.span, "filter predicate result");
    callTargets_.insert_or_assign(&call, target);
    record(call, mapped);
    return filter ? descriptor.element : mapped;
  }

  auto Checker::inferStructLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    if (!expression.structId.has_value())
      throw TypeError("struct literal has no resolved type", expression.span);
    if (interner_.structGenericArity(*expression.structId) != 0)
      throw TypeError(std::format("cannot infer generic arguments for struct literal `{}`; annotate the binding type",
                                  expression.text),
                      expression.span);
    return inferExpectedStruct(expression, interner_.typeForStruct(*expression.structId), locals);
  }

  /// Non-generic enum constructors check like an expected-type constructor of
  /// their own enum. Payloads are not consumed here: the enclosing binding or
  /// call consumes the constructor (see the double-inference note in
  /// inferCall).
  auto Checker::inferEnumLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    if (!expression.enumId.has_value() || !expression.variant.has_value())
      throw TypeError("enum constructor has no resolved variant", expression.span);
    if (interner_.enumGenericArity(*expression.enumId) != 0)
      throw TypeError(std::format("cannot infer generic arguments for enum constructor `{}`", expression.text),
                      expression.span);
    return inferExpectedEnum(expression, interner_.typeForEnum(*expression.enumId), locals, false);
  }

  auto Checker::inferTupleLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    std::vector<TypeId> elements;
    for (const auto &element : expression.operands)
    {
      if (!isSpread(*element))
      {
        const TypeId elementType = infer(*element, locals);
        rejectScopedView(elementType, element->span, "tuples");
        elements.push_back(elementType);
        continue;
      }
      const TypeId operand = infer(*element->operands[0], locals);
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind == TypeKind::Tuple)
        elements.insert(elements.end(), descriptor.elements.begin(), descriptor.elements.end());
      else if (descriptor.kind == TypeKind::TypePack)
        elements.push_back(descriptor.element);
      else
        throw TypeError(std::format("cannot spread value of type {}", interner_.display(operand)), element->span);
    }
    return interner_.internTuple(elements);
  }

  /// Const predicate applications (`Name<T...>`): builtin trait/tuple
  /// introspection or a user const declaration selected by its arguments.
  auto Checker::inferGenericApplication(const hir::Expression &expression) -> TypeId
  {
    if (evaluateTraitIntrospection(expression.text, expression.genericArguments, expression.span).has_value())
      return builtin::Bool;
    std::vector<TypeId> typeArguments;
    for (const auto &argument : expression.genericArguments)
    {
      if (argument.kind != syntax::GenericArgumentKind::Type)
        throw TypeError("const arguments on const predicates are not yet supported", argument.span);
      const TypeId resolved = interner_.resolveInScope(*argument.type, genericBindings_);
      if (interner_.descriptor(resolved).kind == TypeKind::TypeParameter)
        throw TypeError(std::format("cannot evaluate const declaration `{}` for abstract type parameter `{}`",
                                    expression.text, interner_.display(resolved)),
                        argument.span);
      typeArguments.push_back(resolved);
    }
    if (const auto introspected = evaluateTupleIntrospection(expression.text, typeArguments, expression.span);
        introspected.has_value())
      return interner_.constInterner().value(*introspected).kind == const_eval::ConstValueKind::Bool ? builtin::Bool
                                                                                                     : builtin::I64;
    static_cast<void>(selectConstDeclaration(expression.text, typeArguments, expression.span));
    return builtin::Bool;
  }

  // ---- Operators ---------------------------------------------------------------

  auto Checker::inferPrefix(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const auto &op = expression.text;
    const auto &operandExpression = *expression.operands[0];
    TypeId operand{};
    if ((op == "-" || op == "+") && operandExpression.kind == hir::ExpressionKind::IntegerLiteral &&
        operandExpression.numericSuffix.empty())
    {
      // `-9223372036854775808` is in range although its magnitude alone is not.
      operand = builtin::I64;
      record(operandExpression, operand);
      const uint64_t magnitude = parseIntegerLiteralMagnitude(operandExpression.text, builtin::I64, expression.span);
      if (op == "+")
        checkIntegerLiteralMagnitude(magnitude, operandExpression.text, builtin::I64, expression.span);
      else if (magnitude > (1ULL << 63))
        throw TypeError(std::format("integer literal `-{}` is out of range", operandExpression.text), expression.span);
    }
    else
    {
      operand = infer(operandExpression, locals);
    }

    if (op == "ref" || op == "ref mut")
      return inferReference(expression, operand, locals);
    if (op == "span")
      return inferSpan(expression, locals);
    if (op == "*")
    {
      const auto &descriptor = interner_.descriptor(operand);
      if (descriptor.kind != TypeKind::Reference)
        throw TypeError(std::format("cannot dereference value of type {}", interner_.display(operand)),
                        expression.span);
      return descriptor.element;
    }
    if (op == "move" || op == "clone")
      return operand;
    if (op == "...")
      throw TypeError("spread is only valid inside tuple literals", expression.span);
    if (op == "!")
    {
      requireType(builtin::Bool, operand, expression.span, "prefix operand");
      return builtin::Bool;
    }
    if (isIntegerBuiltin(operand) || isFloatBuiltin(operand))
      return operand;
    requireType(builtin::I64, operand, expression.span, "prefix operand");
    return builtin::I64;
  }

  auto Checker::inferReference(const hir::Expression &expression, TypeId operand, const LocalTypes &locals) -> TypeId
  {
    const auto &place = *expression.operands[0];
    const bool mutableReference = expression.text == "ref mut";
    if (!isPlace(place))
      throw TypeError("reference operand is not a place", expression.span);
    const auto root = rootLocalOf(place);
    // A1.3 (deferred): creating a reference rooted through an existing scoped
    // reference would need ref-of-ref shadows; reads/writes through `arr[i]` /
    // `s.field` on reference receivers are supported (A1.2).
    if (root.has_value() && interner_.descriptor(locals.at(root->value)).kind == TypeKind::Reference)
      throw TypeError("creating a reference through a scoped reference is deferred (A1.3); dereference the "
                      "reference first",
                      expression.span);
    if (interner_.descriptor(operand).kind == TypeKind::Reference)
      throw TypeError("cannot create a reference to a reference", expression.span);
    if (mutableReference)
      requireMutableRoot(place, expression.span);
    if (root.has_value())
      recordBorrow(expression, root->value, mutableReference, place.text);
    return interner_.internReference(operand, mutableReference);
  }

  /// INC-21 S1: `span <array-place>` views the whole array; the window form
  /// `span xs[lo..hi]` views the `[lo, hi)` slice. Both are borrowed scoped
  /// views: a shared loan on the source root blocks mutation and reassignment
  /// while the span is live.
  auto Checker::inferSpan(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const hir::Expression *arrayPlace = expression.operands[0].get();
    const hir::Expression *window = nullptr;
    if (arrayPlace->kind == hir::ExpressionKind::Index)
    {
      window = arrayPlace->operands[1].get();
      if (window->kind != hir::ExpressionKind::Binary || window->text != "..")
        throw TypeError("span window requires a range subscript `xs[lo..hi]`", expression.span);
      arrayPlace = arrayPlace->operands[0].get();
    }
    if (!isPlace(*arrayPlace))
      throw TypeError("span operand is not a place", expression.span);
    if (arrayPlace->kind != hir::ExpressionKind::ResolvedName)
      throw TypeError("span over a structure field is deferred (INC-21 S1); span a local binding directly",
                      expression.span);
    const TypeId operandType = infer(*arrayPlace, locals);
    const auto &source = interner_.descriptor(operandType);
    if (!isArrayKind(source.kind))
      throw TypeError(std::format("cannot span value of type {}", interner_.display(operandType)), expression.span);
    const auto root = rootLocalOf(*arrayPlace);
    if (!root.has_value())
      throw TypeError("span operand must be rooted at a local binding", expression.span);
    if (interner_.descriptor(locals.at(root->value)).kind == TypeKind::Reference)
      throw TypeError("creating a span through a scoped reference is deferred (INC-21 S1); dereference the "
                      "reference first",
                      expression.span);
    recordBorrow(expression, root->value, false, arrayPlace->text);
    if (window != nullptr)
    {
      const auto &start = *window->operands[0];
      const auto &end = *window->operands[1];
      requireType(builtin::I64, infer(start, locals), start.span, "span window start");
      requireType(builtin::I64, infer(end, locals), end.span, "span window end");
    }
    return interner_.internSpan(source.element);
  }

  auto Checker::inferBinary(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const auto &op = expression.text;
    const auto &leftExpression = *expression.operands[0];
    const auto &rightExpression = *expression.operands[1];
    TypeId left = infer(leftExpression, locals);
    TypeId right = infer(rightExpression, locals);
    const bool equality = op == "==" || op == "!=";
    const bool ordering = op == "<" || op == "<=" || op == ">" || op == ">=";
    const bool comparison = equality || ordering;

    // Unsuffixed literals adopt the other operand's numeric type (integers may
    // also adopt a float type in comparisons, DEC-A).
    const auto adoptsType = [&](const hir::Expression &literal, TypeId other, TypeId self)
    {
      if (!literal.numericSuffix.empty() || other == self)
        return false;
      if (literal.kind == hir::ExpressionKind::IntegerLiteral)
        return isIntegerBuiltin(other) || (comparison && isFloatBuiltin(other));
      return literal.kind == hir::ExpressionKind::FloatLiteral && isFloatBuiltin(other);
    };
    const auto literalContext = [](const hir::Expression &literal)
    { return literal.kind == hir::ExpressionKind::IntegerLiteral ? "integer literal" : "float literal"; };
    if (adoptsType(rightExpression, left, right))
      right = inferExpected(rightExpression, left, locals, literalContext(rightExpression));
    if (adoptsType(leftExpression, right, left))
      left = inferExpected(leftExpression, right, locals, literalContext(leftExpression));

    if (op == "..")
    {
      if (!isIntegerBuiltin(left))
        requireType(builtin::I64, left, leftExpression.span, "range start");
      if (!isIntegerBuiltin(right))
        requireType(builtin::I64, right, rightExpression.span, "range end");
      requireType(left, right, expression.span, "range bounds");
      return interner_.internRange(left);
    }
    if (op == "<<")
    {
      const auto &array = interner_.descriptor(left);
      if (array.kind == TypeKind::DynamicArray)
      {
        if (rightExpression.kind == hir::ExpressionKind::IntegerLiteral && isIntegerBuiltin(array.element) &&
            right != array.element)
          right = inferExpected(rightExpression, array.element, locals, "appended element");
        requireType(array.element, right, rightExpression.span, "appended element");
        return left;
      }
    }
    if (comparison)
    {
      // A union operand compares as the member type of the other operand.
      const auto narrow = [&](const hir::Expression &unionExpression, TypeId &unionType, TypeId member)
      {
        const auto &descriptor = interner_.descriptor(unionType);
        if (descriptor.kind == TypeKind::Union &&
            std::ranges::find(descriptor.elements, member) != descriptor.elements.end())
        {
          record(unionExpression, member);
          unionType = member;
        }
      };
      narrow(leftExpression, left, right);
      narrow(rightExpression, right, left);
    }
    const bool numericPair = isNumericBuiltin(left) && isNumericBuiltin(right);
    if (comparison && numericPair && left != right)
    {
      const bool bothIntegers = isIntegerBuiltin(left) && isIntegerBuiltin(right);
      if (bothIntegers && isSignedIntegerBuiltin(left) != isSignedIntegerBuiltin(right))
        throw TypeError("E-compare-mixed-sign",
                        std::format("implicit mixed-sign comparison between {} and {}; convert explicitly "
                                    "with toFloat or toSigned",
                                    interner_.display(left), interner_.display(right)),
                        expression.span);
      if (isFloatBuiltin(left) != isFloatBuiltin(right))
        throw TypeError("E-compare-mixed-kind",
                        std::format("implicit mixed numeric-kind comparison between {} and {}; convert "
                                    "explicitly with toFloat or toSigned",
                                    interner_.display(left), interner_.display(right)),
                        expression.span);
    }
    if (!(comparison && numericPair))
      requireType(left, right, expression.span, "binary operands");
    if (equality)
      return builtin::Bool;
    if (ordering)
    {
      if (!numericPair)
        requireType(builtin::I64, left, expression.span, "comparison operand");
      return builtin::Bool;
    }
    if (op == "&&" || op == "||")
    {
      requireType(builtin::Bool, left, expression.span, "logical operand");
      return builtin::Bool;
    }
    if ((op != "+" || left != builtin::String) && !isIntegerBuiltin(left) && !isFloatBuiltin(left))
      requireType(builtin::I64, left, expression.span, "binary operand");
    return left;
  }

  // ---- Places ------------------------------------------------------------------

  auto Checker::inferIndex(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const TypeId receiver = infer(*expression.operands[0], locals);
    const TypeId indexType = infer(*expression.operands[1], locals);
    // D-002 A1.2 place-through-ref: `arr[i]` on `array<T> ref` indexes the
    // referenced value; the read/write lowers through the ref object.
    const bool throughRef = interner_.descriptor(receiver).kind == TypeKind::Reference;
    const auto &descriptor = interner_.descriptor(throughRef ? interner_.descriptor(receiver).element : receiver);
    if (interner_.descriptor(indexType).kind == TypeKind::Range)
    {
      if (throughRef)
        throw TypeError("cannot slice through a scoped reference (A1.3 is deferred); dereference the reference first",
                        expression.span);
      if (descriptor.kind == TypeKind::Span)
        throw TypeError("slicing a span view is deferred (INC-21 S1); reslice with `span`", expression.span);
      if (!isArrayKind(descriptor.kind))
        throw TypeError(std::format("cannot slice value of type {}", interner_.display(receiver)), expression.span);
      return interner_.internDynamicArray(descriptor.element);
    }
    requireType(builtin::I64, infer(*expression.operands[1], locals), expression.operands[1]->span, "array index");
    if (descriptor.kind == TypeKind::Tuple)
    {
      const auto &text = expression.text;
      if (text.empty())
        throw TypeError("tuple index must be an integer literal", expression.operands[1]->span);
      uint64_t index{};
      const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), index);
      if (error != std::errc{} || end != text.data() + text.size() || index >= descriptor.elements.size())
        throw TypeError(std::format("tuple index {} is out of range for length {}", text, descriptor.elements.size()),
                        expression.operands[1]->span);
      return descriptor.elements[index];
    }
    if (descriptor.kind != TypeKind::Span && !isArrayKind(descriptor.kind))
      throw TypeError(std::format("cannot index value of type {}", interner_.display(receiver)), expression.span);
    return descriptor.element;
  }

  auto Checker::inferMember(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    const TypeId receiver = infer(*expression.operands[0], locals);
    // D-002 A1.2 member-through-ref: `s.field` on `S ref` resolves the member
    // on the referenced struct, like `(*s).field`.
    const auto &receiverDescriptor = interner_.descriptor(receiver);
    const auto &descriptor = receiverDescriptor.kind == TypeKind::Reference
                                 ? interner_.descriptor(receiverDescriptor.element)
                                 : receiverDescriptor;
    if (descriptor.kind != TypeKind::Struct)
      throw TypeError(
          std::format("cannot access member `{}` on value of type {}", expression.text, interner_.display(receiver)),
          expression.span);
    const auto found = std::ranges::find(descriptor.fieldNames, expression.text);
    if (found == descriptor.fieldNames.end())
      throw TypeError(std::format("unknown field `{}` in struct `{}`", expression.text, descriptor.name),
                      expression.span);
    const auto field = static_cast<uint32_t>(std::distance(descriptor.fieldNames.begin(), found));
    if (const auto root = rootLocalOf(expression); root.has_value() && moveState_.isFieldMoved(root->value, field))
      throw TypeError("E-move-field",
                      std::format("use of moved field `{}.{}`", expression.operands[0]->text, expression.text),
                      expression.span);
    return descriptor.elements[field];
  }

  auto Checker::inferClosureLiteral(const hir::Expression &expression, const LocalTypes &locals) -> TypeId
  {
    std::vector<TypeId> parameterTypes;
    parameterTypes.reserve(expression.closureParameterIds.size());
    for (size_t index = 0; index < expression.closureParameterIds.size(); ++index)
    {
      if (expression.closureParameterTypes[index] == nullptr)
        throw TypeError(std::format("cannot infer closure parameter `{}`; annotate it or the expected closure type",
                                    expression.closureParameterNames[index]),
                        expression.span);
      parameterTypes.push_back(interner_.resolveInScope(*expression.closureParameterTypes[index], genericBindings_));
    }
    return inferClosure(expression, parameterTypes, locals);
  }

} // namespace NG::typecheck::detail
