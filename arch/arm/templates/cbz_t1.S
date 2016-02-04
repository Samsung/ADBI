#include "thumb_handler.h"
HANDLER_BEGIN

tf_patch_cond:
tf_patch_rn:
    cbz r0, yes
no:
    /* Condition false. */
    ldr.w pc, tf_next_pc
yes:
    /* Condition true. */
    ldr.w pc, tf_cbz_t1_target

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_cbz_t1_target:
    .word BAD

HANDLER_END
