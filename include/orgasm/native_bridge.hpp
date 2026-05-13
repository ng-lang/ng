#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <runtime/value_access.hpp>
#include <functional>
#include <tuple>
#include <utility>

namespace NG::orgasm
{
    using namespace NG::runtime;

    // --- Type conversion: NG runtime -> C++ ---

    template <typename T>
    auto from_ng(const RuntimeRef<StorageCell> &cell) -> T;

    template <>
    inline auto from_ng<int8_t>(const RuntimeRef<StorageCell> &cell) -> int8_t
    {
        return static_cast<int8_t>(read_numeric_cell_as<int64_t>(cell));
    }

    template <>
    inline auto from_ng<int16_t>(const RuntimeRef<StorageCell> &cell) -> int16_t
    {
        return static_cast<int16_t>(read_numeric_cell_as<int64_t>(cell));
    }

    template <>
    inline auto from_ng<int32_t>(const RuntimeRef<StorageCell> &cell) -> int32_t
    {
        return read_numeric_cell_as<int32_t>(cell);
    }

    template <>
    inline auto from_ng<int64_t>(const RuntimeRef<StorageCell> &cell) -> int64_t
    {
        return read_numeric_cell_as<int64_t>(cell);
    }

    template <>
    inline auto from_ng<uint8_t>(const RuntimeRef<StorageCell> &cell) -> uint8_t
    {
        return static_cast<uint8_t>(read_numeric_cell_as<uint64_t>(cell));
    }

    template <>
    inline auto from_ng<uint16_t>(const RuntimeRef<StorageCell> &cell) -> uint16_t
    {
        return static_cast<uint16_t>(read_numeric_cell_as<uint64_t>(cell));
    }

    template <>
    inline auto from_ng<uint32_t>(const RuntimeRef<StorageCell> &cell) -> uint32_t
    {
        return read_numeric_cell_as<uint32_t>(cell);
    }

    template <>
    inline auto from_ng<uint64_t>(const RuntimeRef<StorageCell> &cell) -> uint64_t
    {
        return read_numeric_cell_as<uint64_t>(cell);
    }

    template <>
    inline auto from_ng<float>(const RuntimeRef<StorageCell> &cell) -> float
    {
        return read_numeric_cell_as<float>(cell);
    }

    template <>
    inline auto from_ng<double>(const RuntimeRef<StorageCell> &cell) -> double
    {
        return read_numeric_cell_as<double>(cell);
    }

    template <>
    inline auto from_ng<bool>(const RuntimeRef<StorageCell> &cell) -> bool
    {
        return runtime_value_bool(cell);
    }

    template <>
    inline auto from_ng<Str>(const RuntimeRef<StorageCell> &cell) -> Str
    {
        return runtime_string_value(cell);
    }

    template <>
    inline auto from_ng<RuntimeRef<StorageCell>>(const RuntimeRef<StorageCell> &cell) -> RuntimeRef<StorageCell>
    {
        return cell;
    }

    // --- Type conversion: C++ -> NG runtime ---

    inline auto to_ng(int8_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<int8_t>(val); }
    inline auto to_ng(int16_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<int16_t>(val); }
    inline auto to_ng(int32_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<int32_t>(val); }
    inline auto to_ng(int64_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<int64_t>(val); }
    inline auto to_ng(uint8_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<uint8_t>(val); }
    inline auto to_ng(uint16_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<uint16_t>(val); }
    inline auto to_ng(uint32_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<uint32_t>(val); }
    inline auto to_ng(uint64_t val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<uint64_t>(val); }
    inline auto to_ng(float val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<float>(val); }
    inline auto to_ng(double val) -> RuntimeRef<StorageCell> { return numeral_cell_from_value<double>(val); }
    inline auto to_ng(bool val) -> RuntimeRef<StorageCell> { return make_runtime_boolean(val); }
    inline auto to_ng(const Str &val) -> RuntimeRef<StorageCell> { return make_runtime_string(val); }
    inline auto to_ng(Str &&val) -> RuntimeRef<StorageCell> { return make_runtime_string(std::move(val)); }
    inline auto to_ng(const char *val) -> RuntimeRef<StorageCell> { return make_runtime_string(Str(val)); }
    inline auto to_ng(RuntimeRef<StorageCell> val) -> RuntimeRef<StorageCell> { return std::move(val); }

