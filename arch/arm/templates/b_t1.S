#include "thumb_handler.h"
HANDLER_BEGIN

tf_patch_cond:
    beq.n yes
no:
    /* Condition false. */
    ldr.w pc, tf_next_pc
yes:
    /* Condition true. */
    ldr.w pc, tf_b_t1_target

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_b_t1_target:
    .word BAD

HANDLER_END
