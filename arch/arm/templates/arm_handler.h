#define BAD             0xbaadf00d

#define HANDLER_BEGIN                                                                       \
    .arm;                                                                                   \
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
 
/* All ARM instructions are 4 bytes long and should be word-aligned in memory, so the alignment should never be used.
 * Still, we're using 0xff bytes, which should cause an undefined instruction exception on most processors.
 */
#define HANDLER_CONSTANTS                           \
    .size ool, . - ool;                             \
    .align 2, 0xff;                                 \
    .global constants;                              \
    constants:                                      \
 
#define HANDLER_END                                 \
    tf_handler_address:                             \
    .word BAD;                                      \
    tf_orig_pc:                                     \
    .word BAD;                                      \
    .size handler, . - handler;                     \
    .size constants, . - constants;
