#include "thumb_handler.h"
HANDLER_BEGIN

tf_patch_rt:
    ldr.w r0, tf_ldr_lit_t2_address
tf_patch_rn2rt:
tf_patch_rt2rt:
    ldr.w r0, [r1]
    /* Jump back. */
    ldr.w pc, tf_next_pc

HANDLER_CONSTANTS

tf_ldr_lit_t2_address:
    .word BAD
tf_next_pc:
    .word BAD

HANDLER_END
