/* Header file for injectable source files.
 *
 * Functions defined in this file allow accessing the original register values.  The ADBI trampolines create a special
 * structure on the stack, which contains some of these values.  Some registers are accessed directly (because the
 * handlers and trampolines don't clobber them).
 */

#ifndef __ADBI_HANDLER_H__
#define __ADBI_HANDLER_H__

/**********************  ACHTUNG!  MODIFICATIONS IN THIS FILE MUST BE DONE WITH GREAT CAUTION!  **********************/

#include "common.h"

#ifdef __aarch64__

/* Stack layout just before the branch to the high-level handler is organized as follows:
 *
 *                    |   original stack   | high addresses
 *                    |____________________|
 *                    |         lr'        |
 *    original fp --> |_________fp'________| <-- original frame record
 *                    |                    |
 *                    |   original stack   |
 *    original sp --> |____________________|
 * tracepoint hit:     ____________________
 *                    |         lr         |
 *                    |         r20        |
 *                    |        nzcv        |
 *                    |         r19        |
 *                    |          :         |
 *                    |_________r0_________|
 *                    |       next_pc      |
 *         sp, fp --> |_________fp_________| <-- trampoline frame record
 *                    |////////////////////|
 *                    |/////// free ///////|
 *                    |////////////////////| low addresses
 */

#include <stdint.h>

typedef uint64_t    regval_t;
typedef uint32_t    regval32_t;

struct frame_record_t {
    regval_t fp;
    regval_t lr;    /* Instruction after traced instruction is our return address */
} __attribute__((packed));

struct context_info_t {
    struct frame_record_t frame_rec;
    regval_t    regs[19];
    regval_t    nzcv;
    regval_t    r20;
    regval_t    lr;
} __attribute__((packed));

register const regval_t r18 asm("x18");

register volatile const struct context_info_t * context_info asm("x19");

register const regval_t r21 asm("x21");
register const regval_t r22 asm("x22");
register const regval_t r23 asm("x23");
register const regval_t r24 asm("x24");
register const regval_t r25 asm("x25");
register const regval_t r26 asm("x26");
register const regval_t r27 asm("x27");
register const regval_t r28 asm("x28");

__attribute__((pure))
static inline regval_t get_reg(unsigned int i) {
    switch (i) {
        case 0 ... 17:
            return context_info->regs[i];
        case 18:
            /* Platform register */
            return r18;
        case 19:
            return context_info->regs[18];
        case 20:
            return context_info->r20;
        case 21:
            return r21;
        case 22:
            return r22;
        case 23:
            return r23;
        case 24:
            return r24;
        case 25:
            return r25;
        case 26:
            return r26;
        case 27:
            return r27;
        case 28:
            return r28;
        case 29:
            return context_info->frame_rec.fp;
        case 30:
            return context_info->lr;
        case 31:
            return 0;
    }
}

/* Returns the value of the NZCV register. */
__attribute__((pure))
static inline regval_t get_nzcv() {
    return context_info->nzcv;
}

__attribute__((pure))
static inline regval_t get_next_pc() {
    return context_info->frame_rec.lr;
}

__attribute__((pure))
static inline regval_t get_pc() {
    return context_info->frame_rec.lr - 4;
}

__attribute__((pure))
static inline regval_t get_sp() {
    return ((regval_t) context_info) + sizeof(struct context_info_t);
}

#else /* __aarch64__ */

