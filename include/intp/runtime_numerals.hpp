#pragma once

#include <array>
#include <bit>
#include <cmath>
#include <intp/runtime.hpp>
#include <limits>
#include <type_traits>

namespace NG::runtime
{
    // Overflow-checked arithmetic for signed integers.
    // For unsigned types, standard wrapping semantics are preserved.
    // For floating-point types, standard IEEE semantics are preserved.
    template <class T>
    inline auto checked_add(T a, T b) -> T
    {
        if constexpr (std::integral<T> && std::is_signed_v<T>)
        {
            T result;
            if (__builtin_add_overflow(a, b, &result))
            {
                throw RuntimeException("Integer overflow in addition");
            }
            return result;
        }
        else
        {
            return static_cast<T>(a + b);
        }
    }

    template <class T>
    inline auto checked_sub(T a, T b) -> T
    {
        if constexpr (std::integral<T> && std::is_signed_v<T>)
        {
            T result;
            if (__builtin_sub_overflow(a, b, &result))
            {
                throw RuntimeException("Integer overflow in subtraction");
            }
            return result;
        }
        else
        {
            return static_cast<T>(a - b);
        }
    }

    template <class T>
    inline auto checked_mul(T a, T b) -> T
    {
        if constexpr (std::integral<T> && std::is_signed_v<T>)
        {
            T result;
            if (__builtin_mul_overflow(a, b, &result))
            {
                throw RuntimeException("Integer overflow in multiplication");
            }
            return result;
        }
        else
        {
            return static_cast<T>(a * b);
        }
    }

