#pragma once

#include <stdexcept>
#include <type_traits>
#include <concepts>
#include <cstdint>

#include "config.h"
#include <fwd.hpp>

namespace NG
{

    template <class T>
    concept noncopyable = !std::copyable<T>;

    struct NonCopyable
    {
        constexpr NonCopyable() = default;

        NonCopyable(const NonCopyable &noncopyable) = delete;
        auto operator=(const NonCopyable &) -> NonCopyable & = delete;

        NonCopyable(NonCopyable &&noncopyable) = delete;
        auto operator=(NonCopyable &&noncopyable) -> NonCopyable & = delete;

        ~NonCopyable() = default;
    };

    struct LexException : std::logic_error
    {
        explicit LexException() : logic_error("Error: lex exception found")
        {
        }

        explicit LexException(const std::string &msg) : logic_error(msg)
        {
        }
    };

    struct EOFException : std::out_of_range
    {
        explicit EOFException() : out_of_range("Error: end of file")
        {
        }
    };

    struct NotImplementedException : std::runtime_error
    {
        explicit NotImplementedException() : runtime_error("Error: not implemented")
        {
        }
        explicit NotImplementedException(const std::string &reason) : runtime_error(reason)
        {
        }
    };

    struct ParseException : std::logic_error
    {
        explicit ParseException(const std::string &message) : logic_error(message)
        {
        }
    };

    struct IllegalTypeException : std::runtime_error
    {
        explicit IllegalTypeException(const std::string &message) : runtime_error(message)
        {
        }
    };

    struct AssertionException : std::logic_error
    {
        explicit AssertionException() : logic_error("Assertion Failed")
        {
        }
    };

    struct RuntimeException : std::runtime_error
    {
        explicit RuntimeException(const std::string &messge) : runtime_error(messge)
        {
        }
    };

    struct TypeCheckingException : std::logic_error
    {
        explicit TypeCheckingException(const std::string &message) : logic_error(message)
        {
        }
    };

    template <class T>
    concept codable = std::is_enum_v<T> || std::is_integral_v<T>;

    constexpr auto code(const codable auto &enumValue) -> size_t
    {
        return static_cast<size_t>(enumValue);
    }

    template <codable T>
    auto from_code(size_t code) -> T
    {
        return static_cast<T>(code);
    }

} // namespace NG;
