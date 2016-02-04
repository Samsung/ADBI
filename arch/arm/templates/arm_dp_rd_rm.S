/* OOL handler for the ARM data processing instructions, where:
 *      rd == rm == pc
 *      rn != pc
 */

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond:
    b ool_handler       /* condition is true, execute instruction OOL */
    ldr pc, tf_next_pc  /* condition is false, return to original code */


ool_handler:

    sub sp, #4          /* allocate one word on the stack */
tf_patch_rt2rs1:
    push { r3 }         /* push temporary register */

tf_patch_rt2rs2:
    ldr r3, tf_read_pc  /* load original pc value into temporary register */

tf_insn:
tf_patch_rm2rs:
tf_patch_rd2rs:
    add r3, r0, r3      /* original instruction with rd and rm replaced by rs */

tf_patch_rt2rs3:
    str r3, [sp, #4]    /* store result in second element on the stack */

tf_patch_rt2rs4:
    pop { r3 }          /* pop temporary register */
    pop { pc }          /* pop the pc */


HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_read_pc:
    .word BAD

HANDLER_END
