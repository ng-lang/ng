
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
