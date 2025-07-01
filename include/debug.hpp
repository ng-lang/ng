
#ifndef __NG_DEBUG_HPP
#define __NG_DEBUG_HPP


#include <iostream>
#include <print>
#include <config.h>
#include <vector>
#include <map>
#include <algorithm>
#include <iterator>

template<class K, class V>
std::vector<K> keys_of(std::unordered_map<K, V> map) {
    std::vector<K> keys {};
    std::transform(map.begin(), map.end(), std::back_inserter(keys), [](const auto& pair) { return pair.first; });
    return keys;
}

template<class T>
inline void show(T &&t) {
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::println("[DEBUG] >> {}", t);
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}

template<class... Args>
inline void debug_log(Args &&...args) {
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    show("#-------------------BEGIN-----------------#");
    (show(args), ...);
    show("#--------------------END------------------#");
#endif // NG_CONFIG_ENABLE_DEBUG_LOG
}

#endif // __NG_DEBUG_HPP
