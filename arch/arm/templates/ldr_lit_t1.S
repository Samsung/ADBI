#include "thumb_handler.h"
HANDLER_BEGIN

tf_patch_rt:
    ldr.n r0, tf_ldr_lit_t1_address
tf_patch_rn2rt:
tf_patch_rt2rt:
    /* The instruction below uses a different encoding, the transferred register rd is encoded as rm (on bits 0-2). */
    ldr.n r0, [r1]
    /* Jump back. */
    ldr.w pc, tf_next_pc

HANDLER_CONSTANTS

tf_ldr_lit_t1_address:
    .word BAD
tf_next_pc:
    .word BAD

HANDLER_END
