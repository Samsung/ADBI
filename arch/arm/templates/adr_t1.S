#include "thumb_handler.h"
HANDLER_BEGIN

tf_patch_rd:
    /* In the following instruction, r0 will be replaced by the original instruction's destination register rd. */
    ldr.n r0, tf_adr_t1_val
    /* Jump back. */
    ldr.w pc, tf_next_pc

HANDLER_CONSTANTS

tf_adr_t1_val:
    .word BAD
tf_next_pc:
    .word BAD

HANDLER_END
