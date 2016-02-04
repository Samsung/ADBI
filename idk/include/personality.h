#ifndef PERSONALITY_H_
#define PERSONALITY_H_

#include "syscall_template.h"

/*
 * Flags for bug emulation.
 *
 * These occupy the top three bytes.
 */
enum {
    UNAME26 =           0x0020000,
    ADDR_NO_RANDOMIZE = 0x0040000,  /* disable randomization of VA space */
    FDPIC_FUNCPTRS =    0x0080000,  /* userspace function ptrs point to descriptors
                                     * (signal handling)
                                     */
    MMAP_PAGE_ZERO =    0x0100000,
    ADDR_COMPAT_LAYOUT = 0x0200000,
    READ_IMPLIES_EXEC = 0x0400000,
    ADDR_LIMIT_32BIT =  0x0800000,
    SHORT_INODE =       0x1000000,
    WHOLE_SECONDS =     0x2000000,
    STICKY_TIMEOUTS =   0x4000000,
    ADDR_LIMIT_3GB =    0x8000000,
};

#ifdef __aarch54__

SYSCALL_1_ARGS(get_nr(__NR_personality),
        int, personality, unsigned long persona);

#else

SYSCALL_1_ARGS(get_nr(136),
        int, personality, unsigned long persona);

#endif

#endif /* PERSONALITY_H_ */
