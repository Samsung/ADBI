#include "thumb_handler.h"
HANDLER_BEGIN
    /* Set link register. */
    ldr.w lr, tf_next_pc
    /* Branch. */
    ldr.w pc, tf_bl_t1_target

HANDLER_CONSTANTS

tf_bl_t1_target:
    .word BAD
tf_next_pc:
    .word BAD

HANDLER_END
