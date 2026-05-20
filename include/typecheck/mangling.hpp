#pragma once

#include <typecheck/typeinfo.hpp>

namespace NG::typecheck
{
    enum class MangledSymbolKind : uint8_t
    {
        Function,
        Type,
        Const,
        Impl,
    };

    [[nodiscard]] auto canonical_type_name(const CheckingRef<TypeInfo> &type) -> Str;
    [[nodiscard]] auto canonical_type_name(const TypeInfo &type) -> Str;

    [[nodiscard]] auto mangle_symbol(MangledSymbolKind kind,
                                     const Str &moduleName,
                                     const Str &baseName,
                                     const Vec<Str> &canonicalTypeArgs = {}) -> Str;

    [[nodiscard]] auto mangle_symbol(MangledSymbolKind kind,
                                     const Str &moduleName,
                                     const Str &baseName,
                                     const Vec<CheckingRef<TypeInfo>> &typeArgs) -> Str;
} // namespace NG::typecheck
