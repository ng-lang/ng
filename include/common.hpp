
#ifndef __NG_COMMON_HPP
#define __NG_COMMON_HPP

#include <expected>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <concepts>

#include "config.h"
#include <fwd.hpp>

namespace NG {

    template<class T>
    using ParseResult = std::expected<T, std::string>;

    template<class T>
    concept noncopyable = !std::copyable<T>;

    struct NonCopyable {
        NonCopyable();

        NonCopyable(const NonCopyable &noncopyable) = delete;
        NonCopyable(const NonCopyable &&noncopyable) = delete;

        NonCopyable &operator=(const NonCopyable &) = delete;
        NonCopyable &operator=(const NonCopyable &&noncopyable) = delete;

        virtual ~NonCopyable() = 0;
    };

    struct LexException : std::logic_error {
        explicit LexException() : logic_error("Error: lex exception found") {
        }

        explicit LexException(const char *msg) : logic_error(msg) {
        }
    };

    struct EOFException : std::out_of_range {
        explicit EOFException() : out_of_range("Error: end of file") {
        }
    };

    struct NotImplementedException : std::runtime_error {
        explicit NotImplementedException() : runtime_error("Error: not implemented") {
        }
    };

    struct ParseException : std::logic_error {
        explicit ParseException(const std::string &message) : logic_error(message) {
        }
    };

    struct IllegalTypeException : std::runtime_error {
        explicit IllegalTypeException(const std::string &message) : runtime_error(message) {
        }
    };

    struct AssertionException : std::logic_error {
        explicit AssertionException() : logic_error("Assertion Failed") {
        }
    };

    struct RuntimeException : std::runtime_error {
        explicit RuntimeException(const std::string &messge) : runtime_error(messge) {
        }
    };

    template<class T>
    concept codable = std::is_enum_v<T> || std::is_integral_v<T>;

    uintptr_t code(const codable auto &t) {
        return static_cast<uintptr_t>(t);
    }

    template<codable T>
    T from_code(uintptr_t code) {
        return static_cast<T>(code);
    }

} // namespace NG;

#endif
