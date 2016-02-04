#ifndef GCC_H_
#define GCC_H_

#include "common.h"

int memcmp(const void * s1, const void * s2, size_t n) __attribute__((weak, alias("__adbi_memcmp")));
void * memset(void * dst, int c, size_t n) __attribute__((weak, alias("__adbi_memset")));
void * memcpy(void * dst, const void * src, size_t count) __attribute__((weak, alias("__adbi_memcpy")));
void * memmove(void * dst, const void * src, size_t count) __attribute__((weak, alias("__adbi_memmove")));

int __adbi_memcmp(const void * s1, const void * s2, size_t n)
{
    const unsigned char * p1   = s1;
    const unsigned char * end1 = p1 + n;
    const unsigned char * p2   = s2;
    int d = 0;

    for(;;) {
        if (d || p1 >= end1)
            break;
        d = (int)*p1++ - (int)*p2++;
    }

    return d;
}

void * __adbi_memset(void * dst, int c, size_t n)
{
    char * q   = dst;
    char * end = q + n;

    for (;;) {
        if (q >= end)
            break;
        *q++ = (char) c;
    }

    return dst;
}

void * __adbi_memcpy(void * dst, const void * src, size_t count) {
    char * dstc = dst;
    char * srcc = src;
    while (count--) {
        *dstc = *srcc;
        ++dstc;
        ++srcc;
    }

    return dst;
}

void * __adbi_memmove(void * dst, const void * src, size_t count) {
    if (src == dst)
        return dst;
    else if ((dst < src) || ((src + count) < dst))
        return memcpy(dst, src, count);

    char * dstc = dst + count;
    char * srcc = src + count;
    while (count--) {
        *dstc = *srcc;
        --dstc;
        --srcc;
    }

    return dst;
}

#endif
