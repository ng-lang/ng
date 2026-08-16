// AI-generated code; reviewed for this repository's vNext rewrite.
//
// Shared implementations of the standard pure string natives. The const
// evaluator (ConstNativeHost) and the runtime registry both marshal into
// these operations, so the two previously duplicated implementations
// cannot drift (A6). Bounds violations throw std::out_of_range with the
// canonical message; adapters translate into ConstEvalError /
// BytecodeError with their own spans and prefixes.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace NG::string_ops
{
  [[nodiscard]] auto length(std::string_view text) -> int64_t;
  [[nodiscard]] auto trim(std::string_view text) -> std::string;
  [[nodiscard]] auto toUpper(std::string_view text) -> std::string;
  [[nodiscard]] auto toLower(std::string_view text) -> std::string;
  [[nodiscard]] auto charAt(std::string_view text, int64_t index) -> std::string;
  [[nodiscard]] auto substring(std::string_view text, int64_t start, int64_t end) -> std::string;
  [[nodiscard]] auto contains(std::string_view text, std::string_view needle) -> bool;
  [[nodiscard]] auto startsWith(std::string_view text, std::string_view prefix) -> bool;
  [[nodiscard]] auto endsWith(std::string_view text, std::string_view suffix) -> bool;
  [[nodiscard]] auto replace(std::string_view text, std::string_view needle, std::string_view replacement)
      -> std::string;
  [[nodiscard]] auto split(std::string_view text, std::string_view delimiter) -> std::vector<std::string>;
  [[nodiscard]] auto join(const std::vector<std::string> &items, std::string_view separator) -> std::string;
  /// Throws std::runtime_error("invalid regex pattern") on bad patterns.
  [[nodiscard]] auto regexMatch(std::string_view text, std::string_view pattern) -> bool;
} // namespace NG::string_ops
