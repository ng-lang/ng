#include <orgasm/module.hpp>
#include <cstring>

namespace NG::orgasm
{
    void BytecodeModule::merge(const BytecodeModule &other, const Str &prefix)
    {
        int32_t constOffset = static_cast<int32_t>(constants.size());
        int32_t strOffset = static_cast<int32_t>(strings.size());
        int32_t funOffset = static_cast<int32_t>(functions.size());

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
                    remap_u16_fun(1);
                    i += 4; // funIndex + numArgs
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

            functions.push_back(std::move(remapped));
        }

        // Re-register exports with prefix
        for (const auto &[name, idx] : other.exports)
        {
            Str exportName = prefix.empty() ? name : (prefix + "." + name);
            exports[exportName] = idx + funOffset;
        }
    }
} // namespace NG::orgasm
