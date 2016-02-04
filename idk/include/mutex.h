/* ADBI mutex implementation.
 *
 * Mutexes are implemented using the futex syscall.  This makes them very fast because system calls are only executed
 * when really necessary, i.e:
 *      * Acqiring a mutex lock causes no system call if it's not locked already (so there's no need to wait).
 *      * Releasing a mutex lock causes no system call if no other threads are waiting.
 *
 * The implementation is based on the pthread mutex implementation in the Bionic C library.
 */

#ifndef MUTEX_H_
#define MUTEX_H_

#include "common.h"
#include "futex.h"

#ifdef __aarch64__

/* Atomic decrement, without explicit barriers.  */
ALWAYS_INLINE int atomic_dec(volatile int * ptr) {
    int prev, tmp, status;

    asm volatile (
        "1:     ldxr    %w0, [%4]           \n"
        "       sub     %w1, %w0, #1        \n"
        "       stxr    %w2, %w1, [%4]      \n"
        "       cbnz    %w2, 1b             \n"
        : "=&r" (prev), "=&r" (tmp), "=&r" (status), "+m"(*ptr)
        : "r" (ptr)
        : "cc", "memory");

    return prev;
}

/* Atomic increment, without explicit barriers.  */
ALWAYS_INLINE int atomic_inc(volatile int * ptr) {
    int prev, tmp, status;

    asm volatile (
        "1:     ldxr    %w0, [%4]           \n"
        "       add     %w1, %w0, #1        \n"
        "       stxr    %w2, %w1, [%4]      \n"
        "       cbnz    %w2, 1b             \n"
        : "=&r" (prev), "=&r" (tmp), "=&r" (status), "+m"(*ptr)
        : "r" (ptr)
        : "cc", "memory");

    return prev;
}

/* Compare-and-swap, without any explicit barriers. Note that this function
 * returns 0 on success, and 1 on failure. The opposite convention is typically
 * used on other platforms.
 */
ALWAYS_INLINE int atomic_cmpxchg(int old_value, int new_value, volatile int * ptr) {
    int tmp, oldval;

    asm volatile (
        "1:   ldxr    %w1, [%3]             \n"
        "     cmp     %w1, %w4              \n"
        "     b.ne    2f                    \n"
        "     stxr    %w0, %w5, [%3]        \n"
        "     cbnz    %w0, 1b               \n"
        "2:                                 \n"
        : "=&r" (tmp), "=&r" (oldval), "+o"(*ptr)
        : "r" (ptr), "Ir" (old_value), "r" (new_value)
        : "cc", "memory");

    return oldval != old_value;
}

/* Swap, without any explicit barriers.  */
ALWAYS_INLINE int atomic_swap(int new_value, volatile int * ptr) {
    int prev, status;

    asm volatile (
        "1:   ldxr    %w0, [%3]             \n"
        "     stxr    %w1, %w4, [%3]        \n"
        "     cbnz    %w1, 1b               \n"
        : "=&r" (prev), "=&r" (status), "+o" (*ptr)
        : "r" (ptr), "r" (new_value)
        : "cc", "memory");

    return prev;
}

#else

/* Atomically decrement the integer pointed by ptr. */
static NAKED int atomic_dec(volatile int * ptr) {
    asm("		mov 	r1, r0			\n"
        "1: 	ldrex	r0, [r1]		\n"
        "		sub 	r2, r0, #1		\n"
        "		strex  	r3, r2, [r1]    \n"
        "		cmp 	r3, #0			\n"
        "		bxeq	lr				\n"
        "		b		1b				\n");
}

/* Atomically increment the integer pointed by ptr. */
static NAKED int atomic_inc(volatile int * ptr) {
    asm("		mov 	r1, r0			\n"
        "1: 	ldrex	r0, [r1]		\n"
        "		add 	r2, r0, #1		\n"
        "		strex  	r3, r2, [r1]    \n"
        "		cmp 	r3, #0			\n"
        "		bxeq	lr				\n"
        "		b		1b				\n");
}

/* Atomically compare and conditionally set the value pointed by ptr to _new.
 *
 * The function is equivalent to the following C code (except that it should be executed atomically):
 *      if (*ptr == old) {
 *          *ptr = _new;
 *          return 0;
 *      } else {
 *          return 2;
 *      }
 */
static NAKED int atomic_cmpxchg(int old, int _new, volatile int * ptr) {
    asm("1:  ldrex   r3, [r2] 			\n"      /* int t = *ptr                     */
        "    teq     r0, r3 			\n"      /* if (t != old)                    */
        "    movne	 r3, #2				\n"      /*      { t = 2 }                   */
        "    strexeq r3, r1, [r2] 		\n"      /* else { t = strex(_new, ptr) }    */
        "    teq     r3, #1       		\n"      /* if (t == 1)                      */
        "    beq     1b           		\n"      /*      { goto 1; }                 */
        "    mov     r0, r3       		\n"      /* return t;                        */
        "    bx      lr 				\n"
       );
}

/* Atomically set the value pointed by ptr to val and return the old value of *ptr. */
static NAKED int atomic_swap(int val, volatile int * ptr) {
    asm("1:  ldrex   r2, [r1] 			\n"
        "    strex   r3, r0, [r1]		\n"
        "    teq     r3, #1       		\n"
        "    beq     1b           		\n"
        "    mov     r0, r2       		\n"
        "    bx      lr 				\n"
       );
}

#endif /*__aarch64__ */

/* A mutex is simply an integer (a 32 bit value, ARM processor word). */
typedef int mutex_t;

/* Acquires a mutex lock. */
static void mutex_unlock(mutex_t * mutex) {
    if (atomic_dec(mutex) != 1) {
        *mutex = 0;
        futex(mutex, FUTEX_WAKE_PRIVATE, 1, NULL, NULL, 0);
    }
}

/* Releases a mutex lock. */
static void mutex_lock(mutex_t * mutex) {
    if (atomic_cmpxchg(0, 1, mutex) != 0) {
        while (atomic_swap(2, mutex) != 0)
            futex(mutex, FUTEX_WAIT_PRIVATE, 2, NULL, NULL, 0);
    }
}

#endif /* MUTEX_H_ */
