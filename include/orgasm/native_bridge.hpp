#pragma once

#include <intp/runtime.hpp>
#include <intp/runtime_numerals.hpp>
#include <functional>
#include <tuple>
#include <utility>

namespace NG::orgasm
{
    using namespace NG::runtime;

    // --- Type conversion: NG runtime -> C++ ---

    template <typename T>
    auto from_ng(const RuntimeRef<NGObject> &obj) -> T;

    template <>
    inline auto from_ng<int8_t>(const RuntimeRef<NGObject> &obj) -> int8_t
    {
        return static_cast<int8_t>(NGIntegral<int8_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<int16_t>(const RuntimeRef<NGObject> &obj) -> int16_t
    {
        return static_cast<int16_t>(NGIntegral<int16_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<int32_t>(const RuntimeRef<NGObject> &obj) -> int32_t
    {
        return NGIntegral<int32_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get()));
    }

    template <>
    inline auto from_ng<int64_t>(const RuntimeRef<NGObject> &obj) -> int64_t
    {
        return NGIntegral<int64_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get()));
    }

    template <>
    inline auto from_ng<uint8_t>(const RuntimeRef<NGObject> &obj) -> uint8_t
    {
        return static_cast<uint8_t>(NGIntegral<uint8_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<uint16_t>(const RuntimeRef<NGObject> &obj) -> uint16_t
    {
        return static_cast<uint16_t>(NGIntegral<uint16_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<uint32_t>(const RuntimeRef<NGObject> &obj) -> uint32_t
    {
        return static_cast<uint32_t>(NGIntegral<uint32_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<uint64_t>(const RuntimeRef<NGObject> &obj) -> uint64_t
    {
        return static_cast<uint64_t>(NGIntegral<uint64_t>::valueOf(dynamic_cast<NumeralBase *>(obj.get())));
    }

    template <>
    inline auto from_ng<float>(const RuntimeRef<NGObject> &obj) -> float
    {
        return NGFloatingPoint<float>::valueOf(dynamic_cast<NumeralBase *>(obj.get()));
    }

    template <>
    inline auto from_ng<double>(const RuntimeRef<NGObject> &obj) -> double
    {
        return NGFloatingPoint<double>::valueOf(dynamic_cast<NumeralBase *>(obj.get()));
    }

    template <>
    inline auto from_ng<bool>(const RuntimeRef<NGObject> &obj) -> bool
    {
        return obj->boolValue();
    }

    template <>
    inline auto from_ng<Str>(const RuntimeRef<NGObject> &obj) -> Str
    {
        return std::dynamic_pointer_cast<NGString>(obj)->value;
    }

    // --- Type conversion: C++ -> NG runtime ---

    inline auto to_ng(int8_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<int8_t>>(val); }
    inline auto to_ng(int16_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<int16_t>>(val); }
    inline auto to_ng(int32_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<int32_t>>(val); }
    inline auto to_ng(int64_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<int64_t>>(val); }
    inline auto to_ng(uint8_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<uint8_t>>(val); }
    inline auto to_ng(uint16_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<uint16_t>>(val); }
    inline auto to_ng(uint32_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<uint32_t>>(val); }
    inline auto to_ng(uint64_t val) -> RuntimeRef<NGObject> { return makert<NGIntegral<uint64_t>>(val); }
    inline auto to_ng(float val) -> RuntimeRef<NGObject> { return makert<NGFloatingPoint<float>>(val); }
    inline auto to_ng(double val) -> RuntimeRef<NGObject> { return makert<NGFloatingPoint<double>>(val); }
    inline auto to_ng(bool val) -> RuntimeRef<NGObject> { return NGObject::boolean(val); }
    inline auto to_ng(const Str &val) -> RuntimeRef<NGObject> { return makert<NGString>(val); }
    inline auto to_ng(Str &&val) -> RuntimeRef<NGObject> { return makert<NGString>(std::move(val)); }
    inline auto to_ng(const char *val) -> RuntimeRef<NGObject> { return makert<NGString>(Str(val)); }
    inline auto to_ng(RuntimeRef<NGObject> val) -> RuntimeRef<NGObject> { return std::move(val); }

    // --- Argument extraction from stack ---

    template <typename Tuple, std::size_t... I>
    auto extract_args_impl(const Vec<RuntimeRef<NGObject>> &args, std::index_sequence<I...>) -> Tuple
    {
        return std::make_tuple(from_ng<std::tuple_element_t<I, Tuple>>(args[I])...);
    }

    template <typename... Args>
    auto extract_args(const Vec<RuntimeRef<NGObject>> &args) -> std::tuple<Args...>
    {
        return extract_args_impl<std::tuple<Args...>>(args, std::index_sequence_for<Args...>{});
    }

    // --- Native function wrapper ---

    template <typename Ret, typename... Args>
    auto wrap_native(Ret (*func)(Args...)) -> std::function<RuntimeRef<NGObject>(const Vec<RuntimeRef<NGObject>> &)>
    {
        return [func](const Vec<RuntimeRef<NGObject>> &args) -> RuntimeRef<NGObject> {
            auto tup = extract_args<Args...>(args);
            if constexpr (std::is_void_v<Ret>) {
                std::apply(func, std::move(tup));
                return makert<NGUnit>();
            } else {
                Ret result = std::apply(func, std::move(tup));
                return to_ng(std::move(result));
            }
        };
    }

    // Helper for lambda/functor wrapping
    template <typename Func, typename Ret, typename... Args>
    auto wrap_native_call(Func &f, const Vec<RuntimeRef<NGObject>> &args, Ret (Func::*)(Args...) const) -> RuntimeRef<NGObject>
    {
        auto tup = extract_args<Args...>(args);
        if constexpr (std::is_void_v<Ret>) {
            std::apply(f, std::move(tup));
            return makert<NGUnit>();
        } else {
            Ret result = std::apply(f, std::move(tup));
            return to_ng(std::move(result));
        }
    }

    template <typename Func, typename Ret, typename... Args>
    auto wrap_native_call(Func &f, const Vec<RuntimeRef<NGObject>> &args, Ret (Func::*)(Args...)) -> RuntimeRef<NGObject>
    {
        auto tup = extract_args<Args...>(args);
        if constexpr (std::is_void_v<Ret>) {
            std::apply(f, std::move(tup));
            return makert<NGUnit>();
        } else {
            Ret result = std::apply(f, std::move(tup));
            return to_ng(std::move(result));
        }
    }

    template <typename Func, typename Ret, typename Class, typename... Args>
    auto wrap_native_impl(Func &f, const Vec<RuntimeRef<NGObject>> &args, Ret (Class::*)(Args...) const) -> RuntimeRef<NGObject>
    {
        return wrap_native_call(f, args, &Func::operator());
    }

    template <typename Func, typename Ret, typename Class, typename... Args>
    auto wrap_native_impl(Func &f, const Vec<RuntimeRef<NGObject>> &args, Ret (Class::*)(Args...)) -> RuntimeRef<NGObject>
    {
        return wrap_native_call(f, args, &Func::operator());
    }

    template <typename Func>
    auto wrap_native(Func func) -> std::function<RuntimeRef<NGObject>(const Vec<RuntimeRef<NGObject>> &)>
    {
        return [f = std::move(func)](const Vec<RuntimeRef<NGObject>> &args) mutable -> RuntimeRef<NGObject> {
            using Traits = decltype(&Func::operator());
            return wrap_native_impl(f, args, Traits{});
        };
    }

} // namespace NG::orgasm
