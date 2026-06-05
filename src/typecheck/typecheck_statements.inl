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
