.syntax unified

#define BAD             0xbaadf00d

#define HANDLER_BEGIN                                                                       \
    .thumb;                                                                                 \
    .global handler;                                                                        \
    .type   handler, %function;                                                             \
    handler:                                                                                \
    /* store scratch registers */                                                           \
    push { r0-r6, fp, ip, lr };                                                             \
    /* store original CPSR and PC */                                                        \
    mrs r0, CPSR;                                                                           \
    ldr r1, tf_orig_pc;                                                                     \
    push { r0, r1 };                                                                        \
    /* prepare register info structure */                                                   \
    add ip, sp, #0;                                                                         \
    /* call high-level handler */                                                           \
    ldr r3, tf_handler_address;                                                             \
    blx r3;                                                                                 \
    /* restore original CPSR */                                                             \
    pop { r0, r1 };                                                                         \
    msr CPSR_f, r0;                                                                         \
    /* restore other registers */                                                           \
    pop { r0-r6, fp, ip, lr };                                                              \
    .global ool;                                                                            \
    .type   ool, %function;                                                                 \
    ool:                                                                                    \
 
/* Constants must be word-aligned for loading from code. We align using a 0xde byte, because all instructions
 * beginning with 0xde are permanently undefined. This way an error can easily be catched (if a handler
 * doesn't exit properly). Still, the alignment might be unnecessary. */
#define HANDLER_CONSTANTS                           \
    .size ool, . - ool;                             \
    .align 2, 0xde;                                 \
    .global constants;                              \
    constants:                                      \
 
#define HANDLER_END                                 \
    tf_handler_address:                             \
    .word BAD;                                      \
    tf_orig_pc:                                     \
    .word BAD;                                      \
    .size handler, . - handler;                     \
    .size constants, . - constants;                 \
 
