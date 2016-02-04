#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_sf2sf:		/* b5 */
tf_patch_b40_2_b40_:
tf_patch_op2op:
tf_patch_rt2rt:
	tbz	x16, #0, yes;
no:
	HANDLER_OOL_RETURN()
yes:
	HANDLER_OOL_RETURN(_to_imm14_)

HANDLER_OOL_END

HANDLER_END
