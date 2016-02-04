#include "a64_handler.h"

HANDLER_BEGIN

HANDLER_OOL_BEGIN

tf_patch_cond:
	b.al	yes;
no:
	HANDLER_OOL_RETURN()
yes:
	HANDLER_OOL_RETURN(_to_imm19_)

HANDLER_OOL_END

HANDLER_END
