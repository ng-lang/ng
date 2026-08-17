// AI-generated code; reviewed for this repository's vNext rewrite.
//
// Thin C++ wrappers over libngrt. The VM and const evaluator convert their
// native values into the libngrt ABI (len-prefixed strings / array headers)
// and call these wrappers; the wrappers convert back to std::string/Value.
#pragma once

#include "value.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace NG::ngrt
{
  // std.string
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
  [[nodiscard]] auto regexMatch(std::string_view text, std::string_view pattern) -> bool;

  // std.io
  [[nodiscard]] auto readLine() -> std::string;
  [[nodiscard]] auto readFile(std::string_view path) -> std::string;
  auto writeFile(std::string_view path, std::string_view content) -> void;
  [[nodiscard]] auto currentExecutablePath() -> std::string;

  // std.seq (array<i64>; join above handles array<string>)
  [[nodiscard]] auto arrayLength(const std::vector<Value> &values) -> int64_t;
  [[nodiscard]] auto arraySum(const std::vector<Value> &values) -> int64_t;
  [[nodiscard]] auto arrayContains(const std::vector<Value> &values, int64_t value) -> bool;
  [[nodiscard]] auto arrayReverse(const std::vector<Value> &values) -> Value;

  // std.memory
  [[nodiscard]] auto allocate(int64_t value) -> uint64_t;
  [[nodiscard]] auto load(uint64_t handle) -> int64_t;
  auto store(uint64_t handle, int64_t value) -> void;
  auto release(uint64_t handle) -> void;
  [[nodiscard]] auto outstanding() -> int64_t;
} // namespace NG::ngrt
