#pragma once

#include <cstdint>

namespace NG::orgasm
{
    enum class OpCode : uint8_t
    {
        NOP = 0x00,

        // Stack operations
        PUSH_UNIT,
        PUSH_BOOL,
        PUSH_I8,
        PUSH_I16,
        PUSH_I32,
        PUSH_I64,
        PUSH_U8,
        PUSH_U16,
        PUSH_U32,
        PUSH_U64,
        PUSH_F32,
        PUSH_F64,
        PUSH_STR,
        POP,
        DUP,

        // Data operations
        LOAD_PARAM,
        LOAD_LOCAL,
        STORE_LOCAL,
        LOAD_GLOBAL,
        STORE_GLOBAL,
        MAKE_LOCAL_REF,
        MAKE_GLOBAL_REF,
        MAKE_PROPERTY_REF,
        MAKE_PROPERTY_STR_REF,
        MAKE_INDEX_REF,
        MAKE_TRAIT_REF,
        LOAD_REF,
        STORE_REF,
        MOVE_LOCAL,
        MOVE_GLOBAL,
        MOVE_REF,
        LOAD_CONST,
        LOAD_STR,

        // Arithmetic (generic — dispatch by runtime type)
        ADD,
        SUB,
        MUL,
        DIV,
        MOD,
        NEG,

        // Comparison (generic — dispatch by runtime type)
        EQ,
        LT,
        GT,
        NOT,
        INSTANCE_OF,

        // Control flow
        JUMP,
        JUMP_IF_FALSE,
        CALL,
        CALL_IMPORT,
        RETURN,

        // Object/Array/Tuple
        NEW_OBJECT,
        GET_PROPERTY,       // By field index (O(1), value semantics)
        SET_PROPERTY,       // By field index (O(1), value semantics)
        GET_PROPERTY_STR,   // By string name (dynamic lookup)
        SET_PROPERTY_STR,   // By string name (dynamic lookup)
        NEW_ARRAY,
        GET_INDEX,
        SET_INDEX,
        NEW_TUPLE,
        GET_TUPLE_ITEM,
        INVOKE_MEMBER,
        NEW_TUPLE_SPREAD,
        NEW_ARRAY_SPREAD,
        GET_TUPLE_REST,
        FOLD_MAP_CALL,
        FOLD_FILTER_CALL,
        FOLD_LEFT_CALL,
        FOLD_RIGHT_CALL,
        MAKE_RANGE,
        SLICE_RANGE,

        // Native
        NATIVE_CALL,
        PRINT,
        ASSERT,

        // Bitwise/Shift (Added late)
        LSHIFT,
        RSHIFT,

        // Newtype
        WRAP_NEWTYPE,
        UNWRAP_NEWTYPE,

        // Tagged Union
        CONSTRUCT_TAGGED,   // CONSTRUCT_TAGGED type_idx variant_idx num_payload — construct tagged value
        GET_TAG,            // GET_TAG — push variant index from tagged value on stack
        GET_PAYLOAD,        // GET_PAYLOAD field_idx — push payload[field_idx] from tagged value on stack
        SWITCH_TAG,         // SWITCH_TAG count [tag0:addr] [tag1:addr] ... — jump based on variant tag

        HALT = 0xFF
    };
} // namespace NG::orgasm