    template <class T>
    inline auto checked_negate(T a) -> T
    {
        static_assert(std::is_signed_v<T>, "checked_negate only for signed types");
        if (a == std::numeric_limits<T>::min())
        {
            throw RuntimeException("Integer overflow in negation");
        }
        return static_cast<T>(-a);
    }
    template <class T>
    inline void write_inline_cell_bytes(const RuntimeRef<StorageCell> &cell, T value)
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (!cell)
        {
            throw RuntimeException("Cannot write null storage cell");
        }
        auto bytes = std::bit_cast<std::array<uint8_t, sizeof(T)>>(value);
        cell->bytes.assign(bytes.begin(), bytes.end());
    }

    template <class T>
    [[nodiscard]] inline auto read_inline_cell_bytes(const RuntimeRef<StorageCell> &cell) -> T
    {
        static_assert(std::is_trivially_copyable_v<T>);
        if (!cell || cell->bytes.size() < sizeof(T))
        {
            throw RuntimeException("Storage cell does not contain the requested inline value");
        }
        std::array<uint8_t, sizeof(T)> bytes{};
        std::copy_n(cell->bytes.begin(), bytes.size(), bytes.begin());
        return std::bit_cast<T>(bytes);
    }

    template <class T>
    [[nodiscard]] inline auto numeral_type_name() -> Str
    {
        if constexpr (std::same_as<T, int8_t>) return "i8";
        else if constexpr (std::same_as<T, uint8_t>) return "u8";
        else if constexpr (std::same_as<T, int16_t>) return "i16";
        else if constexpr (std::same_as<T, uint16_t>) return "u16";
        else if constexpr (std::same_as<T, int32_t>) return "i32";
        else if constexpr (std::same_as<T, uint32_t>) return "u32";
        else if constexpr (std::same_as<T, int64_t>) return "i64";
        else if constexpr (std::same_as<T, uint64_t>) return "u64";
        else if constexpr (std::same_as<T, float>) return "f32";
        else if constexpr (std::same_as<T, double>) return "f64";
        else static_assert(sizeof(T) == 0, "Unsupported numeral type");
    }

    template <class T>
    [[nodiscard]] inline auto numeral_type_layout() -> TypeLayout
    {
        return TypeLayout{
            .name = numeral_type_name<T>(),
            .kind = LayoutKind::INLINE_VALUE,
            .size = sizeof(T),
            .alignment = alignof(T),
            .containsPointers = false,
            .triviallyCopyable = true,
            .triviallyMovable = true,
        };
    }

    template <class T>
    [[nodiscard]] inline auto read_numeric_cell_as(const RuntimeRef<StorageCell> &cell) -> T
    {
        auto type = cell ? cell->runtimeType : nullptr;
        auto name = type ? type->name : Str{};
        if (name == "i8") return static_cast<T>(read_inline_cell_bytes<int8_t>(cell));
        if (name == "u8") return static_cast<T>(read_inline_cell_bytes<uint8_t>(cell));
        if (name == "i16") return static_cast<T>(read_inline_cell_bytes<int16_t>(cell));
        if (name == "u16") return static_cast<T>(read_inline_cell_bytes<uint16_t>(cell));
        if (name == "i32" || name == "int") return static_cast<T>(read_inline_cell_bytes<int32_t>(cell));
        if (name == "u32" || name == "uint") return static_cast<T>(read_inline_cell_bytes<uint32_t>(cell));
        if (name == "i64") return static_cast<T>(read_inline_cell_bytes<int64_t>(cell));
        if (name == "u64") return static_cast<T>(read_inline_cell_bytes<uint64_t>(cell));
        if (name == "f32" || name == "float") return static_cast<T>(read_inline_cell_bytes<float>(cell));
        if (name == "f64" || name == "double") return static_cast<T>(read_inline_cell_bytes<double>(cell));
        throw RuntimeException("Not a buffered numeral cell");
    }

    template <class T>
    [[nodiscard]] inline auto numeral_runtime_type() -> RuntimeRef<NGType>;

    template <class T>
    [[nodiscard]] inline auto numeral_cell_from_value(T value) -> RuntimeRef<StorageCell>
    {
        auto type = numeral_runtime_type<T>();
        auto cell = make_storage_cell(type->layout, StorageClass::TEMPORARY, {}, type);
        write_inline_cell_bytes<T>(cell, value);
        cell->initialized = true;
        return cell;
    }

    [[nodiscard]] inline auto negate_numeric_cell(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
    {
        auto type = cell ? cell->runtimeType : nullptr;
        auto name = type ? type->name : Str{};
        if (name == "i8") return numeral_cell_from_value<int8_t>(checked_negate(read_inline_cell_bytes<int8_t>(cell)));
        if (name == "i16") return numeral_cell_from_value<int16_t>(checked_negate(read_inline_cell_bytes<int16_t>(cell)));
        if (name == "i32" || name == "int") return numeral_cell_from_value<int32_t>(checked_negate(read_inline_cell_bytes<int32_t>(cell)));
        if (name == "i64") return numeral_cell_from_value<int64_t>(checked_negate(read_inline_cell_bytes<int64_t>(cell)));
        if (name == "f32" || name == "float") return numeral_cell_from_value<float>(-read_inline_cell_bytes<float>(cell));
        if (name == "f64" || name == "double") return numeral_cell_from_value<double>(-read_inline_cell_bytes<double>(cell));
        if (name == "u8" || name == "u16" || name == "u32" || name == "uint" || name == "u64")
        {
            throw RuntimeException("Cannot negate unsigned integers");
        }
        throw RuntimeException("Cannot negate a non-number");
    }

    template <class T>
    [[nodiscard]] inline auto numeral_runtime_type() -> RuntimeRef<NGType>
    {
        static auto type = makert<NGType>(NGType{
            .name = numeral_type_name<T>(),
            .layout = numeral_type_layout<T>(),
            .showCellHandler =
                [](const RuntimeRef<StorageCell> &cell) {
                    return std::to_string(read_inline_cell_bytes<T>(cell));
                },
            .boolCellHandler =
                [](const RuntimeRef<StorageCell> &cell) {
                    return read_inline_cell_bytes<T>(cell) != 0;
                },
            .cellBinaryOperators =
                {
                    {RuntimeBinaryOperator::Add,
                     [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                         if constexpr (std::integral<T>)
                         {
                             auto otherType = other ? other->runtimeType : nullptr;
                             if (otherType && otherType->name == "String")
                             {
                                 return make_runtime_string(std::to_string(read_inline_cell_bytes<T>(self)) +
                                                            runtime_value_show(other));
                             }
                         }
                         return numeral_cell_from_value<T>(checked_add(read_inline_cell_bytes<T>(self), read_numeric_cell_as<T>(other)));
                     }},
                    {RuntimeBinaryOperator::Subtract,
                     [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                         return numeral_cell_from_value<T>(checked_sub(read_inline_cell_bytes<T>(self), read_numeric_cell_as<T>(other)));
                     }},
                    {RuntimeBinaryOperator::Multiply,
                     [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                         return numeral_cell_from_value<T>(checked_mul(read_inline_cell_bytes<T>(self), read_numeric_cell_as<T>(other)));
                     }},
                    {RuntimeBinaryOperator::Divide,
                     [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                         auto divisor = read_numeric_cell_as<T>(other);
                         if (divisor == 0)
                         {
                             throw RuntimeException("Division by zero");
                         }
                         return numeral_cell_from_value<T>(read_inline_cell_bytes<T>(self) / divisor);
                     }},
                    {RuntimeBinaryOperator::Modulus,
                     [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) -> RuntimeRef<StorageCell> {
                         if constexpr (std::floating_point<T>)
                         {
                             throw std::logic_error("floating point not support modulus operation");
                         }
                         else
                         {
                             auto divisor = read_numeric_cell_as<T>(other);
                             if (divisor == 0)
                             {
                                 throw RuntimeException("Modulus by zero");
                             }
                             return numeral_cell_from_value<T>(read_inline_cell_bytes<T>(self) % divisor);
                         }
                     }},
                },
            .cellOrderHandler =
                [](const RuntimeRef<StorageCell> &self, const RuntimeRef<StorageCell> &other) {
                    auto leftValue = read_inline_cell_bytes<T>(self);
                    auto rightValue = read_numeric_cell_as<T>(other);
                    if constexpr (std::floating_point<T>)
                    {
                        if (std::isnan(leftValue) || std::isnan(rightValue)) return Orders::UNORDERED;
                    }
                    if (leftValue < rightValue) return Orders::LT;
                    if (leftValue > rightValue) return Orders::GT;
                    return Orders::EQ;
                },
        });
        return type;
    }

} // namespace NG::runtime
