
#ifndef __NG_COMMON_HPP
#define __NG_COMMON_HPP

#include <stdexcept>
#include <string>

namespace NG {

    struct NonCopyable {
        NonCopyable();

        NonCopyable(NonCopyable &noncopyable) = delete;

        NonCopyable &operator=(NonCopyable &) = delete;

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

    template<class T>
    uintptr_t code(T &&t) {
        return static_cast<uintptr_t>(t);
    }

} // namespace NG;

#endif
