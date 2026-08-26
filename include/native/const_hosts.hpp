// AI-generated code; reviewed for this repository's vNext rewrite.
//
// ARCH-3: registry of const-capable natives (D-012). See
// src/native/const_hosts.cpp for the table.
#pragma once

#include "const_eval.hpp"

#include <string_view>

namespace NG::native
{
  /// Builds the pure-host callback the type checker hands to const
  /// evaluation: name-dispatched libngrt calls over canonical const values.
  [[nodiscard]] auto makeConstNativeHost() -> const_eval::ConstNativeHost;
} // namespace NG::native
