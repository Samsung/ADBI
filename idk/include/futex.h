#ifndef FUTEX_H_
#define FUTEX_H_

#include "common.h"

#include "syscall_template.h"

#include "time.h"

#define FUTEX_PRIVATE_FLAG  128
#define FUTEX_WAIT 0
#define FUTEX_WAKE 1

#define FUTEX_WAIT_PRIVATE (FUTEX_WAIT | FUTEX_PRIVATE_FLAG)
#define FUTEX_WAKE_PRIVATE (FUTEX_WAKE | FUTEX_PRIVATE_FLAG)

#ifdef __aarch64__

SYSCALL_6_ARGS(get_nr(__NR_futex),
        int, futex, int * uaddr, int op, int val, const struct timespec * timeout, int * uaddr2, int val3);

#else

SYSCALL_6_ARGS(get_nr(240),
        int, futex, int * uaddr, int op, int val, const struct timespec * timeout, int * uaddr2, int val3);

#endif

#endif /* FUTEX_H_ */
