#ifndef UTIL_H
#define UTIL_H

#include <arpa/inet.h>

inline float 
float_swap(float value)
{
    union v {
        float       f;
        uint32_t    i;
    };

    v val;
    val.f = value;
    val.i = htonl(val.i);                 
    return val.f;
};

#endif