    // --- Argument extraction from stack ---

    template <typename Tuple, std::size_t... I>
    auto extract_args_impl(const Vec<RuntimeRef<StorageCell>> &args, std::index_sequence<I...>) -> Tuple
    {
        return std::make_tuple(from_ng<std::tuple_element_t<I, Tuple>>(args[I])...);
    }

    template <typename... Args>
    auto extract_args(const Vec<RuntimeRef<StorageCell>> &args) -> std::tuple<Args...>
    {
        return extract_args_impl<std::tuple<Args...>>(args, std::index_sequence_for<Args...>{});
    }

    // --- Native function wrapper ---

    template <typename Ret, typename... Args>
    auto wrap_native(Ret (*func)(Args...)) -> std::function<RuntimeRef<StorageCell>(const Vec<RuntimeRef<StorageCell>> &)>
    {
        return [func](const Vec<RuntimeRef<StorageCell>> &args) -> RuntimeRef<StorageCell> {
            auto tup = extract_args<Args...>(args);
            if constexpr (std::is_void_v<Ret>) {
                std::apply(func, std::move(tup));
                return unit_cell();
            } else {
                Ret result = std::apply(func, std::move(tup));
                return to_ng(std::move(result));
            }
        };
    }

    // Helper for lambda/functor wrapping
    template <typename Func, typename Ret, typename... Args>
    auto wrap_native_call(Func &f, const Vec<RuntimeRef<StorageCell>> &args, Ret (Func::*)(Args...) const)
        -> RuntimeRef<StorageCell>
    {
        auto tup = extract_args<Args...>(args);
        if constexpr (std::is_void_v<Ret>) {
            std::apply(f, std::move(tup));
            return unit_cell();
        } else {
            Ret result = std::apply(f, std::move(tup));
            return to_ng(std::move(result));
        }
    }

    template <typename Func, typename Ret, typename... Args>
    auto wrap_native_call(Func &f, const Vec<RuntimeRef<StorageCell>> &args, Ret (Func::*)(Args...))
        -> RuntimeRef<StorageCell>
    {
        auto tup = extract_args<Args...>(args);
        if constexpr (std::is_void_v<Ret>) {
            std::apply(f, std::move(tup));
            return unit_cell();
        } else {
            Ret result = std::apply(f, std::move(tup));
            return to_ng(std::move(result));
        }
    }

    template <typename Func, typename Ret, typename Class, typename... Args>
    auto wrap_native_impl(Func &f, const Vec<RuntimeRef<StorageCell>> &args, Ret (Class::*)(Args...) const)
        -> RuntimeRef<StorageCell>
    {
        return wrap_native_call(f, args, &Func::operator());
    }

    template <typename Func, typename Ret, typename Class, typename... Args>
    auto wrap_native_impl(Func &f, const Vec<RuntimeRef<StorageCell>> &args, Ret (Class::*)(Args...))
        -> RuntimeRef<StorageCell>
    {
        return wrap_native_call(f, args, &Func::operator());
    }

    template <typename Func>
    auto wrap_native(Func func) -> std::function<RuntimeRef<StorageCell>(const Vec<RuntimeRef<StorageCell>> &)>
    {
        return [f = std::move(func)](const Vec<RuntimeRef<StorageCell>> &args) mutable -> RuntimeRef<StorageCell> {
            using Traits = decltype(&Func::operator());
            return wrap_native_impl(f, args, Traits{});
        };
    }

    template <typename Func>
    auto wrap_native_callable(Func func) -> NGCallable
    {
        auto wrapped = wrap_native(std::move(func));
        return [wrapped = std::move(wrapped)](const NGSelf &, const NGEnv &, const NGArgs &args) mutable
                   -> RuntimeRef<StorageCell> {
            return wrapped(args);
        };
    }

} // namespace NG::orgasm
