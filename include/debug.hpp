
#ifndef __NG_DEBUG_HPP
#define __NG_DEBUG_HPP


#include <iostream>

template<class T>
void show(T &&t) {
    std::cout << "[DEBUG] >> " << t << "\n";
}

template<class... Args>
void debug_log(Args &&...args) {
    show("#-------------------BEGIN-----------------#");
    (show(args), ...);
    show("#--------------------END------------------#");
}

#endif // __NG_DEBUG_HPP
