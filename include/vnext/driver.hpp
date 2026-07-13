// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <iosfwd>
#include <string_view>
#include <vector>

namespace NG::vnext
{
  /// Runs the vNext command-line frontend. This boundary deliberately depends
  /// only on the vNext syntax pipeline, never on the legacy interpreter.
  [[nodiscard]] auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output,
                               std::ostream &errors) -> int;
} // namespace NG::vnext
