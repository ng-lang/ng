// AI-generated code; reviewed for this repository's vNext rewrite.
// HIR type helpers: syntax type lowering (incl. `T ref(a | b)` origin
// ordinals), type rendering as written, explicit generic arguments, and
// type cloning.
#include "hir_internal.hpp"
#include "syntax/const_expr.hpp"

#include <format>

namespace NG::hir::detail
{
  namespace
  {
    using ParamOrdinals = std::unordered_map<std::string, uint32_t>;

    /// D-002 A1.1: resolves `T ref(a | b)` origin names to the enclosing
    /// function's parameter ordinals, rejecting sources that are not params.
    [[nodiscard]] auto resolveOriginOrdinals(const std::vector<std::string> &names, syntax::SourceSpan span,
                                             const ParamOrdinals *paramOrdinals) -> std::vector<uint32_t>
    {
      if (paramOrdinals == nullptr)
        throw ResolutionError("reference origin list out of function parameter context", span);
      std::vector<uint32_t> ordinals;
      for (const auto &name : names)
      {
        const auto found = paramOrdinals->find(name);
        if (found == paramOrdinals->end())
          throw ResolutionError(std::format("reference origin `{}` is not a parameter of the enclosing function", name),
                                span);
        ordinals.push_back(found->second);
      }
      return ordinals;
    }

    [[nodiscard]] auto joined(const auto &items, std::string_view separator, const auto &render) -> std::string
    {
      std::string result;
      for (size_t index = 0; index < items.size(); ++index)
      {
        if (index != 0)
          result += separator;
        result += render(items[index]);
      }
      return result;
    }
  } // namespace

