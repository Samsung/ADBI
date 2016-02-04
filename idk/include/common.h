#ifndef COMMON_H_
#define COMMON_H_

#define NAKED __attribute__ ((naked))
#define ALWAYS_INLINE static inline __attribute__((always_inline))

#define NULL (0)

typedef unsigned long size_t;
typedef signed long ssize_t;

#define ADBI_HANDLER_ATTR __attribute__((used))

int adbi_dummy() __attribute__((used, weak));
int adbi_dummy() { return 0; }

#endif /* COMMON_H_ */
