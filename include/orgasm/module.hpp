#pragma once

#include <common.hpp>
#include <fwd.hpp>
#include <orgasm/opcode.hpp>

namespace NG::orgasm
{
    // Bump the format/ABI whenever OpCode numeric values or operand layouts change.
    constexpr uint32_t NGO_FORMAT_VERSION = 2;
    constexpr uint32_t NGO_ABI_VERSION = 2;
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
        Map<Str, size_t> functionIndex; ///< Function name → index lookup (built by buildIndex).
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

        /**
         * @brief Appends a function and updates the function name index.
         *
         * @return The index of the appended function.
         */
        auto addFunction(Function function) -> size_t
        {
            functions.push_back(std::move(function));
            const size_t index = functions.size() - 1;
            functionIndex[functions[index].name] = index;
            return index;
        }

        /**
         * @brief Builds the function name index for O(1) lookup.
         */
        void buildIndex()
        {
            functionIndex.clear();
            for (size_t i = 0; i < functions.size(); ++i)
            {
                functionIndex[functions[i].name] = i;
            }
        }

        /**
         * @brief Finds a function index by name. Returns -1 if not found.
         */
        [[nodiscard]] auto findFunction(const Str &name) const -> int32_t
        {
            if (!functionIndex.empty())
            {
                bool indexValid = functionIndex.size() == functions.size();
                for (const auto &[mappedName, idx] : functionIndex)
                {
                    if (idx >= functions.size() || functions[idx].name != mappedName)
                    {
                        indexValid = false;
                        break;
                    }
                }
                if (indexValid)
                {
                    auto it = functionIndex.find(name);
                    return it != functionIndex.end() ? static_cast<int32_t>(it->second) : -1;
                }
            }
            for (size_t i = 0; i < functions.size(); ++i)
            {
                if (functions[i].name == name) return static_cast<int32_t>(i);
            }
            return -1;
        }
    };

    // Non-empty overrideSourceHash is written instead of BytecodeModule::sourceHash.
    void write_bytecode_module(const BytecodeModule &module, const Str &path, const Str &overrideSourceHash = {});
    auto read_bytecode_module(const Str &path, const Str &expectedModuleId = {}) -> BytecodeModule;
    auto bytecode_source_hash(const Str &source) -> Str;
} // namespace NG::orgasm
