#pragma once

#include <iostream>
#include <config.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iterator>
#include <unordered_set>

template <class K, class V>
auto keys_of(std::unordered_map<K, V> map) -> std::vector<K>
{
    std::vector<K> keys{};
    std::transform(map.begin(), map.end(), std::back_inserter(keys),
                   [](const auto &pair)
                   { return pair.first; });
    return keys;
}

template <typename T>
concept Container = !std::same_as<std::decay_t<T>, std::string> && requires(T t) {
    { t.begin() } -> std::input_iterator;
    { t.end() } -> std::input_iterator;
    { t.size() } -> std::convertible_to<std::size_t>;
};

template <typename T>
concept NonContainer = !Container<T>;

template <class T>
    requires NonContainer<T>
inline void show(T &&value)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::cout << "[DEBUG] >> {}" << std::forward<T>(value) << std::endl;
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}

inline void show(Container auto &&value)
{
    std::cout << "[DEBUG] >> Container[";
    for (auto &&x : value)
    {
        std::cout << x << ", ";
    }
    std::cout << "];" << std::endl;
}

template <class... Args>
inline void debug_log(Args &&...args)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    show("#-------------------BEGIN-----------------#");
    (show(std::forward<Args>(args)), ...);
    show("#--------------------END------------------#");
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}
