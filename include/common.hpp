#pragma once

#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <type_traits>

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
     * @brief The position of a token in the source code.
     */
    struct TokenPosition
    {
        size_t line = 0; ///< The line number.
        size_t col = 0;  ///< The column number.
    };

    /**
     * @brief Unified base exception for all NG compiler/runtime errors.
     */
    struct NGException : std::runtime_error
    {
        using std::runtime_error::runtime_error;
    };

    /**
     * @brief Exception thrown during lexical analysis.
     */
    struct LexException : NGException
    {
        explicit LexException() : NGException("Error: lex exception found") {}
        explicit LexException(const std::string &msg) : NGException(msg) {}
    };

    /**
     * @brief Exception thrown during parsing.
     */
    struct ParseException : NGException
    {
        TokenPosition pos;
        explicit ParseException(const std::string &message, TokenPosition pos = {}) : NGException(message), pos(pos) {}
    };

    /**
     * @brief Exception thrown when the end of a file is reached unexpectedly.
     */
    struct EOFException : ParseException
    {
        explicit EOFException() : ParseException("Unexpected end of file") {}
    };

    /**
     * @brief Exception thrown during type checking.
     */
    struct TypeCheckingException : NGException
    {
        TokenPosition pos;
        explicit TypeCheckingException(const std::string &message, TokenPosition pos = {})
            : NGException(message), pos(pos) {}
    };

    /**
     * @brief Generic runtime exception.
     */
    struct RuntimeException : NGException
    {
        TokenPosition pos;
        explicit RuntimeException(const std::string &message, TokenPosition pos = {}) : NGException(message), pos(pos) {}
    };

    struct SequenceCompatibilityException : RuntimeException
    {
        explicit SequenceCompatibilityException(const std::string &message = "Expected Sequence-compatible runtime value")
            : RuntimeException(message) {}
    };

    /**
     * @brief Exception thrown when an assertion fails.
     */
    struct AssertionException : RuntimeException
    {
        explicit AssertionException() : RuntimeException("Assertion Failed") {}
    };

    /**
     * @brief Exception thrown for features that are not yet implemented.
     */
    struct NotImplementedException : RuntimeException
    {
        explicit NotImplementedException() : RuntimeException("Error: not implemented") {}
        explicit NotImplementedException(const std::string &reason) : RuntimeException(reason) {}
    };

    /**
     * @brief Exception thrown for illegal type operations.
     */
    struct IllegalTypeException : RuntimeException
    {
        explicit IllegalTypeException(const std::string &message) : RuntimeException(message) {}
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
    constexpr auto code(const codable auto &enumValue) noexcept -> size_t
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
    constexpr auto from_code(size_t code) noexcept -> T
    {
        return static_cast<T>(code);
    }

} // namespace NG
