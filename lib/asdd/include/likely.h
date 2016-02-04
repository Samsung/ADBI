#ifndef _LIKELY_H
#define _LIKELY_H

#ifndef __GNUC__
#define __builtin_expect(a, b)  (a)
#endif

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#endif