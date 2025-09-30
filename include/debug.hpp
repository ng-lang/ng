#pragma once

#include <config.h>
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <fwd.hpp>
#include <iostream>
#include <iterator>

/**
 * @brief Prints a non-container value for debugging.
 *
 * This function is only enabled when `NG_CONFIG_ENABLE_DEBUG_LOG` is defined.
 *
 * @tparam T The type of the value.
 * @param value The value to print.
 */
template <class T>
    requires NonContainer<T>
inline void show(T &&value)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::cout << "[DEBUG] -- " << std::forward<T>(value) << std::endl;
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}

/**
 * @brief Prints a container for debugging.
 *
 * This function is only enabled when `NG_CONFIG_ENABLE_DEBUG_LOG` is defined.
 *
 * @param value The container to print.
 */
inline void show(Container auto &&value)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::cout << "[DEBUG] -- Container[";
    for (auto &&x : value)
    {
        show(x);
    }
    std::cout << "];" << std::endl;
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}
/**
 * @brief Prints a pair for debugging.
 *
 * This function is only enabled when `NG_CONFIG_ENABLE_DEBUG_LOG` is defined.
 *
 * @tparam A The type of pair's first value.
 * @tparam B The type of pair's second value.
 * @param p The pair to print.
 */
template <class A, class B>
inline void show(const std::pair<A, B> &p)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::cout << "[DEBUG] -- {" << p.first << ", " << p.second << "}" << std::endl;
#endif
}
/**
 * @brief Logs debug messages.
 *
 * This function is only enabled when `NG_CONFIG_ENABLE_DEBUG_LOG` is defined.
 * It prints a header, followed by the given arguments, and a footer.
 *
 * @tparam Args The types of the arguments.
 * @param args The arguments to log.
 */
template <class... Args>
inline void debug_log(Args &&...args)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    show("#-------------------BEGIN-----------------#");
    (show(std::forward<Args>(args)), ...);
    show("#--------------------END------------------#");
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}
