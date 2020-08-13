
#ifndef __NG_DEBUG_HPP
#define __NG_DEBUG_HPP


#include <iostream>
#include <config.h>

template<class T>
inline void show(T &&t) {
#ifdef NG_CONFIG_ENABLE_DEBUG_LOG
    std::cout << "[DEBUG] >> " << t << "\n";
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
