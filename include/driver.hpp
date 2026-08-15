// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "native.hpp"

#include <iosfwd>
#include <string_view>
#include <vector>

namespace NG
{
  /// Extra natives injected into every run (e.g. the imgui binding).
  using NativeRegistration = void (*)(vm::NativeRegistry &);

  /// Runs the NG command-line frontend.
  [[nodiscard]] auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output,
                               std::ostream &errors) -> int;

  /// Runs the frontend with an additional native registration pass.
  [[nodiscard]] auto runDriverWithNatives(const std::vector<std::string_view> &arguments, std::ostream &output,
                                          std::ostream &errors, NativeRegistration registration) -> int;
} // namespace NG
