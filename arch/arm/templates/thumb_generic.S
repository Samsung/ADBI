#include "thumb_handler.h"
HANDLER_BEGIN

tf_insn:
    nop
    /* Jump back. */
    ldr.w pc, tf_next_pc

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD

HANDLER_END
