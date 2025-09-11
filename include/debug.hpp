#pragma once

#include <iostream>
#include <config.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <unordered_set>

/**
 * @brief Extracts the keys from an unordered_map.
 *
 * @tparam K The key type of the map.
 * @tparam V The value type of the map.
 * @param map The map from which to extract keys.
 * @return A vector containing the keys of the map.
 */
template <class K, class V>
auto keys_of(std::unordered_map<K, V> map) -> std::vector<K>
{
    std::vector<K> keys{};
    std::transform(map.begin(), map.end(), std::back_inserter(keys),
                   [](const auto &pair)
                   { return pair.first; });
    return keys;
}

/**
 * @brief Concept for container-like types.
 *
 * A type is considered a container if it has `begin()`, `end()`, and `size()` methods,
 * and is not a `std::string`.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept Container = !std::same_as<std::decay_t<T>, std::string> && requires(T t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

/**
 * @brief Concept for non-container types.
 *
 * @tparam T The type to check.
 */
template <typename T>
concept NonContainer = !Container<T>;

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
    std::cout << "[DEBUG] -- {}" << std::forward<T>(value) << std::endl;
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
    std::cout << "[DEBUG] -- Container[";
    for (auto &&x : value)
    {
        show(x);
    }
    std::cout << "];" << std::endl;
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
