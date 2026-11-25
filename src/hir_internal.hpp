// AI-generated code; reviewed for this repository's vNext rewrite.
// Internal helpers shared by the HIR translation units:
//   hir.cpp             module items, declarations, names, and scopes
//   hir_body.cpp        statements (incl. `?` desugaring) and expressions
//   hir_types.cpp       type lowering, rendering, and cloning
//   hir_clone.cpp       function cloning for monomorphization and closures
// Not part of the public API.
#pragma once

#include "hir.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace NG::hir::detail
{
  /// Lowers a syntax type to HIR. `paramOrdinals` maps the enclosing
  /// function's parameter names to ordinals for `T ref(a | b)` origin lists.
  [[nodiscard]] auto lowerType(const syntax::TypeSyntax &type,
                               const std::unordered_map<std::string, uint32_t> *paramOrdinals = nullptr) -> Type;
  /// Renders a syntax type as written (diagnostics and impl method names).
  [[nodiscard]] auto renderTypeName(const syntax::TypeSyntax &type) -> std::string;
  /// Lowers explicit generic arguments (`name<T, 3>`).
  [[nodiscard]] auto lowerTypeArguments(const std::vector<syntax::GenericArgumentSyntax> &arguments)
      -> std::vector<TypeArgument>;
  [[nodiscard]] auto cloneType(const Type &type) -> Type;
  [[nodiscard]] auto cloneTypeArguments(const std::vector<TypeArgument> &arguments) -> std::vector<TypeArgument>;
} // namespace NG::hir::detail
