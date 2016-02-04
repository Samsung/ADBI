/* OOL handler for the ARM branch and link instruction with an immediate operand.
 *
 *   Opcode:
 *       cccc 1011 iiii iiii  iiii iiii iiii iiii    BL (immediate)
 */

#include "arm_handler.h"
HANDLER_BEGIN

tf_patch_cond1:
    ldr lr, tf_next_pc      /* set the link register */
tf_patch_cond2:
    ldr pc, tf_bl_imm_a1_target
    ldr pc, tf_next_pc      /* return */

HANDLER_CONSTANTS

tf_next_pc:
    .word BAD
tf_bl_imm_a1_target:
    .word BAD

HANDLER_END
