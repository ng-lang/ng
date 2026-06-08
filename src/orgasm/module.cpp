#include <orgasm/module.hpp>
#include <cstring>
#include <fstream>
#include <limits>
#include <sstream>
#include <type_traits>

namespace NG::orgasm
{
    namespace
    {
        constexpr char NGO_MAGIC[4] = {'N', 'G', 'O', '\0'};
        constexpr uint32_t MAX_NGO_STRING_SIZE = 64U * 1024U * 1024U;
        constexpr uint32_t MAX_NGO_VECTOR_SIZE = 1U * 1024U * 1024U;
        constexpr uint32_t MAX_NGO_CODE_SIZE = 64U * 1024U * 1024U;
        constexpr uint32_t MAX_NGO_EXPORTS = 1U * 1024U * 1024U;

        void read_exact_bytes(std::istream &in, char *destination, size_t destinationSize,
                              size_t requestedSize, const Str &field)
        {
            if (destination == nullptr && requestedSize > 0)
            {
                throw RuntimeException("Invalid .ngo destination buffer while reading " + field);
            }
            if (requestedSize > destinationSize)
            {
                throw RuntimeException(".ngo read exceeds destination buffer while reading " + field);
            }
            if (requestedSize > static_cast<size_t>(std::numeric_limits<std::streamsize>::max()))
            {
                throw RuntimeException(".ngo read too large while reading " + field);
            }
            if (requestedSize == 0)
            {
                return;
            }
            auto bytesRead = in.rdbuf()->sgetn(destination, static_cast<std::streamsize>(requestedSize));
            if (bytesRead != static_cast<std::streamsize>(requestedSize))
            {
                throw RuntimeException("Truncated .ngo artifact while reading " + field);
            }
        }

        template <typename T>
        void write_scalar(std::ostream &out, T value)
        {
            static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_enum_v<T>);
            out.write(reinterpret_cast<const char *>(&value), sizeof(T));
            if (!out)
            {
                throw RuntimeException("Failed to write .ngo artifact");
            }
        }

        template <typename T>
        auto read_scalar(std::istream &in, const Str &field) -> T
        {
            static_assert(std::is_integral_v<T> || std::is_floating_point_v<T> || std::is_enum_v<T>);
            T value{};
            read_exact_bytes(in, reinterpret_cast<char *>(&value), sizeof(value), sizeof(value), field);
            return value;
        }

        void write_string(std::ostream &out, const Str &value)
        {
            write_scalar<uint32_t>(out, static_cast<uint32_t>(value.size()));
            out.write(value.data(), static_cast<std::streamsize>(value.size()));
            if (!out)
            {
                throw RuntimeException("Failed to write .ngo string");
            }
        }

        auto read_string(std::istream &in, const Str &field) -> Str
        {
            auto size = read_scalar<uint32_t>(in, field + ".size");
            if (size > MAX_NGO_STRING_SIZE)
            {
                throw RuntimeException(".ngo string too large while reading " + field);
            }
            Str value(size, '\0');
            if (size > 0)
            {
                read_exact_bytes(in, value.data(), value.size(), size, field);
            }
            return value;
        }

        template <typename T, typename WriteItem>
        void write_vector(std::ostream &out, const Vec<T> &items, WriteItem writeItem)
        {
            write_scalar<uint32_t>(out, static_cast<uint32_t>(items.size()));
            for (const auto &item : items)
            {
                writeItem(item);
            }
        }

