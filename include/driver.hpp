// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include <iosfwd>
#include <string_view>
#include <vector>

namespace NG
{
  /// Runs the NG command-line frontend. All execution goes through the QBE
  /// native backend; there is no bytecode/VM tier.
  [[nodiscard]] auto runDriver(const std::vector<std::string_view> &arguments, std::ostream &output,
                               std::ostream &errors) -> int;
} // namespace NG
