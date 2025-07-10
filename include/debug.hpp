#pragma once

#include <iostream>
#include <print>
#include <config.h>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iterator>

template <class K, class V>
auto keys_of(std::unordered_map<K, V> map) -> std::vector<K>
{
    std::vector<K> keys{};
    std::transform(map.begin(), map.end(), std::back_inserter(keys),
                   [](const auto &pair)
                   { return pair.first; });
    return keys;
}

template <class T>
inline void show(T &&value)
{
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::println("[DEBUG] >> {}", std::forward<T>(value));
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
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
