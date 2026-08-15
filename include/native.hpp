// AI-generated code; reviewed for this repository's vNext rewrite.
#pragma once

#include "bytecode.hpp"
#include "value.hpp"
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace NG::vm
{
  /// Host-supplied runtime intrinsic (D-004 `native fun`). The registry is a
  /// session-owned table; the initial slice registers `print`/`assert`
  /// builtins from the driver.
  class NativeRegistry final
  {
  public:
    /// Host intrinsic: arguments plus the callee's static parameter types
    /// (for overload-sensitive builtins such as `print`).
    using NativeFunction = std::function<Value(const std::vector<Value> &, const std::vector<typecheck::TypeId> &)>;

    void registerNative(std::string name, NativeFunction function)
    {
      natives_.insert_or_assign(std::move(name), std::move(function));
    }

    [[nodiscard]] auto lookup(const std::string &name) const -> const NativeFunction *
    {
      const auto found = natives_.find(name);
      return found != natives_.end() ? &found->second : nullptr;
    }

  private:
    std::unordered_map<std::string, NativeFunction> natives_;
  };
} // namespace NG::vm
