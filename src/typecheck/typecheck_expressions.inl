
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
        auto unwrappedTarget = unwrap(targetType);
        if (!unwrappedTarget || unwrappedTarget->tag() != typeinfo_tag::CUSTOMIZED)
        {
          continue;
        }
        auto customPtr = std::static_pointer_cast<CustomizedType>(unwrappedTarget);
        if (auto localTarget = locals.find(customPtr->name); localTarget != locals.end())
        {
          auto unwrappedLocal = unwrap(localTarget->second);
          if (unwrappedLocal && unwrappedLocal->tag() == typeinfo_tag::CUSTOMIZED)
          {
            customPtr = std::static_pointer_cast<CustomizedType>(unwrappedLocal);
          }
        }
        auto &implTraits = trait_impls_by_type[customPtr->name];
        if (std::ranges::find(implTraits, impl.traitName) == implTraits.end())
        {
          implTraits.push_back(impl.traitName);
        }
        auto traitIt = locals.find(impl.traitName);
        if (traitIt != locals.end() && traitIt->second && traitIt->second->tag() == typeinfo_tag::TRAIT)
        {
          auto &trait = static_cast<TraitType &>(*traitIt->second);
          auto &methods = trait.allMethods.empty() ? trait.methods : trait.allMethods;
          for (const auto &[methodName, methodType] : methods)
          {
            customPtr->traitMemberFunctions[trait.name][methodName] = methodType;
            customPtr->memberFunctions[trait.name + "::" + methodName] = methodType;
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
