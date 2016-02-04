/*
    OOL handler for the blx (register) instruction using encoding A1.

    Notes:
        * The handler sets the link register explicitly -- it loads the address of the next instruction in the original
          process segment.  This approach is similar to tail recursion.  However, in our case this is not only an
          optimization.  Note that the handler could be removed from memory after the branch and before return.  In this
          case the return address would become invalid.
        * The destination register can be the link register itself (i.e. blx lr). This is not as uncommon as it might
          seem.  In some cases optimization may choose the link register as the destination register to not clobber
          other registers.  To properly handle these cases as well, we first push the destination register onto the
          stack, then we set the new link register value, and then we finally pop the pushed value into the pc.
*/

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond1:
tf_patch_rt2rm:
    /* conditionally push the destination register (rm) onto the stack */
    push { r0 }
tf_patch_cond2:
    /* conditionally set the link register */
    ldr lr, tf_next_pc
tf_patch_cond3:
    /* conditionally pop the destination register into pc -- this will cause the actual (interworking) branch */
    pop { pc }

    /* this gets executed unconditionally, only when there was no branch */
    ldr pc, tf_next_pc

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD

HANDLER_END