        template <typename T, typename ReadItem>
        auto read_vector(std::istream &in, const Str &field, ReadItem readItem) -> Vec<T>
        {
            auto count = read_scalar<uint32_t>(in, field + ".count");
            if (count > MAX_NGO_VECTOR_SIZE)
            {
                throw RuntimeException(".ngo vector too large while reading " + field);
            }
            Vec<T> items;
            items.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                items.push_back(readItem(field + "[" + std::to_string(i) + "]"));
            }
            return items;
        }

        void write_string_vector(std::ostream &out, const Vec<Str> &items)
        {
            write_vector<Str>(out, items, [&](const Str &item) { write_string(out, item); });
        }

        auto read_string_vector(std::istream &in, const Str &field) -> Vec<Str>
        {
            return read_vector<Str>(in, field, [&](const Str &itemField) { return read_string(in, itemField); });
        }

        void write_string_map(std::ostream &out, const Map<Str, Str> &items)
        {
            write_scalar<uint32_t>(out, static_cast<uint32_t>(items.size()));
            for (const auto &[key, value] : items)
            {
                write_string(out, key);
                write_string(out, value);
            }
        }

        auto read_string_map(std::istream &in, const Str &field) -> Map<Str, Str>
        {
            auto count = read_scalar<uint32_t>(in, field + ".count");
            if (count > MAX_NGO_VECTOR_SIZE)
            {
                throw RuntimeException(".ngo map too large while reading " + field);
            }
            Map<Str, Str> items;
            for (uint32_t i = 0; i < count; ++i)
            {
                auto key = read_string(in, field + "[" + std::to_string(i) + "].key");
                auto value = read_string(in, field + "[" + std::to_string(i) + "].value");
                items.insert_or_assign(std::move(key), std::move(value));
            }
            return items;
        }

        void write_variant(std::ostream &out, const Variant &variant)
        {
            write_string(out, variant.name);
            write_string_vector(out, variant.payloadFields);
        }

        auto read_variant(std::istream &in, const Str &field) -> Variant
        {
            Variant variant;
            variant.name = read_string(in, field + ".name");
            variant.payloadFields = read_string_vector(in, field + ".payloadFields");
            return variant;
        }

        void write_type(std::ostream &out, const Type &type)
        {
            write_string(out, type.name);
            write_string_vector(out, type.properties);
            write_string_vector(out, type.derivedTraits);
            write_vector<Variant>(out, type.variants, [&](const Variant &variant) { write_variant(out, variant); });
        }

        auto read_type(std::istream &in, const Str &field) -> Type
        {
            Type type;
            type.name = read_string(in, field + ".name");
            type.properties = read_string_vector(in, field + ".properties");
            type.derivedTraits = read_string_vector(in, field + ".derivedTraits");
            type.variants = read_vector<Variant>(in, field + ".variants",
                                                 [&](const Str &itemField) { return read_variant(in, itemField); });
            return type;
        }

        void write_function(std::ostream &out, const Function &function)
        {
            write_string(out, function.name);
            write_scalar<int32_t>(out, function.num_locals);
            write_scalar<int32_t>(out, function.num_params);
            write_scalar<uint8_t>(out, function.explicit_receiver ? 1 : 0);
            write_scalar<uint32_t>(out, static_cast<uint32_t>(function.code.size()));
            if (!function.code.empty())
            {
                out.write(reinterpret_cast<const char *>(function.code.data()),
                          static_cast<std::streamsize>(function.code.size()));
                if (!out)
                {
                    throw RuntimeException("Failed to write .ngo function code");
                }
            }
        }

        auto read_function(std::istream &in, const Str &field) -> Function
        {
            Function function;
            function.name = read_string(in, field + ".name");
            function.num_locals = read_scalar<int32_t>(in, field + ".num_locals");
            function.num_params = read_scalar<int32_t>(in, field + ".num_params");
            function.explicit_receiver = read_scalar<uint8_t>(in, field + ".explicit_receiver") != 0;
            auto codeSize = read_scalar<uint32_t>(in, field + ".code.size");
            if (codeSize > MAX_NGO_CODE_SIZE)
            {
                throw RuntimeException(".ngo function code too large while reading " + field);
            }
            function.code.resize(codeSize);
            if (codeSize > 0)
            {
                read_exact_bytes(in, reinterpret_cast<char *>(function.code.data()), function.code.size(),
                                 codeSize, field + ".code");
            }
            return function;
        }

        void write_import(std::ostream &out, const ExternalSymbol &symbol)
        {
            write_string(out, symbol.moduleName);
            write_string(out, symbol.symbolName);
        }

        auto read_import(std::istream &in, const Str &field) -> ExternalSymbol
        {
            return ExternalSymbol{
                .moduleName = read_string(in, field + ".moduleName"),
                .symbolName = read_string(in, field + ".symbolName"),
            };
        }

        void write_impl_metadata(std::ostream &out, const BytecodeImplMetadata &impl)
        {
            write_string(out, impl.traitName);
            write_string(out, impl.targetPattern);
            write_string(out, impl.moduleId);
            write_string_vector(out, impl.genericParamNames);
            write_string_vector(out, impl.whereBounds);
            write_string_map(out, impl.methods);
        }

        auto read_impl_metadata(std::istream &in, const Str &field) -> BytecodeImplMetadata
        {
            BytecodeImplMetadata impl;
            impl.traitName = read_string(in, field + ".traitName");
            impl.targetPattern = read_string(in, field + ".targetPattern");
            impl.moduleId = read_string(in, field + ".moduleId");
            impl.genericParamNames = read_string_vector(in, field + ".genericParamNames");
            impl.whereBounds = read_string_vector(in, field + ".whereBounds");
            impl.methods = read_string_map(in, field + ".methods");
            return impl;
        }

        void write_trait_metadata(std::ostream &out, const BytecodeTraitMetadata &trait)
        {
            write_string(out, trait.name);
            write_string(out, trait.moduleId);
            write_string_vector(out, trait.typeParamNames);
            write_string_vector(out, trait.superTraits);
            write_string_map(out, trait.methods);
            write_string_map(out, trait.allMethods);
        }

        auto read_trait_metadata(std::istream &in, const Str &field) -> BytecodeTraitMetadata
        {
            BytecodeTraitMetadata trait;
            trait.name = read_string(in, field + ".name");
            trait.moduleId = read_string(in, field + ".moduleId");
            trait.typeParamNames = read_string_vector(in, field + ".typeParamNames");
            trait.superTraits = read_string_vector(in, field + ".superTraits");
            trait.methods = read_string_map(in, field + ".methods");
            trait.allMethods = read_string_map(in, field + ".allMethods");
            return trait;
        }
    } // namespace

    auto bytecode_source_hash(const Str &source) -> Str
    {
        uint64_t hash = 1469598103934665603ULL;
        for (unsigned char ch : source)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        std::ostringstream out;
        out << std::hex << hash;
        return out.str();
    }

    void write_bytecode_module(const BytecodeModule &module, const Str &path, const Str &overrideSourceHash)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            throw RuntimeException("Failed to open .ngo artifact for writing: " + path);
        }

        out.write(NGO_MAGIC, sizeof(NGO_MAGIC));
        write_scalar<uint32_t>(out, NGO_FORMAT_VERSION);
        write_scalar<uint32_t>(out, NGO_ABI_VERSION);
        write_scalar<uint32_t>(out, NGO_METADATA_SCHEMA_VERSION);
        write_string(out, module.name);
        write_string(out, overrideSourceHash.empty() ? module.sourceHash : overrideSourceHash);

        write_vector<int64_t>(out, module.constants, [&](int64_t value) { write_scalar<int64_t>(out, value); });
        write_vector<double>(out, module.float_constants, [&](double value) { write_scalar<double>(out, value); });
        write_string_vector(out, module.strings);
        write_vector<Function>(out, module.functions, [&](const Function &function) { write_function(out, function); });
        write_vector<Type>(out, module.types, [&](const Type &type) { write_type(out, type); });
        write_vector<ExternalSymbol>(out, module.imports, [&](const ExternalSymbol &symbol) { write_import(out, symbol); });
        write_scalar<uint32_t>(out, static_cast<uint32_t>(module.exports.size()));
        for (const auto &[name, index] : module.exports)
        {
            write_string(out, name);
            write_scalar<int32_t>(out, index);
        }
        write_string_map(out, module.exportTypeReprs);
        write_vector<BytecodeTraitMetadata>(
            out, module.traitMetadata,
            [&](const BytecodeTraitMetadata &trait) { write_trait_metadata(out, trait); });
        write_vector<BytecodeImplMetadata>(out, module.implMetadata,
                                           [&](const BytecodeImplMetadata &impl) { write_impl_metadata(out, impl); });
    }

    auto read_bytecode_module(const Str &path, const Str &expectedModuleId) -> BytecodeModule
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            throw RuntimeException("Failed to open .ngo artifact for reading: " + path);
        }

        char magic[4]{};
        read_exact_bytes(in, magic, sizeof(magic), sizeof(magic), "magic");
        if (std::memcmp(magic, NGO_MAGIC, sizeof(NGO_MAGIC)) != 0)
        {
            throw RuntimeException("Invalid .ngo artifact magic: " + path);
        }
        auto formatVersion = read_scalar<uint32_t>(in, "formatVersion");
        if (formatVersion != NGO_FORMAT_VERSION)
        {
            throw RuntimeException("Unsupported .ngo format version: " + std::to_string(formatVersion));
        }
        auto abiVersion = read_scalar<uint32_t>(in, "abiVersion");
        if (abiVersion != NGO_ABI_VERSION)
        {
            throw RuntimeException("Unsupported .ngo ABI version: " + std::to_string(abiVersion));
        }
        auto metadataSchemaVersion = read_scalar<uint32_t>(in, "metadataSchemaVersion");
        if (metadataSchemaVersion != NGO_METADATA_SCHEMA_VERSION)
        {
            throw RuntimeException("Unsupported .ngo metadata schema version: " +
                                   std::to_string(metadataSchemaVersion));
        }

        BytecodeModule module;
        module.name = read_string(in, "module.name");
        module.sourceHash = read_string(in, "sourceHash");
        if (!expectedModuleId.empty() && module.name != expectedModuleId)
        {
            throw RuntimeException("Bytecode module id mismatch: " + module.name + ", expected " + expectedModuleId);
        }

        module.constants = read_vector<int64_t>(in, "constants",
                                                [&](const Str &field) { return read_scalar<int64_t>(in, field); });
        module.float_constants = read_vector<double>(in, "float_constants",
                                                     [&](const Str &field) { return read_scalar<double>(in, field); });
        module.strings = read_string_vector(in, "strings");
        module.functions = read_vector<Function>(in, "functions",
                                                 [&](const Str &field) { return read_function(in, field); });
        module.types = read_vector<Type>(in, "types", [&](const Str &field) { return read_type(in, field); });
        module.imports = read_vector<ExternalSymbol>(in, "imports",
                                                     [&](const Str &field) { return read_import(in, field); });
        auto exportCount = read_scalar<uint32_t>(in, "exports.count");
        if (exportCount > MAX_NGO_EXPORTS)
        {
            throw RuntimeException(".ngo exports too large");
        }
        for (uint32_t i = 0; i < exportCount; ++i)
        {
            auto name = read_string(in, "exports[" + std::to_string(i) + "].name");
            auto index = read_scalar<int32_t>(in, "exports[" + std::to_string(i) + "].index");
            module.exports.insert_or_assign(std::move(name), index);
        }
        module.exportTypeReprs = read_string_map(in, "exportTypeReprs");
        module.traitMetadata = read_vector<BytecodeTraitMetadata>(
            in, "traitMetadata", [&](const Str &field) { return read_trait_metadata(in, field); });
        module.implMetadata = read_vector<BytecodeImplMetadata>(
            in, "implMetadata", [&](const Str &field) { return read_impl_metadata(in, field); });
        module.buildIndex();
        return module;
    }

    void BytecodeModule::merge(const BytecodeModule &other, const Str &prefix)
    {
        int32_t constOffset = static_cast<int32_t>(constants.size());
        int32_t strOffset = static_cast<int32_t>(strings.size());
        int32_t funOffset = static_cast<int32_t>(functions.size());
        int32_t typeOffset = static_cast<int32_t>(types.size());

        // Copy constants and strings
        constants.insert(constants.end(), other.constants.begin(), other.constants.end());
        strings.insert(strings.end(), other.strings.begin(), other.strings.end());

        // Copy types
        types.insert(types.end(), other.types.begin(), other.types.end());

        // Copy functions with index remapping
        for (const auto &otherFun : other.functions)
        {
            Function remapped;
            remapped.name = otherFun.name;
            remapped.num_locals = otherFun.num_locals;
            remapped.num_params = otherFun.num_params;
            remapped.code = otherFun.code;

            // Remap operand indices in the bytecode
            auto &code = remapped.code;
            for (size_t i = 0; i < code.size();)
            {
                OpCode op = static_cast<OpCode>(code[i]);
                size_t opStart = i;
                i++; // skip opcode

                // Helper to remap a 16-bit operand
                auto remap_u16 = [&](int32_t offset) {
                    uint16_t idx = static_cast<uint16_t>(code[opStart + offset]) |
                                   (static_cast<uint16_t>(code[opStart + offset + 1]) << 8);
                    idx += static_cast<uint16_t>(strOffset);
                    code[opStart + offset] = static_cast<uint8_t>(idx & 0xFF);
                    code[opStart + offset + 1] = static_cast<uint8_t>((idx >> 8) & 0xFF);
                };

                auto remap_u16_fun = [&](int32_t offset) {
                    uint16_t idx = static_cast<uint16_t>(code[opStart + offset]) |
                                   (static_cast<uint16_t>(code[opStart + offset + 1]) << 8);
                    idx += static_cast<uint16_t>(funOffset);
                    code[opStart + offset] = static_cast<uint8_t>(idx & 0xFF);
                    code[opStart + offset + 1] = static_cast<uint8_t>((idx >> 8) & 0xFF);
                };

                auto remap_u16_const = [&](int32_t offset) {
                    uint16_t idx = static_cast<uint16_t>(code[opStart + offset]) |
                                   (static_cast<uint16_t>(code[opStart + offset + 1]) << 8);
                    idx += static_cast<uint16_t>(constOffset);
                    code[opStart + offset] = static_cast<uint8_t>(idx & 0xFF);
                    code[opStart + offset + 1] = static_cast<uint8_t>((idx >> 8) & 0xFF);
                };

                auto remap_u16_type = [&](int32_t offset) {
                    uint16_t idx = static_cast<uint16_t>(code[opStart + offset]) |
                                   (static_cast<uint16_t>(code[opStart + offset + 1]) << 8);
                    idx += static_cast<uint16_t>(typeOffset);
                    code[opStart + offset] = static_cast<uint8_t>(idx & 0xFF);
                    code[opStart + offset + 1] = static_cast<uint8_t>((idx >> 8) & 0xFF);
                };

                switch (op)
                {
                // Instructions with string index operand
                case OpCode::LOAD_STR:
                    remap_u16(1);
                    i += 2;
                    break;
                case OpCode::INSTANCE_OF:
                    remap_u16(1);
                    i += 2;
                    break;
                case OpCode::SET_PROPERTY:
                    i += 2; // field index, no remap needed
                    break;
                case OpCode::GET_PROPERTY:
                    i += 2; // field index, no remap needed
                    break;
                case OpCode::SET_PROPERTY_STR:
                    remap_u16(1); // string index, needs remap
                    i += 2;
                    break;
                case OpCode::GET_PROPERTY_STR:
                    remap_u16(1); // string index, needs remap
                    i += 2;
                    break;
                case OpCode::INVOKE_MEMBER:
                    remap_u16(1);
                    i += 4; // name + numArgs
                    break;
                case OpCode::NATIVE_CALL:
                    remap_u16(1);
                    i += 4; // name + numArgs
                    break;
                case OpCode::WRAP_NEWTYPE:
                case OpCode::MAKE_TRAIT_REF:
                    remap_u16(1);
                    i += 2;
                    break;

                // Instructions with constant index operand
                case OpCode::LOAD_CONST:
                    remap_u16_const(1);
                    i += 2;
                    break;

                // Instructions with function index operand
                case OpCode::CALL:
                case OpCode::FOLD_MAP_CALL:
                case OpCode::FOLD_FILTER_CALL:
                case OpCode::FOLD_LEFT_CALL:
                case OpCode::FOLD_RIGHT_CALL:
                    remap_u16_fun(1);
                    i += op == OpCode::CALL ? 4 : 2; // CALL has funIndex + numArgs; folds only funIndex.
                    break;
                case OpCode::MAKE_RANGE:
                    i += 1; // inclusive flag
                    break;
                case OpCode::SLICE_RANGE:
                    break;
                case OpCode::CONSTRUCT_TAGGED:
                    remap_u16_type(1);
                    i += 6; // typeIndex + variantIndex + payloadCount
                    break;

                // Instructions with local/global index operand
                case OpCode::LOAD_LOCAL:
                case OpCode::LOAD_PARAM:
                case OpCode::STORE_LOCAL:
                case OpCode::LOAD_GLOBAL:
                case OpCode::STORE_GLOBAL:
                    i += 2;
                    break;

                // Instructions with i32 operand (jump targets - no remap needed)
                case OpCode::JUMP:
                case OpCode::JUMP_IF_FALSE:
                    i += 4;
                    break;

                // Instructions with i64 operand
                case OpCode::PUSH_I64:
                case OpCode::PUSH_U64:
                    i += 8;
                    break;

                // Instructions with i32/f32 operand
                case OpCode::PUSH_I32:
                case OpCode::PUSH_U32:
                case OpCode::PUSH_F32:
                    i += 4;
                    break;

                // Instructions with i16/u16 operand
                case OpCode::PUSH_I16:
                case OpCode::PUSH_U16:
                    i += 2;
                    break;

                // Instructions with single byte operand
                case OpCode::PUSH_I8:
                case OpCode::PUSH_U8:
                case OpCode::PUSH_BOOL:
                    i += 1;
                    break;

                // Instructions with count + flags (spread)
                case OpCode::NEW_TUPLE_SPREAD:
                case OpCode::NEW_ARRAY_SPREAD:
                {
                    uint16_t num = static_cast<uint16_t>(code[i]) | (static_cast<uint16_t>(code[i + 1]) << 8);
                    i += 2 + num; // count + flags
                    break;
                }

                // Instructions with u16 count operand
                case OpCode::NEW_OBJECT:
                case OpCode::NEW_ARRAY:
                case OpCode::NEW_TUPLE:
                case OpCode::PRINT:
                case OpCode::CALL_IMPORT:
                    i += 2;
                    break;

                // Single-byte instructions (no operand)
                default:
                    break;
                }
            }

            addFunction(std::move(remapped));
        }

        // Re-register exports with prefix
        for (const auto &[name, idx] : other.exports)
        {
            Str exportName = prefix.empty() ? name : (prefix + "." + name);
            exports[exportName] = idx + funOffset;
        }
        for (const auto &[name, repr] : other.exportTypeReprs)
        {
            Str exportName = prefix.empty() ? name : (prefix + "." + name);
            exportTypeReprs[exportName] = repr;
        }
        traitMetadata.insert(traitMetadata.end(), other.traitMetadata.begin(), other.traitMetadata.end());
        implMetadata.insert(implMetadata.end(), other.implMetadata.begin(), other.implMetadata.end());
        buildIndex();
    }
} // namespace NG::orgasm
