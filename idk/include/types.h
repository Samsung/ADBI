#ifndef __ADBI_TYPES_H__
#define __ADBI_TYPES_H__

/* Further declarations heavily depend on features available only in GCC. */
#ifndef __GNUC__
#error "Incompatible compiler."
#endif


/* Make sure the sizes of integer types match the values defined by the GNU C standard. */
#if __SIZEOF_SHORT__ != 2
#error "sizeof(short) != 2"
#endif

#if __SIZEOF_INT__ != 4
#error "sizeof(int) != 4"
#endif

#if __SIZEOF_LONG_LONG__ != 8
#error "sizeof(long long) != 8"
#endif


/* Define the floating point type macros. */
#undef  __adbi_fp32_t
#undef  __adbi_fp64_t
#undef  __adbi_fp128_t

#if   __SIZEOF_FLOAT__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   float
#elif __SIZEOF_FLOAT__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   float
#elif __SIZEOF_FLOAT__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  float
#endif

#if   __SIZEOF_DOUBLE__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   double
#elif __SIZEOF_DOUBLE__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   double
#elif __SIZEOF_DOUBLE__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  double
#endif

#if   __SIZEOF_LONG_DOUBLE__ == 4
#undef  __adbi_fp32_t
#define __adbi_fp32_t   long double
#elif __SIZEOF_LONG_DOUBLE__ == 8
#undef  __adbi_fp64_t
#define __adbi_fp64_t   long double
#elif __SIZEOF_LONG_DOUBLE__ == 16
#undef  __adbi_fp128_t
#define __adbi_fp128_t  long double
#endif


/* Warn about missing types. */
#ifndef __adbi_fp32_t
#warning "Architecture does not support 32 bit floating point values."
#endif

#ifndef __adbi_fp64_t
#warning "Architecture does not support 64 bit floating point values."
#endif

#ifndef __adbi_fp128_t
/* 128-bit floats are unusual.  Don't complain if they are missing. */
// #warning Architecture does not support 128 bit floating point values.
#endif

#endif