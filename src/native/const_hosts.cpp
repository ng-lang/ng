// AI-generated code; reviewed for this repository's vNext rewrite.
//
// ARCH-3: registry of const-capable natives (D-012 `= native` capability).
// Pure libngrt-backed hosts callable at compile time from const fun bodies,
// `const if` conditions, and where-clause evaluation. The table replaces the
// inline name-dispatch lambda that used to live in driver.cpp so that adding
// a const-capable native is one row here, next to the other string rows.
//
// Not part of the public API surface beyond makeConstNativeHost().

#include "native/const_hosts.hpp"

#include "const_eval.hpp"
#include "ngrt.hpp"

#include <format>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace NG::native
{
  namespace
  {
    using const_eval::ConstEvalError;
    using const_eval::ConstInterner;
    using const_eval::ConstValueId;
    using const_eval::ConstValueKind;
    using syntax::SourceSpan;

    struct HostContext
    {
      std::string_view name;
      std::span<const ConstValueId> arguments;
      ConstInterner &interner;
      SourceSpan span;

      [[nodiscard]] auto requireString(size_t index) -> std::string
      {
        if (index >= arguments.size() || interner.value(arguments[index]).kind != ConstValueKind::String)
          throw ConstEvalError(std::format("const native `{}` expects a string argument {}", name, index + 1), span);
        return interner.value(arguments[index]).stringValue;
      }

      [[nodiscard]] auto requireInteger(size_t index) -> int64_t
      {
        if (index >= arguments.size() || interner.value(arguments[index]).kind != ConstValueKind::Integer)
          throw ConstEvalError(std::format("const native `{}` expects an integer argument {}", name, index + 1), span);
        return interner.value(arguments[index]).integerValue;
      }
    };

    struct HostEntry
    {
      std::string_view name;
      std::function<ConstValueId(HostContext &)> run;
    };

    const std::vector<HostEntry> kHostTable = {
      {"length", [](HostContext &host) { return host.interner.internInteger(ngrt::length(host.requireString(0))); }},
      {"contains", [](HostContext &host)
       { return host.interner.internBool(ngrt::contains(host.requireString(0), host.requireString(1))); }},
      {"startsWith", [](HostContext &host)
       { return host.interner.internBool(ngrt::startsWith(host.requireString(0), host.requireString(1))); }},
      {"endsWith", [](HostContext &host)
       { return host.interner.internBool(ngrt::endsWith(host.requireString(0), host.requireString(1))); }},
      {"toUpper", [](HostContext &host) { return host.interner.internString(ngrt::toUpper(host.requireString(0))); }},
      {"toLower", [](HostContext &host) { return host.interner.internString(ngrt::toLower(host.requireString(0))); }},
      {"trim", [](HostContext &host) { return host.interner.internString(ngrt::trim(host.requireString(0))); }},
      {"replace",
       [](HostContext &host)
       {
         return host.interner.internString(
             ngrt::replace(host.requireString(0), host.requireString(1), host.requireString(2)));
       }},
      {"substring",
       [](HostContext &host)
       {
         const auto text = host.requireString(0);
         const auto start = host.requireInteger(1);
         const auto end = host.requireInteger(2);
         return host.interner.internString(ngrt::substring(text, start, end));
       }},
      {"charAt",
       [](HostContext &host)
       {
         const auto text = host.requireString(0);
         const auto index = host.requireInteger(1);
         return host.interner.internString(ngrt::charAt(text, index));
       }},
      {"regexMatch", [](HostContext &host)
       { return host.interner.internBool(ngrt::regexMatch(host.requireString(0), host.requireString(1))); }},
      // INC-7: string conversions and integer parsing. toString covers
      // integer/bool arguments only — const values carry no float kind yet,
      // and unsigned arguments arrive as i64-kind integers (exact in slot).
      {"toString",
       [](HostContext &host)
       {
         if (!host.arguments.empty() && host.interner.value(host.arguments[0]).kind == ConstValueKind::Bool)
           return host.interner.internString(ngrt::toStringBool(host.interner.value(host.arguments[0]).boolValue));
         return host.interner.internString(ngrt::toStringSigned(host.requireInteger(0)));
       }},
      {"canParseInt",
       [](HostContext &host) { return host.interner.internBool(ngrt::canParseInt(host.requireString(0))); }},
      {"parseRawInt",
       [](HostContext &host)
       {
         const auto text = host.requireString(0);
         if (!ngrt::canParseInt(text))
           throw ConstEvalError("parseRawInt: input is not a valid i64 literal", host.span);
         return host.interner.internInteger(ngrt::parseRawInt(text));
       }},
    };
  } // namespace

  auto makeConstNativeHost() -> const_eval::ConstNativeHost
  {
    return [](std::string_view name, std::span<const ConstValueId> arguments, ConstInterner &interner,
              SourceSpan span) -> ConstValueId
    {
      HostContext context{.name = name, .arguments = arguments, .interner = interner, .span = span};
      for (const auto &entry : kHostTable)
        if (entry.name == name)
        {
          try
          {
            return entry.run(context);
          }
          catch (const std::out_of_range &error)
          {
            throw ConstEvalError(std::format("const {}", error.what()), span);
          }
          catch (const std::runtime_error &error)
          {
            throw ConstEvalError(std::format("const {}", error.what()), span);
          }
        }
      throw ConstEvalError(std::format("native `{}` is not const-capable", name), span);
    };
  }
} // namespace NG::native
