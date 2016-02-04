#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

	ldr		x30, tf_next_pc;

tf_patch_rn2rn:
	br		x16;

HANDLER_OOL_END

HANDLER_END
