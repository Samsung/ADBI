#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

	ldr		x30, tf_next_pc;

#ifdef USE_X16_IN_BL_TRAMPOLINE
	ldr		x16, tf_relative_imm26_addr;
	br		x16;
#else
	HANDLER_OOL_RETURN(_to_imm26_)
#endif

HANDLER_OOL_END

#ifdef USE_X16_IN_BL_TRAMPOLINE

tf_relative_imm26_addr:
	.dword BADADDR;

#endif

HANDLER_END
