#include "thumb_handler.h"
HANDLER_BEGIN
    /* nop -- nothing to do, just jump back */
    ldr.w pc, tf_next_pc
HANDLER_CONSTANTS
tf_next_pc:
    .word BAD

HANDLER_END
