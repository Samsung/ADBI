#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_sf2sf:
tf_patch_op2op:
tf_patch_rt2rt:
	cbz	x16, yes;
no:
	HANDLER_OOL_RETURN()
yes:
	HANDLER_OOL_RETURN(_to_imm19_)

HANDLER_OOL_END

HANDLER_END
