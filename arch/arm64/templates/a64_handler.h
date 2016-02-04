#define BADADDR 0xdeadbeefbaadcafe

#define HANDLER_BEGIN                               \
    .global handler;                                \
    .type   handler, %function;                     \
handler:                                            \
    /* store thread context on stack */             \
    stp     x20, x30, [sp, #-0x10]!;                \
    ldr     x30, tf_next_pc;                        \
    mrs     x20, nzcv;                              \
    stp     x29, x30, [sp, #-0xb0]!;                \
    stp     x19, x20, [sp, #0xa0];                  \
    stp     x16, x17, [sp, #0x90];                  \
    stp     x14, x15, [sp, #0x80];                  \
    stp     x12, x13, [sp, #0x70];                  \
    stp     x10, x11, [sp, #0x60];                  \
    stp      x8,  x9, [sp, #0x50];                  \
    stp      x6,  x7, [sp, #0x40];                  \
    stp      x4,  x5, [sp, #0x30];                  \
    stp      x2,  x3, [sp, #0x20];                  \
    stp      x0,  x1, [sp, #0x10];                  \
    /* fp points to frame record */                 \
    mov     x29, sp;                                \
    /* x19 points to context structure */           \
    mov     x19, sp;                                \
    /* call handler */                              \
    ldr     x16, tf_handler_address;                \
    blr     x16;                                    \
    /* restore context */                           \
    ldp      x0,  x1, [sp, #0x10];                  \
    ldp      x2,  x3, [sp, #0x20];                  \
    ldp      x4,  x5, [sp, #0x30];                  \
    ldp      x6,  x7, [sp, #0x40];                  \
    ldp      x8,  x9, [sp, #0x50];                  \
    ldp     x10, x11, [sp, #0x60];                  \
    ldp     x12, x13, [sp, #0x70];                  \
    ldp     x14, x15, [sp, #0x80];                  \
    ldp     x16, x17, [sp, #0x90];                  \
    ldp     x19, x20, [sp, #0xa0];                  \
    msr     nzcv, x20;                              \
    ldp     x20, x30, [sp, #0xb0];                  \
    ldr     x29, [sp], #0xc0;

#define HANDLER_OOL_BEGIN                           \
    .global ool;                                    \
    .type   ool, %function;                         \
ool:

#define HANDLER_OOL_RETURN(patch)                   \
tf_handler_return##patch:                           \
    .word   0x00000000;      /* or brk   #0 */      \

#define HANDLER_OOL_END                             \
    .size ool, . - ool;                             \
    .global constants;                              \
constants:                                          \

#define HANDLER_END                                 \
tf_handler_address:                                 \
    .dword BADADDR;                                 \
tf_next_pc:                                         \
    .dword BADADDR;                                 \
    .size handler, . - handler;                     \
    .size constants, . - constants;
