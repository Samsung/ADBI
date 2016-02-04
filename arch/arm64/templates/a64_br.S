#include "a64_handler.h"

/* trampoline for br and ret instructions */

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_rn2rn:
	br		x16

HANDLER_OOL_END

HANDLER_END
