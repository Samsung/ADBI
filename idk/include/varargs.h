/* Variable argument list support */
#ifndef VARARGS_H_
#define VARARGS_H_

#ifndef _VA_LIST
typedef __builtin_va_list va_list;
#define _VA_LIST
#endif

#define va_start(ap, param) __builtin_va_start(ap, param)
#define va_end(ap)          __builtin_va_end(ap)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)

#define __va_copy(dst, src) __builtin_va_copy(dst, src)

#endif
