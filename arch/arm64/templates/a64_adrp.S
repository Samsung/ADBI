#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_rt2rt:
	ldr	x16, tf_adrp_result
	HANDLER_OOL_RETURN()

HANDLER_OOL_END

tf_adrp_result:
	.dword BADADDR;

HANDLER_END
