#include "thumb_handler.h"
HANDLER_BEGIN
    ldr.w lr, tf_next_pc
tf_patch_rm:
    bx r0

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD

HANDLER_END
