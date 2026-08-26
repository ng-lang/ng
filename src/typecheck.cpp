// AI-generated code; reviewed for this repository's vNext rewrite.
// ARCH-4: typecheck.cpp was split into 5 TUs by cohesion.
// This file retains the TypeChecker::check() delegation only.
#include "typecheck.hpp"
#include "typecheck_internal.hpp"

namespace NG::typecheck
{
  auto TypeChecker::check(const hir::Module &module, const const_eval::ConstNativeHost &host) -> TypeCheckResult
  {
    return detail::Checker{}.check(module, host);
  }
} // namespace NG::typecheck
