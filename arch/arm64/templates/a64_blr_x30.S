#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

	mov		x16, x30
	ldr		x30, tf_next_pc;
	br		x16;

HANDLER_OOL_END

HANDLER_END
