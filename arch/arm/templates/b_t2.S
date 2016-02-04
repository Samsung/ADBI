#include "thumb_handler.h"
HANDLER_BEGIN
    ldr.w pc, tf_b_t2_target

HANDLER_CONSTANTS

tf_b_t2_target:
    .word BAD

HANDLER_END