/* Stack layout just before the branch to the high-level handler is organized as follows:
 *
 *                  |                    | high addresses
 *                  |   original stack   |
 *  original sp --> |____________________|
 *                  |         lr         |
 *                  |         ip         |
 *                  |         fp         |
 *                  |         r6         |
 *                  |         r5         |
 *                  |         r4         |
 *                  |         r3         |
 *                  |         r2         |
 *                  |         r1         |
 *                  |         r0         |
 *                  |         pc         |
 *           sp --> |________cpsr________| <-- ip
 *                  |////////////////////|
 *                  |/////// free ///////|
 *                  |////////////////////| low addresses
 *
 *
 * Before the jump to the high-level handler the stack pointer points to the next free stack cell, following the stored
 * registers (so the stored registers are just at the top of the stack).  However, the handler will use the stack for
 * local variables and it might be difficult to restore the original value.  For this reason the trampolines
 * additionally set the ip register (r12) to the address of the structure.
 *
 * Of course, by default, the ip register is used by GCC as a scratch register just like r0-r3.  To prohibit allocation
 * of ip for local variables, the GCC option -ffixed-ip is used during compilation.  Additionally, in the code there is
 * a special global register variable defined as follows:
 *
 *      register struct context_info_t * context_info asm("ip");
 *
 * The context_info_t structure is a C equivalent of the structure created in assembly.  This approach allows easy
 * access to the stored information and prevents clobbering ip.
 *
 * Note that general purpose high registers r8-r11 are not saved at all.  This is because they are used less often in
 * code generated by GCC, especially in Thumb mode.  These registers are, just like the ip register, accessible through
 * global register variables, for example:
 *      register unsigned int r8 asm("r8");
 *
 * In this case it is not necessary to use the -ffixed-reg option, because the r8-r11 registers are not scratch
 * registers by default -- the register allocator will simply see those registers as already allocated.  Note, however,
 * that the r11 register is sometimes used as a frame pointer (fp).  If frame pointer usage is enabled (with the GCC
 * -f-no-omit-frame-pointer option), the handler will clobber this value.  Still, the original fp value should be
 * correctly restored during handler return.
 *
 * A special case is the original value of the stack pointer (sp or r13).  This value can be calculated by adding a
 * constant value to ip.
 *
 * It also must be noted that the stored pc value has a the least significant bit set if the original instruction was
 * a Thumb instruction.  To get the address of the instruction this bit should be masked out.
 */

typedef unsigned int regval_t;

/* The order of the fields in this structure must reflect the structure created on the stack by the trampolines. */
struct context_info_t {
    regval_t cpsr;
    regval_t pc;
    regval_t lowregs[7];
    regval_t fp;
    regval_t ip;
    regval_t lr;
} __attribute__((packed));

/* Pointer to the structure on the stack.  IP must not be clobbered (use -ffixed-ip). */
register volatile const struct context_info_t * context_info asm("ip");

/* Raw register values.  These values must not be modified. */
register const regval_t r7 asm("r7");
register const regval_t r8 asm("r8");
register const regval_t r9 asm("r9");
register const regval_t r10 asm("r10");
//register const regval_t r11 asm("r11");

/* Returns the given register value. */
__attribute__((pure))
static inline regval_t get_reg(unsigned int i) {
    switch (i) {
        case 0 ... 6:
            /* low registers r0-r6 */
            return context_info->lowregs[i];
        case 7:
            return r7;
        case 8:
            return r8;
        case 9:
            return r9;
        case 10:
            return r10;
        case 11:    /* fp */
            return context_info->fp;
        case 12:    /* ip */
            return context_info->ip;
        case 13:    /* sp */
            return ((regval_t) context_info) + sizeof(struct context_info_t);
        case 14:    /* lr */
            return context_info->lr;
        case 15:    /* pc */
            return context_info->pc;
    }
    return 0xffffffff;
}

/* Returns the value of the CPSR register. */
__attribute__((pure))
static inline regval_t get_cpsr() {
    return context_info->cpsr;
}


/* CPSR register */
#define PSR_f 0xff000000
#define PSR_s 0x00ff0000
#define PSR_x 0x0000ff00
#define PSR_c 0x000000ff

#define PSR_N_BIT     0x80000000
#define PSR_Z_BIT     0x40000000
#define PSR_C_BIT     0x20000000
#define PSR_V_BIT     0x10000000
#define PSR_Q_BIT     0x08000000
#define PSR_IT01_MASK 0x06000000
#define PSR_J_BIT     0x01000000
#define PSR_GE_MASK   0x000f0000
#define PSR_IT27_MASK 0x0000fc00
#define PSR_E_BIT     0x00000200
#define PSR_A_BIT     0x00000100
#define PSR_I_BIT     0x00000080
#define PSR_F_BIT     0x00000040
#define PSR_T_BIT     0x00000020

#define cpsr_q(cpsr)  (!!((cpsr) & PSR_Q_BIT))
#define cpsr_ge(cpsr) (((cpsr) & CPSR_GE_MASK) >> 16)


/* ENDIANSTATE */
#define cpsr_e(cpsr)  (!!((cpsr) & PSR_E_BIT))

#define endianstate_is_little_endian(cpsr)  (!cpsr_e(cpsr))
#define endianstate_is_big_endian(cpsr)     (cpsr_e(cpsr))


/* Mask bits */
#define cpsr_a(cpsr)  (!!((cpsr) & PSR_A_BIT))
#define cpsr_i(cpsr)  (!!((cpsr) & PSR_I_BIT))
#define cpsr_f(cpsr)  (!!((cpsr) & PSR_F_BIT))


