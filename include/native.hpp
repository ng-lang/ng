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

    /// How a native boundary treats a value (D-004 ownership annotations;
    /// `native fun` currently defaults every parameter to `Copy` and the
    /// result to `Move` until explicit annotation syntax lands).
    enum class Ownership
    {
      /// The callee receives an independent value; the caller retains its own
      /// copy (copy-first D-015).
      Copy,
      /// The callee may use the value for the call duration but does not own
      /// or consume it.
      Borrow,
      /// The callee takes ownership of the value; the caller must not use it
      /// afterwards.
      Move,
    };

    /// Declared signature of a `native fun` (R9 first slice): the parameter
    /// and result types from the NG declaration plus a pure/const-capable
    /// flag (D-012) and ownership descriptors for AOT copy/move lowering.
    /// An empty parameter vector means "undeclared" (legacy registrations;
    /// no arity validation and no type guidance).
    struct DeclaredSignature
    {
      std::vector<typecheck::TypeId> parameters;
      typecheck::TypeId result{typecheck::builtin::Unit};
      bool pure{};
      /// Per-parameter ownership. Empty means "not declared"; when non-empty
      /// it has the same size as `parameters`.
      std::vector<Ownership> parameterOwnership;
      /// Ownership of the returned value.
      Ownership resultOwnership{Ownership::Move};
    };

    struct Entry
    {
      NativeFunction function;
      DeclaredSignature signature;
    };

    void registerNative(std::string name, NativeFunction function)
    {
      natives_.insert_or_assign(std::move(name), Entry{std::move(function), {}});
    }

    void registerNative(std::string name, NativeFunction function, DeclaredSignature signature)
    {
      natives_.insert_or_assign(std::move(name), Entry{std::move(function), std::move(signature)});
    }

    /// Attaches the declared signature derived from the NG declaration to an
    /// existing entry, or creates an unregistered entry so call sites fail
    /// with a precise diagnostic.
    void declare(std::string name, DeclaredSignature signature)
    {
      auto [found, inserted] = natives_.try_emplace(std::move(name), Entry{nullptr, std::move(signature)});
      if (!inserted) found->second.signature = std::move(signature);
    }

    [[nodiscard]] auto lookup(const std::string &name) const -> const Entry *
    {
      const auto found = natives_.find(name);
      return found != natives_.end() ? &found->second : nullptr;
    }

  private:
    std::unordered_map<std::string, Entry> natives_;
  };
} // namespace NG::vm
