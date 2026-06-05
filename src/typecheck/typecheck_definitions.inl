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

