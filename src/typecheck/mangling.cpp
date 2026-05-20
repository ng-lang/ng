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
    if (auto alias = dynamic_cast<const TypeAliasType *>(&type))
    {
      return canonical_type_name(alias->underlyingType);
    }
    if (auto primitive = dynamic_cast<const PrimitiveType *>(&type))
    {
      return primitive->repr();
    }
    if (auto ref = dynamic_cast<const ReferenceType *>(&type))
    {
      return "ref<" + canonical_type_name(ref->referencedType) + ">";
    }
    if (auto array = dynamic_cast<const ArrayType *>(&type))
    {
      return "[" + canonical_type_name(array->elementType) + "]";
    }
    if (auto tuple = dynamic_cast<const TupleType *>(&type))
    {
      return "(" + join_canonical_types(tuple->elementTypes, ",") + ")";
    }
    if (auto function = dynamic_cast<const FunctionType *>(&type))
    {
      return "fun(" + join_canonical_types(function->parametersType, ",") + ")->" +
             canonical_type_name(function->returnType);
    }
    if (auto paramWithDefault = dynamic_cast<const ParamWithDefaultValueType *>(&type))
    {
      return canonical_type_name(paramWithDefault->paramType) + "=default";
    }
    if (auto custom = dynamic_cast<const CustomizedType *>(&type))
    {
      return qualified_nominal_name(custom->moduleId, custom->name);
    }
    if (auto trait = dynamic_cast<const TraitType *>(&type))
    {
      return qualified_nominal_name(trait->moduleId, trait->name);
    }
    if (auto newType = dynamic_cast<const NewTypeType *>(&type))
    {
      return qualified_nominal_name(newType->moduleId, newType->name);
    }
    if (auto tagged = dynamic_cast<const TaggedUnionType *>(&type))
    {
      return qualified_nominal_name(tagged->moduleId, tagged->name);
    }
    if (auto variant = dynamic_cast<const VariantType *>(&type))
    {
      return qualified_nominal_name(variant->moduleId, variant->unionName) + "." + variant->variantName;
    }
    if (auto unionType = dynamic_cast<const UnionType *>(&type))
    {
      return join_canonical_types(unionType->types, "|");
    }
    if (auto generic = dynamic_cast<const GenericParamType *>(&type))
    {
      return generic->repr();
    }
    if (auto varargs = dynamic_cast<const VarargsType *>(&type))
    {
      return "varargs<" + join_canonical_types(varargs->elementTypes, ",") + ">";
    }
    if (auto genericDef = dynamic_cast<const GenericDefType *>(&type))
    {
      return qualified_nominal_name(genericDef->moduleId, genericDef->repr());
    }
    if (auto genericTypeDef = dynamic_cast<const GenericTypeDef *>(&type))
    {
      return qualified_nominal_name(genericTypeDef->moduleId, genericTypeDef->repr());
    }
    return type.repr();
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
