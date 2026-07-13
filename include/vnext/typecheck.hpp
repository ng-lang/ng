// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "vnext/hir.hpp"
#include <stdexcept>
#include <string>

namespace NG::vnext::typecheck
{
  struct TypeError : std::runtime_error
  {
    syntax::SourceSpan span;

    TypeError(std::string message, syntax::SourceSpan sourceSpan)
      : std::runtime_error(std::move(message)), span(sourceSpan)
    {
    }
  };

  /// First typed-HIR validation slice. It is intentionally a side-table-style
  /// pass over resolved HIR: syntax/HIR nodes remain free of checker mutation.
  class TypeChecker final
  {
  public:
    void check(const hir::Module &module);
  };
} // namespace NG::vnext::typecheck
