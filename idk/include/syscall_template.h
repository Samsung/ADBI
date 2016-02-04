#ifndef SYSCALL_TEMPLATE_H_
#define SYSCALL_TEMPLATE_H_

#include <asm-generic/unistd.h>

#ifndef __NR_mmap
#define __NR_mmap   __NR_mmap2
#endif

#define get_nr2(nr) #nr
#define get_nr(nr) get_nr2(nr)

#ifdef __aarch64__

#define SYSCALL_x_ARGS(nr, rettype, name, ...)                  \
    asm (                                                       \
        "   .section .text." # name ", \"ax\", %progbits    \n" \
        "   .local " # name "                               \n" \
        "   .align                                          \n" \
        "   .type " # name ", %function                     \n" \
        # name ":                                           \n" \
        "   mov    x8, #" nr "                              \n" \
        "   svc    #0                                       \n" \
        "   ret                                             \n" \
        "   .size " # name ", . - " # name "                \n" \
        );                                                      \
    rettype name(__VA_ARGS__)

#define SYSCALL_0_ARGS  SYSCALL_x_ARGS
#define SYSCALL_1_ARGS  SYSCALL_x_ARGS
#define SYSCALL_2_ARGS  SYSCALL_x_ARGS
#define SYSCALL_3_ARGS  SYSCALL_x_ARGS
#define SYSCALL_4_ARGS  SYSCALL_x_ARGS
#define SYSCALL_5_ARGS  SYSCALL_x_ARGS
#define SYSCALL_6_ARGS  SYSCALL_x_ARGS
#define SYSCALL_7_ARGS  SYSCALL_x_ARGS
#define SYSCALL_8_ARGS  SYSCALL_x_ARGS

#else /* __aarch64__ */

/* System calls can take up to 7 arguments.  The arguments to system calls are all passed through registers (r0-r6)
 * and r7 holds the system call number.  Arguments to functions are passed through registers r0-r3 and the stack.
 * To make a system call, we need to take the arguments from the stack and put them into registers.  After the system
 * call returns, the original values of registers (r4-r7) must be restored.
 *
 * Note also that in contrast to Bionic, these functions return the same value as the system call and they never set
 * errno.  Setting errno was removed for simplification -- it would require initialization of a TLS area and other
 * complex TLS-related operations.
 *
 * Before making any system call at least 3 registers are always saved:
 *      r4 -- The Linux kernel may use this register as a scratch register for temporary values (this register is a
 *            special case);
 *      r7 -- This register will be used to hold the system call number, so it will always be clobbered.
 *      lr -- This register does not really have to be stored since it shouldn't be clobbered, but we store it anyway.
 *            All registers are stored in one instruction (push {r4, r7, lr}, so there is no performance penalty for
 *            storing this register).  Having the lr stored, we can later return from the system call and restore
 *            original register values in one instruction (by using pop {r4, r7, pc} instead of pop {r4, r7}; bx lr).
 */

/* System calls, which take 4 or less arguments. */
#define SYSCALL_SIMPLE(nr)                                                  \
    __asm(                                                                  \
            "push    {r4, r7, ip, lr}  \n"                                  \
            "ldr     r7, =" nr "       \n"                                  \
            "swi     #0                \n"                                  \
            "pop     {r4, r7, ip, pc}  \n"                                  \
         );

/* System calls, which take more than 4 arguments. */
#define SYSCALL_ADVANCED(nr, regs, count)                                                                       \
    __asm(                                                                                                      \
            "push    {" # regs ", r7, ip, lr}           \n"  /* store registers */                              \
            "add     ip, sp, #((3 + " # count ") * 4)   \n"  /* load paramters from stack */                    \
            "ldmfd   ip, {" #regs "}                    \n"  /* (BEFORE those we just pushed) */                \
            "ldr     r7, =" nr "                        \n"  /* perform system call */                          \
            "swi     #0                                 \n"                                                     \
            "pop     {" # regs ", r7, ip, pc}           \n"  /* restore registers */                            \
         );

#define SYSCALL_PROTOTYPE(rettype, name, ...)               \
    static rettype NAKED name(__VA_ARGS__)

#define SYSCALL_x_ARGS(nr, rettype, name, ...)              \
    SYSCALL_PROTOTYPE(rettype, name, ## __VA_ARGS__) {      \
        SYSCALL_SIMPLE(nr);                                 \
    }

#define SYSCALL_0_ARGS  SYSCALL_x_ARGS
#define SYSCALL_1_ARGS  SYSCALL_x_ARGS
#define SYSCALL_2_ARGS  SYSCALL_x_ARGS
#define SYSCALL_3_ARGS  SYSCALL_x_ARGS
#define SYSCALL_4_ARGS  SYSCALL_x_ARGS

#define SYSCALL_5_ARGS(nr, rettype, name, ...)              \
    SYSCALL_PROTOTYPE(rettype, name, ## __VA_ARGS__) {      \
        SYSCALL_ADVANCED(nr, r4, 1);                        \
    }

#define SYSCALL_6_ARGS(nr, rettype, name, ...)              \
    SYSCALL_PROTOTYPE(rettype, name, ## __VA_ARGS__) {      \
        SYSCALL_ADVANCED(nr, r4-r5, 2);                     \
    }

#define SYSCALL_7_ARGS(nr, rettype, name, ...)              \
    SYSCALL_PROTOTYPE(rettype, name, ## __VA_ARGS__) {      \
        SYSCALL_ADVANCED(nr, r4-r6, 3);                     \
    }

#endif /* __aarch64__ */

#endif /* SYSCALL_TEMPLATE_H_ */
