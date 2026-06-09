#include <typecheck/mangling.hpp>

#include <charconv>

namespace NG::typecheck
{
  namespace
  {
    auto normalize_module_name(const Str &moduleName) -> Str
    {
      return moduleName.empty() || moduleName == "[noname]" ? Str{"default"} : moduleName;
    }

    auto qualified_nominal_name(const Str &moduleName, const Str &name) -> Str
    {
      auto module = normalize_module_name(moduleName);
      return module + "::" + name;
    }

    auto kind_code(MangledSymbolKind kind) -> Str
    {
      switch (kind)
      {
      case MangledSymbolKind::Function:
        return "F";
      case MangledSymbolKind::Type:
        return "T";
      case MangledSymbolKind::Const:
        return "C";
      case MangledSymbolKind::Impl:
        return "I";
      }
      return "U";
    }

    auto append_segment(Str &out, const Str &segment) -> void
    {
      out += std::to_string(segment.size());
      out += ':';
      out += segment;
    }

    auto join_canonical_types(const Vec<CheckingRef<TypeInfo>> &types, const Str &separator) -> Str
    {
      Str out;
      for (size_t i = 0; i < types.size(); ++i)
      {
        if (i > 0)
        {
          out += separator;
        }
        out += canonical_type_name(types[i]);
      }
      return out;
    }
  } // namespace

  auto canonical_type_name(const CheckingRef<TypeInfo> &type) -> Str
  {
    if (!type)
    {
      return "_";
    }
    return canonical_type_name(*type);
  }

  auto canonical_type_name(const TypeInfo &type) -> Str
  {
    // Use tag() dispatch instead of dynamic_cast for better performance.
    // The default case handles types that just use repr().
    switch (type.tag())
    {
    case typeinfo_tag::TYPE_ALIAS:
      return canonical_type_name(static_cast<const TypeAliasType &>(type).underlyingType);
    case typeinfo_tag::REFERENCE:
      return "ref<" + canonical_type_name(static_cast<const ReferenceType &>(type).referencedType) + ">";
    case typeinfo_tag::ARRAY:
    {
      const auto &a = static_cast<const ArrayType &>(type);
      return "array<" + canonical_type_name(a.elementType) + "," +
             (a.length ? canonical_type_name(a.length) : Str{"?"}) + ">";
    }
    case typeinfo_tag::VECTOR:
      return "vector<" + canonical_type_name(static_cast<const VectorType &>(type).elementType) + ">";
    case typeinfo_tag::SPAN:
      return "span<" + canonical_type_name(static_cast<const SpanType &>(type).elementType) + ">";
    case typeinfo_tag::RANGE:
      return "Range<" + canonical_type_name(static_cast<const RangeType &>(type).elementType) + ">";
    case typeinfo_tag::TUPLE:
      return "(" + join_canonical_types(static_cast<const TupleType &>(type).elementTypes, ",") + ")";
    case typeinfo_tag::FUNCTION:
    {
      const auto &f = static_cast<const FunctionType &>(type);
      return "fun(" + join_canonical_types(f.parametersType, ",") + ")->" + canonical_type_name(f.returnType);
    }
    case typeinfo_tag::PARAM_WITH_DEFAULT_VALUE:
      return canonical_type_name(static_cast<const ParamWithDefaultValueType &>(type).paramType) + "=default";
    case typeinfo_tag::CUSTOMIZED:
    {
      const auto &c = static_cast<const CustomizedType &>(type);
      return qualified_nominal_name(c.moduleId, c.name);
    }
    case typeinfo_tag::TRAIT:
    {
      const auto &t = static_cast<const TraitType &>(type);
      return qualified_nominal_name(t.moduleId, t.name);
    }
    case typeinfo_tag::NEW_TYPE:
    {
      const auto &n = static_cast<const NewTypeType &>(type);
      return qualified_nominal_name(n.moduleId, n.name);
    }
    case typeinfo_tag::TAGGED_UNION:
    {
      const auto &t = static_cast<const TaggedUnionType &>(type);
      return qualified_nominal_name(t.moduleId, t.name);
    }
    case typeinfo_tag::VARIANT:
    {
      const auto &v = static_cast<const VariantType &>(type);
      return qualified_nominal_name(v.moduleId, v.unionName) + "." + v.variantName;
    }
    case typeinfo_tag::UNION:
      return join_canonical_types(static_cast<const UnionType &>(type).types, "|");
    case typeinfo_tag::CONST_VALUE:
    {
      const auto &c = static_cast<const ConstValueType &>(type);
      return "const<" + c.valueType + ":" + c.value + ">";
    }
    case typeinfo_tag::VARARGS:
      return "varargs<" + join_canonical_types(static_cast<const VarargsType &>(type).elementTypes, ",") + ">";
    case typeinfo_tag::GENERIC_DEF:
      return qualified_nominal_name(static_cast<const GenericDefType &>(type).moduleId, type.repr());
    case typeinfo_tag::GENERIC_TYPE_DEF:
      return qualified_nominal_name(static_cast<const GenericTypeDef &>(type).moduleId, type.repr());
    case typeinfo_tag::TYPE_CONSTRUCTOR_APPLICATION:
    {
      const auto &app = static_cast<const TypeConstructorApplicationType &>(type);
      return canonical_type_name(app.constructorType) + "<" + join_canonical_types(app.typeArgs, ",") + ">";
    }
    default:
      return type.repr();
    }
  }

  auto mangle_symbol(MangledSymbolKind kind,
                     const Str &moduleName,
                     const Str &baseName,
                     const Vec<Str> &canonicalTypeArgs) -> Str
  {
    Str out = "$NG";
    append_segment(out, "v1");
    append_segment(out, kind_code(kind));
    append_segment(out, normalize_module_name(moduleName));
    append_segment(out, baseName);
    append_segment(out, std::to_string(canonicalTypeArgs.size()));
    for (const auto &arg : canonicalTypeArgs)
    {
      append_segment(out, arg);
    }
    return out;
  }

  auto mangle_symbol(MangledSymbolKind kind,
                     const Str &moduleName,
                     const Str &baseName,
                     const Vec<CheckingRef<TypeInfo>> &typeArgs) -> Str
  {
    Vec<Str> canonicalArgs;
    canonicalArgs.reserve(typeArgs.size());
    for (const auto &arg : typeArgs)
    {
      canonicalArgs.push_back(canonical_type_name(arg));
    }
    return mangle_symbol(kind, moduleName, baseName, canonicalArgs);
  }
} // namespace NG::typecheck
