#pragma once

#include <stdexcept>
#include <type_traits>
#include <concepts>
#include <cstdint>

#include "config.h"
#include <fwd.hpp>

namespace NG
{
    /**
     * @brief Concept for non-copyable types.
     *
     * @tparam T The type to check.
     */
    template <class T>
    concept noncopyable = !std::copyable<T>;

    /**
     * @brief Base struct for non-copyable classes.
     */
    struct NonCopyable
    {
        constexpr NonCopyable() = default;

        NonCopyable(const NonCopyable &noncopyable) = delete;
        auto operator=(const NonCopyable &) -> NonCopyable & = delete;

        NonCopyable(NonCopyable &&noncopyable) = delete;
        auto operator=(NonCopyable &&noncopyable) -> NonCopyable & = delete;

        ~NonCopyable() = default;
    };

    /**
     * @brief Exception thrown during lexical analysis.
     */
    struct LexException : std::logic_error
    {
        explicit LexException() : logic_error("Error: lex exception found")
        {
        }

        explicit LexException(const std::string &msg) : logic_error(msg)
        {
        }
    };

    /**
     * @brief Exception thrown when the end of a file is reached unexpectedly.
     */
    struct EOFException : std::out_of_range
    {
        explicit EOFException() : out_of_range("Error: end of file")
        {
        }
    };

    /**
     * @brief Exception thrown for features that are not yet implemented.
     */
    struct NotImplementedException : std::runtime_error
    {
        explicit NotImplementedException() : runtime_error("Error: not implemented")
        {
        }
        explicit NotImplementedException(const std::string &reason) : runtime_error(reason)
        {
        }
    };

    /**
     * @brief Exception thrown during parsing.
     */
    struct ParseException : std::logic_error
    {
        explicit ParseException(const std::string &message) : logic_error(message)
        {
        }
    };

    /**
     * @brief Exception thrown for illegal type operations.
     */
    struct IllegalTypeException : std::runtime_error
    {
        explicit IllegalTypeException(const std::string &message) : runtime_error(message)
        {
        }
    };

    /**
     * @brief Exception thrown when an assertion fails.
     */
    struct AssertionException : std::logic_error
    {
        explicit AssertionException() : logic_error("Assertion Failed")
        {
        }
    };

    /**
     * @brief Generic runtime exception.
     */
    struct RuntimeException : std::runtime_error
    {
        explicit RuntimeException(const std::string &messge) : runtime_error(messge)
        {
        }
    };

    /**
     * @brief Exception thrown during type checking.
     */
    struct TypeCheckingException : std::logic_error
    {
        explicit TypeCheckingException(const std::string &message) : logic_error(message)
        {
        }
    };

    /**
     * @brief Concept for types that can be converted to a size_t code.
     *
     * @tparam T The type to check.
     */
    template <class T>
    concept codable = std::is_enum_v<T> || std::is_integral_v<T>;

    /**
     * @brief Converts a codable type to its underlying size_t representation.
     *
     * @param enumValue The value to convert.
     * @return The size_t representation of the value.
     */
    constexpr auto code(const codable auto &enumValue) -> size_t
    {
        return static_cast<size_t>(enumValue);
    }

    /**
     * @brief Converts a size_t code back to a codable type.
     *
     * @tparam T The target type.
     * @param code The size_t code to convert.
     * @return The converted value.
     */
    template <codable T>
    auto from_code(size_t code) -> T
    {
        return static_cast<T>(code);
    }

} // namespace NG;
