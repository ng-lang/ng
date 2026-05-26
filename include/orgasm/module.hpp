#pragma once

#include <common.hpp>
#include <fwd.hpp>
#include <orgasm/opcode.hpp>

namespace NG::orgasm
{
    constexpr uint32_t NGO_FORMAT_VERSION = 1;
    constexpr uint32_t NGO_ABI_VERSION = 1;
    constexpr uint32_t NGO_METADATA_SCHEMA_VERSION = 2;

    /**
     * @brief A function in the ORGASM bytecode.
     */
    struct Function
    {
        Str name;            ///< The name of the function.
        Vec<uint8_t> code;   ///< The bytecode of the function.
        int32_t num_locals;  ///< The number of local variables.
        int32_t num_params;  ///< The number of parameters.
        bool explicit_receiver = false; ///< Whether member dispatch already passes receiver as param 0.
    };

    /**
     * @brief A variant in a tagged union type.
     */
    struct Variant
    {
        Str name;
        Vec<Str> payloadFields;  // Names/types of payload fields
    };

    /**
     * @brief A type in the ORGASM bytecode.
     */
    struct Type
    {
        Str name;
        Vec<Str> properties;
        Vec<Str> derivedTraits;
        Vec<Variant> variants;  // Non-empty if this is a tagged union type
    };

    /**
     * @brief An external symbol (imported from another module).
     */
    struct ExternalSymbol
    {
        Str moduleName;
        Str symbolName;
    };

    struct BytecodeImplMetadata
    {
        Str traitName;
        Str targetPattern;
        Str moduleId;
        Vec<Str> genericParamNames;
        Vec<Str> whereBounds;
        Map<Str, Str> methods;
    };

    struct BytecodeTraitMetadata
    {
        Str name;
        Str moduleId;
        Vec<Str> typeParamNames;
        Vec<Str> superTraits;
        Map<Str, Str> methods;
        Map<Str, Str> allMethods;
    };

    /**
     * @brief A module in the ORGASM bytecode.
     */
    struct BytecodeModule
    {
        Str name;                     ///< The name of the module.
        Str sourceHash;               ///< Optional source/build hash used for stale artifact checks.
        Vec<int64_t> constants;       ///< The numeric constants of the module.
        Vec<double> float_constants;  ///< The floating-point constants of the module.
        Vec<Str> strings;             ///< The string constants of the module.
        Vec<Function> functions;      ///< The functions in the module.
        Vec<Type> types;              ///< The types in the module.
        Vec<ExternalSymbol> imports;  ///< The imported symbols.
        Map<Str, int32_t> exports;    ///< The exported symbols and their indices.
        Map<Str, Str> exportTypeReprs; ///< Exported typechecker metadata by symbol.
        Vec<BytecodeTraitMetadata> traitMetadata; ///< Exported trait shape metadata.
        Vec<BytecodeImplMetadata> implMetadata; ///< Exported trait implementation metadata.

        /**
         * @brief Merges another module into this one.
         *
         * All functions, types, constants, and strings from `other` are appended
         * to this module. Function indices in the merged code are remapped.
         * Exported symbols from `other` are re-registered under `prefix`.
         *
         * @param other The module to merge into this one.
         * @param prefix Optional prefix for merged symbol names (e.g. module name).
         */
        void merge(const BytecodeModule &other, const Str &prefix = "");
    };

    void write_bytecode_module(const BytecodeModule &module, const Str &path, const Str &sourceHash = {});
    auto read_bytecode_module(const Str &path, const Str &expectedModuleId = {}) -> BytecodeModule;
    auto bytecode_source_hash(const Str &source) -> Str;
} // namespace NG::orgasm
