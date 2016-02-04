#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_rt2rt_0:
	ldr	x16, tf_relative_imm19_addr
tf_patch_rt2rt_1:
tf_patch_rn2rt:
tf_patch_ldr_size_lit2reg:
	ldr x16, [x16]

	HANDLER_OOL_RETURN()

HANDLER_OOL_END

tf_relative_imm19_addr:
	.dword	BADADDR

HANDLER_END
