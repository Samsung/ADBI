#include "thumb_handler.h"
HANDLER_BEGIN
    /* Branch. */
    ldr.w pc, tf_b_t4_target

HANDLER_CONSTANTS

tf_b_t4_target:
    .word BAD

HANDLER_END