  auto lowerType(const syntax::TypeSyntax &type, const ParamOrdinals *paramOrdinals) -> Type
  {
    const auto lowered = [&](const syntax::TypeSyntax &inner)
    { return std::make_unique<Type>(lowerType(inner, paramOrdinals)); };
    if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(&type))
      return Type{.kind = TypeKind::Named, .span = type.span, .name = named->name};
    if (const auto *applied = dynamic_cast<const syntax::AppliedTypeSyntax *>(&type))
    {
      Type result{.kind = TypeKind::Applied, .span = type.span, .target = lowered(*applied->constructor)};
      for (const auto &argument : applied->arguments)
      {
        TypeArgument loweredArgument{.kind = argument.kind, .span = argument.span};
        if (argument.kind == syntax::GenericArgumentKind::Type)
          loweredArgument.type = lowered(*argument.type);
        else
          loweredArgument.constExpr = syntax::cloneConstExpr(*argument.constExpr);
        result.arguments.push_back(std::move(loweredArgument));
      }
      return result;
    }
    if (const auto *reference = dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(&type))
    {
      Type result{.kind = TypeKind::ScopedReference,
                  .span = type.span,
                  .target = lowered(*reference->target),
                  .isMutable = reference->isMutable};
      if (!reference->origins.empty())
        result.originOrdinals = resolveOriginOrdinals(reference->origins, type.span, paramOrdinals);
      return result;
    }
    if (const auto *span = dynamic_cast<const syntax::SpanTypeSyntax *>(&type))
      return Type{.kind = TypeKind::ScopedSpan, .span = type.span, .target = lowered(*span->target)};
    if (const auto *pointer = dynamic_cast<const syntax::RawPointerTypeSyntax *>(&type))
      return Type{.kind = TypeKind::RawPointer,
                  .span = type.span,
                  .target = lowered(*pointer->pointee),
                  .isMutable = pointer->isMutable};
    if (const auto *nullable = dynamic_cast<const syntax::NullableTypeSyntax *>(&type))
      return Type{.kind = TypeKind::Nullable, .span = type.span, .target = lowered(*nullable->target)};
    if (const auto *pack = dynamic_cast<const syntax::PackTypeSyntax *>(&type))
      return Type{.kind = TypeKind::Pack, .span = type.span, .target = lowered(*pack->target)};
    if (const auto *unionType = dynamic_cast<const syntax::UnionTypeSyntax *>(&type))
    {
      Type result{.kind = TypeKind::Union, .span = type.span};
      for (const auto &member : unionType->members)
        result.members.push_back(lowerType(*member, paramOrdinals));
      return result;
    }
    if (const auto *closureType = dynamic_cast<const syntax::ClosureTypeSyntax *>(&type))
    {
      Type result{.kind = TypeKind::Closure, .span = type.span};
      for (const auto &parameter : closureType->parameters)
        result.parameters.push_back(lowerType(*parameter, paramOrdinals));
      result.result = lowered(*closureType->result);
      return result;
    }
    throw ResolutionError("unsupported type during name resolution", type.span);
  }

  auto renderTypeName(const syntax::TypeSyntax &type) -> std::string
  {
    const auto render = [](const auto &inner) { return renderTypeName(*inner); };
    if (const auto *named = dynamic_cast<const syntax::NamedTypeSyntax *>(&type))
      return named->name;
    if (const auto *applied = dynamic_cast<const syntax::AppliedTypeSyntax *>(&type))
      return renderTypeName(*applied->constructor) + "<" +
             joined(applied->arguments, ", ",
                    [](const auto &argument)
                    {
                      return argument.kind == syntax::GenericArgumentKind::Type
                                 ? renderTypeName(*argument.type)
                                 : syntax::renderConstExpr(*argument.constExpr);
                    }) +
             ">";
    if (const auto *reference = dynamic_cast<const syntax::ScopedReferenceTypeSyntax *>(&type))
    {
      std::string result = renderTypeName(*reference->target) + (reference->isMutable ? " ref mut" : " ref");
      if (!reference->origins.empty())
        result += "(" + joined(reference->origins, " | ", [](const auto &origin) { return origin; }) + ")";
      return result;
    }
    if (const auto *span = dynamic_cast<const syntax::SpanTypeSyntax *>(&type))
      return renderTypeName(*span->target) + " span";
    if (const auto *pointer = dynamic_cast<const syntax::RawPointerTypeSyntax *>(&type))
      return renderTypeName(*pointer->pointee) + (pointer->isMutable ? " *mut" : " *const");
    if (const auto *nullable = dynamic_cast<const syntax::NullableTypeSyntax *>(&type))
      return renderTypeName(*nullable->target) + "?";
    if (const auto *unionType = dynamic_cast<const syntax::UnionTypeSyntax *>(&type))
      return joined(unionType->members, " | ", render);
    if (const auto *pack = dynamic_cast<const syntax::PackTypeSyntax *>(&type))
      return renderTypeName(*pack->target) + "...";
    if (const auto *closure = dynamic_cast<const syntax::ClosureTypeSyntax *>(&type))
      return "|" + joined(closure->parameters, ", ", render) + "| -> " + renderTypeName(*closure->result);
    throw ResolutionError("unsupported type during name resolution", type.span);
  }

  auto lowerTypeArguments(const std::vector<syntax::GenericArgumentSyntax> &arguments) -> std::vector<TypeArgument>
  {
    std::vector<TypeArgument> lowered;
    lowered.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
      TypeArgument result{.kind = argument.kind, .span = argument.span};
      if (argument.type != nullptr)
        result.type = std::make_unique<Type>(lowerType(*argument.type));
      if (argument.constExpr != nullptr)
        result.constExpr = syntax::cloneConstExpr(*argument.constExpr);
      lowered.push_back(std::move(result));
    }
    return lowered;
  }

  auto cloneTypeArguments(const std::vector<TypeArgument> &arguments) -> std::vector<TypeArgument>
  {
    std::vector<TypeArgument> cloned;
    cloned.reserve(arguments.size());
    for (const auto &argument : arguments)
    {
      TypeArgument copy{.kind = argument.kind, .span = argument.span};
      if (argument.type != nullptr)
        copy.type = std::make_unique<Type>(cloneType(*argument.type));
      if (argument.constExpr != nullptr)
        copy.constExpr = syntax::cloneConstExpr(*argument.constExpr);
      cloned.push_back(std::move(copy));
    }
    return cloned;
  }

  auto cloneType(const Type &type) -> Type
  {
    Type cloned{.kind = type.kind, .span = type.span, .name = type.name, .isMutable = type.isMutable};
    if (type.target != nullptr)
      cloned.target = std::make_unique<Type>(cloneType(*type.target));
    cloned.arguments = cloneTypeArguments(type.arguments);
    for (const auto &member : type.members)
      cloned.members.push_back(cloneType(member));
    for (const auto &parameter : type.parameters)
      cloned.parameters.push_back(cloneType(parameter));
    if (type.result != nullptr)
      cloned.result = std::make_unique<Type>(cloneType(*type.result));
    cloned.originOrdinals = type.originOrdinals;
    return cloned;
  }
} // namespace NG::hir::detail