/* ITSTATE */
#define cpsr_it(cpsr) ({ typeof(cpsr) _cpsr = cpsr; ((_cpsr & PSR_IT27_MASK) >> 10) | ((_cpsr & PSR_IT01_MASK) >> 25); })
// TODO ITSTATE


/* ARM processor mode */
#define USR26_MODE  0x00000000
#define FIQ26_MODE  0x00000001
#define IRQ26_MODE  0x00000002
#define SVC26_MODE  0x00000003
#define USR_MODE    0x00000010
#define FIQ_MODE    0x00000011
#define IRQ_MODE    0x00000012
#define SVC_MODE    0x00000013
#define ABT_MODE    0x00000017
#define UND_MODE    0x0000001b
#define SYSTEM_MODE 0x0000001f
#define MODE32_BIT  0x00000010

#define MODE_MASK   0x0000001f

#define cpsr_m(cpsr)  ((cpsr) & MODE_MASK)


/* Conditional execution */
#define cpsr_n(cpsr)  (!!((cpsr) & PSR_N_BIT))
#define cpsr_z(cpsr)  (!!((cpsr) & PSR_Z_BIT))
#define cpsr_c(cpsr)  (!!((cpsr) & PSR_C_BIT))
#define cpsr_v(cpsr)  (!!((cpsr) & PSR_V_BIT))

#define cpsr_is_eq(cpsr) (cpsr_z((cpsr)) == 1)
#define cpsr_is_ne(cpsr) (cpsr_z((cpsr)) == 0)
#define cpsr_is_cs(cpsr) (cpsr_c((cpsr)) == 1)
#define cpsr_is_cc(cpsr) (cpsr_c((cpsr)) == 0)
#define cpsr_is_mi(cpsr) (cpsr_n((cpsr)) == 1)
#define cpsr_is_pl(cpsr) (cpsr_n((cpsr)) == 0)
#define cpsr_is_vs(cpsr) (cpsr_v((cpsr)) == 1)
#define cpsr_is_vc(cpsr) (cpsr_v((cpsr)) == 0)
#define cpsr_is_hi(cpsr) (((cpsr) & (PSR_C_BIT | PSR_Z_BIT)) == PSR_C_BIT)
#define cpsr_is_ls(cpsr) (((cpsr) & (PSR_C_BIT | PSR_Z_BIT)) == PSR_Z_BIT)
#define cpsr_is_ge(cpsr) ({ typeof(cpsr) _cpsr = cpsr; cpsr_n((_cpsr)) == cpsr_v((_cpsr)); })
#define cpsr_is_lt(cpsr) ({ typeof(cpsr) _cpsr = cpsr; cpsr_n((_cpsr)) != cpsr_v((_cpsr)); })
#define cpsr_is_gt(cpsr) ({ typeof(cpsr) _cpsr = cpsr; !cpsr_z((_cpsr)) && (cpsr_n((_cpsr)) == cpsr_v((_cpsr))); })
#define cpsr_is_le(cpsr) ({ typeof(cpsr) _cpsr = cpsr; cpsr_z((_cpsr)) || (cpsr_n((_cpsr)) != cpsr_v((_cpsr))); })
#define cpsr_is_al(cpsr) (1)


/* ISETSTATE */
#define cpsr_j(cpsr)  (!!((cpsr) & PSR_J_BIT))
#define cpsr_t(cpsr)  (!!((cpsr) & PSR_T_BIT))

#define ISETSTATE_ARM       0x0
#define ISETSTATE_Thumb     0x1
#define ISETSTATE_Jazelle   0x2
#define ISETSTATE_ThumbEE   0x3

#define isetstate(cpsr)             ({ typeof(cpsr) _cpsr = cpsr; ((cpsr_j(_cpsr)) << 1) | (cpsr_t(_cpsr)); })

#define isetstate_is_arm(cpsr)      (isetstate(cpsr) == ISETSTATE_ARM)
#define isetstate_is_thumb(cpsr)    (isetstate(cpsr) == ISETSTATE_Thumb)
#define isetstate_is_jazelle(cpsr)  (isetstate(cpsr) == ISETSTATE_Jazelle)
#define isetstate_is_thumbee(cpsr)  (isetstate(cpsr) == ISETSTATE_ThumbEE)

#endif /* __aarch64__ */

#endif
